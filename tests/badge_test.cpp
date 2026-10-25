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
using OutlineBadge::generateRandomMarkColor;
using OutlineBadge::renderOffscreenTargetPill;
using OutlineBadge::contrastingTextColor;

void testOffscreenTargetPill()
{
    QImage pill = renderOffscreenTargetPill(QLatin1Char('A'), QColor(231, 76, 60), Qt::white, QStringLiteral("Desktop 2"), QStringLiteral("Firefox"), 28, 1.0);
    assert(!pill.isNull());
    assert(pill.width() > 50);
    assert(pill.height() > 30);

    // Empty caption
    QImage pillNoCap = renderOffscreenTargetPill(QLatin1Char('M'), QColor(52, 152, 219), Qt::white, QStringLiteral("Minimized"), QString(), 28, 1.0);
    assert(!pillNoCap.isNull());

    // Covered window pill ("Schowane" / "Covered")
    QImage pillCoveredPl = renderOffscreenTargetPill(QLatin1Char('C'), QColor(46, 204, 113), Qt::black, QStringLiteral("Schowane"), QStringLiteral("Dolphin"), 28, 1.0);
    assert(!pillCoveredPl.isNull());
    assert(pillCoveredPl.width() > 50);
    assert(pillCoveredPl.height() > 30);

    QImage pillCoveredEn = renderOffscreenTargetPill(QLatin1Char('D'), QColor(155, 89, 182), Qt::white, QStringLiteral("Covered"), QStringLiteral("Kate Editor"), 28, 1.0);
    assert(!pillCoveredEn.isNull());
    assert(pillCoveredEn.width() > 50);
    assert(pillCoveredEn.height() > 30);

    // Pill with custom label
    QImage pillWithLabel = renderOffscreenTargetPill(QLatin1Char('B'), QColor(231, 76, 60), Qt::white, QStringLiteral("Pulpit 1"), QStringLiteral("bash"), 28, 1.0, QStringLiteral("Backend"));
    assert(!pillWithLabel.isNull());
    assert(pillWithLabel.width() > 60);
    assert(pillWithLabel.height() > 30);

    // Pill with long custom label (eliding check)
    QImage pillLongLabel = renderOffscreenTargetPill(QLatin1Char('Z'), QColor(52, 152, 219), Qt::white, QStringLiteral("Schowane"), QStringLiteral("Super Long Process Name"), 28, 1.0, QStringLiteral("Very Long Custom Label Exceeding Normal Width"));
    assert(!pillLongLabel.isNull());
    assert(pillLongLabel.width() > 100);
}

void testRandomMarkColor()
{
    // Generates a valid color with empty used list
    QColor c1 = generateRandomMarkColor({});
    assert(c1.isValid());
    assert(c1.alpha() == 255);

    // If one color is used, picks another
    QColor c2 = generateRandomMarkColor({c1});
    assert(c2.isValid());
    assert(c2 != c1);

    // When palette is fully exhausted, fallback HSV generates valid bright color
    QList<QColor> fullList;
    for (int i = 0; i < 20; ++i) {
        fullList.append(generateRandomMarkColor(fullList));
    }
    assert(fullList.size() == 20);
    for (const QColor &c : fullList) {
        assert(c.isValid());
    }
}

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

    // Normal badge with custom label
    QImage labeledBadge = renderBadgeImage(QLatin1Char('A'), QColor(231, 76, 60), Qt::white, false, 28, 1.0, QStringLiteral("Backend"));
    assert(!labeledBadge.isNull());
    assert(labeledBadge.width() > 38);
    assert(labeledBadge.height() == 28);

    // computeBadgeWidth tests
    assert(qRound(OutlineBadge::computeBadgeWidth(28)) == qRound(28 * 1.35));
    assert(OutlineBadge::computeBadgeWidth(28, QStringLiteral("Backend")) > 38);
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

void testContrastCalculation()
{
    // Dark backgrounds must produce white text (Qt::white)
    assert(contrastingTextColor(QColor(0, 0, 0)) == Qt::white);
    assert(contrastingTextColor(QColor(35, 38, 41, 235)) == Qt::white); // default jump badge background
    assert(contrastingTextColor(QColor(QStringLiteral("#e74c3c"))) == Qt::white); // Crimson Red
    assert(contrastingTextColor(QColor(QStringLiteral("#5f27cd"))) == Qt::white); // Deep Violet
    assert(contrastingTextColor(QColor(QStringLiteral("#3498db"))) == Qt::white); // Sky Blue
    assert(contrastingTextColor(QColor(QStringLiteral("#9b59b6"))) == Qt::white); // Amethyst Purple

    // Light backgrounds must produce black text (Qt::black)
    assert(contrastingTextColor(QColor(255, 255, 255)) == Qt::black);
    assert(contrastingTextColor(QColor(QStringLiteral("#f1c40f"))) == Qt::black); // Sunflower Yellow
    assert(contrastingTextColor(QColor(QStringLiteral("#feca57"))) == Qt::black); // Warm Yellow
    assert(contrastingTextColor(QColor(QStringLiteral("#00d2d3"))) == Qt::black); // Cyan
    assert(contrastingTextColor(QColor(QStringLiteral("#1dd1a1"))) == Qt::black); // Bright Teal
    assert(contrastingTextColor(QColor(QStringLiteral("#2ecc71"))) == Qt::black); // Emerald Green
    assert(contrastingTextColor(QColor(220, 220, 220)) == Qt::black); // Light Gray

    // Automatic contrast fallback in renderBadgeImage when textColor is omitted
    QImage autoLight = renderBadgeImage(QLatin1Char('Y'), QColor(QStringLiteral("#f1c40f")));
    assert(!autoLight.isNull());
    QImage autoDark = renderBadgeImage(QLatin1Char('Z'), QColor(QStringLiteral("#232629")));
    assert(!autoDark.isNull());
}

void testSmartAccentExtraction()
{
    using OutlineBadge::extractSmartAccentFromImage;

    // 1. Pure blue icon image (like Dolphin)
    QImage blueImg(32, 32, QImage::Format_RGBA8888);
    blueImg.fill(QColor(29, 153, 243));
    QColor blueAccent = extractSmartAccentFromImage(blueImg);
    assert(blueAccent.isValid());
    assert(blueAccent.hue() >= 190 && blueAccent.hue() <= 220);
    assert(blueAccent.saturationF() > 0.60);

    // 2. Pure green icon image (like Spotify)
    QImage greenImg(32, 32, QImage::Format_RGBA8888);
    greenImg.fill(QColor(46, 204, 113));
    QColor greenAccent = extractSmartAccentFromImage(greenImg);
    assert(greenAccent.isValid());
    assert(greenAccent.hue() >= 130 && greenAccent.hue() <= 160);
    assert(greenAccent.saturationF() > 0.50);

    // 3. Null image returns invalid QColor
    QImage nullImg;
    QColor nullAccent = extractSmartAccentFromImage(nullImg);
    assert(!nullAccent.isValid());
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    testLetterAllocation();
    testBadgeRendering();
    testBadgePositioning();
    testRandomMarkColor();
    testOffscreenTargetPill();
    testContrastCalculation();
    testSmartAccentExtraction();

    std::cout << "All badge tests passed successfully." << std::endl;
    return 0;
}
