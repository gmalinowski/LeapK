/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "historyhelper.h"
#include <cassert>
#include <iostream>

struct MockWindow {
    int id;
    bool deleted = false;
    bool desktop = false;
    bool dock = false;
};

void testRecentWindowsOrderAndLimit()
{
    MockWindow w1{1};
    MockWindow w2{2};
    MockWindow w3{3};
    MockWindow w4{4};

    auto isValid = [](MockWindow *w) {
        return w && !w->deleted && !w->desktop && !w->dock;
    };

    QList<MockWindow *> history;
    MockWindow *activeWindow = nullptr;

    // Step 1: Activate Window 1
    OutlineHistory::recordWindowActivation(history, activeWindow, &w1);
    activeWindow = &w1;
    {
        auto recents = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
        assert(recents.isEmpty()); // Only 1 window exists, which is active, so no recents
    }

    // Step 2: Activate Window 2 (previous is Window 1)
    OutlineHistory::recordWindowActivation(history, activeWindow, &w2);
    activeWindow = &w2;
    {
        // Limit 1: must be Window 1 (the immediate previous active window)
        auto recents = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
        assert(recents.size() == 1);
        assert(recents.at(0) == &w1);
    }

    // Step 3: Activate Window 3 (previous is Window 2)
    OutlineHistory::recordWindowActivation(history, activeWindow, &w3);
    activeWindow = &w3;
    {
        // Limit 1: must be Window 2 (the immediate previous active window, NOT w1!)
        auto recents1 = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
        assert(recents1.size() == 1);
        assert(recents1.at(0) == &w2); // Regression test: must be w2, not w1!

        // Limit 2: must be [w2, w1] in that exact order
        auto recents2 = OutlineHistory::computeRecentWindows(history, activeWindow, 2, true, isValid);
        assert(recents2.size() == 2);
        assert(recents2.at(0) == &w2);
        assert(recents2.at(1) == &w1);
    }

    // Step 4: Activate Window 4 (previous is Window 3)
    OutlineHistory::recordWindowActivation(history, activeWindow, &w4);
    activeWindow = &w4;
    {
        // Limit 1: must be Window 3
        auto recents1 = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
        assert(recents1.size() == 1);
        assert(recents1.at(0) == &w3);

        // Limit 2: must be [w3, w2]
        auto recents2 = OutlineHistory::computeRecentWindows(history, activeWindow, 2, true, isValid);
        assert(recents2.size() == 2);
        assert(recents2.at(0) == &w3);
        assert(recents2.at(1) == &w2);

        // Limit 3: must be [w3, w2, w1]
        auto recents3 = OutlineHistory::computeRecentWindows(history, activeWindow, 3, true, isValid);
        assert(recents3.size() == 3);
        assert(recents3.at(0) == &w3);
        assert(recents3.at(1) == &w2);
        assert(recents3.at(2) == &w1);
    }

    // Step 5: Switch back to Window 2 (from Window 4)
    OutlineHistory::recordWindowActivation(history, activeWindow, &w2);
    activeWindow = &w2;
    {
        // Limit 1: must be Window 4 (the immediate previous active window)
        auto recents1 = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
        assert(recents1.size() == 1);
        assert(recents1.at(0) == &w4);

        // Limit 2: must be [w4, w3]
        auto recents2 = OutlineHistory::computeRecentWindows(history, activeWindow, 2, true, isValid);
        assert(recents2.size() == 2);
        assert(recents2.at(0) == &w4);
        assert(recents2.at(1) == &w3);
    }

    // Step 6: Window 4 is deleted / closed
    history.removeAll(&w4);
    {
        // Limit 1: now must fall back to Window 3
        auto recents1 = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
        assert(recents1.size() == 1);
        assert(recents1.at(0) == &w3);
    }

    // Step 7: Disabled highlightRecent
    {
        auto recents = OutlineHistory::computeRecentWindows(history, activeWindow, 2, false, isValid);
        assert(recents.isEmpty());
    }
}

