/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "badgehelper.h"
#include <QGuiApplication>
#include <cassert>
#include <iostream>

using OutlineBadge::findNextAvailableLetter;
using OutlineBadge::renderBadgeImage;
using OutlineBadge::computeBadgeRect;

void testLetterAllocation()
{
    // Empty list gives first preferred letter ('A')
    assert(findNextAvailableLetter({}) == QLatin1Char('A'));

    // If 'A' is used, gives 'S'
    assert(findNextAvailableLetter({QLatin1Char('A')}) == QLatin1Char('S'));

    // Case insensitivity
    assert(findNextAvailableLetter({QLatin1Char('a'), QLatin1Char('s')}) == QLatin1Char('D'));
}

void testBadgeRendering()
{
    // Default size (28) normal badge
    QImage normalBadge = renderBadgeImage(QLatin1Char('T'), QColor(231, 76, 60), Qt::white, false, 28, 1.0);
    assert(!normalBadge.isNull());
    assert(normalBadge.width() == 38);
    assert(normalBadge.height() == 28);

    // Custom size (20)
    QImage compactBadge = renderBadgeImage(QLatin1Char('A'), QColor(50, 50, 50), Qt::white, false, 20, 1.0);
    assert(!compactBadge.isNull());
    assert(compactBadge.width() == 27);
    assert(compactBadge.height() == 20);

    // Jump mode badge scaled
    QImage jumpBadge = renderBadgeImage(QLatin1Char('F'), QColor(25, 25, 30), QColor(241, 196, 15), true, 28, 2.0);
    assert(!jumpBadge.isNull());
    assert(jumpBadge.width() == 94);
    assert(jumpBadge.height() == 70);
}

void testBadgePositioning()
{
    const QRectF geo(100, 200, 800, 600);
    const qreal bw = 38;
    const qreal bh = 28;
    const qreal margin = 12;

    // Top-Left (0)
    QRectF r0 = computeBadgeRect(geo, 0, bw, bh, margin);
    assert(r0 == QRectF(112, 212, bw, bh));

    // Top (1)
    QRectF r1 = computeBadgeRect(geo, 1, bw, bh, margin);
    assert(r1 == QRectF(100 + (800 - bw) / 2.0, 212, bw, bh));

    // Top-Right (2)
    QRectF r2 = computeBadgeRect(geo, 2, bw, bh, margin);
    assert(r2 == QRectF(900 - margin - bw, 212, bw, bh));

    // Left (3)
    QRectF r3 = computeBadgeRect(geo, 3, bw, bh, margin);
    assert(r3 == QRectF(112, 200 + (600 - bh) / 2.0, bw, bh));

    // Center (4)
    QRectF r4 = computeBadgeRect(geo, 4, bw, bh, margin);
    assert(r4 == QRectF(100 + (800 - bw) / 2.0, 200 + (600 - bh) / 2.0, bw, bh));

    // Right (5)
    QRectF r5 = computeBadgeRect(geo, 5, bw, bh, margin);
    assert(r5 == QRectF(900 - margin - bw, 200 + (600 - bh) / 2.0, bw, bh));

    // Bottom-Left (6)
    QRectF r6 = computeBadgeRect(geo, 6, bw, bh, margin);
    assert(r6 == QRectF(112, 800 - margin - bh, bw, bh));

    // Bottom (7)
    QRectF r7 = computeBadgeRect(geo, 7, bw, bh, margin);
    assert(r7 == QRectF(100 + (800 - bw) / 2.0, 800 - margin - bh, bw, bh));

    // Bottom-Right (8)
    QRectF r8 = computeBadgeRect(geo, 8, bw, bh, margin);
    assert(r8 == QRectF(900 - margin - bw, 800 - margin - bh, bw, bh));

    // Clamping test when window is small
    const QRectF smallGeo(50, 50, 40, 30);
    QRectF smallR = computeBadgeRect(smallGeo, 2, 38, 28, 50);
    assert(smallR.left() >= smallGeo.left());
    assert(smallR.right() <= smallGeo.right());
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    testLetterAllocation();
    testBadgeRendering();
    testBadgePositioning();

    std::cout << "All badge tests passed successfully." << std::endl;
    return 0;
}
