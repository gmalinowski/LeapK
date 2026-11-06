/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "keyboardoverlayhelper.h"
#include <QGuiApplication>
#include <QSet>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testDistinctColorsPalette()
{
    using namespace LeapKKeyboardOverlay;

    const auto &palette = distinctArrowColors();
    assert(palette.size() == 26); // Exactly 26 distinct colors for all 26 alphabet keys

    QSet<QRgb> uniqueRgbs;
    for (const QColor &c : palette) {
        assert(c.isValid());
        assert(c.alpha() == 255);
        assert(!uniqueRgbs.contains(c.rgb()));
        uniqueRgbs.insert(c.rgb());
    }
    assert(uniqueRgbs.size() == 26);

    std::cout << "testDistinctColorsPalette PASSED\n";
}

void testOverlayGeometryComputation()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    // Panel is positioned inside screenRect, centered horizontally, near bottom
    assert(screenRect.contains(layout.panelRect));
    assert(qAbs(layout.panelRect.center().x() - screenRect.center().x()) < 1.0);
    assert(layout.panelRect.bottom() < screenRect.bottom());
    assert(layout.panelRect.top() > screenRect.top());

    // All 26 alphabet keys are present
    assert(layout.keycaps.size() == 26);
    assert(layout.keyMap.size() == 26);

    // Row counts: 10, 9, 7
    int row0 = 0, row1 = 0, row2 = 0;
    for (const auto &kg : layout.keycaps) {
        assert(layout.panelRect.contains(kg.rect));
        if (kg.row == 0) row0++;
        else if (kg.row == 1) row1++;
        else if (kg.row == 2) row2++;
    }
    assert(row0 == 10);
    assert(row1 == 9);
    assert(row2 == 7);

    // Specific key rows
    assert(layout.keyMap.value('Q').row == 0);
    assert(layout.keyMap.value('P').row == 0);
    assert(layout.keyMap.value('A').row == 1);
    assert(layout.keyMap.value('L').row == 1);
    assert(layout.keyMap.value('Z').row == 2);
    assert(layout.keyMap.value('M').row == 2);

    // Horizontal ordering in Row 0: Q < W < E < R < T < Y < U < I < O < P
    const QString row0Keys = QStringLiteral("QWERTYUIOP");
    for (int i = 0; i < row0Keys.length() - 1; ++i) {
        assert(layout.keyMap.value(row0Keys.at(i)).rect.right() < layout.keyMap.value(row0Keys.at(i + 1)).rect.left());
    }

    std::cout << "testOverlayGeometryComputation PASSED\n";
}

void testSpatialKeyAssignment()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);

    // Scenario: 4 windows in a 2x2 grid
    // Top-Left, Top-Right, Bottom-Left, Bottom-Right
    WindowTargetItem wTL;
    wTL.geometry = QRectF(50, 50, 800, 450);
    wTL.title = QStringLiteral("Editor");
    wTL.appClass = QStringLiteral("kate");

    WindowTargetItem wTR;
    wTR.geometry = QRectF(1050, 50, 800, 450);
    wTR.title = QStringLiteral("Browser");
    wTR.appClass = QStringLiteral("firefox");

    WindowTargetItem wBL;
    wBL.geometry = QRectF(50, 550, 800, 450);
    wBL.title = QStringLiteral("Terminal");
    wBL.appClass = QStringLiteral("konsole");

    WindowTargetItem wBR;
    wBR.geometry = QRectF(1050, 550, 800, 450);
    wBR.title = QStringLiteral("Files");
    wBR.appClass = QStringLiteral("dolphin");

    const auto assigned = assignSpatialKeys({wTL, wTR, wBL, wBR}, screenRect);
    assert(assigned.size() == 4);

    // All assigned windows must have unique keys and unique colors
    QSet<QChar> keys;
    QSet<QRgb> colors;
    for (auto it = assigned.constBegin(); it != assigned.constEnd(); ++it) {
        keys.insert(it.key());
        colors.insert(it.value().customColor.rgb());
    }
    assert(keys.size() == 4);
    assert(colors.size() == 4); // Every arrow has a different color!

    // Verify spatial intuition:
    // wTL should get a top-left key (e.g. Q, W, E, R, or A)
    // wTR should get a top-right key (e.g. U, I, O, P, or L)
    // wBL should get a bottom/left key (e.g. Z, X, C, or S)
    // wBR should get a bottom/right key (e.g. B, N, M, or K)
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);
    for (auto it = assigned.constBegin(); it != assigned.constEnd(); ++it) {
        const QChar key = it.key();
        const auto &kg = layout.keyMap.value(key);
        if (it.value().appClass == QStringLiteral("kate")) { // Top-Left
            assert(kg.rect.center().x() < layout.panelRect.center().x());
            assert(kg.row <= 1);
        } else if (it.value().appClass == QStringLiteral("firefox")) { // Top-Right
            assert(kg.rect.center().x() > layout.panelRect.center().x());
            assert(kg.row <= 1);
        }
    }

    std::cout << "testSpatialKeyAssignment PASSED\n";
}

