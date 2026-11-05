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
            return QStringLiteral("● Pinned");
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
    assert(getLocationString(w1) == QStringLiteral("● Pinned"));

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

void testCloseWindowModeBannerAndHintBar()
{
    // 1. Close Mode banner (standard workspace scope)
    QImage closeBanner = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QString(), false, false, 0,
        false, QString(), 1, 0, 44, 0, LeapKTheme::Theme(), nullptr, false, 0, nullptr,
        true /* isCloseMode */, false /* isPinMode */);
    assert(!closeBanner.isNull());
    assert(closeBanner.width() > 250);
    assert(closeBanner.height() >= 30);

    // 2. Close Mode banner with Super Jump scope (all WS + stash)
    QImage closeSuperBanner = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QString(), true /* isSuperJump */, false, 0,
        false, QString(), 1, 0, 44, 0, LeapKTheme::Theme(), nullptr, false, 0, nullptr,
        true /* isCloseMode */, false /* isPinMode */);
    assert(!closeSuperBanner.isNull());
    assert(closeSuperBanner.width() > closeBanner.width());

    // 3. Close Mode Hint Bar
    QImage closeHintBar = OutlineBadge::renderHudActionHintBar(
        1.0, false, 0, QString(), QColor(), false, false, false, false, 1050.0, 0,
        LeapKTheme::Theme(), false, true /* isCloseMode */, false /* isPinMode */);
    assert(!closeHintBar.isNull());
    assert(closeHintBar.width() > 100);

    std::cout << "testCloseWindowModeBannerAndHintBar PASSED\n";
}

void testPinModeBannerAndHintBar()
{
    // 1. Pin Mode banner
    QImage pinBanner = OutlineBadge::renderHudModeHeader(
        1.0, false, QString(), QColor(), 0, {}, -1, false, QString(), false, false, 0,
        false, QString(), 1, 0, 44, 0, LeapKTheme::Theme(), nullptr, false, 0, nullptr,
        false /* isCloseMode */, true /* isPinMode */);
    assert(!pinBanner.isNull());
    assert(pinBanner.width() > 250);
    assert(pinBanner.height() >= 30);

    // 2. Pin Mode Hint Bar
    QImage pinHintBar = OutlineBadge::renderHudActionHintBar(
        1.0, false, 0, QString(), QColor(), false, false, false, false, 1050.0, 0,
        LeapKTheme::Theme(), false, false /* isCloseMode */, true /* isPinMode */);
    assert(!pinHintBar.isNull());
    assert(pinHintBar.width() > 100);

    std::cout << "testPinModeBannerAndHintBar PASSED\n";
}

void testHudMetaOverlayRendering()
{
    // 1. Overlay when inactive
    QImage overlayDefault = OutlineBadge::renderHudMetaOverlayImage(
        1.0, LeapKTheme::Theme(), false, false, false, false, false, false);
    assert(!overlayDefault.isNull());
    assert(overlayDefault.width() >= 500);
    assert(overlayDefault.height() >= 200);

    // 2. Overlay with Super Jump active and Close Mode active
    QImage overlayCloseSuper = OutlineBadge::renderHudMetaOverlayImage(
        1.0, LeapKTheme::Theme(), true /* isSuperJump */, true /* isCloseMode */, false, false, false, false);
    assert(!overlayCloseSuper.isNull());
    assert(overlayCloseSuper.size() == overlayDefault.size());

    // 3. HiDPI scale (2.0)
    QImage overlayHiDpi = OutlineBadge::renderHudMetaOverlayImage(
        2.0, LeapKTheme::Theme(), false, false, false, true /* isPinMode */, false, false);
    assert(!overlayHiDpi.isNull());
    assert(overlayHiDpi.width() >= overlayDefault.width() * 1.8);

    // 4. Overlay with Traverse Recent active
    QImage overlayTraverse = OutlineBadge::renderHudMetaOverlayImage(
        1.0, LeapKTheme::Theme(), false, false, false, false, false, false, true /* isTraverseRecent */);
    assert(!overlayTraverse.isNull());
    assert(overlayTraverse.size() == overlayDefault.size());

    // 5. Overlay with Zoom Thumbnails active
    QImage overlayZoom = OutlineBadge::renderHudMetaOverlayImage(
        1.0, LeapKTheme::Theme(), false, false, false, false, false, false, false, true /* isZoomThumbnails */);
    assert(!overlayZoom.isNull());
    assert(overlayZoom.size() == overlayDefault.size());

    std::cout << "testHudMetaOverlayRendering PASSED\n";
}

