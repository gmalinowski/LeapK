/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QList>

namespace OutlineHistory {

template <typename WindowPtr, typename IsValidFunc>
QList<WindowPtr> computeRecentWindows(const QList<WindowPtr> &history,
                                      WindowPtr activeWindow,
                                      int maxCount,
                                      bool highlightRecent,
                                      IsValidFunc isValid)
{
    QList<WindowPtr> result;
    if (!highlightRecent || maxCount <= 0) {
        return result;
    }

    for (const WindowPtr &win : history) {
        if (win && win != activeWindow && isValid(win)) {
            result.append(win);
            if (result.size() >= maxCount) {
                break;
            }
        }
    }

    return result;
}

template <typename WindowPtr>
void recordWindowActivation(QList<WindowPtr> &history,
                            WindowPtr previousWindow,
                            WindowPtr newActiveWindow,
                            int maxHistorySize = 50)
{
    if (previousWindow) {
        history.removeAll(previousWindow);
        history.prepend(previousWindow);
    }
    if (newActiveWindow) {
        history.removeAll(newActiveWindow);
        history.prepend(newActiveWindow);
    }
    while (history.size() > maxHistorySize) {
        history.removeLast();
    }
}

} // namespace OutlineHistory