void testPreferredKeyPreservation()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);

    WindowTargetItem w1;
    w1.geometry = QRectF(100, 100, 600, 400);
    w1.preferredKey = QChar('F'); // User marked Firefox as 'F'
    w1.title = QStringLiteral("Firefox");
    w1.appClass = QStringLiteral("firefox");

    WindowTargetItem w2;
    w2.geometry = QRectF(800, 100, 600, 400);
    w2.title = QStringLiteral("Kate");
    w2.appClass = QStringLiteral("kate");

    const auto assigned = assignSpatialKeys({w1, w2}, screenRect);
    assert(assigned.size() == 2);
    assert(assigned.contains(QChar('F')));
    assert(assigned.value(QChar('F')).appClass == QStringLiteral("firefox"));

    // Kate must not steal 'F'
    for (auto it = assigned.constBegin(); it != assigned.constEnd(); ++it) {
        if (it.value().appClass == QStringLiteral("kate")) {
            assert(it.key() != QChar('F'));
        }
    }

    std::cout << "testPreferredKeyPreservation PASSED\n";
}

void testOverlayImageRendering()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    WindowTargetItem w1;
    w1.geometry = QRectF(100, 100, 600, 400);
    w1.title = QStringLiteral("Firefox");
    w1.appClass = QStringLiteral("firefox");

    WindowTargetItem w2;
    w2.geometry = QRectF(1100, 100, 600, 400);
    w2.title = QStringLiteral("Terminal");
    w2.appClass = QStringLiteral("konsole");

    const auto assigned = assignSpatialKeys({w1, w2}, screenRect);

    // 1x scale rendering
    QImage img1x = renderOverlayImage(layout, assigned, screenRect, 1.0);
    assert(!img1x.isNull());
    assert(img1x.width() == 1920);
    assert(img1x.height() == 1080);

    // 2x HiDPI scale rendering
    QImage img2x = renderOverlayImage(layout, assigned, screenRect, 2.0);
    assert(!img2x.isNull());
    assert(img2x.width() == 3840);
    assert(img2x.height() == 2160);

    std::cout << "testOverlayImageRendering PASSED\n";
}

void testOccludedTargetBadgePlacements()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    // Three windows stacked on top of each other at roughly the same geometry
    WindowTargetItem bottomWin;
    bottomWin.geometry = QRectF(300, 200, 600, 400);
    bottomWin.zOrder = 2; // lowest z-order (deepest underneath)
    bottomWin.isOccluded = true;
    bottomWin.appClass = QStringLiteral("bottom");

    WindowTargetItem middleWin;
    middleWin.geometry = QRectF(310, 205, 600, 400);
    middleWin.zOrder = 7; // middle z-order
    middleWin.isOccluded = true;
    middleWin.appClass = QStringLiteral("middle");

    WindowTargetItem topWin;
    topWin.geometry = QRectF(320, 210, 600, 400);
    topWin.zOrder = 18; // highest z-order (topmost on surface)
    topWin.isOccluded = false;
    topWin.appClass = QStringLiteral("top");

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('A'), bottomWin);
    assigned.insert(QChar('S'), middleWin);
    assigned.insert(QChar('D'), topWin);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout);
    assert(placements.size() == 3);

    const TargetBadgePlacement &pBottom = placements.value(QChar('A'));
    const TargetBadgePlacement &pMiddle = placements.value(QChar('S'));
    const TargetBadgePlacement &pTop = placements.value(QChar('D'));

    // Requirement 3:
    // 1. okno najbardziej pod spodem po lewej (bottommost on the far left)
    // 2. okno na wierzchu po prawej (topmost on the far right)
    assert(pBottom.center.x() < pMiddle.center.x());
    assert(pMiddle.center.x() < pTop.center.x());

    // 3. odpowiednio w zaleznosci od poziomu wiekszy/mniejszy badge (bottommost smallest, topmost largest)
    assert(pBottom.radius < pMiddle.radius);
    assert(pMiddle.radius < pTop.radius);
    assert(pBottom.radius == 12.0);
    assert(pTop.radius == 21.0);

    // 4. byc rysowany obok a nie sie nakladac (non-overlapping with gap)
    assert((pMiddle.center.x() - pBottom.center.x()) >= (pBottom.radius + pMiddle.radius + 7.5));
    assert((pTop.center.x() - pMiddle.center.x()) >= (pMiddle.radius + pTop.radius + 7.5));

    // Also verify rendering handles stacked badges cleanly
    QImage img = renderOverlayImage(layout, assigned, screenRect, 1.0);
    assert(!img.isNull());

    std::cout << "testOccludedTargetBadgePlacements PASSED\n";
}