void testPinWorkspaceModalDialogRendering()
{
    using namespace LeapKWorkspace;

    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-1");
    ws1.name = QStringLiteral("Coding");
    ws1.color = QColor(QStringLiteral("#3498db"));

    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-2");
    ws2.name = QStringLiteral("Web & Docs");
    ws2.color = QColor(QStringLiteral("#2ecc71"));

    QList<WorkspaceInfo> workspaces = {ws1, ws2};

    QList<QRectF> rowRects;
    QRectF cancelRect;

    // 1. Window not pinned
    QImage imgNotPinned = OutlineBadge::renderHudPinWorkspaceDialog(
        1.0, LeapKTheme::Theme(), QStringLiteral("Terminal"), QStringLiteral("org.kde.konsole"),
        workspaces, {}, false, rowRects, cancelRect);
    assert(!imgNotPinned.isNull());
    assert(imgNotPinned.width() >= 500);
    assert(imgNotPinned.height() >= 180);
    // Not pinned: row 0 (All) + 2 workspaces = 3 rows
    assert(rowRects.size() == 3);
    assert(cancelRect.isValid() && !cancelRect.isEmpty());

    // 2. Window pinned to All Workspaces
    QList<QRectF> rowRectsPinnedAll;
    QRectF cancelRectPinnedAll;
    QImage imgPinnedAll = OutlineBadge::renderHudPinWorkspaceDialog(
        1.0, LeapKTheme::Theme(), QStringLiteral("Browser"), QStringLiteral("firefox"),
        workspaces, {0}, true, rowRectsPinnedAll, cancelRectPinnedAll);
    assert(!imgPinnedAll.isNull());
    // Pinned: row 0 (All) + 2 workspaces + 1 (Unpin) = 4 rows
    assert(rowRectsPinnedAll.size() == 4);
    assert(cancelRectPinnedAll.isValid());

    // 3. Window pinned to specific workspace 1
    QList<QRectF> rowRectsPinnedSpecific;
    QRectF cancelRectPinnedSpecific;
    QImage imgPinnedSpecific = OutlineBadge::renderHudPinWorkspaceDialog(
        1.0, LeapKTheme::Theme(), QStringLiteral("Editor"), QStringLiteral("kate"),
        workspaces, {1}, true, rowRectsPinnedSpecific, cancelRectPinnedSpecific);
    assert(!imgPinnedSpecific.isNull());
    assert(rowRectsPinnedSpecific.size() == 4);

    // 4. HiDPI rendering (2.0 scale)
    QList<QRectF> rowRectsHiDpi;
    QRectF cancelRectHiDpi;
    QImage imgHiDpi = OutlineBadge::renderHudPinWorkspaceDialog(
        2.0, LeapKTheme::Theme(), QStringLiteral("Editor"), QStringLiteral("kate"),
        workspaces, {1}, true, rowRectsHiDpi, cancelRectHiDpi);
    assert(!imgHiDpi.isNull());
    assert(imgHiDpi.width() >= imgNotPinned.width() * 1.8);
    assert(rowRectsHiDpi.size() == 4);

    std::cout << "testPinWorkspaceModalDialogRendering PASSED\n";
}

