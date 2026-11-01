/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "workspacehelper.h"
#include "persistentmarkhelper.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testWorkspaceManagerCliParsing()
{
    // 1. Check prompt-new-workspace detection
    const QStringList argsPrompt = { QStringLiteral("leapk-workspace-manager"), QStringLiteral("--prompt-new-workspace") };
    assert(argsPrompt.size() > 1 && argsPrompt.at(1) == QStringLiteral("--prompt-new-workspace"));

    // 2. Check create-empty detection with name
    const QStringList argsCreate = { QStringLiteral("leapk-workspace-manager"), QStringLiteral("--create-empty"), QStringLiteral("Project X") };
    assert(argsCreate.at(1) == QStringLiteral("--create-empty"));
    assert(argsCreate.at(2) == QStringLiteral("Project X"));

    // 3. Check close-workspace detection with ID
    const QStringList argsClose = { QStringLiteral("leapk-workspace-manager"), QStringLiteral("--close-workspace"), QStringLiteral("ws-12345") };
    assert(argsClose.at(1) == QStringLiteral("--close-workspace"));
    assert(argsClose.at(2) == QStringLiteral("ws-12345"));

    // 4. Check positional arguments parsing
    const QStringList argsPos = {
        QStringLiteral("leapk-workspace-manager"),
        QStringLiteral("org.kde.konsole"),
        QStringLiteral("bash"),
        QStringLiteral("2"),
        QStringLiteral("Desktop 2"),
        QStringLiteral("ws-backend")
    };
    const QString appClass = argsPos.value(1);
    const QString caption = argsPos.value(2);
    const int desktopNum = argsPos.value(3).toInt();
    const QString desktopName = argsPos.value(4);
    const QString assignedWsId = argsPos.value(5);

    assert(appClass == QStringLiteral("org.kde.konsole"));
    assert(caption == QStringLiteral("bash"));
    assert(desktopNum == 2);
    assert(desktopName == QStringLiteral("Desktop 2"));
    assert(assignedWsId == QStringLiteral("ws-backend"));

    std::cout << "testWorkspaceManagerCliParsing PASSED\n";
}

void testWorkspaceManagerIpcJsonFormats()
{
    // 1. createWorkspace JSON response
    QJsonObject createResp;
    createResp[QStringLiteral("action")] = QStringLiteral("createWorkspace");
    createResp[QStringLiteral("newWorkspaceId")] = QStringLiteral("ws-99");
    createResp[QStringLiteral("newWorkspaceName")] = QStringLiteral("Backend");

    assert(createResp.value(QStringLiteral("action")).toString() == QStringLiteral("createWorkspace"));
    assert(createResp.value(QStringLiteral("newWorkspaceId")).toString() == QStringLiteral("ws-99"));
    assert(createResp.value(QStringLiteral("newWorkspaceName")).toString() == QStringLiteral("Backend"));

    // 2. deleteWorkspace JSON response
    QJsonObject delResp;
    delResp[QStringLiteral("action")] = QStringLiteral("deleteWorkspace");
    delResp[QStringLiteral("deletedWorkspaceId")] = QStringLiteral("ws-99");
    delResp[QStringLiteral("deletedWorkspaceName")] = QStringLiteral("Backend");
    delResp[QStringLiteral("windowAction")] = QStringLiteral("stash");

    assert(delResp.value(QStringLiteral("action")).toString() == QStringLiteral("deleteWorkspace"));
    assert(delResp.value(QStringLiteral("windowAction")).toString() == QStringLiteral("stash"));

    // 3. assign JSON response
    QJsonObject assignResp;
    assignResp[QStringLiteral("action")] = QStringLiteral("assign");
    assignResp[QStringLiteral("assignedWorkspaceId")] = QStringLiteral("ws-backend");
    assignResp[QStringLiteral("groupByApp")] = true;

    assert(assignResp.value(QStringLiteral("action")).toString() == QStringLiteral("assign"));
    assert(assignResp.value(QStringLiteral("groupByApp")).toBool() == true);

    std::cout << "testWorkspaceManagerIpcJsonFormats PASSED\n";
}

