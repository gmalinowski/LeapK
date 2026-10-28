/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "badgehelper.h"
#include <QGuiApplication>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testZeroAndSinglePillLayout()
{
    const QRectF screen(0, 0, 1920, 1080);

    // 0 pills
    QList<QRectF> emptyRes = OutlineBadge::computeHudPillPositions({}, screen, 0);
    assert(emptyRes.isEmpty());

    // 1 pill (size 180x40)
    const QSizeF pillSize(180, 40);
    const qreal margin = 24.0;

    // Position 0: Bottom
    {
        QList<QRectF> res = OutlineBadge::computeHudPillPositions({pillSize}, screen, 0, margin);
        assert(res.size() == 1);
        const QRectF &r = res.first();
        assert(r.width() == 180);
        assert(r.height() == 40);
        assert(qAbs(r.center().x() - screen.center().x()) < 1.0);
        assert(qAbs(r.bottom() - (screen.bottom() - margin)) < 1.0);
    }

    // Position 1: Top
    {
        QList<QRectF> res = OutlineBadge::computeHudPillPositions({pillSize}, screen, 1, margin);
        assert(res.size() == 1);
        const QRectF &r = res.first();
        assert(qAbs(r.center().x() - screen.center().x()) < 1.0);
        assert(qAbs(r.top() - (screen.top() + margin)) < 1.0);
    }

    // Position 2: Left
    {
        QList<QRectF> res = OutlineBadge::computeHudPillPositions({pillSize}, screen, 2, margin);
        assert(res.size() == 1);
        const QRectF &r = res.first();
        assert(qAbs(r.center().y() - screen.center().y()) < 1.0);
        assert(qAbs(r.left() - (screen.left() + margin)) < 1.0);
    }

    // Position 3: Right
    {
        QList<QRectF> res = OutlineBadge::computeHudPillPositions({pillSize}, screen, 3, margin);
        assert(res.size() == 1);
        const QRectF &r = res.first();
        assert(qAbs(r.center().y() - screen.center().y()) < 1.0);
        assert(qAbs(r.right() - (screen.right() - margin)) < 1.0);
    }

    std::cout << "testZeroAndSinglePillLayout PASSED\n";
}

void testHorizontalRowWrapping()
{
    // Screen width 800, margin 20 -> available width 760
    const QRectF screen(0, 0, 800, 600);
    const qreal margin = 20.0;
    const qreal gap = 10.0;

    // 8 pills of width 120, height 40
    // In one row: 120 * 8 + 10 * 7 = 1030 > 760 -> must wrap!
    QList<QSizeF> pills;
    for (int i = 0; i < 8; ++i) {
        pills.append(QSizeF(120, 40));
    }

    QList<QRectF> res = OutlineBadge::computeHudPillPositions(pills, screen, 0, margin, gap);
    assert(res.size() == 8);

    // Verify all pills stay within screen bounds
    for (const QRectF &r : res) {
        assert(r.left() >= screen.left() + margin - 0.1);
        assert(r.right() <= screen.right() - margin + 0.1);
        assert(r.top() >= screen.top() + margin - 0.1);
        assert(r.bottom() <= screen.bottom() - margin + 0.1);
    }

    // Row 1 and Row 2 separation
    const qreal y0 = res[0].top();
    const qreal yLast = res.last().top();
    assert(yLast > y0); // Last pill is on a lower row
    assert(yLast >= y0 + 40 + gap - 0.1);

    std::cout << "testHorizontalRowWrapping PASSED\n";
}

void testVerticalColumnWrapping()
{
    // Screen height 500, margin 20 -> available height 460
    const QRectF screen(0, 0, 800, 500);
    const qreal margin = 20.0;
    const qreal gap = 10.0;

    // 8 pills of height 70, width 140
    // In one column: 70 * 8 + 10 * 7 = 630 > 460 -> must wrap into 2 columns!
    QList<QSizeF> pills;
    for (int i = 0; i < 8; ++i) {
        pills.append(QSizeF(140, 70));
    }

    QList<QRectF> res = OutlineBadge::computeHudPillPositions(pills, screen, 2 /* Left */, margin, gap);
    assert(res.size() == 8);

    // Verify all pills stay within screen bounds
    for (const QRectF &r : res) {
        assert(r.left() >= screen.left() + margin - 0.1);
        assert(r.right() <= screen.right() - margin + 0.1);
        assert(r.top() >= screen.top() + margin - 0.1);
        assert(r.bottom() <= screen.bottom() - margin + 0.1);
    }

    // Column 1 and Column 2 separation
    const qreal x0 = res[0].left();
    const qreal xLast = res.last().left();
    assert(xLast > x0); // Last pill is on a right column
    assert(xLast >= x0 + 140 + gap - 0.1);

    std::cout << "testVerticalColumnWrapping PASSED\n";
}

void testGroupContainerFrames()
{
    const QSizeF size(300, 150);
    const qreal scale = 1.25;

    // 1. Group frame with tab at top
    QImage frameTop = OutlineBadge::renderGroupContainerFrame(
        size, QChar('F'), QColor(230, 126, 34), QStringLiteral("Firefox"), 3, scale, 8, 2.0, 0, false);
    assert(!frameTop.isNull());
    assert(frameTop.width() == qRound(size.width() * scale));
    assert(frameTop.height() == qRound(size.height() * scale));

    // 2. Group frame with tab at bottom
    QImage frameBottom = OutlineBadge::renderGroupContainerFrame(
        size, QChar('K'), QColor(52, 152, 219), QStringLiteral("Konsole"), 2, scale, 8, 2.0, 0, true);
    assert(!frameBottom.isNull());
    assert(frameBottom.width() == qRound(size.width() * scale));
    assert(frameBottom.height() == qRound(size.height() * scale));

    std::cout << "testGroupContainerFrames PASSED\n";
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);

    testZeroAndSinglePillLayout();
    testHorizontalRowWrapping();
    testVerticalColumnWrapping();
    testGroupContainerFrames();

    std::cout << "All HUD Layout tests passed successfully!\n";
    return 0;
}
