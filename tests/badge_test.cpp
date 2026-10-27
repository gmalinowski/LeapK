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

    // 8. Multi-pill in row with varying heights in Top (hudPosition = 1) and Bottom (0) -> all pills strictly TOP-ALIGNED at rowY
    QList<QSizeF> varyingHeightPills = { QSizeF(150, 180), QSizeF(140, 120), QSizeF(160, 90) };
    QList<QRectF> rVaryingTop = computeHudPillPositions(varyingHeightPills, screen, 1, 24.0, 12.0);
    assert(rVaryingTop.size() == 3);
    assert(qFuzzyCompare(rVaryingTop[0].top(), 24.0));
    assert(qFuzzyCompare(rVaryingTop[1].top(), 24.0)); // Strictly top-aligned, NOT centered at 24 + (180-120)/2 = 54
    assert(qFuzzyCompare(rVaryingTop[2].top(), 24.0)); // Strictly top-aligned, NOT centered at 24 + (180-90)/2 = 69

    QList<QRectF> rVaryingBottom = computeHudPillPositions(varyingHeightPills, screen, 0, 24.0, 12.0);
    assert(rVaryingBottom.size() == 3);
    const qreal bottomRowY = 1080.0 - 24.0 - 180.0;
    assert(qFuzzyCompare(rVaryingBottom[0].top(), bottomRowY));
    assert(qFuzzyCompare(rVaryingBottom[1].top(), bottomRowY)); // Strictly top-aligned in bottom layout
    assert(qFuzzyCompare(rVaryingBottom[2].top(), bottomRowY));
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

    // 3. renderOffscreenTargetPill checks (Top/Bottom puts thumbnail underneath badges)
    QImage normalPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                                 QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0);
    assert(!normalPill.isNull());

    QImage iconPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                               QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                               QString(), true, testIcon, false, QImage());
    assert(!iconPill.isNull());
    assert(iconPill.width() > normalPill.width());

    // Top/Bottom (default hudPosition=0): thumbnail is placed UNDER the badge, so height increases
    QImage thumbPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                                QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                                QString(), false, QIcon(), true, testThumb);
    assert(!thumbPill.isNull());
    assert(thumbPill.height() > normalPill.height());

    QImage bothPill = renderOffscreenTargetPill(QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                               QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                               QStringLiteral("Browser"), true, testIcon, true, testThumb);
    assert(!bothPill.isNull());
    assert(bothPill.height() > normalPill.height());

    // Left/Right mode (hudPosition=2): thumbnail is side-by-side, icon is at the very end
    OutlineBadge::BadgeConfig cfgSide;
    cfgSide.hudPosition = 2; // Left
    cfgSide.showThumbnail = true;
    cfgSide.showIcon = true;
    QImage sidePill = renderOffscreenTargetPill(cfgSide, QLatin1Char('F'), QColor(231, 76, 60), Qt::white,
                                               QStringLiteral("Desktop 1"), QStringLiteral("Firefox"), 28, 1.0,
                                               QStringLiteral("Browser"), testIcon, testThumb);
    assert(!sidePill.isNull());
    assert(sidePill.width() > normalPill.width());
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

