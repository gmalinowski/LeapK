/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "precedencehelper.h"
#include <cassert>
#include <iostream>

using OutlinePrecedence::ResolvedStyle;
using OutlinePrecedence::parsePrecedence;
using OutlinePrecedence::resolveStyle;

void testParsing()
{
    // Default standard order
    {
        QStringList order = parsePrecedence(QStringLiteral("Marked,Active,Recent"));
        assert(order.size() == 3);
        assert(order.at(0) == QStringLiteral("Marked"));
        assert(order.at(1) == QStringLiteral("Active"));
        assert(order.at(2) == QStringLiteral("Recent"));
    }

    // Reordered with whitespace and case differences
    {
        QStringList order = parsePrecedence(QStringLiteral(" marked , recent , active "));
        assert(order.size() == 3);
        assert(order.at(0) == QStringLiteral("Marked"));
        assert(order.at(1) == QStringLiteral("Recent"));
        assert(order.at(2) == QStringLiteral("Active"));
    }

    // Missing entries get appended in canonical fallback order
    {
        QStringList order = parsePrecedence(QStringLiteral("Recent"));
        assert(order.size() == 3);
        assert(order.at(0) == QStringLiteral("Recent"));
        assert(order.at(1) == QStringLiteral("Marked"));
        assert(order.at(2) == QStringLiteral("Active"));
    }

    // Empty or garbage fallback
    {
        QStringList order = parsePrecedence(QStringLiteral("invalid, garbage"));
        assert(order.size() == 3);
        assert(order.at(0) == QStringLiteral("Marked"));
        assert(order.at(1) == QStringLiteral("Active"));
        assert(order.at(2) == QStringLiteral("Recent"));
    }
}

void testResolutionActiveWinsOverMarked()
{
    // Active > Marked > Recent
    const QStringList precedence = {QStringLiteral("Active"), QStringLiteral("Marked"), QStringLiteral("Recent")};

    // 1. Focused window, not marked -> Active
    assert(resolveStyle(true, false, false, precedence) == ResolvedStyle::Active);

    // 2. Focused window, also marked -> Active wins!
    assert(resolveStyle(true, true, false, precedence) == ResolvedStyle::Active);

    // 3. Inactive window, marked and in recent history -> Marked wins over Recent
    assert(resolveStyle(false, true, true, precedence) == ResolvedStyle::Marked);

    // 4. Inactive window, marked but not in recent history -> Marked
    assert(resolveStyle(false, true, false, precedence) == ResolvedStyle::Marked);

    // 5. Inactive window, not marked, in recent history -> Recent
    assert(resolveStyle(false, false, true, precedence) == ResolvedStyle::Recent);

    // 6. Inactive window, neither marked nor recent -> None
    assert(resolveStyle(false, false, false, precedence) == ResolvedStyle::None);
}

void testResolutionMarkedWinsOverActive()
{
    // Marked > Active > Recent
    const QStringList precedence = {QStringLiteral("Marked"), QStringLiteral("Active"), QStringLiteral("Recent")};

    // 1. Focused window, not marked -> Active
    assert(resolveStyle(true, false, false, precedence) == ResolvedStyle::Active);

    // 2. Focused window, marked -> Marked wins over Active!
    assert(resolveStyle(true, true, false, precedence) == ResolvedStyle::Marked);

    // 3. Inactive window, marked and in recent history -> Marked wins over Recent
    assert(resolveStyle(false, true, true, precedence) == ResolvedStyle::Marked);

    // 4. Inactive window, marked and not in recent history -> Marked
    assert(resolveStyle(false, true, false, precedence) == ResolvedStyle::Marked);

    // 5. Inactive window, in recent history, not marked -> Recent
    assert(resolveStyle(false, false, true, precedence) == ResolvedStyle::Recent);
}

void testResolutionRecentWinsOverMarked()
{
    // Active > Recent > Marked
    const QStringList precedence = {QStringLiteral("Active"), QStringLiteral("Recent"), QStringLiteral("Marked")};

    // 1. Focused window, marked -> Active wins over Marked
    assert(resolveStyle(true, true, false, precedence) == ResolvedStyle::Active);

    // 2. Inactive window, marked AND in recent history -> Recent wins over Marked!
    assert(resolveStyle(false, true, true, precedence) == ResolvedStyle::Recent);

    // 3. Inactive window, marked but NOT in recent history -> Marked (Recent cannot apply to non-recent)
    assert(resolveStyle(false, true, false, precedence) == ResolvedStyle::Marked);

    // 4. Inactive window, not marked, in recent history -> Recent
    assert(resolveStyle(false, false, true, precedence) == ResolvedStyle::Recent);
}

void testResolutionMarkedOverRecentOverActive()
{
    // Marked > Recent > Active
    const QStringList precedence = {QStringLiteral("Marked"), QStringLiteral("Recent"), QStringLiteral("Active")};

    // 1. Focused window, marked -> Marked wins
    assert(resolveStyle(true, true, false, precedence) == ResolvedStyle::Marked);

    // 2. Focused window, not marked -> Active
    assert(resolveStyle(true, false, false, precedence) == ResolvedStyle::Active);

    // 3. Inactive window, marked and in recent history -> Marked wins
    assert(resolveStyle(false, true, true, precedence) == ResolvedStyle::Marked);
}

int main()
{
    testParsing();
    testResolutionActiveWinsOverMarked();
    testResolutionMarkedWinsOverActive();
    testResolutionRecentWinsOverMarked();
    testResolutionMarkedOverRecentOverActive();

    std::cout << "All precedence tests passed successfully." << std::endl;
    return 0;
}