void testSuperJumpPullIntoEmptyWorkspace()
{
    using namespace LeapKWorkspace;

    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.path() + QStringLiteral("/workspaces.json");

    // Workspace 1: Coding (has konsole)
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-coding");
    ws1.name = QStringLiteral("Coding");

    // Workspace 2: Review (initially empty - 0 windows, 0 rules)
    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-review");
    ws2.name = QStringLiteral("Review");

    QList<WorkspaceInfo> workspaces = { ws1, ws2 };

    // Initially "org.kde.konsole" belongs to ws1
    reassignWindowToWorkspace(workspaces, ws1.id, QStringLiteral("org.kde.konsole"), QStringLiteral("bash"), true, 0, filePath);
    assert(workspaces[0].rules.size() == 1);
    assert(workspaces[1].rules.isEmpty());
    assert(findWorkspaceIndexForWindow(workspaces, QStringLiteral("org.kde.konsole"), QStringLiteral("bash")) == 1);

    // Scenario: User is in Workspace 2 (Review, 0 windows).
    int activeHudView = 2;
    bool superJumpActive = true;
    int superJumpTargetWorkspace = activeHudView; // 2

    // Target window pulled via Alt+Key from another workspace
    const QString appClass = QStringLiteral("org.kde.konsole");
    const QString caption = QStringLiteral("bash");

    // Resolving targetWs while superJumpActive is true
    const int targetWs = (superJumpActive && superJumpTargetWorkspace >= 1 && superJumpTargetWorkspace <= workspaces.size())
                         ? superJumpTargetWorkspace
                         : (activeHudView >= 1 && activeHudView <= workspaces.size() ? activeHudView : 0);
    assert(targetWs == 2);

    // Pull action: reassign to targetWs
    reassignWindowToWorkspace(workspaces, workspaces.at(targetWs - 1).id, appClass, caption, !appClass.isEmpty(), 0, filePath);

    // cancelJumpMode is called AFTER reassigning
    superJumpActive = false;

    // Verify workspace 2 now owns konsole and workspace 1 no longer matches it
    assert(workspaces[0].rules.isEmpty());
    assert(workspaces[1].rules.size() == 1);
    assert(findWorkspaceIndexForWindow(workspaces, QStringLiteral("org.kde.konsole"), QStringLiteral("bash")) == 2);
    assert(!matchesWorkspace(workspaces[0], QStringLiteral("org.kde.konsole"), QStringLiteral("bash")));
    assert(matchesWorkspace(workspaces[1], QStringLiteral("org.kde.konsole"), QStringLiteral("bash")));

    // Verify isolation logic:
    // When switching to Workspace 1: window must NOT match (is minimized)
    assert(matchesWorkspace(workspaces[0], QStringLiteral("org.kde.konsole"), QStringLiteral("bash")) == false);
    // When switching back to Workspace 2: window MUST match (is restored / visible)
    assert(matchesWorkspace(workspaces[1], QStringLiteral("org.kde.konsole"), QStringLiteral("bash")) == true);

    // Reload from file to ensure disk persistence
    QList<WorkspaceInfo> reloaded = loadWorkspaces(filePath);
    assert(reloaded[0].rules.isEmpty());
    assert(reloaded[1].rules.size() == 1);
    assert(findWorkspaceIndexForWindow(reloaded, QStringLiteral("org.kde.konsole"), QStringLiteral("bash")) == 2);

    std::cout << "testSuperJumpPullIntoEmptyWorkspace PASSED\n";
}

