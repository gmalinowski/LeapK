// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later

#include "visibilitystate.h"
#include <cstdio>
#include <cmath>

int main()
{
    using namespace OutlineVisibility;
    int failures = 0;
    int checks = 0;

    const auto check = [&](bool condition, const char *name) {
        ++checks;
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };

    Config config;
    config.fadeDuration = 200;
    config.menuHideDelay = 300;
    config.menuShowDelay = 300;

    StateMachine sm;

    // 1. Initial State
    {
        sm.reset(1.0);
        check(sm.visibilityOpacity == 1.0, "initial visibilityOpacity is 1.0");
        check(sm.targetOpacity == 1.0, "initial targetOpacity is 1.0");
        check(!sm.popupBlocked, "initial popupBlocked is false");
        check(sm.isEffectivelyVisible(), "initially effectively visible");
        check(!sm.isTransitionActive(config.fadeDuration), "no transition active initially");
    }

    // 2. Menu Open with Buffer Delay and Fade Out
    {
        sm.reset(1.0);
        // Popup appears
        auto action = sm.handlePopupChange(true, config);
        check(action == StateMachine::TimerAction::StartHideTimer, "popup appearance starts hide timer");
        check(sm.menuHideTimerActive, "hide timer marked active");
        check(sm.targetOpacity == 1.0, "target opacity remains 1.0 during hide buffer delay");
        check(sm.isEffectivelyVisible(), "outline remains visible during buffer delay");

        // Timer expires after 300ms
        sm.handleMenuHideTimerTimeout(true, config);
        check(!sm.menuHideTimerActive, "hide timer marked inactive after timeout");
        check(sm.popupBlocked, "popupBlocked set to true on timeout");
        check(sm.targetOpacity == 0.0, "target opacity set to 0.0 after timeout");
        check(sm.isTransitionActive(config.fadeDuration), "transition is now active");

        // Simulate frame ticks during fade out (200ms duration)
        // Step 1: 100ms in (halfway)
        sm.stepTransitionDelta(0.100, config.fadeDuration);
        check(sm.visibilityOpacity >= 0.45 && sm.visibilityOpacity <= 0.55,
              "opacity is ~0.5 halfway through fade out");
        check(sm.isEffectivelyVisible(), "outline still effectively visible at 0.5 opacity");

        // Step 2: 200ms in (complete)
        sm.stepTransitionDelta(0.100, config.fadeDuration);
        check(sm.visibilityOpacity <= 0.001, "opacity reaches 0.0 at end of fade out");
        check(!sm.isTransitionActive(config.fadeDuration), "transition stops once target reached");
        check(!sm.isEffectivelyVisible(), "outline is no longer effectively visible");
    }

    // 3. Menu Close with Buffer Delay and Fade In
    {
        // Start from faded out state
        sm.reset(0.0);
        sm.popupBlocked = true;
        sm.targetOpacity = 0.0;

        // Popup closed
        auto action = sm.handlePopupChange(false, config);
        check(action == StateMachine::TimerAction::StartShowTimer, "popup close starts show timer");
        check(sm.menuShowTimerActive, "show timer marked active");
        check(sm.targetOpacity == 0.0, "target opacity remains 0.0 during show buffer delay");

        // Timer expires after 300ms
        sm.handleMenuShowTimerTimeout(false, config);
        check(!sm.menuShowTimerActive, "show timer inactive after timeout");
        check(!sm.popupBlocked, "popupBlocked set to false on timeout");
        check(sm.targetOpacity == 1.0, "target opacity set to 1.0 after timeout");
        check(sm.isTransitionActive(config.fadeDuration), "fade in transition active");

        // Step 1: 100ms in (halfway)
        sm.stepTransitionDelta(0.100, config.fadeDuration);
        check(sm.visibilityOpacity >= 0.45 && sm.visibilityOpacity <= 0.55,
              "opacity is ~0.5 halfway through fade in");
        check(sm.isEffectivelyVisible(), "outline effectively visible at 0.5 opacity");

        // Step 2: 200ms in (complete)
        sm.stepTransitionDelta(0.100, config.fadeDuration);
        check(sm.visibilityOpacity >= 0.999, "opacity reaches 1.0 at end of fade in");
        check(!sm.isTransitionActive(config.fadeDuration), "transition stops once 1.0 reached");
        check(sm.isEffectivelyVisible(), "outline fully visible");
    }

    // 4. Rapid Menu Hopping (flick: popup opened and closed within buffer delay)
    {
        sm.reset(1.0);
        auto action1 = sm.handlePopupChange(true, config);
        check(action1 == StateMachine::TimerAction::StartHideTimer, "hide timer started");

        // 100ms later (before 300ms timer fires), popup is closed
        auto action2 = sm.handlePopupChange(false, config);
        check(action2 == StateMachine::TimerAction::CancelHideTimer, "hide timer cancelled on rapid close");
        check(!sm.menuHideTimerActive, "hide timer inactive");
        check(!sm.popupBlocked, "never marked popupBlocked");
        check(sm.targetOpacity == 1.0, "target opacity never changed from 1.0");
        check(sm.visibilityOpacity == 1.0, "visibility opacity remained 1.0");
        check(!sm.isTransitionActive(config.fadeDuration), "no transition was triggered");
    }

    // 5. Submenu Navigation (hopping from File menu to Edit menu)
    {
        // Start with outline hidden (File menu open)
        sm.reset(0.0);
        sm.popupBlocked = true;
        sm.targetOpacity = 0.0;

        // File menu closes
        auto action1 = sm.handlePopupChange(false, config);
        check(action1 == StateMachine::TimerAction::StartShowTimer, "show timer started for menu close");
        check(sm.menuShowTimerActive, "show timer active");

        // 50ms later (before 300ms show timer fires), Edit menu opens
        auto action2 = sm.handlePopupChange(true, config);
        check(action2 == StateMachine::TimerAction::CancelShowTimer, "show timer cancelled when next menu opens");
        check(!sm.menuShowTimerActive, "show timer cancelled");
        check(sm.targetOpacity == 0.0, "target opacity remained 0.0");
        check(sm.visibilityOpacity == 0.0, "visibility opacity remained 0.0");
        check(!sm.isTransitionActive(config.fadeDuration), "no spurious fade-in transition");
    }

    // 6. Instant Fade Transition (fadeDuration = 0)
    {
        Config instantFadeCfg = config;
        instantFadeCfg.fadeDuration = 0;

        sm.reset(1.0);
        sm.handlePopupChange(true, instantFadeCfg);
        sm.handleMenuHideTimerTimeout(true, instantFadeCfg);
        check(sm.visibilityOpacity == 0.0, "instant fade duration snaps visibilityOpacity to 0.0 immediately on timeout");
        check(!sm.isTransitionActive(0), "transition is not active when fadeDuration is 0");

        sm.handlePopupChange(false, instantFadeCfg);
        sm.handleMenuShowTimerTimeout(false, instantFadeCfg);
        check(sm.visibilityOpacity == 1.0, "instant fade duration snaps visibilityOpacity to 1.0 immediately on timeout");
    }

    // 7. Instant Delays (menuHideDelay = 0, menuShowDelay = 0)
    {
        Config instantDelayCfg = config;
        instantDelayCfg.menuHideDelay = 0;
        instantDelayCfg.menuShowDelay = 0;

        sm.reset(1.0);
        auto action1 = sm.handlePopupChange(true, instantDelayCfg);
        check(action1 == StateMachine::TimerAction::None, "no timer started when menuHideDelay is 0");
        check(sm.targetOpacity == 0.0, "target opacity set to 0.0 immediately");
        check(sm.popupBlocked, "popupBlocked set immediately");

        auto action2 = sm.handlePopupChange(false, instantDelayCfg);
        check(action2 == StateMachine::TimerAction::None, "no timer started when menuShowDelay is 0");
        check(sm.targetOpacity == 1.0, "target opacity set to 1.0 immediately");
        check(!sm.popupBlocked, "popupBlocked cleared immediately");
    }

    // 8. Regression Test for Menu Close / Deleted Window Bug
    {
        // Bug scenario:
        // A popup opened and outline was hidden.
        sm.reset(0.0);
        sm.popupBlocked = true;
        sm.targetOpacity = 0.0;

        // When popup closes in Wayland, window is deleted (isDeleted() == true).
        // Old code checked if (!isBlockingPopup(w)) return;
        // which returned false because isBlockingPopup(w) rejected deleted windows!
        // The fix calls updatePopupBlockingState() / handlePopupChange(false).
        auto action = sm.handlePopupChange(false, config);
        check(action == StateMachine::TimerAction::StartShowTimer, "unblocking triggers show timer");
        check(sm.menuShowTimerActive, "show timer running");

        sm.handleMenuShowTimerTimeout(false, config);
        check(sm.targetOpacity == 1.0, "target opacity restored to 1.0");

        // Advance fade in to verify outline reappears
        sm.stepTransitionDelta(0.200, config.fadeDuration);
        check(sm.visibilityOpacity == 1.0, "outline successfully reappears at 1.0");
        check(sm.isEffectivelyVisible(), "outline is effectively visible");
    }

    // 9. Self-Healing Safeguard
    {
        // Simulate desync where popupBlocked is true, but no popup actually exists and no timer is active
        sm.reset(0.0);
        sm.popupBlocked = true;
        sm.targetOpacity = 0.0;
        sm.menuShowTimerActive = false;

        check(sm.needsSelfHeal(false), "needsSelfHeal detects desync when blocked without popup");

        bool healed = sm.selfHeal(false, config);
        check(healed, "selfHeal detects desync when blocked without popup");
        check(sm.menuShowTimerActive, "selfHeal automatically started show sequence");
        check(!sm.needsSelfHeal(false), "needsSelfHeal is false once show timer is active");

        // And if already in sync:
        bool healedAgain = sm.selfHeal(false, config);
        check(!healedAgain, "selfHeal does not trigger when already recovering or in sync");
    }

    // 10. Idle Pause Recovery (No Instant Snap After Menu Open)
    {
        sm.reset(0.0);
        sm.popupBlocked = true;
        sm.targetOpacity = 0.0;
        sm.lastTickSec = 1.0; // Simulated timestamp when menu opened

        // Menu closes 5 seconds later
        sm.handlePopupChange(false, config);
        sm.handleMenuShowTimerTimeout(false, config);
        check(sm.targetOpacity == 1.0, "target opacity is 1.0 after show delay");
        check(sm.visibilityOpacity == 0.0, "visibility opacity starts at 0.0");

        // First frame tick occurs at elapsedSec = 6.316s (over 5 seconds after lastTickSec)
        sm.stepTransition(6.316, 500); // 500ms fade duration
        check(sm.visibilityOpacity > 0.001 && sm.visibilityOpacity <= 0.10,
              "first frame after long idle period advances by single frame (~16ms), NOT jumping to 1.0");
        check(sm.isTransitionActive(500), "transition remains active across subsequent frames");

        // Step by step verification over 500ms
        for (int frame = 1; frame < 30; ++frame) {
            sm.stepTransition(6.316 + frame * 0.0166, 500);
        }
        check(sm.visibilityOpacity >= 0.95 && sm.visibilityOpacity <= 1.0,
              "opacity smoothly reaches target across 30 frames for 500ms duration");
    }

    // 11. Reaction to FadeDuration Time Setting
    {
        // Test that a longer duration produces proportionally smaller opacity steps
        sm.reset(0.0);
        sm.setTargetOpacity(1.0, 1000); // 1000ms duration
        for (int frame = 0; frame < 10; ++frame) {
            sm.stepTransition(10.0 + frame * 0.016, 1000);
        }
        const qreal opacity1000 = sm.visibilityOpacity;

        sm.reset(0.0);
        sm.setTargetOpacity(1.0, 200); // 200ms duration
        for (int frame = 0; frame < 10; ++frame) {
            sm.stepTransition(10.0 + frame * 0.016, 200);
        }
        const qreal opacity200 = sm.visibilityOpacity;

        check(opacity1000 >= 0.10 && opacity1000 <= 0.25, "1000ms fade produces ~16% progress after 10 frames");
        check(opacity200 >= 0.70 && opacity200 <= 0.90, "200ms fade produces ~80% progress after 10 frames");
        check(opacity200 > opacity1000 * 3.0, "animation strictly reacts to FadeDuration setting");
    }

    // 12. Window Switch Delay (Concurrent Switch Hide and Show Delays)
    {
        // When switching windows with windowHideDelay > 0 and windowShowDelay > 0:
        // m_windowShowTimer is active (showPending = true), but m_delayedHideWindow is still waiting to hide (delayedWindowEligible = true).
        // Effect MUST remain active so KWin compositor continues rendering the delayed hide window outline.
        check(StateMachine::isEffectActive(true, true, true, true),
              "effect remains active when show delay is pending but delayed hide window is waiting to hide");
        check(!StateMachine::isEffectActive(true, true, true, false),
              "effect is inactive when show delay is pending and no delayed hide window is waiting");
        check(StateMachine::isEffectActive(true, false, true, false),
              "effect is active when active window is eligible and no show delay is pending");
        check(!StateMachine::isEffectActive(false, true, true, true),
              "effect is inactive when outline is disabled regardless of pending delays");
    }

    // 13. Switch Hide Fade Transition (DelayedHideState)
    {
        DelayedHideState dhs;
        dhs.reset();
        check(dhs.opacity == 1.0, "initial delayed hide opacity is 1.0");
        check(!dhs.fading, "initial delayed hide fading is false");
        check(dhs.isEffectivelyVisible(), "initially effectively visible");
        check(!dhs.isTransitionActive(200), "initially no transition active");

        // 13.1 Buffer delay then fade
        dhs.startBuffer();
        check(dhs.opacity == 1.0, "buffer maintains opacity at 1.0");
        check(!dhs.fading, "buffer does not mark fading active");
        check(!dhs.stepTransition(1.0, 200), "stepTransition does not step during buffer");

        // Timeout triggers fade-out over 200ms
        dhs.startFade(200);
        check(dhs.fading, "startFade activates fading flag");
        check(dhs.isTransitionActive(200), "transition is active during fade");

        // Halfway (100ms)
        dhs.stepTransitionDelta(0.100, 200);
        check(dhs.opacity >= 0.45 && dhs.opacity <= 0.55, "delayed hide opacity ~0.5 halfway through fade-out");
        check(dhs.isEffectivelyVisible(), "still effectively visible at 0.5 opacity");

        // Finished (200ms)
        dhs.stepTransitionDelta(0.100, 200);
        check(dhs.opacity <= 0.001, "delayed hide opacity reaches 0.0 at end of fade-out");
        check(!dhs.fading, "fading flag cleared once 0.0 reached");
        check(!dhs.isEffectivelyVisible(), "no longer effectively visible");
        check(!dhs.isTransitionActive(200), "transition stops once 0.0 reached");

        // 13.2 Instant switch hide (fadeDuration = 0)
        dhs.startBuffer();
        dhs.startFade(0);
        check(dhs.opacity == 0.0, "fadeDuration 0 snaps delayed hide opacity immediately to 0.0");
        check(!dhs.fading, "instant fade does not remain in fading state");
        check(!dhs.isEffectivelyVisible(), "instant fade is immediately invisible");

        // 13.3 Smooth timestamp stepping across frames
        dhs.startBuffer();
        dhs.startFade(500); // 500ms duration
        dhs.stepTransition(10.0, 500);
        check(dhs.opacity < 1.0 && dhs.opacity >= 0.95, "first frame tick steps smoothly");
        for (int frame = 1; frame < 32; ++frame) {
            dhs.stepTransition(10.0 + frame * 0.0166, 500);
        }
        check(dhs.opacity <= 0.001, "smoothly reaches 0.0 across 500ms");
    }

    std::printf("VisibilityStateTest: %d checks, %d failures\n", checks, failures);
    return failures > 0 ? 1 : 0;
}