void testAutoContrastTypography()
{
    // Luminance contrast rules:
    // Bright yellow (#f1c40f) -> Black text
    assert(contrastingTextColor(QColor(QStringLiteral("#f1c40f"))) == Qt::black);
    // Pure white (#ffffff) -> Black text
    assert(contrastingTextColor(QColor(QStringLiteral("#ffffff"))) == Qt::black);
    // Bright cyan (#00d2d3) -> Black text
    assert(contrastingTextColor(QColor(QStringLiteral("#00d2d3"))) == Qt::black);

    // Dark charcoal (#232629) -> White text
    assert(contrastingTextColor(QColor(QStringLiteral("#232629"))) == Qt::white);
    // Deep midnight blue (#1b263b) -> White text
    assert(contrastingTextColor(QColor(QStringLiteral("#1b263b"))) == Qt::white);

    OutlineBadge::BadgeConfig cfg;
    cfg.letterColorAuto = true;
    cfg.labelColorAuto = true;
    cfg.captionColorAuto = true;
    cfg.locationColorAuto = true;

    // Render badge with Auto contrast on bright yellow background
    QImage yellowBadge = renderBadgeImage(cfg, QLatin1Char('A'), QColor(QStringLiteral("#f1c40f")), QColor(), false, 28, 1.0, QStringLiteral("Test"));
    assert(!yellowBadge.isNull());
    // Sample center of letter area: text drawn in black on yellow background
    bool foundDarkPixel = false;
    for (int y = 5; y < yellowBadge.height() - 5; ++y) {
        for (int x = 5; x < 25; ++x) {
            const QColor pCol = yellowBadge.pixelColor(x, y);
            if (pCol.red() < 60 && pCol.green() < 60 && pCol.blue() < 60 && pCol.alpha() > 200) {
                foundDarkPixel = true;
                break;
            }
        }
        if (foundDarkPixel) break;
    }
    assert(foundDarkPixel);

    // Disable auto contrast and set explicit pink letter
    cfg.letterColorAuto = false;
    cfg.letterColor = QColor(QStringLiteral("#ff3366"));
    QImage customColBadge = renderBadgeImage(cfg, QLatin1Char('B'), QColor(QStringLiteral("#232629")), QColor(), false, 28, 1.0);
    assert(!customColBadge.isNull());

    // Contrast safeguard test (Retro CRT Amber issue prevention):
    // When letter color is amber (#ffb000), but badge background is ALSO amber (#ffb000, e.g. recent window),
    // the system MUST NOT draw amber-on-amber (zero contrast). It must automatically fall back to dark/black text.
    assert(OutlineBadge::contrastRatio(QColor(QStringLiteral("#ffb000")), QColor(QStringLiteral("#ffb000"))) < 1.05);
    assert(OutlineBadge::contrastRatio(QColor(QStringLiteral("#ffb000")), QColor(QStringLiteral("#1a1200"))) > 6.0);
    assert(OutlineBadge::contrastRatio(QColor(QStringLiteral("#ffb000")), QColor(QStringLiteral("#000000"))) > 15.0);

    cfg.letterColorAuto = false;
    cfg.letterColor = QColor(QStringLiteral("#ffb000")); // Amber letter

    // 1. Amber background (#ffb000) -> Must fall back to dark text to guarantee contrast!
    QImage amberOnAmberBadge = renderBadgeImage(cfg, QLatin1Char('D'), QColor(QStringLiteral("#ffb000")), QColor(), false, 28, 1.0, QString(), QIcon(), QImage(), QStringLiteral("Space"), true);
    assert(!amberOnAmberBadge.isNull());
    bool foundContrastingDarkText = false;
    for (int y = 5; y < amberOnAmberBadge.height() - 5; ++y) {
        for (int x = 5; x < 25; ++x) {
            const QColor pCol = amberOnAmberBadge.pixelColor(x, y);
            if (pCol.red() < 60 && pCol.green() < 60 && pCol.blue() < 60 && pCol.alpha() > 200) {
                foundContrastingDarkText = true;
                break;
            }
        }
        if (foundContrastingDarkText) break;
    }
    assert(foundContrastingDarkText);

    // 2. Dark CRT background (#1a1200) -> Contrast ratio > 6.0, so amber text (#ffb000) MUST be preserved!
    QImage amberOnDarkBadge = renderBadgeImage(cfg, QLatin1Char('D'), QColor(QStringLiteral("#1a1200")), QColor(), false, 28, 1.0);
    assert(!amberOnDarkBadge.isNull());
    bool foundAmberLetterPixel = false;
    for (int y = 5; y < amberOnDarkBadge.height() - 5; ++y) {
        for (int x = 5; x < 25; ++x) {
            const QColor pCol = amberOnDarkBadge.pixelColor(x, y);
            if (pCol.red() > 200 && pCol.green() > 140 && pCol.blue() < 50 && pCol.alpha() > 200) {
                foundAmberLetterPixel = true;
                break;
            }
        }
        if (foundAmberLetterPixel) break;
    }
    assert(foundAmberLetterPixel);

    // 3. HUD pill with Amber on Amber -> Must also fall back to contrasting dark letter
    QImage hudAmberPill = renderOffscreenTargetPill(cfg, QLatin1Char('D'), QColor(QStringLiteral("#ffb000")), QColor(), QStringLiteral("Pulpit 1"), QStringLiteral("Terminal"), 36, 1.0, QString(), QIcon(), QImage(), true, QStringLiteral("Space"), true);
    assert(!hudAmberPill.isNull());
    bool foundHudDarkText = false;
    for (int y = 5; y < hudAmberPill.height() - 5; ++y) {
        for (int x = 5; x < 35; ++x) {
            const QColor pCol = hudAmberPill.pixelColor(x, y);
            if (pCol.red() < 60 && pCol.green() < 60 && pCol.blue() < 60 && pCol.alpha() > 200) {
                foundHudDarkText = true;
                break;
            }
        }
        if (foundHudDarkText) break;
    }
    assert(foundHudDarkText);
}

