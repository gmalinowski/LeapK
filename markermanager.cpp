/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "unifiedmanagerdialog.h"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-marker-manager"));
    app.setApplicationDisplayName(QObject::tr("LeapK - Marker & Group Manager"));

    // 1. CLI list flag
    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--list")) {
        const auto rules = LeapKMarks::loadPersistentMarks();
        QJsonArray arr;
        for (const auto &r : rules) {
            arr.append(LeapKMarks::ruleToJson(r));
        }
        std::cout << QJsonDocument(arr).toJson(QJsonDocument::Indented).toStdString() << std::endl;
        return 0;
    }

    // 2. CLI delete flag
    if (argc > 2 && QString::fromUtf8(argv[1]) == QLatin1String("--delete")) {
        const QString delId = QString::fromUtf8(argv[2]).trimmed();
        auto rules = LeapKMarks::loadPersistentMarks();
        bool ok = LeapKMarks::deletePersistentMark(rules, delId);
        if (ok) {
            QDBusMessage msg = QDBusMessage::createMethodCall(
                QStringLiteral("org.kde.KWin"),
                QStringLiteral("/Effects"),
                QStringLiteral("org.kde.kwin.Effects"),
                QStringLiteral("reconfigureEffect")
            );
            msg << QStringLiteral("leapk");
            QDBusConnection::sessionBus().send(msg);
            std::cout << "Deleted rule: " << delId.toStdString() << std::endl;
            return 0;
        } else {
            std::cerr << "Rule not found: " << delId.toStdString() << std::endl;
            return 1;
        }
    }

    // 3. Interactive Manager GUI: parse positional and optional flags
    QString appClass;
    QString windowCaption;
    int desktopNum = 0;
    QString desktopName;
    QList<OpenWindowInfo> openWindows;
    QString themeId;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == QLatin1String("--theme") && (i + 1) < argc) {
            themeId = QString::fromUtf8(argv[++i]).trimmed();
        } else if (arg == QLatin1String("--open-windows") && (i + 1) < argc) {
            const QString winJson = QString::fromUtf8(argv[++i]);
            const QJsonDocument doc = QJsonDocument::fromJson(winJson.toUtf8());
            if (doc.isArray()) {
                for (const auto &val : doc.array()) {
                    if (val.isObject()) {
                        const QJsonObject obj = val.toObject();
                        OpenWindowInfo info;
                        info.app = obj.value(QStringLiteral("app")).toString();
                        info.caption = obj.value(QStringLiteral("caption")).toString();
                        info.desktop = obj.value(QStringLiteral("desktop")).toInt(0);
                        openWindows.append(info);
                    }
                }
            }
        } else if (appClass.isEmpty()) {
            appClass = arg.trimmed();
        } else if (windowCaption.isEmpty()) {
            windowCaption = arg.trimmed();
        } else if (desktopNum == 0 && arg.toInt() > 0) {
            desktopNum = arg.toInt();
        } else if (desktopName.isEmpty()) {
            desktopName = arg.trimmed();
        }
    }

    const LeapKTheme::Theme theme = LeapKTheme::loadActiveTheme(themeId);
    QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();

    ManagerContext ctx;
    ctx.appClass = appClass;
    ctx.windowCaption = windowCaption;
    ctx.desktopNum = desktopNum;
    ctx.desktopName = desktopName;
    ctx.openWindows = openWindows;
    ctx.workspaces = workspaces;
    ctx.theme = theme;
    ctx.defaultTab = TabMarkers;

    UnifiedManagerDialog dlg(ctx);
    if (dlg.exec() == QDialog::Accepted) {
        // If workspaces were modified during the session, save them as well
        const auto updatedWorkspaces = dlg.workspaces();
        LeapKWorkspace::saveWorkspaces(updatedWorkspaces);
        return 0;
    }

    return 0;
}
