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

void testComputeHudPillPositions()
{
    using OutlineBadge::computeHudPillPositions;

    const QRectF screen(0, 0, 1920, 1080);

    // 1. Empty list
    assert(computeHudPillPositions({}, screen).isEmpty());

    // 2. Single pill, bottom (default hudPosition = 0)
    QList<QSizeF> singlePill = { QSizeF(200, 36) };
    QList<QRectF> rBottom = computeHudPillPositions(singlePill, screen, 0, 24.0, 12.0);
    assert(rBottom.size() == 1);
    assert(qFuzzyCompare(rBottom[0].left(), (1920.0 - 200.0) / 2.0));
    assert(qFuzzyCompare(rBottom[0].top(), 1080.0 - 24.0 - 36.0));
    assert(qFuzzyCompare(rBottom[0].width(), 200.0));
    assert(qFuzzyCompare(rBottom[0].height(), 36.0));

    // 3. Single pill, top (hudPosition = 1)
    QList<QRectF> rTop = computeHudPillPositions(singlePill, screen, 1, 24.0, 12.0);
    assert(rTop.size() == 1);
    assert(qFuzzyCompare(rTop[0].left(), (1920.0 - 200.0) / 2.0));
    assert(qFuzzyCompare(rTop[0].top(), 24.0));

    // 4. Single pill, left (hudPosition = 2)
    QList<QRectF> rLeft = computeHudPillPositions(singlePill, screen, 2, 24.0, 12.0);
    assert(rLeft.size() == 1);
    assert(qFuzzyCompare(rLeft[0].left(), 24.0));
    assert(qFuzzyCompare(rLeft[0].top(), (1080.0 - 36.0) / 2.0));

    // 5. Single pill, right (hudPosition = 3)
    QList<QRectF> rRight = computeHudPillPositions(singlePill, screen, 3, 24.0, 12.0);
    assert(rRight.size() == 1);
    assert(qFuzzyCompare(rRight[0].left(), 1920.0 - 24.0 - 200.0));
    assert(qFuzzyCompare(rRight[0].top(), (1080.0 - 36.0) / 2.0));

    // 6. Multi-row wrapping at bottom on smaller screen
    // screen 500x500, margin 20 -> availWidth 460
    // 3 pills of width 200, gap 10 -> row 1 has 2 pills (200 + 10 + 200 = 410), row 2 has 1 pill (200)
    QRectF smallScreen(0, 0, 500, 500);
    QList<QSizeF> threePills = { QSizeF(200, 30), QSizeF(200, 30), QSizeF(200, 30) };
    QList<QRectF> rWrapBottom = computeHudPillPositions(threePills, smallScreen, 0, 20.0, 10.0);
    assert(rWrapBottom.size() == 3);
    // Cluster height = 30 + 10 + 30 = 70. rowY = 500 - 20 - 70 = 410.
    // Row 1 centered: startX = (500 - 410) / 2 = 45.
    assert(qFuzzyCompare(rWrapBottom[0].left(), 45.0));
    assert(qFuzzyCompare(rWrapBottom[0].top(), 410.0));
    assert(qFuzzyCompare(rWrapBottom[1].left(), 255.0));
    assert(qFuzzyCompare(rWrapBottom[1].top(), 410.0));
    // Row 2 centered: startX = (500 - 200) / 2 = 150.
    assert(qFuzzyCompare(rWrapBottom[2].left(), 150.0));
    assert(qFuzzyCompare(rWrapBottom[2].top(), 450.0));

    // 7. Multi-column wrapping at left with varying widths (left-aligned)
    // screen 500x500, margin 20 -> availHeight 460
    // 3 pills of height 200, varying widths (100, 50, 80), gap 10
    // col 1 has 2 pills (height 410, maxWidth 100), col 2 has 1 pill (height 200, maxWidth 80)
    QList<QSizeF> threeTallPills = { QSizeF(100, 200), QSizeF(50, 200), QSizeF(80, 200) };
    QList<QRectF> rWrapLeft = computeHudPillPositions(threeTallPills, smallScreen, 2, 20.0, 10.0);
    assert(rWrapLeft.size() == 3);
    // Col 1 centered vertically: startY = (500 - 410) / 2 = 45. colX = 20.
    // Both pills 0 and 1 must be strictly left-aligned at colX = 20.0!
    assert(qFuzzyCompare(rWrapLeft[0].left(), 20.0));
    assert(qFuzzyCompare(rWrapLeft[0].top(), 45.0));
    assert(qFuzzyCompare(rWrapLeft[0].width(), 100.0));
    assert(qFuzzyCompare(rWrapLeft[1].left(), 20.0)); // Left-aligned, NOT centered at 20 + (100-50)/2 = 45
    assert(qFuzzyCompare(rWrapLeft[1].top(), 255.0));
    assert(qFuzzyCompare(rWrapLeft[1].width(), 50.0));
    // Col 2 centered vertically: startY = (500 - 200) / 2 = 150. colX = 20 + 100 + 10 = 130.
    assert(qFuzzyCompare(rWrapLeft[2].left(), 130.0));
    assert(qFuzzyCompare(rWrapLeft[2].top(), 150.0));
    assert(qFuzzyCompare(rWrapLeft[2].width(), 80.0));
}