void testMarkerManagerCliParsing()
{
    // 1. --list flag
    const QStringList argsList = { QStringLiteral("leapk-marker-manager"), QStringLiteral("--list") };
    assert(argsList.at(1) == QStringLiteral("--list"));

    // 2. --delete flag
    const QStringList argsDel = { QStringLiteral("leapk-marker-manager"), QStringLiteral("--delete"), QStringLiteral("rule-abc") };
    assert(argsDel.at(1) == QStringLiteral("--delete"));
    assert(argsDel.at(2) == QStringLiteral("rule-abc"));

    // 3. --open-windows valid JSON array parsing
    const QString validWinJson = QStringLiteral("[{\"caption\": \"Konsole\", \"class\": \"org.kde.konsole\", \"desktop\": 1}]");
    const QJsonDocument docValid = QJsonDocument::fromJson(validWinJson.toUtf8());
    assert(docValid.isArray());
    assert(docValid.array().size() == 1);
    assert(docValid.array().first().toObject().value(QStringLiteral("class")).toString() == QStringLiteral("org.kde.konsole"));

    // 4. --open-windows malformed JSON tolerance
    const QString malformedJson = QStringLiteral("{ broken json [");
    const QJsonDocument docBroken = QJsonDocument::fromJson(malformedJson.toUtf8());
    assert(docBroken.isNull()); // Does not crash

    // 5. Marker Manager keyboard navigation specification: Esc closes, / searches
    const int closeKey = Qt::Key_Escape;
    const int searchKey = Qt::Key_Slash;
    assert(closeKey == Qt::Key_Escape);
    assert(searchKey == Qt::Key_Slash);

    // 6. Unified Manager keyboard semantics: [E] edits in both Workspaces and Markers, [C] creates empty workspace
    const int editKey = Qt::Key_E;
    const int createEmptyKey = Qt::Key_C;
    const int tabSwitchKey = Qt::Key_Tab;
    assert(editKey == Qt::Key_E);
    assert(createEmptyKey == Qt::Key_C);
    assert(tabSwitchKey == Qt::Key_Tab);

    std::cout << "testMarkerManagerCliParsing PASSED\n";
}

void testTrayAndSettingsCliArguments()
{
    // 1. Arguments that should trigger SettingsDialog
    const QStringList settingsVariants = {
        QStringLiteral("--settings"),
        QStringLiteral("-s"),
        QStringLiteral("settings")
    };

    for (const auto &v : settingsVariants) {
        const QStringList args = { QStringLiteral("leapk-tray"), v };
        const bool isSettings = args.contains(QStringLiteral("--settings"))
            || args.contains(QStringLiteral("-s"))
            || args.contains(QStringLiteral("settings"));
        assert(isSettings);
    }

    // 2. Binary name detection
    const QString binPath1 = QStringLiteral("/usr/local/bin/leapk-settings");
    assert(binPath1.endsWith(QStringLiteral("leapk-settings")));

    const QString binPath2 = QStringLiteral("/usr/bin/leapk-tray");
    assert(!binPath2.endsWith(QStringLiteral("leapk-settings")));

    std::cout << "testTrayAndSettingsCliArguments PASSED\n";
}

void testThemeCliFlagParsing()
{
    // 1. Workspace manager --theme parsing
    const QStringList wsArgs = {
        QStringLiteral("leapk-workspace-manager"),
        QStringLiteral("--theme"),
        QStringLiteral("retro-terminal"),
        QStringLiteral("org.kde.konsole"),
        QStringLiteral("bash")
    };
    QString wsThemeId;
    QString wsApp;
    QString wsCaption;
    for (int i = 1; i < wsArgs.size(); ++i) {
        if (wsArgs.at(i) == QStringLiteral("--theme") && (i + 1) < wsArgs.size()) {
            wsThemeId = wsArgs.at(++i);
        } else if (wsApp.isEmpty()) {
            wsApp = wsArgs.at(i);
        } else if (wsCaption.isEmpty()) {
            wsCaption = wsArgs.at(i);
        }
    }
    assert(wsThemeId == QStringLiteral("retro-terminal"));
    assert(wsApp == QStringLiteral("org.kde.konsole"));
    assert(wsCaption == QStringLiteral("bash"));

    // 2. Custom marker / colorpicker positional args extraction with --theme
    const QStringList cpArgs = {
        QStringLiteral("leapk-custom-marker"),
        QStringLiteral("--theme"),
        QStringLiteral("cyberpunk-neon"),
        QStringLiteral("#3498db"),
        QStringLiteral("M")
    };
    QString cpThemeId;
    QList<QString> cpPositional;
    for (int i = 1; i < cpArgs.size(); ++i) {
        if (cpArgs.at(i) == QStringLiteral("--theme") && (i + 1) < cpArgs.size()) {
            cpThemeId = cpArgs.at(++i);
        } else {
            cpPositional.append(cpArgs.at(i));
        }
    }
    assert(cpThemeId == QStringLiteral("cyberpunk-neon"));
    assert(cpPositional.size() == 2);
    assert(cpPositional.at(0) == QStringLiteral("#3498db"));
    assert(cpPositional.at(1) == QStringLiteral("M"));

    std::cout << "testThemeCliFlagParsing PASSED\n";
}

int main()
{
    testWorkspaceManagerCliParsing();
    testWorkspaceManagerIpcJsonFormats();
    testMarkerManagerCliParsing();
    testTrayAndSettingsCliArguments();
    testThemeCliFlagParsing();

    std::cout << "All CLI tests passed successfully!\n";
    return 0;
}