void testWindowPreviewOnWindowBadges()
{
    OutlineBadge::BadgeConfig cfg;
    cfg.windowBadgeSize = 28;
    cfg.thumbnailWidth = 56;
    cfg.thumbnailHeight = 32;
    cfg.showThumbnail = false;

    QImage badgeNoThumb = renderBadgeImage(cfg, QLatin1Char('K'), QColor(41, 128, 185), Qt::white, false, 28, 1.0);
    assert(!badgeNoThumb.isNull());
    assert(badgeNoThumb.height() == 28);

    // Create a 64x36 test thumbnail with distinct red/blue halves
    QImage testThumb(64, 36, QImage::Format_RGBA8888);
    testThumb.fill(QColor(0, 120, 215, 255));
    {
        QPainter p(&testThumb);
        p.fillRect(0, 0, 32, 36, QColor(220, 40, 40, 255));
    }

    cfg.showThumbnail = true;
    const qreal computedW = OutlineBadge::computeBadgeWidth(cfg, 28);
    QImage badgeWithThumb = renderBadgeImage(cfg, QLatin1Char('K'), QColor(41, 128, 185), Qt::white, false, 28, 1.0,
                                             QString(), QIcon(), testThumb);
    assert(!badgeWithThumb.isNull());
    assert(badgeWithThumb.height() == 28);
    assert(badgeWithThumb.width() == qRound(computedW));
    assert(badgeWithThumb.width() > badgeNoThumb.width());

    // Verify thumbnail pixels were actually drawn onto the badge
    bool foundThumbPixel = false;
    for (int y = 4; y < badgeWithThumb.height() - 4; ++y) {
        for (int x = 30; x < badgeWithThumb.width() - 4; ++x) {
            const QColor col = badgeWithThumb.pixelColor(x, y);
            // Should find red pixel from the left half of testThumb
            if (col.red() > 180 && col.blue() < 80) {
                foundThumbPixel = true;
                break;
            }
        }
        if (foundThumbPixel) break;
    }
    assert(foundThumbPixel);
}

