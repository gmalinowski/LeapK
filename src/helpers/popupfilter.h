// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <unordered_set>

namespace OutlinePopupFilter
{

// Some Wayland clients (including Firefox) use interactive xdg_popups without
// an explicit grab or a semantic menu type. Limit this fallback to the active
// window's transient tree, and exclude tooltips and input-transparent surfaces.
// Parameterized to test the policy without running a compositor.
template<typename Window>
bool isOwnedInteractivePopup(Window *popup, Window *active, bool acceptsInput)
{
    if (!popup || !active || popup == active || !acceptsInput
        || popup->isDeleted() || !popup->isPopupWindow() || popup->isTooltip()) {
        return false;
    }
    std::unordered_set<Window *> visited;
    for (Window *parent = popup->transientFor(); parent && !parent->isDeleted()
         && visited.insert(parent).second; parent = parent->transientFor()) {
        if (parent == active) {
            return true;
        }
    }
    return false;
}

} // namespace OutlinePopupFilter