void testSpatialKeyboardCustomDisplayAndExtensions()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);

    OverlayDisplayConfig cfg;
    cfg.keySize = 64.0;
    cfg.keyFontSize = 16;
    cfg.lineWidth = 4.0;
    cfg.badgeRadius = 24.0;
    cfg.badgeFontSize = 15;
    cfg.extendToHud = true;

    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect, QRectF(), cfg);
    // Keycaps width should match cfg.keySize
    assert(qAbs(layout.keycaps.first().rect.width() - 64.0) < 0.1);

    WindowTargetItem wFocused;
    wFocused.geometry = QRectF(100, 100, 400, 300);
    wFocused.title = QStringLiteral("Active Window");
    wFocused.isCurrentFocused = true; // Currently focused window!

    WindowTargetItem wOther;
    wOther.geometry = QRectF(600, 100, 400, 300);
    wOther.title = QStringLiteral("Other Window");
    wOther.isCurrentFocused = false;
    wOther.hudCardRect = QRectF(650, 950, 120, 80);

    WindowTargetItem wOffscreen;
    wOffscreen.geometry = QRectF(2500, 100, 400, 300); // Offscreen!
    wOffscreen.title = QStringLiteral("Offscreen Window");
    wOffscreen.isVisibleOnScreen = false;
    wOffscreen.hudCardRect = QRectF(800, 950, 120, 80);

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('A'), wFocused);
    assigned.insert(QChar('S'), wOther);
    assigned.insert(QChar('D'), wOffscreen);

    // Compute placements
    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout, cfg);
    assert(placements.contains(QChar('S')));
    assert(placements.value(QChar('S')).radius == 24.0); // matches cfg.badgeRadius

    // Render with an invalid key trigger
    const QImage img = renderOverlayImage(layout, assigned, screenRect, 1.0, cfg, QChar('Z'));
    assert(!img.isNull());
    assert(img.width() == 1920);
    assert(img.height() == 1080);

    std::cout << "testSpatialKeyboardCustomDisplayAndExtensions PASSED\n";
}

void testUntanglingLaserLines()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    // Scenario 1: Two windows, one on far left, one on far right
    WindowTargetItem wLeft;
    wLeft.geometry = QRectF(100, 200, 400, 300);
    wLeft.appClass = QStringLiteral("kate");

    WindowTargetItem wRight;
    wRight.geometry = QRectF(1400, 200, 400, 300);
    wRight.appClass = QStringLiteral("firefox");

    const auto assigned2 = assignSpatialKeys({wLeft, wRight}, screenRect);
    assert(assigned2.size() == 2);

    QChar keyLeft, keyRight;
    for (auto it = assigned2.constBegin(); it != assigned2.constEnd(); ++it) {
        if (it.value().appClass == QStringLiteral("kate")) keyLeft = it.key();
        else if (it.value().appClass == QStringLiteral("firefox")) keyRight = it.key();
    }
    assert(!keyLeft.isNull() && !keyRight.isNull());

    const QPointF pKeyLeft(layout.keyMap.value(keyLeft).rect.center().x(), layout.keyMap.value(keyLeft).rect.top() + 3.0);
    const QPointF pKeyRight(layout.keyMap.value(keyRight).rect.center().x(), layout.keyMap.value(keyRight).rect.top() + 3.0);
    const QPointF pTargetLeft = wLeft.geometry.center();
    const QPointF pTargetRight = wRight.geometry.center();

    // The key on the left must be assigned to the window on the left!
    assert(pKeyLeft.x() < pKeyRight.x());

    // Laser lines must NOT intersect!
    assert(!segmentsIntersect(pKeyLeft, pTargetLeft, pKeyRight, pTargetRight));

    // Scenario 2: 4 windows in a 2x2 grid
    WindowTargetItem wTL; wTL.geometry = QRectF(80, 80, 500, 350); wTL.appClass = QStringLiteral("tl");
    WindowTargetItem wTR; wTR.geometry = QRectF(1300, 80, 500, 350); wTR.appClass = QStringLiteral("tr");
    WindowTargetItem wBL; wBL.geometry = QRectF(80, 500, 500, 350); wBL.appClass = QStringLiteral("bl");
    WindowTargetItem wBR; wBR.geometry = QRectF(1300, 500, 500, 350); wBR.appClass = QStringLiteral("br");

    const auto assigned4 = assignSpatialKeys({wTL, wTR, wBL, wBR}, screenRect);
    assert(assigned4.size() == 4);

    QList<QPair<QPointF, QPointF>> segments;
    for (auto it = assigned4.constBegin(); it != assigned4.constEnd(); ++it) {
        const QPointF pStart(layout.keyMap.value(it.key()).rect.center().x(), layout.keyMap.value(it.key()).rect.top() + 3.0);
        const QPointF pEnd = it.value().geometry.center();
        segments.append({pStart, pEnd});
    }

    // Verify pairwise non-crossing between all 4 laser lines!
    for (int i = 0; i < segments.size(); ++i) {
        for (int j = i + 1; j < segments.size(); ++j) {
            assert(!segmentsIntersect(segments[i].first, segments[i].second, segments[j].first, segments[j].second));
        }
    }

    std::cout << "testUntanglingLaserLines PASSED\n";
}