void testCrossDesktopJumpSuppressesIntermediateActivation()
{
    // Scenario reported by user:
    // Desktop 1: Window D1_left (marked "S"), Window D1_right
    // Desktop 2: Window D2_active
    MockWindow d1Left{1};
    MockWindow d1Right{2};
    MockWindow d2Active{3};

    auto isValid = [](MockWindow *w) {
        return w && !w->deleted && !w->desktop && !w->dock;
    };

    QList<MockWindow *> history;
    MockWindow *activeWindow = nullptr;

    // 1. Initial desktop 1 activity: d1Left was focused, then d1Right was focused, then switch to desktop 2 d2Active
    OutlineHistory::recordWindowActivation(history, activeWindow, &d1Left);
    activeWindow = &d1Left;
    OutlineHistory::recordWindowActivation(history, activeWindow, &d1Right);
    activeWindow = &d1Right;
    OutlineHistory::recordWindowActivation(history, activeWindow, &d2Active);
    activeWindow = &d2Active;

    assert(activeWindow == &d2Active);
    assert(history.first() == &d2Active);

    // 2. User on Desktop 2 triggers Jump to "S" (d1Left on Desktop 1)
    MockWindow *expectedTarget = &d1Left;

    // Simulate KWin switching to desktop 1: KWin temporarily emits windowActivated for d1Right (intermediate activation)
    MockWindow *spuriousIntermediate = &d1Right;
    if (expectedTarget != nullptr && spuriousIntermediate != expectedTarget) {
        // Intermediate activation suppressed! Do NOT touch activeWindow or history!
    } else {
        OutlineHistory::recordWindowActivation(history, activeWindow, spuriousIntermediate);
        activeWindow = spuriousIntermediate;
    }

    // Now KWin activates the actual expected target d1Left
    assert(expectedTarget == &d1Left);
    OutlineHistory::recordWindowActivation(history, activeWindow, &d1Left);
    activeWindow = &d1Left;
    expectedTarget = nullptr;

    // 3. User is now on d1Left. Immediate previous window in history MUST BE d2Active (Desktop 2), NOT d1Right!
    auto recents = OutlineHistory::computeRecentWindows(history, activeWindow, 1, true, isValid);
    assert(recents.size() == 1);
    assert(recents.at(0) == &d2Active); // Must return directly to Desktop 2 window!
}

void testFindRecentWindowAndToggle()
{
    MockWindow w1{1};
    MockWindow w2{2};
    MockWindow w3{3};

    auto isValid = [](MockWindow *w) {
        return w && !w->deleted && !w->desktop && !w->dock;
    };

    QList<MockWindow *> history;
    MockWindow *activeWindow = nullptr;

    // Single window: no recent window exists
    OutlineHistory::recordWindowActivation(history, activeWindow, &w1);
    activeWindow = &w1;
    assert(OutlineHistory::findRecentWindow(history, activeWindow, isValid) == nullptr);

    // Two windows: w1 then w2 activated
    OutlineHistory::recordWindowActivation(history, activeWindow, &w2);
    activeWindow = &w2;
    // Recent window must be w1
    assert(OutlineHistory::findRecentWindow(history, activeWindow, isValid) == &w1);

    // Three windows: w1, w2, then w3 activated
    OutlineHistory::recordWindowActivation(history, activeWindow, &w3);
    activeWindow = &w3;
    // Recent window must be w2 (most recent previous)
    assert(OutlineHistory::findRecentWindow(history, activeWindow, isValid) == &w2);

    // Toggle back to w2 (simulate Meta+Space Space jump)
    MockWindow *recent = OutlineHistory::findRecentWindow(history, activeWindow, isValid);
    assert(recent == &w2);
    OutlineHistory::recordWindowActivation(history, activeWindow, recent);
    activeWindow = recent; // activeWindow is now w2
    // Now recent window must be w3!
    assert(OutlineHistory::findRecentWindow(history, activeWindow, isValid) == &w3);

    // Toggle back to w3 (simulate second Meta+Space Space jump)
    recent = OutlineHistory::findRecentWindow(history, activeWindow, isValid);
    assert(recent == &w3);
    OutlineHistory::recordWindowActivation(history, activeWindow, recent);
    activeWindow = recent; // activeWindow is back to w3
    // Now recent window must be w2 again!
    assert(OutlineHistory::findRecentWindow(history, activeWindow, isValid) == &w2);

    // If w2 is closed / deleted, recent must fall back to w1
    w2.deleted = true;
    assert(OutlineHistory::findRecentWindow(history, activeWindow, isValid) == &w1);
}

