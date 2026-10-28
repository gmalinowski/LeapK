/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "workspacehelper.h"
#include <QTemporaryDir>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testRuleSerialization()
{
    using namespace LeapKWorkspace;

    WindowMatchRule r;
    r.matchApp = QStringLiteral("org.kde.konsole");
    r.matchCaption = QStringLiteral("nvim");
    r.matchDesktop = 2;

    QJsonObject json = ruleToJson(r);
    WindowMatchRule restored = ruleFromJson(json);

    assert(restored.matchApp == r.matchApp);
    assert(restored.matchCaption == r.matchCaption);
    assert(restored.matchDesktop == r.matchDesktop);

    std::cout << "testRuleSerialization PASSED\n";
}

void testWorkspaceSerialization()
{
    using namespace LeapKWorkspace;

    WorkspaceInfo ws;
    ws.id = QStringLiteral("ws-backend");
    ws.name = QStringLiteral("Backend Dev");
    ws.color = QColor(QStringLiteral("#3498db"));
    ws.shortcutKey = QChar('B');
    ws.createdAt = 1700000000000;

    WindowMatchRule r1;
    r1.matchApp = QStringLiteral("org.kde.konsole");
    r1.matchCaption = QStringLiteral("cargo run");
    r1.matchDesktop = 0;
    ws.rules.append(r1);

    WindowMatchRule r2;
    r2.matchApp = QStringLiteral("firefox");
    r2.matchCaption = QStringLiteral("localhost:8080");
    r2.matchDesktop = 1;
    ws.rules.append(r2);

    QJsonObject json = workspaceToJson(ws);
    WorkspaceInfo restored = workspaceFromJson(json);

    assert(restored.id == ws.id);
    assert(restored.name == ws.name);
    assert(restored.color == ws.color);
    assert(restored.shortcutKey == ws.shortcutKey);
    assert(restored.createdAt == ws.createdAt);
    assert(restored.rules.size() == 2);
    assert(restored.rules.at(0).matchApp == r1.matchApp);
    assert(restored.rules.at(0).matchCaption == r1.matchCaption);
    assert(restored.rules.at(1).matchApp == r2.matchApp);
    assert(restored.rules.at(1).matchDesktop == 1);

    std::cout << "testWorkspaceSerialization PASSED\n";
}

void testSaveAndLoadWorkspaces()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-1");
    ws1.name = QStringLiteral("Frontend");
    ws1.color = QColor(QStringLiteral("#2ecc71"));

    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-2");
    ws2.name = QStringLiteral("Backend");
    ws2.color = QColor(QStringLiteral("#e74c3c"));

    QList<WorkspaceInfo> list = { ws1, ws2 };
    bool saved = saveWorkspaces(list, filePath);
    assert(saved);

    QList<WorkspaceInfo> loaded = loadWorkspaces(filePath);
    assert(loaded.size() == 2);
    assert(loaded.at(0).id == ws1.id);
    assert(loaded.at(0).name == QStringLiteral("Frontend"));
    assert(loaded.at(1).id == ws2.id);
    assert(loaded.at(1).name == QStringLiteral("Backend"));

    std::cout << "testSaveAndLoadWorkspaces PASSED\n";
}

void testWorkspaceMatching()
{
    using namespace LeapKWorkspace;

    WorkspaceInfo ws;
    ws.id = QStringLiteral("ws-backend");
    ws.name = QStringLiteral("Backend");

    // Rule 1: Konsole running cargo/rust
    WindowMatchRule r1;
    r1.matchApp = QStringLiteral("org.kde.konsole");
    r1.matchCaption = QStringLiteral("cargo");
    ws.rules.append(r1);

    // Rule 2: Postman on any desktop
    WindowMatchRule r2;
    r2.matchApp = QStringLiteral("postman");
    ws.rules.append(r2);

    // Rule 3: Browser on desktop 2 only
    WindowMatchRule r3;
    r3.matchApp = QStringLiteral("firefox");
    r3.matchDesktop = 2;
    ws.rules.append(r3);

    // 1. Konsole with "cargo build" -> should match rule 1
    assert(matchesWorkspace(ws, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo build — Konsole"), 1));

    // 2. Konsole with generic bash -> should NOT match (caption mismatch)
    assert(!matchesWorkspace(ws, QStringLiteral("org.kde.konsole"), QStringLiteral("bash — Konsole"), 1));

    // 3. Postman -> should match rule 2 regardless of caption or desktop
    assert(matchesWorkspace(ws, QStringLiteral("Postman"), QStringLiteral("API Collections"), 3));

    // 4. Firefox on desktop 2 -> should match rule 3
    assert(matchesWorkspace(ws, QStringLiteral("firefox"), QStringLiteral("Documentation"), 2));

    // 5. Firefox on desktop 1 -> should NOT match (desktop mismatch)
    assert(!matchesWorkspace(ws, QStringLiteral("firefox"), QStringLiteral("Documentation"), 1));

    // 6. Completely unrelated app
    assert(!matchesWorkspace(ws, QStringLiteral("vlc"), QStringLiteral("Media Player"), 1));

    std::cout << "testWorkspaceMatching PASSED\n";
}

void testCreateEmptyWorkspace()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    QList<WorkspaceInfo> list;
    WorkspaceInfo ws = createEmptyWorkspace(list, QStringLiteral("Clean Project"), filePath);

    assert(list.size() == 1);
    assert(ws.name == QStringLiteral("Clean Project"));
    assert(ws.rules.isEmpty());
    assert(ws.color.isValid());

    // Clean empty workspace must not match any window
    assert(!matchesWorkspace(ws, QStringLiteral("firefox"), QStringLiteral("Google"), 1));
    assert(!matchesWorkspace(ws, QStringLiteral("org.kde.konsole"), QStringLiteral("bash"), 1));

    // Reload from file to ensure persistent storage
    QList<WorkspaceInfo> reloaded = loadWorkspaces(filePath);
    assert(reloaded.size() == 1);
    assert(reloaded.first().name == QStringLiteral("Clean Project"));
    assert(reloaded.first().rules.isEmpty());

    // Creating second workspace without name auto-increments
    WorkspaceInfo ws2 = createEmptyWorkspace(reloaded, QString(), filePath);
    assert(reloaded.size() == 2);
    assert(ws2.name == QStringLiteral("Workspace 2"));
    assert(ws2.rules.isEmpty());

    std::cout << "testCreateEmptyWorkspace PASSED\n";
}

int main()
{
    testRuleSerialization();
    testWorkspaceSerialization();
    testSaveAndLoadWorkspaces();
    testWorkspaceMatching();
    testCreateEmptyWorkspace();
    std::cout << "All workspace tests passed successfully!\n";
    return 0;
}