void testLabelAndStackTheming()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);

    OverlayDisplayConfig cfg;
    cfg.fontFamily = QStringLiteral("monospace");
    cfg.targetLabelFontSize = 11;
    cfg.stackScaleReduction = 0.50; // 50% scale reduction
    cfg.stackOpacityReduction = 0.60; // 60% opacity reduction
    cfg.targetBadgeShape = QStringLiteral("chamfered");

    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect, QRectF(), cfg);

    WindowTargetItem wStackedUnder;
    wStackedUnder.geometry = QRectF(400, 300, 500, 350);
    wStackedUnder.zOrder = 1;
    wStackedUnder.isOccluded = true;
    wStackedUnder.customLabel = QStringLiteral("Full Custom Window Title");

    WindowTargetItem wStackedTop;
    wStackedTop.geometry = QRectF(410, 305, 500, 350);
    wStackedTop.zOrder = 10;
    wStackedTop.isOccluded = false;
    wStackedTop.appClass = QStringLiteral("systemsettings");

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('A'), wStackedUnder);
    assigned.insert(QChar('B'), wStackedTop);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout, cfg);
    assert(placements.size() == 2);

    const auto &pUnder = placements.value(QChar('A'));
    const auto &pTop = placements.value(QChar('B'));

    // Check stack scale reduction and opacity reduction
    assert(pUnder.radius < pTop.radius);
    assert(pUnder.opacity < pTop.opacity);
    assert(std::abs(pUnder.opacity - 0.40) < 0.05); // 1.0 - 0.60 = 0.40
    assert(std::abs(pTop.opacity - 1.0) < 0.001);

    // Test all badge shapes
    const QString shapes[] = {
        QStringLiteral("circle"), QStringLiteral("rounded_rect"),
        QStringLiteral("pill"), QStringLiteral("sharp"),
        QStringLiteral("chamfered"), QStringLiteral("ring")
    };

    for (const auto &sh : shapes) {
        cfg.targetBadgeShape = sh;
        const QImage img = renderOverlayImage(layout, assigned, screenRect, 1.0, cfg);
        assert(!img.isNull());
    }

    std::cout << "testLabelAndStackTheming PASSED\n";
}

void testSpatialKeyGroupBypass()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);

    // Two windows with the exact same preferredKey (e.g. from an app group rule for "Terminal" -> 'T')
    WindowTargetItem wTerm1;
    wTerm1.geometry = QRectF(100, 200, 600, 400);
    wTerm1.preferredKey = QChar('T');
    wTerm1.title = QStringLiteral("Terminal 1");
    wTerm1.appClass = QStringLiteral("konsole");

    WindowTargetItem wTerm2;
    wTerm2.geometry = QRectF(900, 200, 600, 400);
    wTerm2.preferredKey = QChar('T');
    wTerm2.title = QStringLiteral("Terminal 2");
    wTerm2.appClass = QStringLiteral("konsole");

    const auto assigned = assignSpatialKeys({wTerm1, wTerm2}, screenRect);
    assert(assigned.size() == 2);

    // One window keeps 'T', the second gets a unique spatial key
    assert(assigned.contains(QChar('T')));
    QChar key1 = QChar('T');
    QChar key2;
    for (auto it = assigned.constBegin(); it != assigned.constEnd(); ++it) {
        if (it.key() != key1) {
            key2 = it.key();
            break;
        }
    }
    assert(!key2.isNull());
    assert(key1 != key2);

    // Ensure both windows are present and unique
    const auto &item1 = assigned.value(key1);
    const auto &item2 = assigned.value(key2);
    assert(item1.title != item2.title);

    std::cout << "testSpatialKeyGroupBypass PASSED\n";
}

void testTilingAndBackgroundBadgeSeparation()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    // Tiled terminal in left slot
    WindowTargetItem wTile;
    wTile.geometry = QRectF(0, 0, 960, 1080);
    wTile.title = QStringLiteral("Terminal 2 (Tiled)");
    wTile.appClass = QStringLiteral("konsole");
    wTile.zOrder = 5;

    // Background terminal floating / partially underneath
    WindowTargetItem wBackground;
    wBackground.geometry = QRectF(400, 200, 1000, 700);
    wBackground.title = QStringLiteral("Terminal 1 (Background)");
    wBackground.appClass = QStringLiteral("konsole");
    wBackground.zOrder = 2;
    wBackground.isOccluded = true;

    // Another focused window in right slot
    WindowTargetItem wFocused;
    wFocused.geometry = QRectF(960, 0, 960, 1080);
    wFocused.title = QStringLiteral("Editor");
    wFocused.appClass = QStringLiteral("kate");
    wFocused.zOrder = 10;
    wFocused.isCurrentFocused = true;

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('L'), wTile);
    assigned.insert(QChar('S'), wBackground);
    assigned.insert(QChar('E'), wFocused);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout);

    // Both non-focused windows must receive their own badges
    assert(placements.contains(QChar('L')));
    assert(placements.contains(QChar('S')));

    const auto &pL = placements.value(QChar('L'));
    const auto &pS = placements.value(QChar('S'));

    // They must NOT be glued together into a multi-window stack cluster!
    assert(pL.stackTotal == 1);
    assert(pS.stackTotal == 1);

    // Each badge must be placed on its own window!
    assert(std::abs(pL.center.x() - wTile.geometry.center().x()) < 50.0);
    assert(std::abs(pS.center.x() - wBackground.geometry.center().x()) < 50.0);

    // Distance between them should reflect their distinct spatial window positions (> 300px)
    const qreal dist = std::hypot(pL.center.x() - pS.center.x(), pL.center.y() - pS.center.y());
    assert(dist > 300.0);

    std::cout << "testTilingAndBackgroundBadgeSeparation PASSED\n";
}