void testHistoryNavigationCommitOnRelease()
{
    MockWindow w1{1};
    MockWindow w2{2};
    MockWindow w3{3};

    QList<MockWindow *> history = { &w1, &w2, &w3 };
    MockWindow *activeWindow = &w1;
    int navIndex = 0;
    bool isNavigating = false;

    // Simulate pressing Shift+Meta+,
    auto onPrevRecent = [&]() {
        if (!isNavigating) {
            navIndex = 0;
            isNavigating = true;
        }
        if (navIndex + 1 < history.size()) {
            navIndex++;
            activeWindow = history.at(navIndex);
        }
    };

    // Simulate key release (Meta or Shift released)
    auto onKeyRelease = [&]() {
        if (isNavigating) {
            if (navIndex >= 0 && navIndex < history.size()) {
                MockWindow *committed = history.at(navIndex);
                history.removeAll(committed);
                history.prepend(committed);
                activeWindow = committed;
            }
            navIndex = 0;
            isNavigating = false;
        }
    };

    // 1. Initial state: history is [w1, w2, w3], active is w1
    assert(history.at(0) == &w1);
    assert(history.at(1) == &w2);
    assert(history.at(2) == &w3);

    // 2. Press Shift+Meta+,: moves to w2
    onPrevRecent();
    assert(isNavigating == true);
    assert(navIndex == 1);
    assert(activeWindow == &w2);
    // History list is NOT yet modified during navigation
    assert(history.at(0) == &w1);

    // 3. Immediately release keys: commits w2 to top of history
    onKeyRelease();
    assert(isNavigating == false);
    assert(navIndex == 0);
    assert(activeWindow == &w2);
    assert(history.at(0) == &w2); // w2 is now at top!
    assert(history.at(1) == &w1);
    assert(history.at(2) == &w3);

    // 4. Press Shift+Meta+, twice before release: moves w2 -> w1 -> w3
    onPrevRecent(); // to w1
    assert(activeWindow == &w1);
    onPrevRecent(); // to w3
    assert(activeWindow == &w3);
    assert(navIndex == 2);

    // 5. Release keys: commits w3 to top of history immediately
    onKeyRelease();
    assert(isNavigating == false);
    assert(activeWindow == &w3);
    assert(history.at(0) == &w3); // w3 is now top!
    assert(history.at(1) == &w2);
    assert(history.at(2) == &w1);

    std::cout << "testHistoryNavigationCommitOnRelease PASSED\n";
}

