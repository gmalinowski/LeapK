/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "workspacehelper.h"
#include <QTemporaryDir>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testComplexRuleMatching()
{
    using namespace LeapKWorkspace;

    WorkspaceInfo ws;
    ws.id = QStringLiteral("ws-complex");
    ws.name = QStringLiteral("Complex Rules");

    // Rule 1: Neovim with brackets and paths in caption
    WindowMatchRule r1;
    r1.matchApp = QStringLiteral("org.kde.konsole");
    r1.matchCaption = QStringLiteral("[leapk]");
    ws.rules.append(r1);

    // Rule 2: Specific browser on Desktop 3 only
    WindowMatchRule r2;
    r2.matchApp = QStringLiteral("google-chrome");
    r2.matchDesktop = 3;
    ws.rules.append(r2);

    // Rule 3: Postman on any desktop
    WindowMatchRule r3;
    r3.matchApp = QStringLiteral("postman");
    ws.rules.append(r3);

    // 1. Bracketed caption match
    assert(matchesWorkspace(ws, QStringLiteral("org.kde.konsole"), QStringLiteral("~/dev/[leapk]/leapk.cpp — Konsole"), 1));

    // 2. Case insensitivity in app and caption
    assert(matchesWorkspace(ws, QStringLiteral("ORG.KDE.KONSOLE"), QStringLiteral("[LEAPK] main branch"), 1));

    // 3. Caption mismatch
    assert(!matchesWorkspace(ws, QStringLiteral("org.kde.konsole"), QStringLiteral("bash — Konsole"), 1));

    // 4. Desktop mismatch for Chrome
    assert(!matchesWorkspace(ws, QStringLiteral("google-chrome"), QStringLiteral("Documentation"), 1));
    assert(!matchesWorkspace(ws, QStringLiteral("google-chrome"), QStringLiteral("Documentation"), 2));
    assert(matchesWorkspace(ws, QStringLiteral("google-chrome"), QStringLiteral("Documentation"), 3));

    // 5. Postman matches on any desktop
    assert(matchesWorkspace(ws, QStringLiteral("postman"), QStringLiteral("API Collections"), 1));
    assert(matchesWorkspace(ws, QStringLiteral("postman"), QStringLiteral("Test Suite"), 4));

    std::cout << "testComplexRuleMatching PASSED\n";
}

void testMultiRulePrecedenceAndFirstMatch()
{
    using namespace LeapKWorkspace;

    WorkspaceInfo wsWork;
    wsWork.id = QStringLiteral("ws-work");
    wsWork.name = QStringLiteral("Work");

    WindowMatchRule rWork;
    rWork.matchApp = QStringLiteral("firefox");
    rWork.matchCaption = QStringLiteral("Jira");
    wsWork.rules.append(rWork);

    WorkspaceInfo wsPersonal;
    wsPersonal.id = QStringLiteral("ws-personal");
    wsPersonal.name = QStringLiteral("Personal");

    WindowMatchRule rPersonal;
    rPersonal.matchApp = QStringLiteral("firefox");
    rPersonal.matchCaption = QStringLiteral("YouTube");
    wsPersonal.rules.append(rPersonal);

    QList<WorkspaceInfo> workspaces = { wsWork, wsPersonal };

    // 1. Jira window goes to Work (index 1)
    assert(findWorkspaceIndexForWindow(workspaces, QStringLiteral("firefox"), QStringLiteral("PROJ-123 · Jira — Mozilla Firefox"), 1) == 1);
    const auto *foundWork = findWorkspaceForWindow(workspaces, QStringLiteral("firefox"), QStringLiteral("PROJ-123 · Jira — Mozilla Firefox"), 1);
    assert(foundWork != nullptr);
    assert(foundWork->id == wsWork.id);

    // 2. YouTube window goes to Personal (index 2)
    assert(findWorkspaceIndexForWindow(workspaces, QStringLiteral("firefox"), QStringLiteral("Chill Music — YouTube — Mozilla Firefox"), 1) == 2);
    const auto *foundPersonal = findWorkspaceForWindow(workspaces, QStringLiteral("firefox"), QStringLiteral("Chill Music — YouTube — Mozilla Firefox"), 1);
    assert(foundPersonal != nullptr);
    assert(foundPersonal->id == wsPersonal.id);

    // 3. Generic window without matching caption goes to Standard (index 0)
    assert(findWorkspaceIndexForWindow(workspaces, QStringLiteral("firefox"), QStringLiteral("Wikipedia"), 1) == 0);
    assert(findWorkspaceForWindow(workspaces, QStringLiteral("firefox"), QStringLiteral("Wikipedia"), 1) == nullptr);

    std::cout << "testMultiRulePrecedenceAndFirstMatch PASSED\n";
}