void testLaserTargetsMatchingHudBadgeOrder()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    OverlayDisplayConfig cfg;
    cfg.badgeRadius = 18.0;
    cfg.stackScaleReduction = 0.35;
    cfg.stackOpacityReduction = 0.40;
    cfg.extendToHud = true;

    // Two stacked terminals at identical coordinates
    WindowTargetItem wTermUnder;
    wTermUnder.geometry = QRectF(400, 200, 600, 450);
    wTermUnder.title = QStringLiteral("Terminal 1 (Underneath)");
    wTermUnder.appClass = QStringLiteral("konsole");
    wTermUnder.zOrder = 2;
    wTermUnder.isOccluded = true;
    wTermUnder.isVisibleOnScreen = true;
    // In HUD, TermUnder is on the RIGHT dock slot (e.g. x = 1200)
    wTermUnder.hudCardRect = QRectF(1200, 950, 120, 60);

    WindowTargetItem wTermTop;
    wTermTop.geometry = QRectF(400, 200, 600, 450);
    wTermTop.title = QStringLiteral("Terminal 2 (Topmost)");
    wTermTop.appClass = QStringLiteral("konsole");
    wTermTop.zOrder = 10;
    wTermTop.isOccluded = false;
    wTermTop.isVisibleOnScreen = true;
    // In HUD, TermTop is on the LEFT dock slot (e.g. x = 400)
    wTermTop.hudCardRect = QRectF(400, 950, 120, 60);

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('A'), wTermUnder);
    assigned.insert(QChar('S'), wTermTop);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout, cfg);
    assert(placements.size() == 2);

    const auto &pUnder = placements.value(QChar('A'));
    const auto &pTop = placements.value(QChar('S'));

    // Requirement: targets must be sorted to match the layout of badges in HUD!
    // Since TermTop has HUD card on the left (x=400) and TermUnder has HUD card on the right (x=1200),
    // TermTop's target badge on screen MUST be positioned to the left of TermUnder's target badge!
    assert(pTop.center.x() < pUnder.center.x());

    // Laser extension lines between target badges and HUD cards MUST NOT CROSS!
    const QPointF pHudTop = wTermTop.hudCardRect.center();
    const QPointF pHudUnder = wTermUnder.hudCardRect.center();
    assert(!segmentsIntersect(pTop.center, pHudTop, pUnder.center, pHudUnder));

    // Depth status must be preserved: topmost has larger radius and opacity, underneath has smaller
    assert(pTop.radius > pUnder.radius);
    assert(pTop.opacity > pUnder.opacity);
    assert(pTop.stackLevel == 1);
    assert(pUnder.stackLevel == 0);

    // Inverted case: TermUnder has HUD card on left (x=300), TermTop has HUD card on right (x=1100)
    wTermUnder.hudCardRect = QRectF(300, 950, 120, 60);
    wTermTop.hudCardRect = QRectF(1100, 950, 120, 60);
    assigned[QChar('A')] = wTermUnder;
    assigned[QChar('S')] = wTermTop;

    const auto placementsInverted = computeTargetBadgePlacements(assigned, screenRect, layout, cfg);
    const auto &pUnderInv = placementsInverted.value(QChar('A'));
    const auto &pTopInv = placementsInverted.value(QChar('S'));

    // TermUnder's badge is placed on the left, TermTop's badge on the right
    assert(pUnderInv.center.x() < pTopInv.center.x());
    assert(!segmentsIntersect(pUnderInv.center, wTermUnder.hudCardRect.center(),
                              pTopInv.center, wTermTop.hudCardRect.center()));

    std::cout << "testLaserTargetsMatchingHudBadgeOrder PASSED\n";
}

void testTargetLabelsCollisionAvoidanceAndOpacity()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);

    OverlayDisplayConfig cfg;
    cfg.targetLabelVisible = true;
    cfg.targetLabelFontSize = 10;
    cfg.targetLabelBg = QColor(20, 24, 34);

    // Two stacked terminals with long titles in a cluster
    WindowTargetItem wTerm1;
    wTerm1.geometry = QRectF(500, 300, 600, 450);
    wTerm1.title = QStringLiteral("Terminal 1 — Active Working Directory Project Alpha");
    wTerm1.appClass = QStringLiteral("konsole");
    wTerm1.zOrder = 2;
    wTerm1.isOccluded = true;
    wTerm1.isVisibleOnScreen = true;

    WindowTargetItem wTerm2;
    wTerm2.geometry = QRectF(500, 300, 600, 450);
    wTerm2.title = QStringLiteral("Terminal 2 — System Logs and Compiler Diagnostics");
    wTerm2.appClass = QStringLiteral("konsole");
    wTerm2.zOrder = 10;
    wTerm2.isOccluded = false;
    wTerm2.isVisibleOnScreen = true;

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('A'), wTerm1);
    assigned.insert(QChar('S'), wTerm2);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout, cfg);
    assert(placements.size() == 2);

    const auto labels = computeTargetLabelPlacements(placements, screenRect, layout, cfg);
    assert(labels.size() == 2);

    const auto &lbl1 = labels[0];
    const auto &lbl2 = labels[1];

    // Requirement 1: labels must avoid overlapping each other
    assert(!lbl1.rect.intersects(lbl2.rect));

    // Requirement 2: labels must have NO transparency (alpha = 255) for crisp, clear readability
    assert(lbl1.bgColor.alpha() == 255);
    assert(lbl2.bgColor.alpha() == 255);
    assert(lbl1.textColor.alpha() == 255);
    assert(lbl2.textColor.alpha() == 255);
    assert(lbl1.borderColor.alpha() == 255);
    assert(lbl2.borderColor.alpha() == 255);

    // Verify neither label obscures any target badge's key letter circle
    for (const auto &bp : placements) {
        const QRectF bCircle(bp.center.x() - bp.radius, bp.center.y() - bp.radius, bp.radius * 2.0, bp.radius * 2.0);
        assert(!lbl1.rect.intersects(bCircle));
        assert(!lbl2.rect.intersects(bCircle));
    }

    std::cout << "testTargetLabelsCollisionAvoidanceAndOpacity PASSED\n";
}