void testHudMarkColorModes()
{
    OutlineBadge::BadgeConfig cfg;
    cfg.hudBadgeSize = 36;
    const QColor markColor(QStringLiteral("#f1c40f")); // Bright yellow mark

    // Mode 0: Dark card with mark border & badge (Default / Recommended)
    cfg.hudMarkColorMode = 0;
    QImage hudDark = renderOffscreenTargetPill(cfg, QLatin1Char('M'), markColor, Qt::black,
                                             QStringLiteral("Desktop 1"), QStringLiteral("Terminal"), 36, 1.0,
                                             QStringLiteral("Dev"), QIcon(), QImage(), true);
    assert(!hudDark.isNull());
    // Sample background of the card in the gap between badge and text
    const int sampleX = 8 + (hudDark.height() - 8) + 4;
    const int sampleY = hudDark.height() / 2;
    const QColor bgDark = hudDark.pixelColor(sampleX, sampleY);
    // Charcoal base has low RGB values
    assert(bgDark.red() < 40 && bgDark.green() < 40 && bgDark.blue() < 45);
    // Badge background inside letter badge is vibrant undimmed yellow
    const QColor badgeBg = hudDark.pixelColor(12, 10);
    assert(badgeBg.red() > 220 && badgeBg.green() > 180 && badgeBg.blue() < 30);

    // Mode 1: Full mark color
    cfg.hudMarkColorMode = 1;
    QImage hudFull = renderOffscreenTargetPill(cfg, QLatin1Char('M'), markColor, Qt::black,
                                             QStringLiteral("Desktop 1"), QStringLiteral("Terminal"), 36, 1.0,
                                             QStringLiteral("Dev"), QIcon(), QImage(), true);
    assert(!hudFull.isNull());
    const QColor bgFull = hudFull.pixelColor(sampleX, sampleY);
    assert(bgFull.red() > 180 && bgFull.green() > 140 && bgFull.blue() < 60);

    // Mode 2: Tinted mark color
    cfg.hudMarkColorMode = 2;
    QImage hudTinted = renderOffscreenTargetPill(cfg, QLatin1Char('M'), markColor, Qt::black,
                                               QStringLiteral("Desktop 1"), QStringLiteral("Terminal"), 36, 1.0,
                                               QStringLiteral("Dev"), QIcon(), QImage(), true);
    assert(!hudTinted.isNull());
    const QColor bgTinted = hudTinted.pixelColor(sampleX, sampleY);
    // Tinted has intermediate red/green higher than charcoal
    assert(bgTinted.red() > 60 && bgTinted.green() > 50);
}