void testWindowMigrationAndExclusivity()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    WorkspaceInfo wsA;
    wsA.id = QStringLiteral("ws-a");
    wsA.name = QStringLiteral("Team Alpha");

    WorkspaceInfo wsB;
    wsB.id = QStringLiteral("ws-b");
    wsB.name = QStringLiteral("Team Beta");

    QList<WorkspaceInfo> list = { wsA, wsB };

    // Assign Alacritty to Team Alpha
    reassignWindowToWorkspace(list, wsA.id, QStringLiteral("Alacritty"), QStringLiteral("dev-server"), false, 0, filePath);
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("Alacritty"), QStringLiteral("dev-server"), 1) == 1);

    // Migrate Alacritty to Team Beta
    bool migrated = reassignWindowToWorkspace(list, wsB.id, QStringLiteral("Alacritty"), QStringLiteral("dev-server"), false, 0, filePath);
    assert(migrated);

    // Team Alpha must NOT contain Alacritty anymore
    assert(list.at(0).rules.isEmpty());
    // Team Beta must contain Alacritty
    assert(list.at(1).rules.size() == 1);
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("Alacritty"), QStringLiteral("dev-server"), 1) == 2);

    // Unassign window completely
    reassignWindowToWorkspace(list, QString(), QStringLiteral("Alacritty"), QStringLiteral("dev-server"), false, 0, filePath);
    assert(list.at(0).rules.isEmpty());
    assert(list.at(1).rules.isEmpty());
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("Alacritty"), QStringLiteral("dev-server"), 1) == 0);

    std::cout << "testWindowMigrationAndExclusivity PASSED\n";
}

void testWorkspaceSerializationFaultTolerance()
{
    using namespace LeapKWorkspace;

    // 1. JSON missing optional fields
    QJsonObject minimalJson;
    minimalJson[QStringLiteral("name")] = QStringLiteral("Minimal Workspace");

    WorkspaceInfo parsed = workspaceFromJson(minimalJson);
    assert(parsed.name == QStringLiteral("Minimal Workspace"));
    assert(parsed.color.isValid()); // defaults to #3498db
    assert(parsed.rules.isEmpty());
    assert(parsed.shortcutKey.isNull());

    // 2. Corrupted color string
    QJsonObject corruptColorJson;
    corruptColorJson[QStringLiteral("name")] = QStringLiteral("Bad Color");
    corruptColorJson[QStringLiteral("color")] = QStringLiteral("invalid-hex-value");
    WorkspaceInfo badColorWs = workspaceFromJson(corruptColorJson);
    assert(!badColorWs.color.isValid()); // QColor handles invalid string gracefully

    // 3. Round-trip through JSON
    QJsonObject serialized = workspaceToJson(badColorWs);
    assert(serialized.value(QStringLiteral("color")).toString() == QStringLiteral("#3498db")); // serialized handles invalid fallback

    std::cout << "testWorkspaceSerializationFaultTolerance PASSED\n";
}

int main()
{
    testComplexRuleMatching();
    testMultiRulePrecedenceAndFirstMatch();
    testWindowMigrationAndExclusivity();
    testWorkspaceSerializationFaultTolerance();

    std::cout << "All workspace rules tests passed successfully!\n";
    return 0;
}