void testKeyboardOverlayTilingOnly()
{
    using namespace LeapKKeyboardOverlay;

    // Test 1: When disabled completely, isKeyboardOverlayActive is false regardless of tiling
    assert(!isKeyboardOverlayActive(/*enabled*/ false, /*tilingOnly*/ false, /*tilingActive*/ false));
    assert(!isKeyboardOverlayActive(/*enabled*/ false, /*tilingOnly*/ false, /*tilingActive*/ true));
    assert(!isKeyboardOverlayActive(/*enabled*/ false, /*tilingOnly*/ true,  /*tilingActive*/ false));
    assert(!isKeyboardOverlayActive(/*enabled*/ false, /*tilingOnly*/ true,  /*tilingActive*/ true));

    // Test 2: When enabled and tilingOnly is false, it is active in both floating and tiling modes
    assert(isKeyboardOverlayActive(/*enabled*/ true, /*tilingOnly*/ false, /*tilingActive*/ false));
    assert(isKeyboardOverlayActive(/*enabled*/ true, /*tilingOnly*/ false, /*tilingActive*/ true));

    // Test 3: When enabled and tilingOnly is true:
    // - inactive when tiling is false (floating mode)
    // - active when tiling is true (tiling mode)
    assert(!isKeyboardOverlayActive(/*enabled*/ true, /*tilingOnly*/ true, /*tilingActive*/ false));
    assert(isKeyboardOverlayActive(/*enabled*/ true, /*tilingOnly*/ true, /*tilingActive*/ true));

    // Test 4: OverlayDisplayConfig defaults
    OverlayDisplayConfig cfg;
    assert(cfg.enabled == true);
    assert(cfg.tilingOnly == false);
    assert(cfg.targetLabelVisible == false);
    assert(cfg.keySize == 64.0);

    std::cout << "testKeyboardOverlayTilingOnly PASSED\n";
}

void testLaserTargetsAvoidHudBadges()
{
    using namespace LeapKKeyboardOverlay;

    const QRectF screenRect(0, 0, 1920, 1080);
    const QRectF hudBounds(300, 960, 1320, 110);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect, hudBounds);
    assert(layout.hudBounds == hudBounds);

    OverlayDisplayConfig cfg;
    cfg.targetLabelVisible = true;

    // Case 1: An offscreen window whose HUD card is located in the HUD dock
    WindowTargetItem wOffscreen;
    wOffscreen.geometry = QRectF(2500, 100, 800, 600); // off-screen
    wOffscreen.isVisibleOnScreen = false;
    wOffscreen.hudCardRect = QRectF(900, 970, 120, 80);
    wOffscreen.title = QStringLiteral("Offscreen Dolphin");
    wOffscreen.appClass = QStringLiteral("dolphin");

    // Case 2: An on-screen window near the bottom whose geometry or placement would overlap HUD
    WindowTargetItem wOnScreen;
    wOnScreen.geometry = QRectF(400, 800, 600, 250);
    wOnScreen.isVisibleOnScreen = true;
    wOnScreen.hudCardRect = QRectF(450, 970, 120, 80);
    wOnScreen.title = QStringLiteral("Terminal at Bottom");
    wOnScreen.appClass = QStringLiteral("konsole");

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('D'), wOffscreen);
    assigned.insert(QChar('K'), wOnScreen);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout, cfg);
    assert(placements.size() == 2);

    // Verify on-screen target badge does not intersect HUD bounds or HUD card
    const auto &pOn = placements.value(QChar('K'));
    const QRectF badgeRectOn(pOn.center.x() - pOn.radius, pOn.center.y() - pOn.radius,
                             pOn.radius * 2.0, pOn.radius * 2.0);
    assert(!badgeRectOn.intersects(layout.hudBounds));
    assert(!badgeRectOn.intersects(wOnScreen.hudCardRect));
    assert(pOn.center.y() + pOn.radius <= layout.hudBounds.top() + 1.0);

    // Verify computeTargetLabelPlacements keeps labels outside HUD bounds and HUD cards
    const auto labels = computeTargetLabelPlacements(placements, screenRect, layout, cfg);
    for (const auto &lbl : labels) {
        assert(!lbl.rect.intersects(layout.hudBounds));
        assert(!lbl.rect.intersects(wOffscreen.hudCardRect));
        assert(!lbl.rect.intersects(wOnScreen.hudCardRect));
    }

    // Verify rendering produces valid overlay without crash
    const QImage img = renderOverlayImage(layout, assigned, screenRect, 1.0, cfg);
    assert(!img.isNull());
    assert(img.size() == screenRect.size().toSize());

    std::cout << "testLaserTargetsAvoidHudBadges PASSED\n";
}