void testHudPositionsAndThumbnailLayout()
{
    // Create distinct test assets
    QImage magentaIconImg(20, 20, QImage::Format_RGBA8888);
    magentaIconImg.fill(QColor(255, 0, 255)); // Bright Magenta
    QIcon testIcon(QPixmap::fromImage(magentaIconImg));

    QImage cyanThumbImg(80, 45, QImage::Format_RGBA8888);
    cyanThumbImg.fill(QColor(0, 255, 255)); // Bright Cyan

    OutlineBadge::BadgeConfig cfg;
    cfg.showIcon = true;
    cfg.showThumbnail = true;
    cfg.thumbnailWidth = 80;
    cfg.thumbnailHeight = 45;
    cfg.iconSize = 20;

    // 1. Top/Bottom HUD position (0 = Bottom, 1 = Top) -> preview must be UNDER badges
    for (int pos : {0, 1}) {
        cfg.hudPosition = pos;

        // Without thumbnail: compact single-row pill
        cfg.showThumbnail = false;
        QImage pillNoThumb = renderOffscreenTargetPill(cfg, QLatin1Char('A'), QColor(40, 44, 52), Qt::white,
                                                      QStringLiteral("Desktop 1"), QStringLiteral("Editor"), 36, 1.0,
                                                      QString(), testIcon, QImage());
        assert(!pillNoThumb.isNull());
        const int singleRowH = pillNoThumb.height();

        // With thumbnail: window preview is stacked underneath, increasing height by thumbH + gap
        cfg.showThumbnail = true;
        QImage pillWithThumb = renderOffscreenTargetPill(cfg, QLatin1Char('A'), QColor(40, 44, 52), Qt::white,
                                                        QStringLiteral("Desktop 1"), QStringLiteral("Editor"), 36, 1.0,
                                                        QString(), testIcon, cyanThumbImg);
        assert(!pillWithThumb.isNull());
        assert(pillWithThumb.height() > singleRowH + 30);

        // Verify cyan pixels from the thumbnail appear in the lower half (under badges)
        const int midX = pillWithThumb.width() / 2;
        const int lowerY = pillWithThumb.height() - 20;
        const QColor lowerColor = pillWithThumb.pixelColor(midX, lowerY);
        assert(lowerColor.green() > 200 && lowerColor.blue() > 200);
    }

    // 2. Left/Right HUD position (2 = Left, 3 = Right) -> icon between badge and preview: [Badge] [Icon] [Preview] [Text]
    for (int pos : {2, 3}) {
        cfg.hudPosition = pos;
        cfg.hudShowThumbnail = true;
        cfg.hudShowIcon = true;
        cfg.showThumbnail = true;
        cfg.showIcon = true;

        QImage pillSide = renderOffscreenTargetPill(cfg, QLatin1Char('B'), QColor(30, 34, 40), Qt::white,
                                                   QStringLiteral("Desktop 2"), QStringLiteral("Firefox"), 36, 1.0,
                                                   QStringLiteral("Web"), testIcon, cyanThumbImg);
        assert(!pillSide.isNull());

        // In side mode, it's a single row, so height is standard pill height
        assert(pillSide.height() < 60);

        // Icon (magenta) is BETWEEN letter badge and preview (around x = 60..70)
        const int iconSampleX = 66;
        const int sampleY = pillSide.height() / 2;
        const QColor iconColor = pillSide.pixelColor(iconSampleX, sampleY);
        assert(iconColor.red() > 200 && iconColor.blue() > 200 && iconColor.green() < 60);

        // Preview (cyan) is to the right of icon (around x = 110..130)
        const int thumbSampleX = 115;
        const QColor thumbColor = pillSide.pixelColor(thumbSampleX, sampleY);
        assert(thumbColor.green() > 200 && thumbColor.blue() > 200);
    }
}

void testIndependentWindowAndHudBadges()
{
    QImage magentaIconImg(20, 20, QImage::Format_RGBA8888);
    magentaIconImg.fill(QColor(255, 0, 255));
    QIcon testIcon(QPixmap::fromImage(magentaIconImg));

    QImage cyanThumbImg(80, 45, QImage::Format_RGBA8888);
    cyanThumbImg.fill(QColor(0, 255, 255));

    OutlineBadge::BadgeConfig cfg;
    // Window badges: only icon, NO preview
    cfg.windowShowIcon = true;
    cfg.windowShowThumbnail = false;
    cfg.windowIconSize = 18;
    cfg.windowBadgeSize = 28;

    // HUD badges: only preview, NO icon
    cfg.hudShowIcon = false;
    cfg.hudShowThumbnail = true;
    cfg.hudBadgeSize = 40;
    cfg.hudThumbnailWidth = 90;
    cfg.hudThumbnailHeight = 50;

    // 1. Render window badge
    QImage winBadge = renderBadgeImage(cfg, QLatin1Char('W'), QColor(35, 38, 41), Qt::white,
                                       false, 0, 1.0, QString(), testIcon, cyanThumbImg);
    assert(!winBadge.isNull());
    bool hasMagenta = false;
    bool hasCyan = false;
    for (int y = 0; y < winBadge.height(); ++y) {
        for (int x = 0; x < winBadge.width(); ++x) {
            const QColor c = winBadge.pixelColor(x, y);
            if (c.red() > 200 && c.blue() > 200 && c.green() < 60) hasMagenta = true;
            if (c.green() > 200 && c.blue() > 200 && c.red() < 60) hasCyan = true;
        }
    }
    assert(hasMagenta);
    assert(!hasCyan);

    // 2. Render HUD pill
    QImage hudPill = renderOffscreenTargetPill(cfg, QLatin1Char('H'), QColor(35, 38, 41), Qt::white,
                                              QStringLiteral("Desk 1"), QStringLiteral("Test"), 0, 1.0,
                                              QString(), testIcon, cyanThumbImg);
    assert(!hudPill.isNull());
    bool hudHasMagenta = false;
    bool hudHasCyan = false;
    for (int y = 0; y < hudPill.height(); ++y) {
        for (int x = 0; x < hudPill.width(); ++x) {
            const QColor c = hudPill.pixelColor(x, y);
            if (c.red() > 200 && c.blue() > 200 && c.green() < 60) hudHasMagenta = true;
            if (c.green() > 200 && c.blue() > 200 && c.red() < 60) hudHasCyan = true;
        }
    }
    assert(!hudHasMagenta);
    assert(hudHasCyan);
}

