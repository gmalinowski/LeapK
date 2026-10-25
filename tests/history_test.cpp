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

int main()
{
    testRecentWindowsOrderAndLimit();
    testCrossDesktopJumpSuppressesIntermediateActivation();
    std::cout << "All history tests passed successfully." << std::endl;
    return 0;
}