void testKeycapMiddleElisionAndTargetLabelsDisabled()
{
    using namespace LeapKKeyboardOverlay;

    // 1. Verify default config has targetLabelVisible = false
    OverlayDisplayConfig defaultCfg;
    assert(!defaultCfg.targetLabelVisible);
    assert(defaultCfg.keySize == 64.0);

    // 2. Test elideMiddleTitle mathematics
    QFont font(QStringLiteral("sans-serif"), 8);
    QFontMetrics fm(font);

    // Short titles that fit within available width must remain 100% whole
    const QString short1 = QStringLiteral("Dolphin");
    const QString short2 = QStringLiteral("Kate");
    const qreal availW = 60.0;
    if (fm.horizontalAdvance(short1) <= availW) {
        assert(elideMiddleTitle(short1, fm, availW) == short1);
    }
    if (fm.horizontalAdvance(short2) <= availW) {
        assert(elideMiddleTitle(short2, fm, availW) == short2);
    }

    // Long titles must be middle-elided with ellipsis (e.g. "Terminal (bash) - neovim" => "Termin...neovim")
    const QString longTitle = QStringLiteral("Terminal (bash) - neovim");
    const qreal wideAvailW = 100.0;
    const QString elidedWide = elideMiddleTitle(longTitle, fm, wideAvailW);
    assert(!elidedWide.isEmpty());
    assert(elidedWide.length() < longTitle.length());
    assert(fm.horizontalAdvance(elidedWide) <= wideAvailW + 1.0);
    assert(elidedWide.startsWith(QStringLiteral("Term")));
    assert(elidedWide.endsWith(QStringLiteral("vim")));
    assert(elidedWide.contains(QStringLiteral("…")) || elidedWide.contains(QStringLiteral("...")));

    const qreal tightAvailW = 50.0;
    const QString elidedTight = elideMiddleTitle(longTitle, fm, tightAvailW);
    assert(!elidedTight.isEmpty());
    assert(elidedTight.length() < elidedWide.length());
    assert(fm.horizontalAdvance(elidedTight) <= tightAvailW + 1.0);
    assert(elidedTight.contains(QStringLiteral("…")) || elidedTight.contains(QStringLiteral("...")));

    // 3. Verify target labels are omitted by default
    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect, QRectF(), defaultCfg);

    WindowTargetItem w1;
    w1.geometry = QRectF(100, 100, 600, 400);
    w1.title = longTitle;
    w1.appClass = QStringLiteral("konsole");

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('T'), w1);

    const auto placements = computeTargetBadgePlacements(assigned, screenRect, layout, defaultCfg);
    assert(placements.size() == 1);

    // When targetLabelVisible is false, computeTargetLabelPlacements returns empty list
    const auto labels = computeTargetLabelPlacements(placements, screenRect, layout, defaultCfg);
    assert(labels.isEmpty());

    // 4. Render overlay with the middle-elided keycap label
    const QImage img = renderOverlayImage(layout, assigned, screenRect, 1.0, defaultCfg);
    assert(!img.isNull());

    std::cout << "testKeycapMiddleElisionAndTargetLabelsDisabled PASSED\n";
}

void testLaserTargetsShortestPathToHudBadges()
{
    using namespace LeapKKeyboardOverlay;

    // 1. Direct mathematical verification of closestPointOnRoundedRectPerimeter
    const QRectF hudRect(500, 950, 200, 60);
    const QPointF hudCenter = hudRect.center(); // (600, 980)

    // Point A: Located directly above the HUD badge, offset from horizontal center
    const QPointF ptAbove(540, 400);
    const QPointF contactAbove = closestPointOnRoundedRectPerimeter(ptAbove, hudRect, 8.0, 2.0);
    // Must connect to top edge at ptAbove.x(), NOT at center.x()!
    assert(std::abs(contactAbove.x() - 540.0) < 0.01);
    assert(std::abs(contactAbove.y() - (hudRect.top() - 2.0)) < 0.01);
    assert(contactAbove.x() != hudCenter.x());
    // Distance to contact point must be strictly shorter than distance to HUD center
    const qreal distToContactA = QLineF(ptAbove, contactAbove).length();
    const qreal distToCenterA = QLineF(ptAbove, hudCenter).length();
    assert(distToContactA < distToCenterA - 20.0);

    // Point B: Located directly to the left of the HUD badge
    const QPointF ptLeft(200, 970);
    const QPointF contactLeft = closestPointOnRoundedRectPerimeter(ptLeft, hudRect, 8.0, 2.0);
    assert(std::abs(contactLeft.x() - (hudRect.left() - 2.0)) < 0.01);
    assert(std::abs(contactLeft.y() - 970.0) < 0.01);
    assert(contactLeft.y() != hudCenter.y());
    const qreal distToContactB = QLineF(ptLeft, contactLeft).length();
    const qreal distToCenterB = QLineF(ptLeft, hudCenter).length();
    assert(distToContactB < distToCenterB - 90.0);

    // Point C: Located diagonally off top-left corner
    const QPointF ptDiag(400, 850);
    const QPointF contactDiag = closestPointOnRoundedRectPerimeter(ptDiag, hudRect, 8.0, 2.0);
    assert(contactDiag.x() <= hudRect.left() + 8.0);
    assert(contactDiag.y() <= hudRect.top() + 8.0);
    const qreal distToContactC = QLineF(ptDiag, contactDiag).length();
    const qreal distToCenterC = QLineF(ptDiag, hudCenter).length();
    assert(distToContactC < distToCenterC);

    // 2. Integration with renderOverlayImage for both on-screen (extension) and offscreen (direct) targets
    const QRectF screenRect(0, 0, 1920, 1080);
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);
    OverlayDisplayConfig cfg;
    cfg.extendToHud = true;

    WindowTargetItem wOnscreen;
    wOnscreen.geometry = QRectF(200, 200, 500, 400);
    wOnscreen.title = QStringLiteral("Terminal (Onscreen)");
    wOnscreen.isVisibleOnScreen = true;
    wOnscreen.hudCardRect = QRectF(500, 980, 180, 50);

    WindowTargetItem wOffscreen;
    wOffscreen.geometry = QRectF(2500, 100, 500, 400); // Offscreen
    wOffscreen.title = QStringLiteral("Dolphin (Offscreen)");
    wOffscreen.isVisibleOnScreen = false;
    wOffscreen.hudCardRect = QRectF(1100, 980, 180, 50);

    QMap<QChar, WindowTargetItem> assigned;
    assigned.insert(QChar('A'), wOnscreen);
    assigned.insert(QChar('S'), wOffscreen);

    const QImage img = renderOverlayImage(layout, assigned, screenRect, 1.0, cfg);
    assert(!img.isNull());
    assert(img.width() == 1920 && img.height() == 1080);

    std::cout << "testLaserTargetsShortestPathToHudBadges PASSED\n";
}

