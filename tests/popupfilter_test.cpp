// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#include "popupfilter.h"
#include <cstdio>

struct Window {
    bool deleted = false;
    bool popup = false;
    bool tooltip = false;
    Window *parent = nullptr;
    bool isDeleted() const { return deleted; }
    bool isPopupWindow() const { return popup; }
    bool isTooltip() const { return tooltip; }
    Window *transientFor() const { return parent; }
};

int main()
{
    using OutlinePopupFilter::isOwnedInteractivePopup;
    int failures = 0;
    int checks = 0;
    const auto check = [&](bool result, bool expected, const char *name) {
        ++checks;
        if (result != expected) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };
    Window active;
    Window other;
    Window menu{.popup = true, .parent = &active};
    Window submenu{.popup = true, .parent = &menu};
    check(isOwnedInteractivePopup(&menu, &active, true), true, "ungrabbed interactive menu");
    check(isOwnedInteractivePopup(&submenu, &active, true), true, "nested submenu");
    check(isOwnedInteractivePopup(&menu, &active, false), false, "input-transparent popup");
    check(isOwnedInteractivePopup(&menu, &other, true), false, "other active window");
    check(isOwnedInteractivePopup(&active, &active, true), false, "active window itself");
    check(isOwnedInteractivePopup<Window>(nullptr, &active, true), false, "missing popup");
    check(isOwnedInteractivePopup<Window>(&menu, nullptr, true), false, "missing active window");
    menu.tooltip = true;
    check(isOwnedInteractivePopup(&menu, &active, true), false, "explicit tooltip");
    menu.tooltip = false;
    menu.deleted = true;
    check(isOwnedInteractivePopup(&menu, &active, true), false, "closed menu");
    check(isOwnedInteractivePopup(&submenu, &active, true), false, "closed ancestor");
    menu.deleted = false;
    menu.popup = false;
    check(isOwnedInteractivePopup(&menu, &active, true), false, "ordinary dialog");
    menu.popup = true;
    menu.parent = nullptr;
    check(isOwnedInteractivePopup(&menu, &active, true), false, "unowned popup");
    menu.parent = &submenu;
    check(isOwnedInteractivePopup(&menu, &active, true), false, "cyclic transient chain");
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
