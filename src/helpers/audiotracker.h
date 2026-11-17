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
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QVariantMap>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <sys/types.h>
#include <unistd.h>

namespace OutlineAudio {

struct WindowInfo {
    pid_t pid = 0;
    QString caption;
    QString windowClass;

    WindowInfo() = default;
    WindowInfo(pid_t p, const QString &c, const QString &wc = QString())
        : pid(p), caption(c), windowClass(wc) {}
    WindowInfo(const QPair<pid_t, QString> &pair)
        : pid(pair.first), caption(pair.second) {}
};

struct AudioStream {
    pid_t pid = 0;
    QString title;       // media.name or xesam:title
    QString appName;     // application.name or MPRIS Identity
    QString appId;       // pipewire.access.portal.app_id, DesktopEntry, application.process.binary
    QString cgroupScope; // cgroup scope name (e.g. app-flatpak-dev.aunetx.deezer-1565785742.scope)
};

inline QString getPidCgroupScope(pid_t pid)
{
    if (pid <= 1) {
        return QString();
    }
    QFile f(QStringLiteral("/proc/%1/cgroup").arg(pid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    const QByteArray content = f.readAll();
    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        const int slashIdx = line.lastIndexOf('/');
        if (slashIdx >= 0) {
            const QString slice = QString::fromUtf8(line.mid(slashIdx + 1));
            if (slice.endsWith(QStringLiteral(".scope")) || slice.endsWith(QStringLiteral(".service"))) {
                return slice;
            }
        }
    }
    return QString();
}

inline QString normalizeAppString(const QString &input)
{
    QString s = input.trimmed().toLower();
    if (s.endsWith(QStringLiteral(".desktop"))) {
        s.chop(8);
    }
    s.remove(QLatin1Char('-'));
    s.remove(QLatin1Char('_'));
    s.remove(QLatin1Char('.'));
    return s.simplified();
}

inline bool appNamesOrIdsMatch(const QString &streamApp, const QString &winApp)
{
    if (streamApp.trimmed().isEmpty() || winApp.trimmed().isEmpty()) {
        return false;
    }
    const QString s1 = normalizeAppString(streamApp);
    const QString s2 = normalizeAppString(winApp);
    if (s1.isEmpty() || s2.isEmpty()) {
        return false;
    }
    if (s1 == s2 || s1.contains(s2) || s2.contains(s1)) {
        return true;
    }
    // Token overlap: split by '.', '-', '_', ' '
    static const QRegularExpression sep(QStringLiteral("[.\\-_ ]+"));
    const QStringList tokens1 = streamApp.toLower().remove(QStringLiteral(".desktop")).split(sep, Qt::SkipEmptyParts);
    const QStringList tokens2 = winApp.toLower().remove(QStringLiteral(".desktop")).split(sep, Qt::SkipEmptyParts);
    for (const QString &t1 : tokens1) {
        if (t1.size() < 4) continue;
        if (t1 == QStringLiteral("stable") || t1 == QStringLiteral("desktop") ||
            t1 == QStringLiteral("binary") || t1 == QStringLiteral("electron") ||
            t1 == QStringLiteral("portal") || t1 == QStringLiteral("instance")) {
            continue;
        }
        for (const QString &t2 : tokens2) {
            if (t2.size() < 4) continue;
            if (t1 == t2) return true;
        }
    }
    return false;
}

inline QList<AudioStream> parsePactlSinkInputsDetailed(const QString &pactlOutput, pid_t selfPid = 0)
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
    QString blockPortalAppId;
    QString blockPortalInstanceId;
    QString blockBinary;
    QString blockIconName;

    auto finishBlock = [&]() {
        if (inBlock && !isCorked && !isMuted && (blockPid > 0 || !blockPortalAppId.isEmpty()) && (selfPid <= 0 || blockPid != selfPid)) {
            QString appId = !blockPortalAppId.isEmpty() ? blockPortalAppId : (!blockBinary.isEmpty() ? blockBinary : blockIconName);
            QString cgroupScope;
            if (!blockPortalAppId.isEmpty() && !blockPortalInstanceId.isEmpty()) {
                cgroupScope = QStringLiteral("app-flatpak-%1-%2.scope").arg(blockPortalAppId, blockPortalInstanceId);
            }
            streams.append({blockPid, blockMediaName.trimmed(), blockAppName.trimmed(), appId.trimmed(), cgroupScope.trimmed()});
        }
        inBlock = false;
        isCorked = false;
        isMuted = false;
        blockPid = 0;
        blockMediaName.clear();
        blockAppName.clear();
        blockPortalAppId.clear();
        blockPortalInstanceId.clear();
        blockBinary.clear();
        blockIconName.clear();
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
        } else if (line.contains(QStringLiteral("pipewire.access.portal.app_id"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                blockPortalAppId = val.trimmed();
            }
        } else if (line.contains(QStringLiteral("pipewire.access.portal.instance_id"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                blockPortalInstanceId = val.trimmed();
            }
        } else if (line.contains(QStringLiteral("application.process.binary"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                blockBinary = val.trimmed();
            }
        } else if (line.contains(QStringLiteral("application.icon_name")) || line.contains(QStringLiteral("application.icon-name"))) {
            const int eqIdx = line.indexOf(QLatin1Char('='));
            if (eqIdx >= 0) {
                QString val = line.mid(eqIdx + 1).trimmed();
                if (val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"')) && val.size() >= 2) {
                    val = val.mid(1, val.size() - 2);
                }
                blockIconName = val.trimmed();
            }
        }
    }
    finishBlock();

    return streams;
}

inline QSet<pid_t> parsePactlSinkInputs(const QString &pactlOutput, pid_t selfPid = 0)
{
    QSet<pid_t> pids;
    const auto streams = parsePactlSinkInputsDetailed(pactlOutput, selfPid);
    for (const auto &s : streams) {
        if (s.pid > 0) {
            pids.insert(s.pid);
        }
    }
    return pids;
}

inline QList<AudioStream> parsePipeWireDumpStreams(const QByteArray &jsonBytes, pid_t selfPid = 0)
{
    QList<AudioStream> streams;
    if (jsonBytes.isEmpty()) {
        return streams;
    }

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
        return streams;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        if (!val.isObject()) continue;
        const QJsonObject obj = val.toObject();
        const QJsonObject info = obj.value(QStringLiteral("info")).toObject();
        const QString state = info.value(QStringLiteral("state")).toString();
        // PipeWire node states: "running", "idle", "suspended", "paused", "error"
        // Only "running" indicates the node is actively producing/streaming audio right now
        if (state != QStringLiteral("running")) {
            continue;
        }

        const QJsonObject props = info.value(QStringLiteral("props")).toObject();
        const QString mediaClass = props.value(QStringLiteral("media.class")).toString();
        if (mediaClass != QStringLiteral("Stream/Output/Audio")) {
            continue;
        }

        pid_t pid = 0;
        const QJsonValue pidVal = props.value(QStringLiteral("application.process.id"));
        if (pidVal.isDouble()) {
            pid = static_cast<pid_t>(pidVal.toInteger());
        } else if (pidVal.isString()) {
            pid = static_cast<pid_t>(pidVal.toString().toLongLong());
        }

        if (selfPid > 0 && pid == selfPid) {
            continue;
        }

        const QString mediaName = props.value(QStringLiteral("media.name")).toString();
        const QString appName = props.value(QStringLiteral("application.name")).toString();
        const QString portalAppId = props.value(QStringLiteral("pipewire.access.portal.app_id")).toString();
        const QJsonValue portalInstanceVal = props.value(QStringLiteral("pipewire.access.portal.instance_id"));
        QString portalInstanceId;
        if (portalInstanceVal.isDouble()) {
            portalInstanceId = QString::number(static_cast<qint64>(portalInstanceVal.toDouble()));
        } else if (portalInstanceVal.isString()) {
            portalInstanceId = portalInstanceVal.toString();
        }
        const QString binary = props.value(QStringLiteral("application.process.binary")).toString();
        const QString iconName = props.value(QStringLiteral("application.icon-name")).toString();

        QString appId = !portalAppId.isEmpty() ? portalAppId : (!binary.isEmpty() ? binary : iconName);
        QString cgroupScope;
        if (!portalAppId.isEmpty() && !portalInstanceId.isEmpty()) {
            cgroupScope = QStringLiteral("app-flatpak-%1-%2.scope").arg(portalAppId, portalInstanceId);
        }

        streams.append({pid, mediaName.trimmed(), appName.trimmed(), appId.trimmed(), cgroupScope.trimmed()});
    }

    return streams;
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

struct MprisPausedInfo {
    QSet<pid_t> pids;
    QSet<QString> cgroupScopes;
    QSet<QString> appIds;
};

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

        // Query Player properties via DBus GetAll to reliably read complex properties (e.g. Metadata dictionary)
        QDBusMessage playerMsg = QDBusMessage::createMethodCall(
            svc, QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll")
        );
        playerMsg << QStringLiteral("org.mpris.MediaPlayer2.Player");
        QDBusMessage playerReply = bus.call(playerMsg);
        if (playerReply.type() != QDBusMessage::ReplyMessage || playerReply.arguments().isEmpty()) {
            continue;
        }

        QVariantMap playerProps;
        const QVariant argVal = playerReply.arguments().at(0);
        if (argVal.userType() == qMetaTypeId<QDBusArgument>()) {
            playerProps = qdbus_cast<QVariantMap>(argVal.value<QDBusArgument>());
        }

        const QString status = playerProps.value(QStringLiteral("PlaybackStatus")).toString();
        if (status != QStringLiteral("Playing")) {
            continue;
        }

        pid_t pid = 0;
        QString title;

        if (playerProps.contains(QStringLiteral("Metadata"))) {
            const QVariant metaVal = playerProps.value(QStringLiteral("Metadata"));
            if (metaVal.userType() == qMetaTypeId<QDBusArgument>()) {
                const QVariantMap metaMap = qdbus_cast<QVariantMap>(metaVal.value<QDBusArgument>());
                if (metaMap.contains(QStringLiteral("kde:pid"))) {
                    pid = static_cast<pid_t>(metaMap.value(QStringLiteral("kde:pid")).toInt());
                }
                if (metaMap.contains(QStringLiteral("xesam:title"))) {
                    title = metaMap.value(QStringLiteral("xesam:title")).toString();
                }
            }
        }

        if (pid <= 0) {
            QDBusReply<uint> pidReply = iface->servicePid(svc);
            if (pidReply.isValid() && pidReply.value() > 0) {
                pid = static_cast<pid_t>(pidReply.value());
            }
        }

        // Query root MediaPlayer2 properties for DesktopEntry and Identity
        QString desktopEntry;
        QString identity;
        QDBusMessage rootMsg = QDBusMessage::createMethodCall(
            svc, QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll")
        );
        rootMsg << QStringLiteral("org.mpris.MediaPlayer2");
        QDBusMessage rootReply = bus.call(rootMsg);
        if (rootReply.type() == QDBusMessage::ReplyMessage && !rootReply.arguments().isEmpty()) {
            const QVariant rootVal = rootReply.arguments().at(0);
            if (rootVal.userType() == qMetaTypeId<QDBusArgument>()) {
                const QVariantMap rootProps = qdbus_cast<QVariantMap>(rootVal.value<QDBusArgument>());
                desktopEntry = rootProps.value(QStringLiteral("DesktopEntry")).toString();
                identity = rootProps.value(QStringLiteral("Identity")).toString();
            }
        }

        const QString cgroupScope = getPidCgroupScope(pid);
        QString appId = !desktopEntry.isEmpty() ? desktopEntry : svc.mid(23); // strip "org.mpris.MediaPlayer2."

        if (pid > 0 || !cgroupScope.isEmpty() || !appId.isEmpty()) {
            streams.append({pid, title.trimmed(), identity.trimmed(), appId.trimmed(), cgroupScope.trimmed()});
        }
    }
    return streams;
}

inline QSet<pid_t> queryMprisPlayingPids()
{
    QSet<pid_t> pids;
    const auto streams = queryMprisPlayingStreams();
    for (const auto &s : streams) {
        if (s.pid > 0) {
            pids.insert(s.pid);
        }
    }
    return pids;
}

inline MprisPausedInfo queryMprisPausedInfo()
{
    MprisPausedInfo info;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return info;
    }

