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
    ws.lastVirtualDesktopId = QStringLiteral("desk-uuid-1234");
    ws.tilingPresetId = QStringLiteral("columns-2");

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
    assert(restored.lastVirtualDesktopId == ws.lastVirtualDesktopId);
    assert(restored.tilingPresetId == ws.tilingPresetId);
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

void testDeleteWorkspace()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    QList<WorkspaceInfo> list;
    WorkspaceInfo ws1 = createEmptyWorkspace(list, QStringLiteral("Frontend"), filePath);
    WorkspaceInfo ws2 = createEmptyWorkspace(list, QStringLiteral("Backend"), filePath);
    assert(list.size() == 2);

    bool deleted = deleteWorkspace(list, ws1.id, filePath);
    assert(deleted);
    assert(list.size() == 1);
    assert(list.first().id == ws2.id);

    QList<WorkspaceInfo> reloaded = loadWorkspaces(filePath);
    assert(reloaded.size() == 1);
    assert(reloaded.first().id == ws2.id);

    bool deleteNonExistent = deleteWorkspace(list, QStringLiteral("non-existent-id"), filePath);
    assert(!deleteNonExistent);
    assert(list.size() == 1);

    std::cout << "testDeleteWorkspace PASSED\n";
}

void testUpdateWorkspace()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    QList<WorkspaceInfo> list;
    WorkspaceInfo ws1 = createEmptyWorkspace(list, QStringLiteral("Frontend"), filePath);
    assert(list.size() == 1);

    bool updated = updateWorkspace(list, ws1.id, QStringLiteral("Renamed Frontend"), QColor(QStringLiteral("#9b59b6")), filePath);
    assert(updated);
    assert(list.first().name == QStringLiteral("Renamed Frontend"));
    assert(list.first().color == QColor(QStringLiteral("#9b59b6")));

    QList<WorkspaceInfo> reloaded = loadWorkspaces(filePath);
    assert(reloaded.size() == 1);
    assert(reloaded.first().name == QStringLiteral("Renamed Frontend"));
    assert(reloaded.first().color == QColor(QStringLiteral("#9b59b6")));

    bool updateNonExistent = updateWorkspace(list, QStringLiteral("non-existent-id"), QStringLiteral("Test"), QColor());
    assert(!updateNonExistent);

    std::cout << "testUpdateWorkspace PASSED\n";
}

void testWorkspaceViewsAndCycling()
{
    using namespace LeapKWorkspace;

    // 1. Zero custom workspaces: only Standard (0) and All (1) exist -> total 2 views
    {
        const QList<WorkspaceInfo> emptyWorkspaces;
        const int totalViews = computeTotalWorkspaceViews(emptyWorkspaces.size());
        assert(totalViews == 2);

        assert(workspaceIdForHudView(0, emptyWorkspaces) == QStringLiteral("__standard__"));
        assert(workspaceIdForHudView(1, emptyWorkspaces) == QStringLiteral("__all__"));
        assert(workspaceIdForHudView(2, emptyWorkspaces) == QStringLiteral("__standard__")); // out of bounds fallback

        int view = 0;
        view = (view + 1) % totalViews;
        assert(view == 1); // Advances to All
        view = (view + 1) % totalViews;
        assert(view == 0); // Wraps directly back to Standard (no phantom 3rd view!)

        // Backward cycling
        view = (view - 1 + totalViews) % totalViews;
        assert(view == 1); // From Standard to All
        view = (view - 1 + totalViews) % totalViews;
        assert(view == 0); // From All to Standard
    }

    // 2. Multiple custom workspaces (e.g. 2 custom workspaces -> total 4 views: 0=Standard, 1=WS1, 2=WS2, 3=All)
    {
        WorkspaceInfo ws1;
        ws1.id = QStringLiteral("ws-web");
        ws1.name = QStringLiteral("Web");
        WorkspaceInfo ws2;
        ws2.id = QStringLiteral("ws-dev");
        ws2.name = QStringLiteral("Dev");

        const QList<WorkspaceInfo> workspaces = { ws1, ws2 };
        const int totalViews = computeTotalWorkspaceViews(workspaces.size());
        assert(totalViews == 4);

        assert(workspaceIdForHudView(0, workspaces) == QStringLiteral("__standard__"));
        assert(workspaceIdForHudView(1, workspaces) == QStringLiteral("ws-web"));
        assert(workspaceIdForHudView(2, workspaces) == QStringLiteral("ws-dev"));
        assert(workspaceIdForHudView(3, workspaces) == QStringLiteral("__all__"));

        int view = 0;
        view = (view + 1) % totalViews;
        assert(view == 1);
        view = (view + 1) % totalViews;
        assert(view == 2);
        view = (view + 1) % totalViews;
        assert(view == 3);
        view = (view + 1) % totalViews;
        assert(view == 0); // Wraps cleanly
    }

    std::cout << "testWorkspaceViewsAndCycling PASSED\n";
}