void testStepHistoryIndexAndTraverse()
{
    // 1. Edge cases: count <= 1
    assert(OutlineHistory::stepHistoryIndex(0, 1, 0) == 0);
    assert(OutlineHistory::stepHistoryIndex(0, -1, 0) == 0);
    assert(OutlineHistory::stepHistoryIndex(0, 1, 1) == 0);
    assert(OutlineHistory::stepHistoryIndex(0, -1, 1) == 0);

    // 2. Count = 2
    assert(OutlineHistory::stepHistoryIndex(0, 1, 2) == 1);
    assert(OutlineHistory::stepHistoryIndex(1, 1, 2) == 0); // wrap forward
    assert(OutlineHistory::stepHistoryIndex(0, -1, 2) == 1); // wrap backward
    assert(OutlineHistory::stepHistoryIndex(1, -1, 2) == 0);

    // 3. Count = 5
    int idx = 0;
    // Step forward 0 -> 1 -> 2 -> 3 -> 4 -> 0
    idx = OutlineHistory::stepHistoryIndex(idx, 1, 5);
    assert(idx == 1);
    idx = OutlineHistory::stepHistoryIndex(idx, 1, 5);
    assert(idx == 2);
    idx = OutlineHistory::stepHistoryIndex(idx, 1, 5);
    assert(idx == 3);
    idx = OutlineHistory::stepHistoryIndex(idx, 1, 5);
    assert(idx == 4);
    idx = OutlineHistory::stepHistoryIndex(idx, 1, 5);
    assert(idx == 0); // circular wrap

    // Step backward 0 -> 4 -> 3 -> 2 -> 1 -> 0
    idx = OutlineHistory::stepHistoryIndex(idx, -1, 5);
    assert(idx == 4); // circular wrap backward
    idx = OutlineHistory::stepHistoryIndex(idx, -1, 5);
    assert(idx == 3);
    idx = OutlineHistory::stepHistoryIndex(idx, -1, 5);
    assert(idx == 2);
    idx = OutlineHistory::stepHistoryIndex(idx, -1, 5);
    assert(idx == 1);
    idx = OutlineHistory::stepHistoryIndex(idx, -1, 5);
    assert(idx == 0);

    // 4. Stateful simulation of Meta-held Space traversing (Alt+Tab style)
    MockWindow w1{1}, w2{2}, w3{3}, w4{4};
    QList<MockWindow *> history = {&w1, &w2, &w3, &w4};
    MockWindow *activeWindow = &w1;
    bool isTraversing = false;
    int navIndex = 0;

    auto onTraverse = [&](int direction) {
        if (!isTraversing) {
            isTraversing = true;
            navIndex = 0;
        }
        navIndex = OutlineHistory::stepHistoryIndex(navIndex, direction, history.size());
        activeWindow = history.at(navIndex);
    };

    auto onCommit = [&]() {
        if (isTraversing) {
            MockWindow *committed = history.at(navIndex);
            history.removeAll(committed);
            history.prepend(committed);
            activeWindow = committed;
            navIndex = 0;
            isTraversing = false;
        }
    };

    bool hudMetaHeld = true;
    auto isCheatsheetVisible = [&]() {
        return hudMetaHeld && !isTraversing;
    };

    // Initially Meta is held in Leap Mode before first space traversal: cheatsheet is visible
    assert(isCheatsheetVisible() == true);

    // User presses Meta+Space, then Space again (forward): moves to w2
    onTraverse(1);
    assert(isTraversing == true);
    assert(navIndex == 1);
    assert(activeWindow == &w2);
    // After first space press, cheatsheet MUST disappear so it does not obscure the screen
    assert(isCheatsheetVisible() == false);

    // Taps Space again: moves to w3
    onTraverse(1);
    assert(navIndex == 2);
    assert(activeWindow == &w3);
    assert(isCheatsheetVisible() == false);

    // Taps Shift+Space (direction -1): moves back to w2
    onTraverse(-1);
    assert(navIndex == 1);
    assert(activeWindow == &w2);
    assert(isCheatsheetVisible() == false);

    // Releases Meta: commits w2 to top of history
    onCommit();
    hudMetaHeld = false;
    assert(isTraversing == false);
    assert(activeWindow == &w2);
    assert(history.at(0) == &w2);
    assert(history.at(1) == &w1);
    assert(history.at(2) == &w3);
    assert(history.at(3) == &w4);
    assert(isCheatsheetVisible() == false);

    std::cout << "testStepHistoryIndexAndTraverse PASSED\n";
}

