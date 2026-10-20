// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>
#include <cmath>

namespace OutlineVisibility
{

struct Config
{
    int fadeDuration = 200;      // ms (fade transition duration)
    int menuHideDelay = 300;     // ms (buffer delay before hiding outline for popup)
    int menuShowDelay = 300;     // ms (buffer delay before restoring outline after popup)
    int windowHideDelay = 0;     // ms (buffer delay before removing outline on window switch)
    int windowShowDelay = 0;     // ms (buffer delay before showing outline on window switch)
};

class StateMachine
{
public:
    qreal visibilityOpacity = 1.0;
    qreal targetOpacity = 1.0;
    bool popupBlocked = false;
    qreal lastTickSec = -1.0;

    bool menuHideTimerActive = false;
    bool menuShowTimerActive = false;

    void reset(qreal initialOpacity = 1.0)
    {
        visibilityOpacity = initialOpacity;
        targetOpacity = initialOpacity;
        popupBlocked = false;
        lastTickSec = -1.0;
        menuHideTimerActive = false;
        menuShowTimerActive = false;
    }

    void setTargetOpacity(qreal target, int fadeDurationMs)
    {
        targetOpacity = target;
        lastTickSec = -1.0;
        if (fadeDurationMs <= 0) {
            visibilityOpacity = target;
        }
    }

    enum class TimerAction {
        None,
        StartHideTimer,
        StartShowTimer,
        CancelHideTimer,
        CancelShowTimer
    };

    TimerAction handlePopupChange(bool popupPresent, const Config &cfg)
    {
        if (popupPresent) {
            TimerAction action = TimerAction::None;
            if (menuShowTimerActive) {
                menuShowTimerActive = false;
                action = TimerAction::CancelShowTimer;
            }

            if (popupBlocked && targetOpacity <= 0.0001) {
                menuHideTimerActive = false;
                return action;
            }

            if (cfg.menuHideDelay <= 0) {
                popupBlocked = true;
                menuHideTimerActive = false;
                setTargetOpacity(0.0, cfg.fadeDuration);
                return action;
            } else {
                if (!menuHideTimerActive) {
                    menuHideTimerActive = true;
                    return TimerAction::StartHideTimer;
                }
            }
            return action;
        } else {
            TimerAction action = TimerAction::None;
            if (menuHideTimerActive) {
                menuHideTimerActive = false;
                action = TimerAction::CancelHideTimer;
            }

            if (!popupBlocked && targetOpacity >= 0.9999) {
                menuShowTimerActive = false;
                return action;
            }

            if (cfg.menuShowDelay <= 0) {
                popupBlocked = false;
                menuShowTimerActive = false;
                setTargetOpacity(1.0, cfg.fadeDuration);
                return action;
            } else {
                if (!menuShowTimerActive) {
                    menuShowTimerActive = true;
                    return TimerAction::StartShowTimer;
                }
            }
            return action;
        }
    }

    void handleMenuHideTimerTimeout(bool popupPresent, const Config &cfg)
    {
        menuHideTimerActive = false;
        if (popupPresent) {
            popupBlocked = true;
            setTargetOpacity(0.0, cfg.fadeDuration);
        }
    }

    void handleMenuShowTimerTimeout(bool popupPresent, const Config &cfg)
    {
        menuShowTimerActive = false;
        if (!popupPresent) {
            popupBlocked = false;
            setTargetOpacity(1.0, cfg.fadeDuration);
        }
    }

    bool needsSelfHeal(bool popupPresent) const
    {
        if (!popupPresent && popupBlocked && !menuShowTimerActive) {
            return true;
        }
        if (popupPresent && !popupBlocked && !menuHideTimerActive) {
            return true;
        }
        return false;
    }

    bool selfHeal(bool popupPresent, const Config &cfg)
    {
        if (!popupPresent && popupBlocked && !menuShowTimerActive) {
            handlePopupChange(false, cfg);
            return true;
        }
        if (popupPresent && !popupBlocked && !menuHideTimerActive) {
            handlePopupChange(true, cfg);
            return true;
        }
        return false;
    }

    bool stepTransitionDelta(qreal dtSec, int fadeDurationMs)
    {
        if (fadeDurationMs <= 0) {
            const bool changed = (std::abs(visibilityOpacity - targetOpacity) > 0.0001);
            visibilityOpacity = targetOpacity;
            return changed;
        }

        if (std::abs(visibilityOpacity - targetOpacity) < 0.0001) {
            const bool changed = (std::abs(visibilityOpacity - targetOpacity) > 0.0);
            visibilityOpacity = targetOpacity;
            return changed;
        }

        const qreal step = dtSec / (fadeDurationMs / 1000.0);
        if (visibilityOpacity < targetOpacity) {
            visibilityOpacity = qMin(targetOpacity, visibilityOpacity + step);
        } else {
            visibilityOpacity = qMax(targetOpacity, visibilityOpacity - step);
        }
        return true;
    }

    bool stepTransition(qreal elapsedSec, int fadeDurationMs)
    {
        qreal dt = 0.016;
        if (lastTickSec >= 0.0 && elapsedSec >= lastTickSec) {
            const qreal rawDt = elapsedSec - lastTickSec;
            // Normal 60fps frame is ~0.016s. Allow up to 0.05s (20 FPS).
            // If more than 50ms elapsed (e.g. idle or stopped timer),
            // treat as a fresh single frame step (0.016s) to prevent jumping.
            dt = (rawDt <= 0.05) ? rawDt : 0.016;
        }
        lastTickSec = elapsedSec;
        return stepTransitionDelta(dt, fadeDurationMs);
    }

    bool isTransitionActive(int fadeDurationMs) const
    {
        if (fadeDurationMs <= 0) {
            return false;
        }
        return std::abs(visibilityOpacity - targetOpacity) > 0.0001;
    }

    bool isEffectivelyVisible() const
    {
        return visibilityOpacity > 0.001;
    }
};

} // namespace OutlineVisibility
