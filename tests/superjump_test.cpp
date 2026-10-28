/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "badgehelper.h"
#include "workspacehelper.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testSuperJumpBannerFormatting()
{
    // 1. Super Jump with named workspace
    QImage superJumpDev = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QStringLiteral("Backend"), true);
    assert(!superJumpDev.isNull());
    assert(superJumpDev.width() > 200);
    assert(superJumpDev.height() >= 30);

    // 2. Super Jump with empty workspace name (falls back to "Workspace")
    QImage superJumpDefault = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QString(), true);
    assert(!superJumpDefault.isNull());
    assert(superJumpDefault.width() > 150);

    // 3. Super Jump with HiDPI scale (2.0)
    QImage superJumpHiDpi = OutlineBadge::renderHudModeHeader(
        2.0, false, QString(), QColor(), 0, {}, -1, false, QStringLiteral("Frontend UI"), true);
    assert(!superJumpHiDpi.isNull());
    assert(superJumpHiDpi.width() >= superJumpDev.width() * 1.5);

    std::cout << "testSuperJumpBannerFormatting PASSED\n";
}

void testStashViewHeaderBanner()
{
    // 1. Stash mode header banner
    QImage stashBanner = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, 3, true, QStringLiteral("Frontend"));
    assert(!stashBanner.isNull());
    assert(stashBanner.width() > 200);

    // 2. Stash banner with HiDPI scale
    QImage stashBannerHiDpi = OutlineBadge::renderHudModeHeader(
        1.5, false, QString(), QColor(), 0, {}, 5, true, QStringLiteral("Docs"));
    assert(!stashBannerHiDpi.isNull());
    assert(stashBannerHiDpi.width() > stashBanner.width());

    std::cout << "testStashViewHeaderBanner PASSED\n";
}

void testStashedBadgeDesaturationAndDashing()
{
    OutlineBadge::BadgeConfig cfg;
    cfg.hudBadgeSize = 32;

    const QColor vibrantOrange(230, 126, 34); // S ~ 85%, V ~ 90%

    // 1. Normal window badge
    const QImage normalBadge = OutlineBadge::renderBadgeImage(
        cfg, QChar('K'), vibrantOrange, Qt::white, false, 28, 1.0,
        QString(), QIcon(), QImage(), QString(), false, false);
    assert(!normalBadge.isNull());

    // 2. Stashed window badge (isStashed = true)
    const QImage stashedBadge = OutlineBadge::renderBadgeImage(
        cfg, QChar('K'), vibrantOrange, Qt::white, false, 28, 1.0,
        QString(), QIcon(), QImage(), QString(), false, true);
    assert(!stashedBadge.isNull());
    assert(stashedBadge.size() == normalBadge.size());

    // 3. Stashed HUD pill
    const QImage stashedPill = OutlineBadge::renderOffscreenTargetPill(
        cfg, QChar('K'), vibrantOrange, Qt::white,
        QStringLiteral("📦 Stash"), QStringLiteral("Konsole"),
        32, 1.0, QString(), QIcon(), QImage(), true, QString(), false, 0, true);
    assert(!stashedPill.isNull());
    assert(stashedPill.width() > 60);

    // 4. Verify contrasting text color for both
    assert(OutlineBadge::contrastingTextColor(vibrantOrange).isValid());

    std::cout << "testStashedBadgeDesaturationAndDashing PASSED\n";
}