void testAltTabCandidateCycling()
{
    // Simulation of Alt+Tab style non-disruptive cycling for Shift+, / Shift+. and Meta+Space
    MockWindow w1{1}, w2{2}, w3{3}, w4{4};
    QList<MockWindow *> orderedWindows = {&w1, &w2, &w3, &w4};
    MockWindow *activeWindow = &w1;
    MockWindow *originWindow = &w1;
    MockWindow *candidateWindow = nullptr;
    bool shiftCycleActive = false;

    auto cycleBadge = [&](int delta) {
        MockWindow *curr = candidateWindow ? candidateWindow : activeWindow;
        int currIdx = orderedWindows.indexOf(curr);
        int nextIdx = (currIdx + delta + orderedWindows.size()) % orderedWindows.size();
        MockWindow *target = orderedWindows.at(nextIdx);

        shiftCycleActive = true;
        candidateWindow = target;
        // Notice: activeWindow is NOT changed here! No window is raised or popped to foreground.
    };

    auto onShiftRelease = [&]() {
        if (shiftCycleActive) {
            shiftCycleActive = false;
            MockWindow *chosen = candidateWindow;
            candidateWindow = nullptr;
            if (chosen) {
                activeWindow = chosen; // Commit jump on release!
            }
        }
    };

    auto onEscapeCancel = [&]() {
        shiftCycleActive = false;
        candidateWindow = nullptr;
        activeWindow = originWindow; // Restore origin window
    };

    // 1. Shift+.: candidate moves w1 -> w2, but activeWindow remains w1
    cycleBadge(1);
    assert(shiftCycleActive == true);
    assert(candidateWindow == &w2);
    assert(activeWindow == &w1); // Unchanged!

    // 2. Shift+. again: candidate moves w2 -> w3, activeWindow still w1
    cycleBadge(1);
    assert(candidateWindow == &w3);
    assert(activeWindow == &w1); // Still unchanged!

    // 3. Shift release: commits jump to w3
    onShiftRelease();
    assert(shiftCycleActive == false);
    assert(candidateWindow == nullptr);
    assert(activeWindow == &w3); // Now activated!

    // 4. Test cancel on Escape
    originWindow = activeWindow; // w3
    cycleBadge(1); // candidate -> w4
    assert(candidateWindow == &w4);
    assert(activeWindow == &w3);
    onEscapeCancel();
    assert(shiftCycleActive == false);
    assert(candidateWindow == nullptr);
    assert(activeWindow == &w3); // Unchanged, restored origin!

    // 5. Test Meta+Space non-disruptive traversal
    QList<MockWindow *> history = {&w1, &w2, &w3, &w4};
    activeWindow = &w1;
    originWindow = &w1;
    candidateWindow = nullptr;
    bool metaTraverseActive = false;
    int navIndex = 0;

    auto traverseRecent = [&](int direction) {
        if (!metaTraverseActive) {
            metaTraverseActive = true;
            navIndex = 0;
        }
        navIndex = OutlineHistory::stepHistoryIndex(navIndex, direction, history.size());
        candidateWindow = history.at(navIndex);
        // Notice: activeWindow is NOT activated during traversal
    };

    auto onMetaRelease = [&]() {
        if (metaTraverseActive) {
            metaTraverseActive = false;
            MockWindow *committed = candidateWindow ? candidateWindow : history.at(navIndex);
            candidateWindow = nullptr;
            if (committed) {
                history.removeAll(committed);
                history.prepend(committed);
                activeWindow = committed;
            }
            navIndex = 0;
        }
    };

    auto onMetaTraverseCancel = [&]() {
        metaTraverseActive = false;
        candidateWindow = nullptr;
        navIndex = 0;
        activeWindow = originWindow;
    };

    // First Space in traverse: candidate -> w2, activeWindow remains w1
    traverseRecent(1);
    assert(metaTraverseActive == true);
    assert(candidateWindow == &w2);
    assert(activeWindow == &w1);

    // Second Space: candidate -> w3, activeWindow still w1
    traverseRecent(1);
    assert(candidateWindow == &w3);
    assert(activeWindow == &w1);

    // Release Meta: commits w3
    onMetaRelease();
    assert(metaTraverseActive == false);
    assert(candidateWindow == nullptr);
    assert(activeWindow == &w3);
    assert(history.first() == &w3);

    // Test cancel
    originWindow = activeWindow; // w3
    traverseRecent(1); // candidate -> w1
    assert(candidateWindow == &w1);
    assert(activeWindow == &w3);
    onMetaTraverseCancel();
    assert(candidateWindow == nullptr);
    assert(activeWindow == &w3);

    std::cout << "testAltTabCandidateCycling PASSED\n";
}

int main()
{
    testRecentWindowsOrderAndLimit();
    testCrossDesktopJumpSuppressesIntermediateActivation();
    testFindRecentWindowAndToggle();
    testHistoryNavigationCommitOnRelease();
    testStepHistoryIndexAndTraverse();
    testAltTabCandidateCycling();
    std::cout << "All history tests passed successfully." << std::endl;
    return 0;
}