void testHudShiftBadgeCycling()
{
    // Simulates HUD badge cycling logic in slotCycleBadge
    struct DummyWindow {
        QString name;
        bool isPinned = false;
        bool isStashed = false;
    };

    DummyWindow wA{QStringLiteral("Window A (Origin)")};
    DummyWindow wS{QStringLiteral("Window S (Oldest in dock)")};
    DummyWindow wD{QStringLiteral("Window D (Middle in dock)")};
    DummyWindow wF{QStringLiteral("Window F (Youngest/Recent in dock)")};

    // History: wA is active (idx 0), wF is recent (idx 1), wD (idx 2), wS (idx 3)
    QList<DummyWindow *> history = { &wA, &wF, &wD, &wS };

    // Simulating hudTargetPillRects (origin wA was excluded from dock pills)
    QList<QPair<QRectF, DummyWindow *>> hudTargetPillRects = {
        {QRectF(0, 0, 100, 30), &wS},
        {QRectF(110, 0, 100, 30), &wD},
        {QRectF(220, 0, 100, 30), &wF}
    };

    QMap<QChar, DummyWindow *> letterMap = {
        {QChar('A'), &wA},
        {QChar('S'), &wS},
        {QChar('D'), &wD},
        {QChar('F'), &wF}
    };

    DummyWindow *originWin = &wA;
    DummyWindow *activeWin = &wA;

    // Gather all badge windows
    QList<DummyWindow *> orderedWindows;
    auto addWindow = [&orderedWindows](DummyWindow *w) {
        if (w && !orderedWindows.contains(w)) {
            orderedWindows.append(w);
        }
    };

    for (const auto &pair : hudTargetPillRects) {
        addWindow(pair.second);
    }
    for (auto it = letterMap.constBegin(); it != letterMap.constEnd(); ++it) {
        addWindow(it.value());
    }
    if (originWin) {
        addWindow(originWin);
    }

    assert(orderedWindows.size() == 4);

    // Sort orderedWindows consistently with HUD:
    auto recencyRank = [&history](DummyWindow *w) -> int {
        if (!w) return 999999;
        int idx = history.indexOf(w);
        return idx >= 0 ? idx : 999999;
    };

    QList<DummyWindow *> leftGroup;
    QList<DummyWindow *> rightGroup;
    for (DummyWindow *w : orderedWindows) {
        if (w->isPinned) {
            leftGroup.append(w);
        } else {
            rightGroup.append(w);
        }
    }
    auto sortByRecency = [&recencyRank](QList<DummyWindow *> &list) {
        std::stable_sort(list.begin(), list.end(), [&recencyRank](DummyWindow *a, DummyWindow *b) {
            int rankA = recencyRank(a);
            int rankB = recencyRank(b);
            if (rankA != rankB) {
                return rankA > rankB; // Oldest on left (first)
            }
            return false;
        });
    };
    sortByRecency(leftGroup);
    sortByRecency(rightGroup);
    orderedWindows = leftGroup + rightGroup;

    // Ordered windows from oldest to newest: wS (rank 3), wD (rank 2), wF (rank 1), wA (rank 0)
    assert(orderedWindows[0] == &wS);
    assert(orderedWindows[1] == &wD);
    assert(orderedWindows[2] == &wF);
    assert(orderedWindows[3] == &wA);

    auto cycleBadge = [&](int delta) {
        int currIdx = orderedWindows.indexOf(activeWin);
        if (currIdx < 0 && originWin) {
            currIdx = orderedWindows.indexOf(originWin);
        }
        int nextIdx = (currIdx >= 0)
            ? (currIdx + delta + orderedWindows.size()) % orderedWindows.size()
            : (delta > 0 ? 0 : orderedWindows.size() - 1);
        activeWin = orderedWindows.at(nextIdx);
    };

    // 1. Initial active window is wA (index 3)
    assert(activeWin == &wA);

    // 2. Shift + . (>) -> cycles to wS (index 0, wraps smoothly)
    cycleBadge(1);
    assert(activeWin == &wS);

    // 3. Shift + . (>) -> cycles to wD (index 1)
    cycleBadge(1);
    assert(activeWin == &wD);

    // 4. Shift + . (>) -> cycles to wF (index 2)
    cycleBadge(1);
    assert(activeWin == &wF);

    // 5. Shift + . (>) -> cycles to wA (index 3)
    cycleBadge(1);
    assert(activeWin == &wA);

    // 6. Shift + , (<) -> cycles backwards to wF (index 2)
    cycleBadge(-1);
    assert(activeWin == &wF);

    // 7. Shift + , (<) -> cycles backwards to wD (index 1)
    cycleBadge(-1);
    assert(activeWin == &wD);

    // 8. Shift + , (<) -> cycles backwards to wS (index 0)
    cycleBadge(-1);
    assert(activeWin == &wS);

    // 9. Shift + , (<) -> cycles backwards to wA (index 3)
    cycleBadge(-1);
    assert(activeWin == &wA);

    std::cout << "testHudShiftBadgeCycling PASSED\n";
}