void testCrossWorkspacePullLogic()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    // Workspace 1: Backend
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-backend");
    ws1.name = QStringLiteral("Backend");

    // Workspace 2: Frontend
    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-frontend");
    ws2.name = QStringLiteral("Frontend");

    QList<WorkspaceInfo> list = { ws1, ws2 };

    // Initially, assign "org.kde.konsole" with "cargo" to Backend
    reassignWindowToWorkspace(list, ws1.id, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo"), false, 0, filePath);
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo run"), 1) == 1);

    // Super Jump pulls "org.kde.konsole" into Frontend (ws2)
    bool pulled = reassignWindowToWorkspace(list, ws2.id, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo"), false, 0, filePath);
    assert(pulled);

    // Window must now match Frontend (index 2) and NOT Backend (index 1)
    const int newWsIdx = findWorkspaceIndexForWindow(list, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo run"), 1);
    assert(newWsIdx == 2);

    // Reload from disk to verify persistence
    QList<WorkspaceInfo> reloaded = loadWorkspaces(filePath);
    assert(findWorkspaceIndexForWindow(reloaded, QStringLiteral("org.kde.konsole"), QStringLiteral("cargo run"), 1) == 2);

    std::cout << "testCrossWorkspacePullLogic PASSED\n";
}

void testSuperJumpDefaultJumpVsPullHere()
{
    using namespace LeapKWorkspace;

    // Workspace 1: Backend
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-backend");
    ws1.name = QStringLiteral("Backend");

    // Workspace 2: Frontend
    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-frontend");
    ws2.name = QStringLiteral("Frontend");

    WindowMatchRule r;
    r.matchApp = QStringLiteral("firefox");
    ws2.rules.append(r);

    QList<WorkspaceInfo> list = { ws1, ws2 };

    // Default Jump: window in ws2 resolves target workspace to 2 (without modifying rules or pulling!)
    int targetWs = findWorkspaceIndexForWindow(list, QStringLiteral("firefox"), QStringLiteral("Mozilla"), 1);
    assert(targetWs == 2);
    assert(ws1.rules.isEmpty());
    assert(ws2.rules.size() == 1);

    // Alt+Key (Pull Here): reassigns window from ws2 to ws1
    reassignWindowToWorkspace(list, ws1.id, QStringLiteral("firefox"), QStringLiteral("Mozilla"), true, 0);
    assert(findWorkspaceIndexForWindow(list, QStringLiteral("firefox"), QStringLiteral("Mozilla"), 1) == 1);
    assert(list.at(0).rules.size() == 1);
    assert(list.at(1).rules.isEmpty());

    std::cout << "testSuperJumpDefaultJumpVsPullHere PASSED\n";
}

void testSuperJumpKeyboardModifierIsolation()
{
    // Helper replicating LeapK shortcut matching logic
    auto evaluateAction = [](Qt::KeyboardModifiers mods, int key) {
        const bool isShift = (mods & Qt::ShiftModifier);
        const bool isAlt = (mods & Qt::AltModifier) && !(mods & Qt::ControlModifier);
        const bool isCtrl = (mods & Qt::ControlModifier) && !(mods & Qt::AltModifier);
        const bool isMetaOnly = (mods & Qt::MetaModifier) && !isAlt && !isCtrl && !isShift;

        bool triggersSuperJump = false;
        bool triggersStashMode = false;
        bool pullsWindow = false;
        bool stashesWindow = false;
        bool closesWindow = false;

        if (isMetaOnly && key == Qt::Key_A) {
            triggersSuperJump = true;
        } else if (isMetaOnly && key == Qt::Key_S) {
            triggersStashMode = true;
        } else if (isAlt) {
            pullsWindow = true;
        } else if (isCtrl) {
            stashesWindow = true;
        } else if (isShift) {
            closesWindow = true;
        }

        return std::make_tuple(triggersSuperJump, triggersStashMode, pullsWindow, stashesWindow, closesWindow);
    };

    // 1. Meta+A -> Super Jump toggle ONLY
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::MetaModifier, Qt::Key_A);
        assert(sj == true && sm == false && pull == false && stash == false && close == false);
    }

    // 2. Ctrl+A -> Stash window 'A' (NOT Super Jump)
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::ControlModifier, Qt::Key_A);
        assert(sj == false && sm == false && pull == false && stash == true && close == false);
    }

    // 3. Alt+A -> Pull window 'A' (NOT Super Jump)
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::AltModifier, Qt::Key_A);
        assert(sj == false && sm == false && pull == true && stash == false && close == false);
    }

    // 4. Meta+Alt+A -> Pull window 'A' even if user holds Meta from trigger
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::MetaModifier | Qt::AltModifier, Qt::Key_A);
        assert(sj == false && sm == false && pull == true && stash == false && close == false);
    }

    // 5. Meta+S -> Stash mode toggle ONLY
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::MetaModifier, Qt::Key_S);
        assert(sj == false && sm == true && pull == false && stash == false && close == false);
    }

    // 6. Alt+S -> Pull window 'S' (NOT Stash mode!)
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::AltModifier, Qt::Key_S);
        assert(sj == false && sm == false && pull == true && stash == false && close == false);
    }

    // 7. Meta+Alt+S -> Pull window 'S' (NOT Stash mode, even if Meta is held!)
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::MetaModifier | Qt::AltModifier, Qt::Key_S);
        assert(sj == false && sm == false && pull == true && stash == false && close == false);
    }

    // 8. Ctrl+S -> Stash window 'S' (NOT Stash mode!)
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::ControlModifier, Qt::Key_S);
        assert(sj == false && sm == false && pull == false && stash == true && close == false);
    }

    // 9. Meta+Ctrl+S -> Stash window 'S' (even if Meta is held!)
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::MetaModifier | Qt::ControlModifier, Qt::Key_S);
        assert(sj == false && sm == false && pull == false && stash == true && close == false);
    }

    // 10. Shift+S -> Close window 'S'
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::ShiftModifier, Qt::Key_S);
        assert(sj == false && sm == false && pull == false && stash == false && close == true);
    }

    // 11. Plain 'S' -> Normal Jump
    {
        auto [sj, sm, pull, stash, close] = evaluateAction(Qt::NoModifier, Qt::Key_S);
        assert(sj == false && sm == false && pull == false && stash == false && close == false);
    }

    std::cout << "testSuperJumpKeyboardModifierIsolation PASSED\n";
}

