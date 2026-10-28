/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <sys/types.h>
#include <unistd.h>

namespace OutlineAudio {

struct AudioStream {
    pid_t pid = 0;
    QString title;   // media.name or xesam:title
    QString appName; // application.name
};

inline QList<AudioStream> parsePactlSinkInputsDetailed(const QString &pactlOutput)
{
    QList<AudioStream> streams;
    if (pactlOutput.isEmpty()) {
        return streams;
    }

    const QStringList lines = pactlOutput.split(QLatin1Char('\n'));
    bool inBlock = false;
    bool isCorked = false;
    bool isMuted = false;
    pid_t blockPid = 0;
    QString blockMediaName;
    QString blockAppName;

    auto finishBlock = [&]() {
        if (inBlock && !isCorked && !isMuted && blockPid > 0) {
            streams.append({blockPid, blockMediaName.trimmed(), blockAppName.trimmed()});
        }
        inBlock = false;
        isCorked = false;
        isMuted = false;
        blockPid = 0;
        blockMediaName.clear();
        blockAppName.clear();
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
        } else if (line.contains(QStringLiteral("pulse.corked"))) {
            if (line.contains(QStringLiteral("\"true\"")) || line.contains(QStringLiteral("true"))) {
                isCorked = true;
            }
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
        } else if (line.contains(QStringLiteral("media.name"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                blockMediaName = val.trimmed();
            }
        } else if (line.contains(QStringLiteral("application.name"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                blockAppName = val.trimmed();
            }
        }
    }
    finishBlock();

    return streams;
}

inline QSet<pid_t> parsePactlSinkInputs(const QString &pactlOutput)
{
    QSet<pid_t> pids;
    const auto streams = parsePactlSinkInputsDetailed(pactlOutput);
    for (const auto &s : streams) {
        pids.insert(s.pid);
    }
    return pids;
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

inline bool isPidRelated(pid_t a, pid_t b, int maxDepth = 4)
{
    if (a <= 0 || b <= 0) {
        return false;
    }
    if (a == b) {
        return true;
    }

    // Check if a is descendant of b
    pid_t curr = a;
    for (int d = 0; d < maxDepth; ++d) {
        const pid_t p = getParentPid(curr);
        if (p <= 1) break;
        if (p == b) return true;
        curr = p;
    }

    // Check if b is descendant of a
    curr = b;
    for (int d = 0; d < maxDepth; ++d) {
        const pid_t p = getParentPid(curr);
        if (p <= 1) break;
        if (p == a) return true;
        curr = p;
    }

    return false;
}

inline QList<AudioStream> queryMprisPlayingStreams()
{
    QList<AudioStream> streams;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return streams;
    }

    auto iface = bus.interface();
    if (!iface) {
        return streams;
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
            pid_t pid = 0;
            QString title;

            const QVariant metaVar = player.property("Metadata");
            if (metaVar.isValid()) {
                const QVariantMap metaMap = qdbus_cast<QVariantMap>(metaVar.value<QDBusArgument>());
                if (metaMap.contains(QStringLiteral("kde:pid"))) {
                    pid = static_cast<pid_t>(metaMap.value(QStringLiteral("kde:pid")).toInt());
                }
                if (metaMap.contains(QStringLiteral("xesam:title"))) {
                    title = metaMap.value(QStringLiteral("xesam:title")).toString();
                }
            }

            if (pid <= 0) {
                QDBusReply<uint> pidReply = iface->servicePid(svc);
                if (pidReply.isValid() && pidReply.value() > 0) {
                    pid = static_cast<pid_t>(pidReply.value());
                }
            }

            if (pid > 0) {
                streams.append({pid, title.trimmed(), svc});
            }
        }
    }
    return streams;
}

inline QSet<pid_t> queryMprisPlayingPids()
{
    QSet<pid_t> pids;
    const auto streams = queryMprisPlayingStreams();
    for (const auto &s : streams) {
        pids.insert(s.pid);
    }
    return pids;
}

inline bool isGenericMediaTitle(const QString &title)
{
    const QString t = title.trimmed().toLower();
    if (t.size() < 3) {
        return true;
    }
    static const QSet<QString> s_generic = {
        QStringLiteral("playback"),
        QStringLiteral("audiostream"),
        QStringLiteral("audio stream"),
        QStringLiteral("output"),
        QStringLiteral("sink"),
        QStringLiteral("webrtc"),
        QStringLiteral("webrtc_voice_engine"),
        QStringLiteral("speech-dispatcher"),
        QStringLiteral("speech-dispatcher-generic"),
        QStringLiteral("speex"),
        QStringLiteral("bell"),
        QStringLiteral("system sounds"),
        QStringLiteral("default"),
        QStringLiteral("pulseaudio"),
        QStringLiteral("pipewire")
    };
    return s_generic.contains(t);
}

inline QString cleanNormalizedString(const QString &input)
{
    QString s = input.trimmed().toLower();
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s.at(i);
        if (!c.isLetterOrNumber() && c != QLatin1Char(' ')) {
            s[i] = QLatin1Char(' ');
        }
    }
    return s.simplified();
}