    auto iface = bus.interface();
    if (!iface) {
        return info;
    }

    const QStringList services = iface->registeredServiceNames();
    for (const QString &svc : services) {
        if (!svc.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
            continue;
        }

        QDBusMessage playerMsg = QDBusMessage::createMethodCall(
            svc, QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("GetAll")
        );
        playerMsg << QStringLiteral("org.mpris.MediaPlayer2.Player");
        QDBusMessage playerReply = bus.call(playerMsg);
        if (playerReply.type() != QDBusMessage::ReplyMessage || playerReply.arguments().isEmpty()) {
            continue;
        }

        QVariantMap playerProps;
        const QVariant argVal = playerReply.arguments().at(0);
        if (argVal.userType() == qMetaTypeId<QDBusArgument>()) {
            playerProps = qdbus_cast<QVariantMap>(argVal.value<QDBusArgument>());
        }

        const QString status = playerProps.value(QStringLiteral("PlaybackStatus")).toString();
        if (status == QStringLiteral("Paused") || status == QStringLiteral("Stopped")) {
            pid_t pid = 0;
            if (playerProps.contains(QStringLiteral("Metadata"))) {
                const QVariant metaVal = playerProps.value(QStringLiteral("Metadata"));
                if (metaVal.userType() == qMetaTypeId<QDBusArgument>()) {
                    const QVariantMap metaMap = qdbus_cast<QVariantMap>(metaVal.value<QDBusArgument>());
                    if (metaMap.contains(QStringLiteral("kde:pid"))) {
                        pid = static_cast<pid_t>(metaMap.value(QStringLiteral("kde:pid")).toInt());
                    }
                }
            }

            if (pid <= 0) {
                QDBusReply<uint> pidReply = iface->servicePid(svc);
                if (pidReply.isValid() && pidReply.value() > 0) {
                    pid = static_cast<pid_t>(pidReply.value());
                }
            }

            if (pid > 0) {
                info.pids.insert(pid);
                const QString cgroup = getPidCgroupScope(pid);
                if (!cgroup.isEmpty()) {
                    info.cgroupScopes.insert(cgroup);
                }
            }

            const QString svcSuffix = svc.mid(23);
            if (!svcSuffix.isEmpty()) {
                info.appIds.insert(svcSuffix.toLower());
            }
        }
    }
    return info;
}