void testRecentBadgeAndIncludeAll()
{
    using OutlineBadge::BadgeConfig;
    using OutlineBadge::computeBadgeWidth;
    using OutlineBadge::renderBadgeImage;
    using OutlineBadge::renderOffscreenTargetPill;

    BadgeConfig cfg;
    assert(!cfg.hudIncludeAll);
    assert(!cfg.livePreviews);
    assert(cfg.badgeTimeout == 5);
    assert(cfg.jumpRecentKey == QStringLiteral("Space"));

    // 1. computeBadgeWidth with recentKey
    const qreal standardW = computeBadgeWidth(cfg, 34);
    const qreal recentW = computeBadgeWidth(cfg, 34, QString(), QStringLiteral("Space"));
    assert(recentW > standardW);

    // 2. renderBadgeImage with recentKey
    QImage recentBadge = renderBadgeImage(cfg, QLatin1Char('D'), QColor(52, 152, 219), Qt::white,
                                          true, 34, 1.0, QString(), QIcon(), QImage(),
                                          QStringLiteral("Space"), true);
    assert(!recentBadge.isNull());
    assert(recentBadge.width() == qRound(recentW));
    assert(recentBadge.width() > qRound(standardW));

    // 2b. renderBadgeImage with recentKey + label + thumbnail
    cfg.windowShowThumbnail = true;
    QImage testThumb(56, 32, QImage::Format_ARGB32_Premultiplied);
    testThumb.fill(QColor(255, 0, 0));
    const QString testLabel = QStringLiteral("Editor");
    const qreal recentFullW = computeBadgeWidth(cfg, 34, testLabel, QStringLiteral("Space"));
    QImage fullBadge = renderBadgeImage(cfg, QLatin1Char('D'), QColor(52, 152, 219), Qt::white,
                                        true, 34, 1.0, testLabel, QIcon(), testThumb,
                                        QStringLiteral("Space"), true);
    assert(!fullBadge.isNull());
    assert(fullBadge.width() == qRound(recentFullW));
    assert(fullBadge.width() > recentBadge.width());

    // 3. renderOffscreenTargetPill with recentKey and isRecent
    QImage recentPill = renderOffscreenTargetPill(cfg, QLatin1Char('D'), QColor(52, 152, 219), Qt::white,
                                                  QStringLiteral("Pulpit 1"), QStringLiteral("Dolphin"),
                                                  36, 1.0, QString(), QIcon(), QImage(),
                                                  false, QStringLiteral("Space"), true);
    assert(!recentPill.isNull());
    assert(recentPill.width() > 100);
}

