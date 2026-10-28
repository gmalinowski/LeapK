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

void testHudDividerLayout()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QList<QSizeF> pills = {
        QSizeF(120, 36),
        QSizeF(120, 36),
        QSizeF(120, 36),
        QSizeF(120, 36)
    };

    // 1. Horizontal Bottom layout with divider after index 1
    QList<QRectF> dividerRects;
    QList<QRectF> rects = OutlineBadge::computeHudPillPositions(
        pills, screen, 0, 24.0, 12.0, -1.0, 1, 28.0, &dividerRects);
    assert(rects.size() == 4);
    assert(dividerRects.size() == 1);

    // Normal gap between 0 and 1 is 12.0
    const qreal gap01 = rects[1].left() - rects[0].right();
    assert(qAbs(gap01 - 12.0) < 0.1);

    // Divider gap between 1 and 2 is 28.0
    const qreal gap12 = rects[2].left() - rects[1].right();
    assert(qAbs(gap12 - 28.0) < 0.1);

    // Divider rect is positioned between pill 1 and pill 2
    assert(dividerRects[0].left() >= rects[1].right() - 0.1);
    assert(dividerRects[0].right() <= rects[2].left() + 0.1);

    // Render vertical divider image
    QImage divImg = OutlineBadge::renderHudDividerLine(dividerRects[0].size(), 1.0, true);
    assert(!divImg.isNull());
    assert(divImg.width() == qRound(dividerRects[0].width()));
    assert(divImg.height() == qRound(dividerRects[0].height()));

    // 2. Vertical Left layout with divider after index 1
    QList<QRectF> vDividers;
    QList<QRectF> vRects = OutlineBadge::computeHudPillPositions(
        pills, screen, 2, 24.0, 12.0, -1.0, 1, 28.0, &vDividers);
    assert(vRects.size() == 4);
    assert(vDividers.size() == 1);
    const qreal vGap12 = vRects[2].top() - vRects[1].bottom();
    assert(qAbs(vGap12 - 28.0) < 0.1);

    // Render horizontal divider image
    QImage vDivImg = OutlineBadge::renderHudDividerLine(vDividers[0].size(), 1.0, false);
    assert(!vDivImg.isNull());

    // 3. Render pinned HUD pill
    OutlineBadge::BadgeConfig cfg;
    cfg.hudBadgeSize = 28;
    QImage pinnedPill = OutlineBadge::renderOffscreenTargetPill(
        cfg, QChar('P'), QColor(241, 196, 15), Qt::black,
        QStringLiteral("● Pinned"), QStringLiteral("Dolphin"),
        28, 1.0, QString(), QIcon(), QImage(), false, QString(), false, 0, false, true);
    assert(!pinnedPill.isNull());
    assert(pinnedPill.width() > 60);

    // 4. Render high-contrast pin dot badge
    QImage pinDot = OutlineBadge::renderPinDotBadgeImage(24, 1.0);
    assert(!pinDot.isNull());
    assert(pinDot.width() >= 24);
    assert(pinDot.height() >= 24);

    std::cout << "testHudDividerLayout PASSED\n";
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QGuiApplication app(argc, argv);

    testZeroAndSinglePillLayout();
    testHorizontalRowWrapping();
    testVerticalColumnWrapping();
    testGroupContainerFrames();
    testHudDividerLayout();

    std::cout << "All HUD Layout tests passed successfully!\n";
    return 0;
}