inline QSet<pid_t> queryMprisPausedPids()
{
    return queryMprisPausedInfo().pids;
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
            QStringLiteral("deezer"),
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

inline bool isStreamMatchingWindow(const AudioStream &s, const WindowInfo &w)
{
    // 1. Direct PID match or process hierarchy relationship
    if (w.pid > 0 && s.pid > 0 && isPidRelated(w.pid, s.pid)) {
        return true;
    }

    // 2. Shared CGroup scope (Flatpaks, Systemd scopes/slices)
    if (w.pid > 0 && !s.cgroupScope.isEmpty()) {
        const QString winCgroup = getPidCgroupScope(w.pid);
        if (!winCgroup.isEmpty() && winCgroup == s.cgroupScope) {
            return true;
        }
    }

    // 3. Application identifier / WMClass match
    if (!s.appId.isEmpty()) {
        if (appNamesOrIdsMatch(s.appId, w.windowClass)) {
            return true;
        }
    }
    if (!s.appName.isEmpty()) {
        if (appNamesOrIdsMatch(s.appName, w.windowClass)) {
            return true;
        }
    }

    // 4. Window caption match with track title or application identity
    if (!s.title.isEmpty() && !isGenericMediaTitle(s.title)) {
        if (mediaTitleMatchesCaption(s.title, w.caption)) {
            return true;
        }
    }
    if (!s.appName.isEmpty() && !isGenericMediaTitle(s.appName) && s.appName.size() >= 4) {
        if (w.caption.contains(s.appName, Qt::CaseInsensitive)) {
            return true;
        }
    }

    return false;
}

class AudioTracker {
public:
    AudioTracker() = default;

    void refresh(const QList<WindowInfo> &knownWindows)
    {
        const pid_t selfPid = getpid();
        QList<AudioStream> streams;

        // 1. Authoritative MPRIS state
        const QList<AudioStream> mprisPlaying = queryMprisPlayingStreams();
        const MprisPausedInfo mprisPaused = queryMprisPausedInfo();
        streams.append(mprisPlaying);

        QSet<pid_t> mprisPlayingPids;
        for (const auto &s : mprisPlaying) {
            if (s.pid > 0) {
                mprisPlayingPids.insert(s.pid);
            }
        }

        auto isMprisExplicitlyPaused = [&](const AudioStream &s) -> bool {
            if (s.pid > 0) {
                if (mprisPaused.pids.contains(s.pid)) return true;
                for (pid_t p : mprisPaused.pids) {
                    if (isPidRelated(s.pid, p)) return true;
                }
            }
            if (!s.cgroupScope.isEmpty() && mprisPaused.cgroupScopes.contains(s.cgroupScope)) {
                return true;
            }
            if (!s.appId.isEmpty()) {
                const QString normAppId = s.appId.toLower();
                for (const auto &pApp : mprisPaused.appIds) {
                    if (normAppId.contains(pApp) || pApp.contains(normAppId)) {
                        return true;
                    }
                }
            }
            return false;
        };

        // 2. Hardware / Audio Server streams (PipeWire pw-dump Node preferred for real-time node state)
        bool pwSuccess = false;
        QProcess proc;
        proc.start(QStringLiteral("pw-dump"), {QStringLiteral("Node")});
        if (proc.waitForFinished(60) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            const QByteArray out = proc.readAllStandardOutput();
            const QList<AudioStream> pwStreams = parsePipeWireDumpStreams(out, selfPid);
            for (const auto &s : pwStreams) {
                if (isMprisExplicitlyPaused(s) && !mprisPlayingPids.contains(s.pid)) {
                    continue;
                }
                streams.append(s);
            }
            pwSuccess = true;
        }

        // 3. Fallback to pactl if PipeWire pw-dump was unavailable or failed
        if (!pwSuccess) {
            QProcess pactlProc;
            pactlProc.start(QStringLiteral("pactl"), {QStringLiteral("list"), QStringLiteral("sink-inputs")});
            if (pactlProc.waitForFinished(60)) {
                const QString pactlOut = QString::fromUtf8(pactlProc.readAllStandardOutput());
                const auto pactlStreams = parsePactlSinkInputsDetailed(pactlOut, selfPid);
                for (const auto &s : pactlStreams) {
                    if (isMprisExplicitlyPaused(s) && !mprisPlayingPids.contains(s.pid)) {
                        continue;
                    }
                    streams.append(s);
                }
            }
        }

        m_activeStreams = streams;
        m_playingPids.clear();
        for (const auto &s : streams) {
            if (s.pid > 0 && s.pid != selfPid) {
                m_playingPids.insert(s.pid);
            }
        }

        // Associate active streams with known windows and record host window PIDs
        for (const auto &w : knownWindows) {
            if (w.pid <= 0) continue;
            for (const auto &s : streams) {
                if (isStreamMatchingWindow(s, w)) {
                    m_playingPids.insert(w.pid);
                    break;
                }
            }
        }

        m_knownWindows = knownWindows;
        m_lastRefresh = QDateTime::currentDateTime();
    }

    void refresh(const QList<QPair<pid_t, QString>> &knownWindows = {})
    {
        QList<WindowInfo> converted;
        converted.reserve(knownWindows.size());
        for (const auto &p : knownWindows) {
            converted.append(WindowInfo(p));
        }
        refresh(converted);
    }

    bool isWindowPlaying(pid_t winPid,
                         const QString &winCaption = QString(),
                         const QString &windowClass = QString()) const
    {
        if (winPid <= 0 || m_activeStreams.isEmpty()) {
            return false;
        }

        WindowInfo thisWin(winPid, winCaption, windowClass);
        if (thisWin.windowClass.isEmpty()) {
            for (const auto &kw : m_knownWindows) {
                if (kw.pid == winPid) {
                    thisWin.windowClass = kw.windowClass;
                    break;
                }
            }
        }

        QList<AudioStream> matchingStreams;
        for (const auto &s : m_activeStreams) {
            if (isStreamMatchingWindow(s, thisWin)) {
                matchingStreams.append(s);
            }
        }

        if (matchingStreams.isEmpty()) {
            return false;
        }

        if (winCaption.trimmed().isEmpty()) {
            return true;
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

        // Check if this window directly matches the track/media title
        for (const auto &title : specificTitles) {
            if (mediaTitleMatchesCaption(title, winCaption)) {
                return true;
            }
        }

        // Check if another known window of the same app/stream matches
        bool anyOtherWindowMatches = false;
        for (const auto &otherWin : m_knownWindows) {
            if (otherWin.pid == winPid && otherWin.caption == winCaption) {
                continue;
            }
            bool otherMatchesStream = false;
            for (const auto &s : matchingStreams) {
                if (isStreamMatchingWindow(s, otherWin)) {
                    otherMatchesStream = true;
                    break;
                }
            }
            if (otherMatchesStream) {
                for (const auto &title : specificTitles) {
                    if (mediaTitleMatchesCaption(title, otherWin.caption)) {
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
            m_activeStreams.append({p, QString(), QString(), QString(), QString()});
        }
    }

    void setMockStreams(const QList<AudioStream> &streams, const QList<WindowInfo> &knownWindows = {})
    {
        m_activeStreams = streams;
        m_playingPids.clear();
        for (const auto &s : streams) {
            if (s.pid > 0) {
                m_playingPids.insert(s.pid);
            }
        }
        for (const auto &w : knownWindows) {
            if (w.pid <= 0) continue;
            for (const auto &s : streams) {
                if (isStreamMatchingWindow(s, w)) {
                    m_playingPids.insert(w.pid);
                    break;
                }
            }
        }
        m_knownWindows = knownWindows;
    }

    void setMockStreams(const QList<AudioStream> &streams, const QList<QPair<pid_t, QString>> &knownWindows)
    {
        QList<WindowInfo> converted;
        converted.reserve(knownWindows.size());
        for (const auto &p : knownWindows) {
            converted.append(WindowInfo(p));
        }
        setMockStreams(streams, converted);
    }

private:
    QSet<pid_t> m_playingPids;
    QList<AudioStream> m_activeStreams;
    QList<WindowInfo> m_knownWindows;
    QDateTime m_lastRefresh;
};

} // namespace OutlineAudio