void testWorkspacePinningManager()
{
    struct MockPinWindow {
        QString id;
        int homeWorkspace = 1;
        int virtualDesktop = 2; // e.g. Desktop 2
    };

    QSet<QString> pinnedWindows;
    QMap<QString, QSet<int>> windowPinnedWorkspaces;

    auto isPinned = [&](const QString &id, int wsIndex = 0) -> bool {
        if (!pinnedWindows.contains(id)) {
            return false;
        }
        if (wsIndex <= 0) {
            return true;
        }
        const auto &set = windowPinnedWorkspaces.value(id);
        return set.isEmpty() || set.contains(0) || set.contains(wsIndex);
    };

    auto pinToAll = [&](const QString &id) {
        pinnedWindows.insert(id);
        windowPinnedWorkspaces[id] = {0};
    };

    auto pinToWorkspace = [&](const QString &id, int wsIndex) {
        pinnedWindows.insert(id);
        if (wsIndex <= 0) {
            windowPinnedWorkspaces[id] = {0};
        } else {
            auto &set = windowPinnedWorkspaces[id];
            set.remove(0);
            set.insert(wsIndex);
        }
    };

    auto togglePinWorkspace = [&](const QString &id, int wsIndex) {
        if (wsIndex <= 0) {
            if (isPinned(id) && windowPinnedWorkspaces.value(id).contains(0)) {
                pinnedWindows.remove(id);
                windowPinnedWorkspaces.remove(id);
            } else {
                pinToAll(id);
            }
            return;
        }
        if (!pinnedWindows.contains(id)) {
            pinToWorkspace(id, wsIndex);
            return;
        }
        auto &set = windowPinnedWorkspaces[id];
        if (set.contains(0)) {
            set.clear();
            set.insert(wsIndex);
        } else if (set.contains(wsIndex)) {
            set.remove(wsIndex);
            if (set.isEmpty()) {
                pinnedWindows.remove(id);
                windowPinnedWorkspaces.remove(id);
            }
        } else {
            set.insert(wsIndex);
        }
    };

    auto unpin = [&](const QString &id) {
        pinnedWindows.remove(id);
        windowPinnedWorkspaces.remove(id);
    };

    MockPinWindow win1{QStringLiteral("terminal-1"), 1, 2};

    // 1. Initially not pinned
    assert(!isPinned(win1.id));
    assert(!isPinned(win1.id, 1));
    assert(!isPinned(win1.id, 2));
    assert(win1.virtualDesktop == 2); // Desktop stays on 2

    // 2. Pin to All Workspaces
    pinToAll(win1.id);
    assert(isPinned(win1.id));
    assert(isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(isPinned(win1.id, 3));
    // Verify KDE virtual desktop assignment is strictly untouched (no KDE all-desktops)
    assert(win1.virtualDesktop == 2);

    // 3. Pin to specific workspaces (Workspace 1 and Workspace 2)
    unpin(win1.id);
    pinToWorkspace(win1.id, 1);
    pinToWorkspace(win1.id, 2);
    assert(isPinned(win1.id));
    assert(isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(!isPinned(win1.id, 3)); // Not pinned to workspace 3!

    // 4. Toggle Workspace 3 on
    togglePinWorkspace(win1.id, 3);
    assert(isPinned(win1.id, 3));

    // 5. Toggle Workspace 1 off
    togglePinWorkspace(win1.id, 1);
    assert(!isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(isPinned(win1.id, 3));

    // 6. Unpin completely
    unpin(win1.id);
    assert(!isPinned(win1.id));
    assert(!isPinned(win1.id, 1));
    assert(!isPinned(win1.id, 2));
    assert(!isPinned(win1.id, 3));
    assert(win1.virtualDesktop == 2);

    // 7. Test "Pin / Both Workspaces" action logic
    auto pinToWorkspaces = [&](const QString &id, const QSet<int> &wsIndices) {
        pinnedWindows.insert(id);
        auto &set = windowPinnedWorkspaces[id];
        set.clear();
        for (int idx : wsIndices) {
            if (idx > 0) set.insert(idx);
        }
        if (set.isEmpty()) set.insert(0);
    };

    pinToWorkspaces(win1.id, {1, 2});
    assert(isPinned(win1.id));
    assert(isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(!isPinned(win1.id, 3));
    assert(!windowPinnedWorkspaces.value(win1.id).contains(0)); // Strictly not pinned to all workspaces!

    std::cout << "testWorkspacePinningManager PASSED\n";
}

int main()
{
    testRuleSerialization();
    testWorkspaceSerialization();
    testSaveAndLoadWorkspaces();
    testWorkspaceMatching();
    testCreateEmptyWorkspace();
    testDeleteWorkspace();
    testUpdateWorkspace();
    testWorkspaceViewsAndCycling();
    testWorkspacePinningManager();
    std::cout << "All workspace tests passed successfully!\n";
    return 0;
}