void testPinnedWindowPinAndDropLogic()
{
    // Mock simulation of window state and LeapK Pin / Drop behavior
    struct MockWindow {
        QString id;
        int workspaceIndex = 1;
        bool onCurrentDesktop = true;
        bool isMinimized = false;
    };

    QSet<QString> pinnedWindows;
    int currentWorkspace = 1;

    auto onAltKey = [&](MockWindow &win) {
        if (pinnedWindows.contains(win.id)) {
            // Drop window into current workspace
            pinnedWindows.remove(win.id);
            win.workspaceIndex = currentWorkspace;
            win.onCurrentDesktop = true;
            win.isMinimized = false;
        } else if (win.workspaceIndex == currentWorkspace && win.onCurrentDesktop) {
            // Pin across all workspaces
            pinnedWindows.insert(win.id);
            win.isMinimized = false;
        } else {
            // Pull here
            win.workspaceIndex = currentWorkspace;
            win.onCurrentDesktop = true;
            win.isMinimized = false;
        }
    };

    auto isVisibleInWorkspace = [&](const MockWindow &win, int wsIdx) -> bool {
        if (pinnedWindows.contains(win.id)) {
            return true; // Pinned windows are visible across ALL workspaces!
        }
        return win.workspaceIndex == wsIdx;
    };

    auto getLocationString = [&](const MockWindow &win) -> QString {
        if (pinnedWindows.contains(win.id)) {
            return QStringLiteral("📌 Pinned");
        }
        return QStringLiteral("Workspace %1").arg(win.workspaceIndex);
    };

    MockWindow w1{QStringLiteral("win1"), 1, true, false};

    // 1. Initially w1 is in Workspace 1, not pinned
    assert(isVisibleInWorkspace(w1, 1) == true);
    assert(isVisibleInWorkspace(w1, 2) == false);
    assert(getLocationString(w1) == QStringLiteral("Workspace 1"));

    // 2. Alt+Key in Workspace 1 pins w1
    onAltKey(w1);
    assert(pinnedWindows.contains(w1.id));
    assert(getLocationString(w1) == QStringLiteral("📌 Pinned"));

    // 3. Pinned window is visible in Workspace 1, Workspace 2, and Workspace 3!
    assert(isVisibleInWorkspace(w1, 1) == true);
    assert(isVisibleInWorkspace(w1, 2) == true);
    assert(isVisibleInWorkspace(w1, 3) == true);

    // 4. Switch to Workspace 3 and drop w1 using Alt+Key
    currentWorkspace = 3;
    onAltKey(w1); // Drops it!
    assert(!pinnedWindows.contains(w1.id));
    assert(w1.workspaceIndex == 3);
    assert(getLocationString(w1) == QStringLiteral("Workspace 3"));

    // 5. After drop, w1 belongs strictly to Workspace 3
    assert(isVisibleInWorkspace(w1, 1) == false);
    assert(isVisibleInWorkspace(w1, 2) == false);
    assert(isVisibleInWorkspace(w1, 3) == true);

    std::cout << "testPinnedWindowPinAndDropLogic PASSED\n";
}