void testFixedLetterColorsFeature()
{
    using namespace LeapKKeyboardOverlay;

    // 1. Verify deterministic 26-color mapping for letters A-Z
    QSet<QRgb> seenRgbs;
    for (char c = 'A'; c <= 'Z'; ++c) {
        const QChar upperCh = QLatin1Char(c);
        const QChar lowerCh = QLatin1Char(c + 32);

        const QColor colUpper = colorForLetter(upperCh);
        const QColor colLower = colorForLetter(lowerCh);

        assert(colUpper.isValid());
        assert(colUpper.alpha() == 255);
        assert(colUpper == colLower); // Case-insensitive mapping
        assert(!seenRgbs.contains(colUpper.rgb())); // Unique color per letter
        seenRgbs.insert(colUpper.rgb());
    }
    assert(seenRgbs.size() == 26);

    // Fallback for non-letter characters
    assert(colorForLetter(QLatin1Char('1')) == QColor(0, 229, 255));
    assert(colorForLetter(QLatin1Char('?')) == QColor(0, 229, 255));

    // 2. Spatial assignment assigns fixed letter colors to unmarked windows
    const QRectF screenRect(0, 0, 1920, 1080);
    WindowTargetItem w1;
    w1.geometry = QRectF(100, 100, 400, 300);
    w1.title = QStringLiteral("Window 1");

    WindowTargetItem w2;
    w2.geometry = QRectF(1200, 100, 400, 300);
    w2.title = QStringLiteral("Window 2");

    // Marked window with an explicit custom color
    WindowTargetItem wMarked;
    wMarked.geometry = QRectF(600, 500, 400, 300);
    wMarked.title = QStringLiteral("Marked Window");
    const QColor markColor(QStringLiteral("#FF0055"));
    wMarked.customColor = markColor;

    const auto assigned = assignSpatialKeys({w1, w2, wMarked}, screenRect, /*fixedLetterColors*/ true);
    assert(assigned.size() == 3);

    for (auto it = assigned.constBegin(); it != assigned.constEnd(); ++it) {
        const QChar key = it.key();
        const WindowTargetItem &item = it.value();
        if (item.title == QStringLiteral("Marked Window")) {
            // Marked window color MUST take precedence over letter color
            assert(item.customColor == markColor);
        } else {
            // Unmarked window MUST receive deterministic letter color
            assert(item.customColor == colorForLetter(key));
        }
    }

    // 3. Verify OverlayDisplayConfig fixedLetterColors toggle and rendering
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);
    OverlayDisplayConfig cfgEnabled;
    cfgEnabled.fixedLetterColors = true;
    const QImage imgEnabled = renderOverlayImage(layout, assigned, screenRect, 1.0, cfgEnabled);
    assert(!imgEnabled.isNull());

    OverlayDisplayConfig cfgDisabled;
    cfgDisabled.fixedLetterColors = false;
    const QImage imgDisabled = renderOverlayImage(layout, assigned, screenRect, 1.0, cfgDisabled);
    assert(!imgDisabled.isNull());

    std::cout << "testFixedLetterColorsFeature PASSED\n";
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    testDistinctColorsPalette();
    testOverlayGeometryComputation();
    testSpatialKeyAssignment();
    testPreferredKeyPreservation();
    testOccludedTargetBadgePlacements();
    testOverlayImageRendering();
    testSpatialKeyboardCustomDisplayAndExtensions();
    testUntanglingLaserLines();
    testLabelAndStackTheming();
    testSpatialKeyGroupBypass();
    testTilingAndBackgroundBadgeSeparation();
    testLaserTargetsMatchingHudBadgeOrder();
    testTargetLabelsCollisionAvoidanceAndOpacity();
    testKeyboardOverlayTilingOnly();
    testLaserTargetsAvoidHudBadges();
    testKeycapMiddleElisionAndTargetLabelsDisabled();
    testLaserTargetsShortestPathToHudBadges();
    testFixedLetterColorsFeature();

    std::cout << "All LeapK Keyboard Overlay tests passed successfully!\n";
    return 0;
}