void testHudBadgeOcclusionAvoidance()
{
    using OutlineBadge::BadgeConfig;
    using OutlineBadge::computeBadgeRect;
    using OutlineBadge::computeBadgeWidth;
    using OutlineBadge::computeHudPillPositions;
    using OutlineBadge::computeHudPillSize;
    using OutlineBadge::renderOffscreenTargetPill;

    BadgeConfig cfg;
    cfg.hudPosition = 0; // Bottom
    cfg.hudBadgeSize = 36;

    // 1. Verify computeHudPillSize matches renderOffscreenTargetPill dimensions
    const QString loc = QStringLiteral("Desktop 1");
    const QString cap = QStringLiteral("Firefox Browser");
    const QSizeF pillSize = computeHudPillSize(cfg, loc, cap, 36, 1.0);
    const QImage renderedPill = renderOffscreenTargetPill(cfg, QLatin1Char('F'), QColor(231, 76, 60), Qt::white, loc, cap, 36, 1.0);
    assert(qRound(pillSize.width()) == renderedPill.width());
    assert(qRound(pillSize.height()) == renderedPill.height());

    // 2. HUD Pill positioning on a 1920x1080 screen
    const QRectF screenRect(0, 0, 1920, 1080);
    QList<QSizeF> hudPills = { pillSize, pillSize };
    QList<QRectF> hudRects = computeHudPillPositions(hudPills, screenRect, 0);
    assert(hudRects.size() == 2);
    // Both HUD pills are at the bottom of the screen
    for (const QRectF &hr : hudRects) {
        assert(hr.bottom() <= screenRect.bottom());
        assert(hr.top() >= 950.0); // comfortably near the bottom
    }

    // 3. A window positioned at the bottom of the screen overlapping HUD pills
    const QRectF windowGeo(400, 700, 1120, 380);
    const int badgeH = 34;
    const qreal badgeW = computeBadgeWidth(cfg, badgeH);

    // Initial position: Bottom (pos 7)
    QRectF bottomBadgeRect = computeBadgeRect(windowGeo, 7, badgeW, badgeH, 0);
    // bottomBadgeRect should overlap with at least one HUD pill at the bottom
    bool bottomOverlapsHud = false;
    for (const QRectF &hr : hudRects) {
        if (hr.intersects(bottomBadgeRect)) {
            bottomOverlapsHud = true;
            break;
        }
    }
    assert(bottomOverlapsHud);

    // Occlusion check & alternative position search:
    const int altPositions[] = { 4 /* Center */, 1 /* Top */, 0 /* TopLeft */, 2 /* TopRight */, 7 /* Bottom */, 6 /* BottomLeft */, 8 /* BottomRight */, 3 /* Left */, 5 /* Right */ };
    bool foundVisiblePosition = false;
    QRectF finalBadgeRect = bottomBadgeRect;

    for (int altPos : altPositions) {
        if (altPos == 7) continue; // skip initial position
        QRectF altRect = computeBadgeRect(windowGeo, altPos, badgeW, badgeH, 0);
        bool occludedByHud = false;
        for (const QRectF &hr : hudRects) {
            if (hr.intersects(altRect)) {
                occludedByHud = true;
                break;
            }
        }
        if (!occludedByHud) {
            finalBadgeRect = altRect;
            foundVisiblePosition = true;
            break;
        }
    }

    // Assert that an alternative unoccluded position was found (Center: pos 4)
    assert(foundVisiblePosition);
    assert(finalBadgeRect != bottomBadgeRect);
    // Verify final badge position does NOT intersect any HUD card
    for (const QRectF &hr : hudRects) {
        assert(!hr.intersects(finalBadgeRect));
    }
}

void testWindowBadgesEnabledConfig()
{
    OutlineBadge::BadgeConfig cfgDefault;
    assert(cfgDefault.windowBadgesEnabled == true);

    OutlineBadge::BadgeConfig cfgHudOnly;
    cfgHudOnly.windowBadgesEnabled = false;
    cfgHudOnly.hudIncludeAll = true;
    assert(cfgHudOnly.windowBadgesEnabled == false);
    assert(cfgHudOnly.hudIncludeAll == true);
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
    testAutoContrastTypography();
    testWindowPreviewOnWindowBadges();
    testHudMarkColorModes();
    testHudPositionsAndThumbnailLayout();
    testIndependentWindowAndHudBadges();
    testRecentBadgeAndIncludeAll();
    testHudBadgeOcclusionAvoidance();
    testWindowBadgesEnabledConfig();

    std::cout << "All badge tests passed successfully." << std::endl;
    return 0;
}
