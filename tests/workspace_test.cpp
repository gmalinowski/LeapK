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

    auto isPinned = [&](const QString &id, int wsIndex = -1) -> bool {
        if (!pinnedWindows.contains(id)) {
            return false;
        }
        if (wsIndex < 0) {
            return true;
        }
        const auto &set = windowPinnedWorkspaces.value(id);
        if (set.isEmpty() || set.contains(0)) {
            return true;
        }
        if (wsIndex > 0 && set.contains(wsIndex)) {
            return true;
        }
        return false;
    };

    auto pinToAll = [&](const QString &id) {
        pinnedWindows.insert(id);
        windowPinnedWorkspaces[id] = {0};
    };

    auto pinToWorkspace = [&](const QString &id, int wsIndex, int homeWs = 1) {
        pinnedWindows.insert(id);
        if (wsIndex <= 0) {
            windowPinnedWorkspaces[id] = {0};
        } else {
            auto &set = windowPinnedWorkspaces[id];
            if (set.isEmpty() && homeWs > 0 && homeWs != wsIndex) {
                set.insert(homeWs);
            }
            set.remove(0);
            set.insert(wsIndex);
        }
    };

    auto togglePinWorkspace = [&](const QString &id, int wsIndex, int homeWs = 1) {
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
            pinToWorkspace(id, wsIndex, homeWs);
            return;
        }
        auto &set = windowPinnedWorkspaces[id];
        if (set.contains(0)) {
            set.clear();
            if (homeWs > 0 && homeWs != wsIndex) {
                set.insert(homeWs);
            }
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
    assert(!isPinned(win1.id, 0));
    assert(!isPinned(win1.id, 1));
    assert(!isPinned(win1.id, 2));
    assert(win1.virtualDesktop == 2); // Desktop stays on 2

    // 2. Pin to All Workspaces
    pinToAll(win1.id);
    assert(isPinned(win1.id));
    assert(isPinned(win1.id, 0));
    assert(isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(isPinned(win1.id, 3));
    // Verify KDE virtual desktop assignment is strictly untouched (no KDE all-desktops)
    assert(win1.virtualDesktop == 2);

    // 3. Pin to specific workspaces (Workspace 1 and Workspace 2)
    unpin(win1.id);
    pinToWorkspace(win1.id, 2, win1.homeWorkspace); // Home workspace 1 preserved!
    assert(isPinned(win1.id));
    assert(!isPinned(win1.id, 0)); // Not globally pinned
    assert(isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(!isPinned(win1.id, 3)); // Not pinned to workspace 3!

    // 4. Toggle Workspace 3 on
    togglePinWorkspace(win1.id, 3, win1.homeWorkspace);
    assert(isPinned(win1.id, 3));

    // 5. Toggle Workspace 1 off
    togglePinWorkspace(win1.id, 1, win1.homeWorkspace);
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
    assert(!isPinned(win1.id, 0)); // Strictly not pinned to all workspaces!
    assert(isPinned(win1.id, 1));
    assert(isPinned(win1.id, 2));
    assert(!isPinned(win1.id, 3));
    assert(!windowPinnedWorkspaces.value(win1.id).contains(0)); // Strictly not pinned to all workspaces!

    std::cout << "testWorkspacePinningManager PASSED\n";
}

void testMultiWorkspacePinningAndDesktopSync()
{
    struct MockWindow {
        QString id;
        int homeWorkspace = 1;
        QSet<int> virtualDesktops; // Desktop IDs in KWin
        bool isMinimized = false;
        bool isOnAllDesktops = false;

        bool isOnDesktop(int desktopId) const {
            return isOnAllDesktops || virtualDesktops.contains(desktopId);
        }
    };

    QSet<QString> pinnedWindows;
    QMap<QString, QSet<int>> windowPinnedWorkspaces;

    auto isPinned = [&](const QString &id, int wsIndex = -1) -> bool {
        if (!pinnedWindows.contains(id)) return false;
        if (wsIndex < 0) return true;
        const auto &set = windowPinnedWorkspaces.value(id);
        if (set.isEmpty() || set.contains(0)) return true;
        if (wsIndex > 0 && set.contains(wsIndex)) return true;
        return false;
    };

    // Simulate workspace view isolation and desktop switching
    auto applyWorkspaceIsolationAndDesktopSync = [&](MockWindow &win, int activeHudView, int activeDesktop) {
        if (isPinned(win.id, activeHudView)) {
            // Pinned to this workspace: unminimize and sync virtual desktop
            win.isMinimized = false;
            if (!win.isOnAllDesktops) {
                win.virtualDesktops.insert(activeDesktop);
            }
        } else if (win.homeWorkspace == activeHudView) {
            // Home workspace: unminimize
            win.isMinimized = false;
        } else {
            // Other workspace: isolate (minimize)
            win.isMinimized = true;
        }
    };

    auto canRenderOnScreenBadge = [&](const MockWindow &win, int activeDesktop) -> bool {
        return !win.isMinimized && win.isOnDesktop(activeDesktop);
    };

    auto canConnectLaserTargetToWindow = [&](const MockWindow &win, int activeDesktop) -> bool {
        // Spatial keyboard connects directly to window target badge when window is visible on screen
        const bool isVisibleOnScreen = !win.isMinimized && win.isOnDesktop(activeDesktop);
        return isVisibleOnScreen;
    };

    // Window initially created in Workspace 1 (on Desktop 1)
    MockWindow win{QStringLiteral("deezer-music"), 1, {1}, false, false};

    // Pin window to Workspace 1 and Workspace 2
    pinnedWindows.insert(win.id);
    windowPinnedWorkspaces[win.id] = {1, 2};

    // 1. User is in Workspace 1 (Desktop 1)
    applyWorkspaceIsolationAndDesktopSync(win, 1, 1);
    assert(isPinned(win.id, 1) == true);
    assert(!win.isMinimized);
    assert(win.isOnDesktop(1) == true);
    assert(canRenderOnScreenBadge(win, 1) == true);
    assert(canConnectLaserTargetToWindow(win, 1) == true);

    // 2. User switches to Workspace 2 (Desktop 2)
    applyWorkspaceIsolationAndDesktopSync(win, 2, 2);
    assert(isPinned(win.id, 2) == true);
    assert(!win.isMinimized);
    assert(win.isOnDesktop(2) == true);
    // On-screen badge MUST be displayed on window pinned to Workspace 2!
    assert(canRenderOnScreenBadge(win, 2) == true);
    // Laser target MUST connect to window target badge on window pinned to Workspace 2!
    assert(canConnectLaserTargetToWindow(win, 2) == true);

    // 3. User switches to Workspace 3 (Desktop 3) - window is NOT pinned to Workspace 3
    applyWorkspaceIsolationAndDesktopSync(win, 3, 3);
    assert(isPinned(win.id, 3) == false);
    assert(win.isMinimized == true); // Isolated / hidden!
    assert(canRenderOnScreenBadge(win, 3) == false);
    assert(canConnectLaserTargetToWindow(win, 3) == false);

    // 4. User switches back to Workspace 1 (Desktop 1)
    applyWorkspaceIsolationAndDesktopSync(win, 1, 1);
    assert(isPinned(win.id, 1) == true);
    assert(!win.isMinimized);
    assert(win.isOnDesktop(1) == true);
    assert(canRenderOnScreenBadge(win, 1) == true);
    assert(canConnectLaserTargetToWindow(win, 1) == true);

    std::cout << "testMultiWorkspacePinningAndDesktopSync PASSED\n";
}

void testMultiInstanceWindowWorkspaceAssignment()
{
    using namespace LeapKWorkspace;

    // Mock window representation
    struct MockWindow {
        quintptr id;
        QString appClass;
        QString caption;
        int desktop;
        bool minimized;
    };

    MockWindow dolphin1{1001, QStringLiteral("org.kde.dolphin"), QStringLiteral("~/Documents — Dolphin"), 1, false};
    MockWindow dolphin2{1002, QStringLiteral("org.kde.dolphin"), QStringLiteral("~/Downloads — Dolphin"), 1, false};

    QList<WorkspaceInfo> workspaces;
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-work");
    ws1.name = QStringLiteral("Work");
    WindowMatchRule r1;
    r1.matchApp = QStringLiteral("org.kde.dolphin");
    r1.matchDesktop = 0;
    ws1.rules.append(r1);

    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-personal");
    ws2.name = QStringLiteral("Personal");

    workspaces.append(ws1);
    workspaces.append(ws2);

    // Runtime instance map (simulates m_windowWorkspace)
    QMap<quintptr, int> windowWorkspace;

    // Helper: findWorkspaceForWindow(target)
    auto findWorkspaceForWindow = [&](const MockWindow &win) -> int {
        if (windowWorkspace.contains(win.id)) {
            return windowWorkspace.value(win.id);
        }
        int found = findWorkspaceIndexForWindow(workspaces, win.appClass, win.caption, win.desktop);
        windowWorkspace[win.id] = found;
        return found;
    };

    // Helper: addWindowToWorkspace(win, wsIdx) with coexistence protection
    auto addWindowToWorkspace = [&](MockWindow &win, int targetWsIdx, const QList<MockWindow*> &allWindows) {
        windowWorkspace[win.id] = targetWsIdx;
        const QString appClass = win.appClass.trimmed();

        if (targetWsIdx >= 1 && targetWsIdx <= workspaces.size()) {
            bool otherInOtherWs = false;
            for (const auto *other : allWindows) {
                if (other && other->id != win.id && other->appClass == appClass) {
                    const int otherWs = windowWorkspace.value(other->id, 0);
                    if (otherWs > 0 && otherWs != targetWsIdx) {
                        otherInOtherWs = true;
                        break;
                    }
                }
            }

            if (otherInOtherWs) {
                // Preserve rules in other workspaces, ensure target workspace has a rule
                auto &ws = workspaces[targetWsIdx - 1];
                bool alreadyHasRule = false;
                for (const auto &r : ws.rules) {
                    if (r.matchApp.compare(appClass, Qt::CaseInsensitive) == 0) {
                        alreadyHasRule = true;
                        break;
                    }
                }
                if (!alreadyHasRule) {
                    WindowMatchRule r;
                    r.matchApp = appClass;
                    r.matchDesktop = win.desktop;
                    ws.rules.append(r);
                }
            } else {
                reassignWindowToWorkspace(workspaces, workspaces.at(targetWsIdx - 1).id, appClass, win.caption, true, 0);
            }
        }
    };

    // 1. Initial lookup: Dolphin 1 matches Workspace 1 via static rule
    assert(findWorkspaceForWindow(dolphin1) == 1);
    assert(windowWorkspace.value(dolphin1.id) == 1);

    // 2. Open Dolphin 2 while in Workspace 2 (view 2)
    const int activeHudView = 2;
    windowWorkspace[dolphin2.id] = activeHudView;
    addWindowToWorkspace(dolphin2, 2, {&dolphin1, &dolphin2});

    // Verify both window instances are tracked independently in their respective workspaces
    assert(findWorkspaceForWindow(dolphin1) == 1);
    assert(findWorkspaceForWindow(dolphin2) == 2);

    // Verify Workspace 1 rule was NOT destroyed
    assert(workspaces.at(0).rules.size() >= 1);
    assert(workspaces.at(0).rules.at(0).matchApp == QStringLiteral("org.kde.dolphin"));

    // Verify Workspace 2 now has a rule for Dolphin as well
    assert(workspaces.at(1).rules.size() >= 1);
    assert(workspaces.at(1).rules.at(0).matchApp == QStringLiteral("org.kde.dolphin"));

    // 3. Test workspace isolation simulation
    auto applyIsolation = [&](int hudViewIndex, QList<MockWindow*> wins) {
        for (auto *w : wins) {
            const bool matches = (findWorkspaceForWindow(*w) == hudViewIndex);
            w->minimized = !matches;
        }
    };

    // View Workspace 1: dolphin1 visible, dolphin2 minimized
    applyIsolation(1, {&dolphin1, &dolphin2});
    assert(dolphin1.minimized == false);
    assert(dolphin2.minimized == true);

    // View Workspace 2: dolphin2 visible, dolphin1 minimized
    applyIsolation(2, {&dolphin1, &dolphin2});
    assert(dolphin1.minimized == true);
    assert(dolphin2.minimized == false);

    // 4. Test HUD counts per workspace
    auto countWindowsForWorkspace = [&](int wsIdx, const QList<MockWindow*> &wins) -> int {
        int count = 0;
        for (const auto *w : wins) {
            if (findWorkspaceForWindow(*w) == wsIdx) {
                count++;
            }
        }
        return count;
    };

    assert(countWindowsForWorkspace(1, {&dolphin1, &dolphin2}) == 1);
    assert(countWindowsForWorkspace(2, {&dolphin1, &dolphin2}) == 1);

    // 5. Test workspace deletion shift
    // Deleting Workspace 1:
    // Window in ws 1 is removed from instance map
    // Window in ws 2 has its ws index shifted 2 -> 1
    workspaces.removeAt(0); // Workspace 2 becomes Workspace 1
    for (auto it = windowWorkspace.begin(); it != windowWorkspace.end(); ) {
        if (it.value() == 1) {
            it = windowWorkspace.erase(it);
        } else if (it.value() > 1) {
            *it = it.value() - 1;
            ++it;
        } else {
            ++it;
        }
    }

    assert(!windowWorkspace.contains(dolphin1.id));
    assert(windowWorkspace.value(dolphin2.id) == 1); // Dolphin 2 shifted cleanly to Workspace 1

    std::cout << "testMultiInstanceWindowWorkspaceAssignment PASSED\n";
}

void testWorkspaceShortcutKeyMapping()
{
    using namespace LeapKWorkspace;

    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-1");
    ws1.name = QStringLiteral("Backend");
    ws1.shortcutKey = QChar('B');

    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-2");
    ws2.name = QStringLiteral("Frontend");
    ws2.shortcutKey = QChar('1');

    WorkspaceInfo ws3;
    ws3.id = QStringLiteral("ws-3");
    ws3.name = QStringLiteral("Research");
    // No shortcutKey (isNull)

    QList<WorkspaceInfo> workspaces = { ws1, ws2, ws3 };

    // Function matching HUD direct jump logic
    auto findWorkspaceByShortcut = [&](QChar key) -> int {
        if (key.isNull()) return -1;
        const QChar upperKey = key.toUpper();
        for (int i = 0; i < workspaces.size(); ++i) {
            if (!workspaces.at(i).shortcutKey.isNull() &&
                workspaces.at(i).shortcutKey.toUpper() == upperKey) {
                return i + 1; // 1-based index
            }
        }
        return -1;
    };

    assert(findWorkspaceByShortcut(QChar('B')) == 1);
    assert(findWorkspaceByShortcut(QChar('b')) == 1); // Case-insensitive
    assert(findWorkspaceByShortcut(QChar('1')) == 2); // Digit shortcut
    assert(findWorkspaceByShortcut(QChar('R')) == -1); // ws3 has no shortcut
    assert(findWorkspaceByShortcut(QChar('X')) == -1); // Unbound key

    // Tab formatting check
    auto formatTabName = [](const WorkspaceInfo &ws) -> QString {
        return !ws.shortcutKey.isNull()
            ? QStringLiteral("[%1] %2").arg(ws.shortcutKey.toUpper(), ws.name)
            : ws.name;
    };

    assert(formatTabName(ws1) == QStringLiteral("[B] Backend"));
    assert(formatTabName(ws2) == QStringLiteral("[1] Frontend"));
    assert(formatTabName(ws3) == QStringLiteral("Research"));

    // Serialization round-trip with digits and letters
    QJsonObject json1 = workspaceToJson(ws1);
    WorkspaceInfo restored1 = workspaceFromJson(json1);
    assert(restored1.shortcutKey == QChar('B'));

    QJsonObject json2 = workspaceToJson(ws2);
    WorkspaceInfo restored2 = workspaceFromJson(json2);
    assert(restored2.shortcutKey == QChar('1'));

    QJsonObject json3 = workspaceToJson(ws3);
    WorkspaceInfo restored3 = workspaceFromJson(json3);
    assert(restored3.shortcutKey.isNull());

    std::cout << "testWorkspaceShortcutKeyMapping PASSED\n";
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
    testMultiWorkspacePinningAndDesktopSync();
    testMultiInstanceWindowWorkspaceAssignment();
    testWorkspaceShortcutKeyMapping();
    std::cout << "All workspace tests passed successfully!\n";
    return 0;
}
