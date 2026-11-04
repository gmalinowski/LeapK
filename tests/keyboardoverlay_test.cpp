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

    std::cout << "All LeapK Keyboard Overlay tests passed successfully!\n";
    return 0;
}
