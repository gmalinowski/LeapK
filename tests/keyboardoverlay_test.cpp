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

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    testDistinctColorsPalette();
    testOverlayGeometryComputation();
    testSpatialKeyAssignment();
    testPreferredKeyPreservation();
    testOccludedTargetBadgePlacements();
    testOverlayImageRendering();

    std::cout << "All LeapK Keyboard Overlay tests passed successfully!\n";
    return 0;
}