void testIconAndThumbnailBadges()
{
    // 1. computeBadgeWidth checks
    const qreal baseW = OutlineBadge::computeBadgeWidth(28);
    const qreal withIconW = OutlineBadge::computeBadgeWidth(28, QString(), true, false);
    const qreal withThumbW = OutlineBadge::computeBadgeWidth(28, QString(), false, true);
    const qreal withBothW = OutlineBadge::computeBadgeWidth(28, QString(), true, true);
    const qreal withAllW = OutlineBadge::computeBadgeWidth(28, QStringLiteral("Terminal"), true, true);

    assert(baseW == 38);
    assert(withIconW > baseW);
    assert(withThumbW > baseW);
    assert(withBothW > withIconW);
    assert(withBothW > withThumbW);
    assert(withAllW > withBothW);

    // Create a dummy QIcon
    QPixmap iconPix(32, 32);
    iconPix.fill(QColor(52, 152, 219));
    QIcon testIcon(iconPix);

    // Create a dummy window thumbnail QImage
    QImage testThumb(160, 100, QImage::Format_RGBA8888);
    testThumb.fill(QColor(46, 204, 113));

    // 2. renderBadgeImage checks
    QImage normalBadge = renderBadgeImage(QLatin1Char('A'), QColor(35, 38, 41), Qt::white, false, 28, 1.0);
    assert(!normalBadge.isNull());
    assert(normalBadge.width() == 38);

    QImage iconBadge = renderBadgeImage(QLatin1Char('A'), QColor(35, 38, 41), Qt::white, false, 28, 1.0,
                                        QString(), true, testIcon, false, QImage());
    assert(!iconBadge.isNull());
    assert(iconBadge.width() > normalBadge.width());

    QImage thumbBadge = renderBadgeImage(QLatin1Char('A'), QColor(35, 38, 41), Qt::white, false, 28, 1.0,
                                         QString(), false, QIcon(), true, testThumb);
    assert(!thumbBadge.isNull());
    assert(thumbBadge.width() > normalBadge.width());

    QImage combinedBadge = renderBadgeImage(QLatin1Char('A'), QColor(35, 38, 41), Qt::white, false, 28, 1.0,
                                           QStringLiteral("Web"), true, testIcon, true, testThumb);
    assert(!combinedBadge.isNull());
    assert(combinedBadge.width() > iconBadge.width());
    assert(combinedBadge.width() > thumbBadge.width());

    // 3. renderOffscreenTargetPill checks
    QImage normalPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                                 QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0);
    assert(!normalPill.isNull());

    QImage iconPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                               QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                               QString(), true, testIcon, false, QImage());
    assert(!iconPill.isNull());
    assert(iconPill.width() > normalPill.width());

    QImage thumbPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                                QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                                QString(), false, QIcon(), true, testThumb);
    assert(!thumbPill.isNull());
    assert(thumbPill.width() > normalPill.width());

    QImage bothPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                               QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                               QStringLiteral("Browser"), true, testIcon, true, testThumb);
    assert(!bothPill.isNull());
    assert(bothPill.width() > iconPill.width());
    assert(bothPill.width() > thumbPill.width());
}

void testBadgeConfigAndStyling()
{
    OutlineBadge::BadgeConfig cfg;
    assert(cfg.windowBadgeSize == 28);
    assert(cfg.jumpBadgeSize == 34);
    assert(cfg.hudBadgeSize == 36);
    assert(cfg.iconSize == 20);
    assert(cfg.thumbnailWidth == 56);
    assert(cfg.thumbnailHeight == 32);
    assert(cfg.cornerRadius == 6);
    assert(cfg.opacityPercent == 92);

    // Custom sizing
    cfg.windowBadgeSize = 32;
    cfg.jumpBadgeSize = 48;
    cfg.hudBadgeSize = 44;
    cfg.iconSize = 24;
    cfg.thumbnailWidth = 72;
    cfg.thumbnailHeight = 40;

    QImage winBadge = renderBadgeImage(cfg, QLatin1Char('W'), QColor(41, 128, 185), Qt::white, false);
    assert(!winBadge.isNull());
    assert(winBadge.height() == 32);

    QImage jumpBadge = renderBadgeImage(cfg, QLatin1Char('J'), QColor(39, 174, 96), Qt::white, true);
    assert(!jumpBadge.isNull());
    assert(jumpBadge.height() == 48);

    // Custom appearance & colors
    cfg.defaultBgColor = QColor(45, 52, 54);
    cfg.cornerRadius = 0; // Sharp rectangle
    cfg.opacityPercent = 75;
    cfg.letterColor = QColor(255, 235, 59);
    cfg.labelUnmarkedColor = QColor(255, 87, 34);
    cfg.letterFontFamily = QStringLiteral("monospace");
    cfg.letterFontSize = 16;
    cfg.labelFontFamily = QStringLiteral("sans-serif");
    cfg.labelFontSize = 12;

    QImage customBadge = renderBadgeImage(cfg, QLatin1Char('M'), QColor(), QColor(), false, 0, 1.0, QStringLiteral("Custom"));
    assert(!customBadge.isNull());
    assert(customBadge.height() == 32);
    assert(customBadge.width() > winBadge.width());

    // Offscreen HUD pill with BadgeConfig
    cfg.captionColor = QColor(236, 240, 241);
    cfg.locationColor = QColor(241, 196, 15);
    cfg.captionFontFamily = QStringLiteral("sans-serif");
    cfg.captionFontSize = 13;
    cfg.locationFontFamily = QStringLiteral("sans-serif");
    cfg.locationFontSize = 11;

    QImage hudPill = renderOffscreenTargetPill(cfg, QLatin1Char('H'), QColor(), QColor(),
                                             QStringLiteral("Desktop 3"), QStringLiteral("Dolphin File Manager"),
                                             0, 1.0, QStringLiteral("Files"));
    assert(!hudPill.isNull());
    assert(hudPill.height() >= 44);
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
    testComputeHudPillPositions();
    testIconAndThumbnailBadges();
    testBadgeConfigAndStyling();

    std::cout << "All badge tests passed successfully." << std::endl;
    return 0;
}