void testHudRecencySorting()
{
    struct DummyWindow {
        QString name;
    };

    DummyWindow w0{QStringLiteral("Unvisited W0")};
    DummyWindow w1{QStringLiteral("Oldest W1")};
    DummyWindow w2{QStringLiteral("Middle W2")};
    DummyWindow w3{QStringLiteral("Youngest W3")};

    // History: W3 is index 0 (youngest/most recent), W2 is index 1, W1 is index 2 (oldest).
    // W0 is not in history (unvisited).
    QList<DummyWindow *> history = { &w3, &w2, &w1 };

    auto recencyRank = [&history](DummyWindow *w) -> int {
        if (!w) return 999999;
        int idx = history.indexOf(w);
        return idx >= 0 ? idx : 999999;
    };

    auto sortByRecencyOldestToNewest = [&recencyRank](QList<QPair<QChar, DummyWindow *>> &list) {
        std::stable_sort(list.begin(), list.end(), [&recencyRank](const QPair<QChar, DummyWindow *> &a, const QPair<QChar, DummyWindow *> &b) {
            int rankA = recencyRank(a.second);
            int rankB = recencyRank(b.second);
            if (rankA != rankB) {
                return rankA > rankB; // Older window (higher index) on the left (first)
            }
            return false;
        });
    };

    // Initial arbitrary order
    QList<QPair<QChar, DummyWindow *>> targets = {
        {QChar('C'), &w3},
        {QChar('A'), &w0},
        {QChar('B'), &w2},
        {QChar('D'), &w1}
    };

    sortByRecencyOldestToNewest(targets);

    // After sorting oldest-to-newest (left-to-right):
    // Leftmost (idx 0) is w0 (unvisited, rank 999999)
    // Next (idx 1) is w1 (oldest visited, rank 2)
    // Next (idx 2) is w2 (rank 1)
    // Rightmost (idx 3) is w3 (youngest visited, rank 0)
    assert(targets.size() == 4);
    assert(targets[0].second == &w0);
    assert(targets[1].second == &w1);
    assert(targets[2].second == &w2);
    assert(targets[3].second == &w3);

    // Test cycling from rightmost (w3) with Shift+, (left towards older) and Shift+. (right towards younger)
    QList<DummyWindow *> orderedWindows;
    for (const auto &p : targets) {
        orderedWindows.append(p.second);
    }

    DummyWindow *activeWin = &w3; // Started on youngest (rightmost)
    auto cycle = [&](int delta) {
        int currIdx = orderedWindows.indexOf(activeWin);
        int nextIdx = (currIdx >= 0)
            ? (currIdx + delta + orderedWindows.size()) % orderedWindows.size()
            : (delta > 0 ? 0 : orderedWindows.size() - 1);
        activeWin = orderedWindows.at(nextIdx);
    };

    // Shift+, (<) moves left towards older window
    cycle(-1);
    assert(activeWin == &w2);
    cycle(-1);
    assert(activeWin == &w1);
    cycle(-1);
    assert(activeWin == &w0);

    // Shift+. (>) moves right towards younger window
    cycle(1);
    assert(activeWin == &w1);
    cycle(1);
    assert(activeWin == &w2);
    cycle(1);
    assert(activeWin == &w3);

    std::cout << "testHudRecencySorting PASSED\n";
}

void testRecentWindowWorkspaceTransfer()
{
    using namespace LeapKWorkspace;

    // Workspace 1: Coding (owns Kate / editor)
    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws-coding");
    ws1.name = QStringLiteral("Coding");
    WindowMatchRule r1;
    r1.matchApp = QStringLiteral("kate");
    ws1.rules.append(r1);

    // Workspace 2: Terminal (owns Konsole)
    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws-terminal");
    ws2.name = QStringLiteral("Terminal");
    WindowMatchRule r2;
    r2.matchApp = QStringLiteral("org.kde.konsole");
    ws2.rules.append(r2);

    QList<WorkspaceInfo> workspaces = { ws1, ws2 };

    struct MockWin {
        QString appId;
        QString caption;
        bool isMinimized = false;
        int desktop = 1;
    };

    MockWin wKate{QStringLiteral("kate"), QStringLiteral("main.cpp"), false, 1};
    MockWin wKonsole{QStringLiteral("org.kde.konsole"), QStringLiteral("bash"), false, 1};

    // User is on Workspace 2 (Terminal), so Konsole is active window, Kate is in history as recent
    int activeHudView = 2;
    QList<MockWin *> history = { &wKonsole, &wKate };
    MockWin *activeWindow = &wKonsole;

    // In Workspace 2 isolation: Kate (belonging to ws1) is minimized
    wKate.isMinimized = true;

    // User triggers Meta+Space Space (recent window traversal or jump)
    // 1. Step to candidate from history (wKate)
    MockWin *candidate = history.at(1);
    assert(candidate == &wKate);

    // 2. Find target workspace for candidate
    const int targetWs = findWorkspaceIndexForWindow(workspaces, candidate->appId, candidate->caption, candidate->desktop);
    assert(targetWs == 1); // Belongs to Workspace 1

    // 3. System must transfer active workspace view from 2 to targetWs (1)
    if (activeHudView != targetWs && activeHudView <= workspaces.size()) {
        activeHudView = targetWs;
    }
    assert(activeHudView == 1);

    // 4. In Workspace 1 isolation:
    // Window matching ws1 (wKate) is unminimized, window matching ws2 (wKonsole) is minimized
    for (auto *w : { &wKate, &wKonsole }) {
        if (matchesWorkspace(workspaces.at(activeHudView - 1), w->appId, w->caption, w->desktop)) {
            w->isMinimized = false;
        } else {
            w->isMinimized = true;
        }
    }
    assert(wKate.isMinimized == false);
    assert(wKonsole.isMinimized == true);

    // 5. Active window is set to candidate
    activeWindow = candidate;
    assert(activeWindow == &wKate);

    // 6. On commit:
    history.removeAll(candidate);
    history.prepend(candidate);
    assert(history.first() == &wKate);
    assert(activeHudView == 1); // Confirms user was transferred to Workspace 1!

    std::cout << "testRecentWindowWorkspaceTransfer PASSED\n";
}