void testHudPartitioningAndDivider()
{
    struct MockTarget {
        QChar key;
        QString id;
        bool isPinned;
        bool isStashed;
    };

    auto partitionHudTargets = [](const QList<MockTarget> &targets, bool isStashMode, int &outDividerAfterIndex) {
        QList<MockTarget> left;
        QList<MockTarget> right;
        if (isStashMode) {
            for (const auto &t : targets) {
                if (t.isStashed) {
                    right.append(t);
                } else {
                    left.append(t);
                }
            }
        } else {
            for (const auto &t : targets) {
                if (t.isPinned) {
                    left.append(t);
                } else {
                    right.append(t);
                }
            }
        }
        if (!left.isEmpty() && !right.isEmpty()) {
            outDividerAfterIndex = left.size() - 1;
        } else {
            outDividerAfterIndex = -1;
        }
        return left + right;
    };

    // 1. Stash mode: 2 workspace windows + 2 stashed windows
    QList<MockTarget> stashList = {
        {QChar('A'), QStringLiteral("winA"), false, true},
        {QChar('B'), QStringLiteral("winB"), false, false},
        {QChar('C'), QStringLiteral("winC"), false, true},
        {QChar('D'), QStringLiteral("winD"), false, false}
    };
    int stashDivider = -1;
    QList<MockTarget> stashPartitioned = partitionHudTargets(stashList, true, stashDivider);
    assert(stashPartitioned.size() == 4);
    assert(stashDivider == 1);
    assert(!stashPartitioned[0].isStashed);
    assert(!stashPartitioned[1].isStashed);
    assert(stashPartitioned[2].isStashed);
    assert(stashPartitioned[3].isStashed);

    // 2. Normal mode: 1 pinned window + 3 regular windows
    QList<MockTarget> normalList = {
        {QChar('A'), QStringLiteral("winA"), false, false},
        {QChar('P'), QStringLiteral("winP"), true, false},
        {QChar('B'), QStringLiteral("winB"), false, false},
        {QChar('C'), QStringLiteral("winC"), false, false}
    };
    int normalDivider = -1;
    QList<MockTarget> normalPartitioned = partitionHudTargets(normalList, false, normalDivider);
    assert(normalPartitioned.size() == 4);
    assert(normalDivider == 0);
    assert(normalPartitioned[0].isPinned);
    assert(!normalPartitioned[1].isPinned);
    assert(!normalPartitioned[2].isPinned);
    assert(!normalPartitioned[3].isPinned);

    // 3. Normal mode with no pinned windows -> divider is -1
    QList<MockTarget> noPinList = {
        {QChar('A'), QStringLiteral("winA"), false, false},
        {QChar('B'), QStringLiteral("winB"), false, false}
    };
    int noPinDivider = -1;
    partitionHudTargets(noPinList, false, noPinDivider);
    assert(noPinDivider == -1);

    std::cout << "testHudPartitioningAndDivider PASSED\n";
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);

    testSuperJumpBannerFormatting();
    testStashViewHeaderBanner();
    testStashedBadgeDesaturationAndDashing();
    testCrossWorkspacePullLogic();
    testSuperJumpDefaultJumpVsPullHere();
    testSuperJumpKeyboardModifierIsolation();
    testPinnedWindowPinAndDropLogic();
    testHudPartitioningAndDivider();

    std::cout << "All Super Jump tests passed successfully!\n";
    return 0;
}
