/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QString>
#include <QColor>
#include <QChar>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QDateTime>

namespace LeapKWorkspace {

struct WindowMatchRule {
    QString matchApp;       // e.g. "org.kde.konsole", "firefox"
    QString matchCaption;   // substring in caption/title (optional)
    int matchDesktop = 0;   // 0 = any desktop, >0 = specific desktop
};

inline QJsonObject ruleToJson(const WindowMatchRule &r)
{
    QJsonObject obj;
    obj[QStringLiteral("matchApp")] = r.matchApp;
    obj[QStringLiteral("matchCaption")] = r.matchCaption;
    obj[QStringLiteral("matchDesktop")] = r.matchDesktop;
    return obj;
}

inline WindowMatchRule ruleFromJson(const QJsonObject &obj)
{
    WindowMatchRule r;
    r.matchApp = obj.value(QStringLiteral("matchApp")).toString();
    r.matchCaption = obj.value(QStringLiteral("matchCaption")).toString();
    r.matchDesktop = obj.value(QStringLiteral("matchDesktop")).toInt(0);
    return r;
}

struct WorkspaceInfo {
    QString id;             // e.g. "backend", "ws-3829"
    QString name;           // e.g. "Backend"
    QColor color;           // e.g. #3498db
    QChar shortcutKey;      // optional quick-assign key, e.g. '1', 'B'
    QList<WindowMatchRule> rules;
    qint64 createdAt = 0;
};

inline QJsonObject workspaceToJson(const WorkspaceInfo &ws)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = ws.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : ws.id;
    obj[QStringLiteral("name")] = ws.name;
    obj[QStringLiteral("color")] = ws.color.isValid() ? ws.color.name(QColor::HexRgb) : QStringLiteral("#3498db");
    if (!ws.shortcutKey.isNull()) {
        obj[QStringLiteral("shortcutKey")] = QString(ws.shortcutKey);
    }
    QJsonArray rulesArr;
    for (const auto &r : ws.rules) {
        rulesArr.append(ruleToJson(r));
    }
    obj[QStringLiteral("rules")] = rulesArr;
    obj[QStringLiteral("createdAt")] = ws.createdAt > 0 ? ws.createdAt : QDateTime::currentMSecsSinceEpoch();
    return obj;
}

inline WorkspaceInfo workspaceFromJson(const QJsonObject &obj)
{
    WorkspaceInfo ws;
    ws.id = obj.value(QStringLiteral("id")).toString();
    ws.name = obj.value(QStringLiteral("name")).toString();
    ws.color = QColor(obj.value(QStringLiteral("color")).toString(QStringLiteral("#3498db")));
    const QString keyStr = obj.value(QStringLiteral("shortcutKey")).toString().trimmed();
    ws.shortcutKey = keyStr.isEmpty() ? QChar() : keyStr.at(0).toUpper();
    ws.createdAt = obj.value(QStringLiteral("createdAt")).toVariant().toLongLong();

    const QJsonArray rulesArr = obj.value(QStringLiteral("rules")).toArray();
    for (const auto &val : rulesArr) {
        if (val.isObject()) {
            ws.rules.append(ruleFromJson(val.toObject()));
        }
    }
    return ws;
}

inline QString defaultWorkspacesDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString dir = base + QStringLiteral("/leapk");
    QDir().mkpath(dir);
    return dir;
}

inline QString defaultWorkspacesFilePath()
{
    return defaultWorkspacesDirectory() + QStringLiteral("/workspaces.json");
}

inline QList<WorkspaceInfo> loadWorkspaces(const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultWorkspacesFilePath() : filePath;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return {};
    }

    QList<WorkspaceInfo> list;
    const QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        if (val.isObject()) {
            list.append(workspaceFromJson(val.toObject()));
        }
    }
    return list;
}

inline bool saveWorkspaces(const QList<WorkspaceInfo> &workspaces, const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultWorkspacesFilePath() : filePath;
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QJsonArray arr;
    for (const auto &ws : workspaces) {
        arr.append(workspaceToJson(ws));
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}

// Checks if a window matches any rule of a workspace
inline bool matchesWorkspace(const WorkspaceInfo &ws, const QString &appId, const QString &caption, int desktop = 0)
{
    for (const auto &r : ws.rules) {
        if (!r.matchApp.isEmpty()) {
            const QString cleanRuleApp = r.matchApp.trimmed().toLower();
            const QString cleanApp = appId.trimmed().toLower();
            if (!cleanApp.contains(cleanRuleApp) && !cleanRuleApp.contains(cleanApp)) {
                continue;
            }
        }
        if (r.matchDesktop > 0 && desktop > 0 && desktop != r.matchDesktop) {
            continue;
        }
        if (!r.matchCaption.trimmed().isEmpty()) {
            if (!caption.contains(r.matchCaption.trimmed(), Qt::CaseInsensitive)) {
                continue;
            }
        }
        return true;
    }
    return false;
}

inline WorkspaceInfo createEmptyWorkspace(QList<WorkspaceInfo> &workspaces, const QString &customName = QString(), const QString &filePath = QString())
{
    static const QList<QColor> s_colors = {
        QColor(52, 152, 219),  // Blue
        QColor(46, 204, 113),  // Green
        QColor(155, 89, 182),  // Purple
        QColor(230, 126, 34),  // Orange
        QColor(241, 196, 15),  // Yellow
        QColor(231, 76, 60),   // Red
        QColor(26, 188, 156)   // Turquoise
    };

    const int nextNum = workspaces.size() + 1;
    const QString name = !customName.trimmed().isEmpty()
        ? customName.trimmed()
        : QStringLiteral("Workspace %1").arg(nextNum);

    WorkspaceInfo ws;
    ws.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ws.name = name;
    ws.color = s_colors.at(workspaces.size() % s_colors.size());
    ws.createdAt = QDateTime::currentMSecsSinceEpoch();
    ws.rules.clear();

    workspaces.append(ws);
    saveWorkspaces(workspaces, filePath);
    return ws;
}

inline bool deleteWorkspace(QList<WorkspaceInfo> &workspaces, const QString &wsId, const QString &filePath = QString())
{
    for (int i = workspaces.size() - 1; i >= 0; --i) {
        if (workspaces.at(i).id == wsId) {
            workspaces.removeAt(i);
            saveWorkspaces(workspaces, filePath);
            return true;
        }
    }
    return false;
}

} // namespace LeapKWorkspace
