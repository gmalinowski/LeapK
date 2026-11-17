/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "unifiedmanagerdialog.h"
#include <QApplication>
#include <QInputDialog>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("leapk-workspace-manager"));
    app.setApplicationDisplayName(QObject::tr("LeapK - Workspace Manager"));

    // 1. Prompt new workspace CLI action
    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--prompt-new-workspace")) {
        QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();
        const int nextNum = workspaces.size() + 1;
        const QString defaultName = QStringLiteral("Workspace %1").arg(nextNum);

        bool ok = false;
        const QString name = QInputDialog::getText(
            nullptr,
            QStringLiteral("New Workspace"),
            QStringLiteral("Enter a name for the new Workspace:"),
            QLineEdit::Normal,
            defaultName,
            &ok
        );

        if (!ok || name.trimmed().isEmpty()) {
            return 1;
        }

        const auto ws = LeapKWorkspace::createEmptyWorkspace(workspaces, name.trimmed());
        QJsonObject resp;
        resp[QStringLiteral("action")] = QStringLiteral("createWorkspace");
        resp[QStringLiteral("newWorkspaceId")] = ws.id;
        resp[QStringLiteral("newWorkspaceName")] = ws.name;
        std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        return 0;
    }

    // 2. Create empty workspace CLI action
    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--create-empty")) {
        QString name = (argc > 2 && argv[2]) ? QString::fromUtf8(argv[2]).trimmed() : QString();
        QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();
        const auto ws = LeapKWorkspace::createEmptyWorkspace(workspaces, name);
        std::cout << "Created empty workspace: " << ws.name.toStdString() << " (" << ws.id.toStdString() << ")" << std::endl;
        return 0;
    }

    // 3. Close workspace CLI action
    if (argc > 1 && QString::fromUtf8(argv[1]) == QLatin1String("--close-workspace")) {
        const QString targetWsId = (argc > 2 && argv[2]) ? QString::fromUtf8(argv[2]).trimmed() : QString();
        QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();
        int foundIdx = -1;
        for (int i = 0; i < workspaces.size(); ++i) {
            if (workspaces.at(i).id == targetWsId) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx < 0) {
            std::cerr << "Workspace not found: " << targetWsId.toStdString() << std::endl;
            return 1;
        }

        const auto ws = workspaces.at(foundIdx);
        QString themeId;
        for (int i = 1; i < argc; ++i) {
            if (QString::fromUtf8(argv[i]) == QLatin1String("--theme") && (i + 1) < argc) {
                themeId = QString::fromUtf8(argv[++i]).trimmed();
            }
        }
        const auto theme = LeapKTheme::loadActiveTheme(themeId);
        CloseWorkspaceDialog dlg(ws.name, theme);
        if (dlg.exec() == QDialog::Accepted) {
            const auto act = dlg.selectedAction();
            QString actStr = QStringLiteral("standard");
            if (act == CloseWorkspaceDialog::CloseWindows) {
                actStr = QStringLiteral("close");
            } else if (act == CloseWorkspaceDialog::MoveToStash) {
                actStr = QStringLiteral("stash");
            }

            workspaces.removeAt(foundIdx);
            LeapKWorkspace::saveWorkspaces(workspaces);

            QJsonObject resp;
            resp[QStringLiteral("action")] = QStringLiteral("deleteWorkspace");
            resp[QStringLiteral("deletedWorkspaceId")] = ws.id;
            resp[QStringLiteral("deletedWorkspaceName")] = ws.name;
            resp[QStringLiteral("windowAction")] = actStr;

            QJsonArray delArr;
            QJsonObject delInfo;
            delInfo[QStringLiteral("id")] = ws.id;
            delInfo[QStringLiteral("name")] = ws.name;
            delInfo[QStringLiteral("action")] = actStr;
            delArr.append(delInfo);
            resp[QStringLiteral("deletedWorkspaces")] = delArr;

            std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
            return 0;
        }
        return 1;
    }

    // 4. Interactive Manager GUI: parse positional and optional flags
    QString appClass;
    QString windowCaption;
    int desktopNum = 0;
    QString desktopName;
    QString assignedWsId;
    QString themeId;

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromUtf8(argv[i]);
        if (arg == QLatin1String("--theme") && (i + 1) < argc) {
            themeId = QString::fromUtf8(argv[++i]).trimmed();
        } else if (appClass.isEmpty()) {
            appClass = arg.trimmed();
        } else if (windowCaption.isEmpty()) {
            windowCaption = arg.trimmed();
        } else if (desktopNum == 0 && arg.toInt() > 0) {
            desktopNum = arg.toInt();
        } else if (desktopName.isEmpty()) {
            desktopName = arg.trimmed();
        } else if (assignedWsId.isEmpty()) {
            assignedWsId = arg.trimmed();
        }
    }

    const LeapKTheme::Theme theme = LeapKTheme::loadActiveTheme(themeId);
    QList<LeapKWorkspace::WorkspaceInfo> workspaces = LeapKWorkspace::loadWorkspaces();

    ManagerContext ctx;
    ctx.appClass = appClass;
    ctx.windowCaption = windowCaption;
    ctx.desktopNum = desktopNum;
    ctx.desktopName = desktopName;
    ctx.assignedWsId = assignedWsId;
    ctx.workspaces = workspaces;
    ctx.theme = theme;
    ctx.defaultTab = TabWorkspaces;

    UnifiedManagerDialog dlg(ctx);
    if (dlg.exec() == QDialog::Accepted) {
        const auto updatedList = dlg.workspaces();
        LeapKWorkspace::saveWorkspaces(updatedList);

        QJsonObject resp;
        resp[QStringLiteral("action")] = dlg.assignedWorkspaceId().isEmpty() ? QStringLiteral("unassign") : QStringLiteral("assign");
        resp[QStringLiteral("assignedWorkspaceId")] = dlg.assignedWorkspaceId();
        resp[QStringLiteral("groupByApp")] = dlg.groupByApp();

        QJsonArray arr;
        for (const auto &ws : updatedList) {
            arr.append(LeapKWorkspace::workspaceToJson(ws));
        }
        resp[QStringLiteral("workspaces")] = arr;
        resp[QStringLiteral("deletedWorkspaces")] = dlg.deletedWorkspaces();

        std::cout << QJsonDocument(resp).toJson(QJsonDocument::Compact).toStdString() << std::endl;
        return 0;
    }

    return 1;
}
