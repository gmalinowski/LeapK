/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <sys/types.h>
#include <unistd.h>

namespace OutlineAudio {

inline QSet<pid_t> parsePactlSinkInputs(const QString &pactlOutput)
{
    QSet<pid_t> playingPids;
    if (pactlOutput.isEmpty()) {
        return playingPids;
    }

    const QStringList lines = pactlOutput.split(QLatin1Char('\n'));
    bool inBlock = false;
    bool isCorked = false;
    bool isMuted = false;
    pid_t blockPid = 0;

    auto finishBlock = [&]() {
        if (inBlock && !isCorked && !isMuted && blockPid > 0) {
            playingPids.insert(blockPid);
        }
        inBlock = false;
        isCorked = false;
        isMuted = false;
        blockPid = 0;
    };

    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.startsWith(QStringLiteral("Sink Input #"))) {
            finishBlock();
            inBlock = true;
            continue;
        }

        if (!inBlock) {
            continue;
        }

        if (line.startsWith(QStringLiteral("Corked:"))) {
            isCorked = line.contains(QStringLiteral("yes"), Qt::CaseInsensitive);
        } else if (line.startsWith(QStringLiteral("Mute:"))) {
            isMuted = line.contains(QStringLiteral("yes"), Qt::CaseInsensitive);
        } else if (line.contains(QStringLiteral("application.process.id"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                bool ok = false;
                const pid_t parsed = static_cast<pid_t>(val.toLongLong(&ok));
                if (ok && parsed > 0) {
                    blockPid = parsed;
                }
            }
        }
    }
    finishBlock();

    return playingPids;
}

inline pid_t getParentPid(pid_t pid)
{
    if (pid <= 1) {
        return 0;
    }
    QFile f(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0;
    }
    const QByteArray line = f.readLine();
    const int lastParen = line.lastIndexOf(')');
    if (lastParen < 0 || lastParen + 2 >= line.size()) {
        return 0;
    }
    const QByteArray rest = line.mid(lastParen + 2).trimmed();
    const QList<QByteArray> parts = rest.split(' ');
    if (parts.size() >= 2) {
        bool ok = false;
        const pid_t ppid = static_cast<pid_t>(parts.at(1).toInt(&ok));
        return ok ? ppid : 0;
    }
    return 0;
}

inline bool isWindowPidPlaying(pid_t winPid, const QSet<pid_t> &playingPids, int maxDepth = 4)
{
    if (winPid <= 0 || playingPids.isEmpty()) {
        return false;
    }
    if (playingPids.contains(winPid)) {
        return true;
    }

    // Check if any playing PID is a child/descendant of winPid
    for (pid_t p : playingPids) {
        pid_t curr = p;
        for (int d = 0; d < maxDepth; ++d) {
            const pid_t ppid = getParentPid(curr);
            if (ppid <= 1) {
                break;
            }
            if (ppid == winPid) {
                return true;
            }
            curr = ppid;
        }
    }

    return false;
}

inline QSet<pid_t> queryMprisPlayingPids()
{
    QSet<pid_t> pids;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return pids;
    }

    auto iface = bus.interface();
    if (!iface) {
        return pids;
    }

    const QStringList services = iface->registeredServiceNames();
    for (const QString &svc : services) {
        if (!svc.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
            continue;
        }

        QDBusInterface player(svc, QStringLiteral("/org/mpris/MediaPlayer2"),
                              QStringLiteral("org.mpris.MediaPlayer2.Player"), bus);
        if (!player.isValid()) {
            continue;
        }

        const QVariant statusVar = player.property("PlaybackStatus");
        if (statusVar.isValid() && statusVar.toString() == QStringLiteral("Playing")) {
            QDBusReply<uint> pidReply = iface->servicePid(svc);
            if (pidReply.isValid() && pidReply.value() > 0) {
                pids.insert(static_cast<pid_t>(pidReply.value()));
            }
        }
    }
    return pids;
}

class AudioTracker {
public:
    AudioTracker() = default;

    void refresh()
    {
        QSet<pid_t> pids;

        // 1. Query MPRIS via D-Bus
        const QSet<pid_t> mprisPids = queryMprisPlayingPids();
        pids.unite(mprisPids);

        // 2. Query pactl list sink-inputs if available
        QProcess proc;
        proc.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sink-inputs")});
        if (proc.waitForFinished(100)) {
            const QString pactlOut = QString::fromUtf8(proc.readAllStandardOutput());
            pids.unite(parsePactlSinkInputs(pactlOut));
        }

        m_playingPids = pids;
        m_lastRefresh = QDateTime::currentDateTime();
    }

    bool isWindowPlaying(pid_t winPid) const
    {
        return isWindowPidPlaying(winPid, m_playingPids);
    }

    const QSet<pid_t> &playingPids() const
    {
        return m_playingPids;
    }

    bool hasPlayingAudio() const
    {
        return !m_playingPids.isEmpty();
    }

    void setMockPids(const QSet<pid_t> &pids)
    {
        m_playingPids = pids;
    }

private:
    QSet<pid_t> m_playingPids;
    QDateTime m_lastRefresh;
};

} // namespace OutlineAudio