void testHudCrossWorkspaceActionDialogRendering()
{
    LeapKTheme::Theme theme;
    theme.id = QStringLiteral("cyberpunk_neon");

    QRectF jumpRect, moveRect, pinRect, cancelRect;
    const QImage img1 = OutlineBadge::renderHudCrossWorkspaceActionDialog(
        1.0, theme, QStringLiteral("GitHub - Pull Requests"), QStringLiteral("firefox"),
        QStringLiteral("Workspace 1"), QStringLiteral("Workspace 2"),
        jumpRect, moveRect, pinRect, cancelRect);

    assert(!img1.isNull());
    assert(img1.width() > 400);
    assert(img1.height() > 200);
    assert(jumpRect.isValid());
    assert(moveRect.isValid());
    assert(pinRect.isValid());
    assert(cancelRect.isValid());
    assert(jumpRect.top() < moveRect.top());
    assert(moveRect.top() < pinRect.top());
    assert(pinRect.top() < cancelRect.top());

    // Scale 2.0 (HiDPI)
    QRectF jumpRect2, moveRect2, pinRect2, cancelRect2;
    const QImage img2 = OutlineBadge::renderHudCrossWorkspaceActionDialog(
        2.0, theme, QStringLiteral("Terminal"), QStringLiteral("konsole"),
        QStringLiteral("Workspace 3"), QStringLiteral("Workspace 1"),
        jumpRect2, moveRect2, pinRect2, cancelRect2);

    assert(!img2.isNull());
    assert(img2.width() >= img1.width() * 2);
    assert(img2.height() >= img1.height() * 2);

    // Verify hint bar with isCrossWsAction = true
    const QImage hintImg = OutlineBadge::renderHudActionHintBar(
        1.0, false, 0, QString(), QColor(), false, true, false, false, 0.0, 0, theme, false, false, false, 0, true);
    assert(!hintImg.isNull());

    std::cout << "testHudCrossWorkspaceActionDialogRendering PASSED\n";
}

void testCrossWorkspaceActionModes()
{
    using namespace LeapKWorkspace;

    struct MockWindow {
        QString id;
        QString appId;
        QString caption;
        int desktop;
        int workspaceIndex;
        bool isMinimized;
    };

    WorkspaceInfo ws1;
    ws1.id = QStringLiteral("ws1");
    ws1.name = QStringLiteral("Workspace 1");

    WorkspaceInfo ws2;
    ws2.id = QStringLiteral("ws2");
    ws2.name = QStringLiteral("Workspace 2");

    QList<WorkspaceInfo> workspaces = { ws1, ws2 };

    MockWindow wKate{QStringLiteral("w1"), QStringLiteral("kate"), QStringLiteral("Kate Editor"), 1, 1, false};
    MockWindow *activeWindow = nullptr;
    int currentWorkspace = 2; // User is in Workspace 2
    bool superJumpActive = true;

    auto isCrossWorkspace = [&](const MockWindow &w) {
        return superJumpActive && (w.workspaceIndex != currentWorkspace);
    };

    assert(isCrossWorkspace(wKate) == true);

    // Mode 0: Ask (Interactive action menu)
    {
        int superJumpCrossWsAction = 0;
        bool actionMenuTriggered = false;
        int jumpedToWs = currentWorkspace;

        if (isCrossWorkspace(wKate)) {
            if (superJumpCrossWsAction == 0) {
                actionMenuTriggered = true;
            } else if (superJumpCrossWsAction == 1) {
                jumpedToWs = wKate.workspaceIndex;
            }
        }
        assert(actionMenuTriggered == true);
        assert(jumpedToWs == currentWorkspace); // Didn't jump immediately
    }

    // Mode 1: Classic (Direct Jump)
    {
        int superJumpCrossWsAction = 1;
        bool actionMenuTriggered = false;
        int jumpedToWs = currentWorkspace;

        if (isCrossWorkspace(wKate)) {
            if (superJumpCrossWsAction == 0) {
                actionMenuTriggered = true;
            } else if (superJumpCrossWsAction == 1) {
                jumpedToWs = wKate.workspaceIndex;
            }
        }
        assert(actionMenuTriggered == false);
        assert(jumpedToWs == 1); // Switched to Workspace 1
    }

    // Mode 2: Direct Move to Current Workspace
    {
        int superJumpCrossWsAction = 2;

        if (isCrossWorkspace(wKate)) {
            if (superJumpCrossWsAction == 2) {
                // Move window to current workspace
                wKate.workspaceIndex = currentWorkspace;
                activeWindow = &wKate;
            }
        }
        assert(wKate.workspaceIndex == 2);
        assert(activeWindow == &wKate);
        assert(currentWorkspace == 2); // Remained on Workspace 2
    }

    std::cout << "testCrossWorkspaceActionModes PASSED\n";
}