inline bool mediaTitleMatchesCaption(const QString &mediaTitle, const QString &caption)
{
    if (mediaTitle.trimmed().isEmpty() || caption.trimmed().isEmpty()) {
        return false;
    }

    // 1. Direct substring match
    if (caption.contains(mediaTitle.trimmed(), Qt::CaseInsensitive)) {
        return true;
    }

    // 2. Normalized alphanumeric string match
    const QString normTitle = cleanNormalizedString(mediaTitle);
    const QString normCaption = cleanNormalizedString(caption);

    if (normTitle.size() >= 3 && normCaption.contains(normTitle)) {
        return true;
    }

    // 3. Remove common site & browser suffixes
    auto stripCommonSuffixes = [](QString str) -> QString {
        static const QStringList suffixes = {
            QStringLiteral("mozilla firefox"),
            QStringLiteral("firefox"),
            QStringLiteral("google chrome"),
            QStringLiteral("chromium"),
            QStringLiteral("brave"),
            QStringLiteral("vivaldi"),
            QStringLiteral("microsoft edge"),
            QStringLiteral("youtube"),
            QStringLiteral("soundcloud"),
            QStringLiteral("bandcamp"),
            QStringLiteral("spotify"),
            QStringLiteral("twitch")
        };
        for (const auto &s : suffixes) {
            str.remove(s);
        }
        return str.simplified();
    };

    const QString strippedTitle = stripCommonSuffixes(normTitle);
    const QString strippedCaption = stripCommonSuffixes(normCaption);

    if (strippedTitle.size() >= 4 && strippedCaption.contains(strippedTitle)) {
        return true;
    }
    if (strippedCaption.size() >= 4 && strippedTitle.contains(strippedCaption)) {
        return true;
    }

    // 4. Token overlap of significant words (length >= 4)
    const QStringList titleWords = strippedTitle.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    int matchCount = 0;
    int totalSignificantWords = 0;

    for (const QString &w : titleWords) {
        if (w.size() < 4) continue;
        if (w == QStringLiteral("with") || w == QStringLiteral("from") ||
            w == QStringLiteral("that") || w == QStringLiteral("this") ||
            w == QStringLiteral("what") || w == QStringLiteral("will") ||
            w == QStringLiteral("have") || w == QStringLiteral("your")) {
            continue;
        }
        totalSignificantWords++;
        if (normCaption.contains(w)) {
            matchCount++;
        }
    }

    if (totalSignificantWords >= 2 && matchCount >= 2) {
        return true;
    }
    if (totalSignificantWords == 1 && matchCount == 1 && strippedTitle.size() >= 6) {
        return true;
    }

    return false;
}

class AudioTracker {
public:
    AudioTracker() = default;

    void refresh(const QList<QPair<pid_t, QString>> &knownWindows = {})
    {
        QList<AudioStream> streams;
        streams.append(queryMprisPlayingStreams());

        QProcess proc;
        proc.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sink-inputs")});
        if (proc.waitForFinished(100)) {
            const QString pactlOut = QString::fromUtf8(proc.readAllStandardOutput());
            streams.append(parsePactlSinkInputsDetailed(pactlOut));
        }

        m_activeStreams = streams;
        m_playingPids.clear();
        for (const auto &s : streams) {
            if (s.pid > 0) {
                m_playingPids.insert(s.pid);
            }
        }

        m_knownWindows = knownWindows;
        m_lastRefresh = QDateTime::currentDateTime();
    }

    bool isWindowPlaying(pid_t winPid, const QString &winCaption = QString()) const
    {
        if (winPid <= 0 || m_playingPids.isEmpty()) {
            return false;
        }

        if (!isWindowPidPlaying(winPid, m_playingPids)) {
            return false;
        }

        if (winCaption.trimmed().isEmpty()) {
            return true;
        }

        QList<AudioStream> matchingStreams;
        for (const auto &s : m_activeStreams) {
            if (s.pid == winPid || isPidRelated(s.pid, winPid)) {
                matchingStreams.append(s);
            }
        }

        if (matchingStreams.isEmpty()) {
            return false;
        }

        QStringList specificTitles;
        for (const auto &s : matchingStreams) {
            if (!s.title.isEmpty() && !isGenericMediaTitle(s.title)) {
                specificTitles.append(s.title);
            }
        }

        if (specificTitles.isEmpty()) {
            return true;
        }

        // Check if this window directly matches
        for (const auto &title : specificTitles) {
            if (mediaTitleMatchesCaption(title, winCaption)) {
                return true;
            }
        }

        // Check if another known window of the same PID matches
        bool anyOtherWindowMatches = false;
        for (const auto &pair : m_knownWindows) {
            const pid_t otherPid = pair.first;
            const QString &otherCaption = pair.second;
            if (otherCaption != winCaption && (otherPid == winPid || isPidRelated(otherPid, winPid))) {
                for (const auto &title : specificTitles) {
                    if (mediaTitleMatchesCaption(title, otherCaption)) {
                        anyOtherWindowMatches = true;
                        break;
                    }
                }
                if (anyOtherWindowMatches) {
                    break;
                }
            }
        }

        if (anyOtherWindowMatches) {
            return false;
        }

        return true;
    }

    const QSet<pid_t> &playingPids() const
    {
        return m_playingPids;
    }

    const QList<AudioStream> &activeStreams() const
    {
        return m_activeStreams;
    }

    bool hasPlayingAudio() const
    {
        return !m_playingPids.isEmpty();
    }

    void setMockPids(const QSet<pid_t> &pids)
    {
        m_playingPids = pids;
        m_activeStreams.clear();
        for (pid_t p : pids) {
            m_activeStreams.append({p, QString(), QString()});
        }
    }

    void setMockStreams(const QList<AudioStream> &streams, const QList<QPair<pid_t, QString>> &knownWindows = {})
    {
        m_activeStreams = streams;
        m_playingPids.clear();
        for (const auto &s : streams) {
            m_playingPids.insert(s.pid);
        }
        m_knownWindows = knownWindows;
    }

private:
    QSet<pid_t> m_playingPids;
    QList<AudioStream> m_activeStreams;
    QList<QPair<pid_t, QString>> m_knownWindows;
    QDateTime m_lastRefresh;
};

} // namespace OutlineAudio