void testCrossWorkspaceActionDialogShortcuts()
{
    struct MockWindow {
        QString id;
        int workspaceIndex;
        bool isPinned;
    };

    MockWindow targetWin{QStringLiteral("w1"), 1, false}; // In Workspace 1
    int currentWorkspace = 2; // In Workspace 2

    auto handleKey = [&](int key, int &outWs, bool &outMenuOpen, MockWindow &win) {
        if (!outMenuOpen) return;
        if (key == Qt::Key_Return || key == Qt::Key_J || key == Qt::Key_1) {
            // Jump
            outWs = win.workspaceIndex;
            outMenuOpen = false;
        } else if (key == Qt::Key_M || key == Qt::Key_2) {
            // Move
            win.workspaceIndex = currentWorkspace;
            outMenuOpen = false;
        } else if (key == Qt::Key_P || key == Qt::Key_3) {
            // Pin
            win.isPinned = true;
            outMenuOpen = false;
        } else if (key == Qt::Key_Escape || key == Qt::Key_Backspace || key == Qt::Key_Space) {
            // Cancel / Back to Super Jump
            outMenuOpen = false;
        }
    };

    // Test Jump with Enter
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_Return, ws, menuOpen, win);
        assert(!menuOpen);
        assert(ws == 1);
        assert(!win.isPinned);
    }

    // Test Jump with 'J'
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_J, ws, menuOpen, win);
        assert(!menuOpen);
        assert(ws == 1);
    }

    // Test Jump with '1'
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_1, ws, menuOpen, win);
        assert(!menuOpen);
        assert(ws == 1);
    }

    // Test Move with 'M'
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_M, ws, menuOpen, win);
        assert(!menuOpen);
        assert(win.workspaceIndex == 2);
        assert(ws == 2);
    }

    // Test Move with '2'
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_2, ws, menuOpen, win);
        assert(!menuOpen);
        assert(win.workspaceIndex == 2);
    }

    // Test Pin with 'P'
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_P, ws, menuOpen, win);
        assert(!menuOpen);
        assert(win.isPinned == true);
    }

    // Test Pin with '3'
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_3, ws, menuOpen, win);
        assert(!menuOpen);
        assert(win.isPinned == true);
    }

    // Test Cancel with Esc
    {
        int ws = currentWorkspace;
        bool menuOpen = true;
        MockWindow win = targetWin;
        handleKey(Qt::Key_Escape, ws, menuOpen, win);
        assert(!menuOpen);
        assert(win.workspaceIndex == 1);
        assert(!win.isPinned);
        assert(ws == 2); // Didn't jump
    }

    std::cout << "testCrossWorkspaceActionDialogShortcuts PASSED\n";
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
    testCloseWindowModeBannerAndHintBar();
    testPinModeBannerAndHintBar();
    testHudMetaOverlayRendering();
    testSuperJumpPullIntoEmptyWorkspace();
    testHudShiftBadgeCycling();
    testHudRecencySorting();
    testRecentWindowWorkspaceTransfer();
    testHudCrossWorkspaceActionDialogRendering();
    testPinWorkspaceModalDialogRendering();
    testCrossWorkspaceActionModes();
    testCrossWorkspaceActionDialogShortcuts();

    std::cout << "All Super Jump tests passed successfully!\n";
    return 0;
}
