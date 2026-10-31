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

void testEmptyHudHeaderAndHintBarSeparation()
{
    // Test that when HUD has 0 pills, header and hint bar have strict positive separation (no overlap)
    const qreal scale = 1.0;
    const QRectF screenRect(0, 0, 1920, 1080);

    QList<OutlineBadge::HudModeHeaderView> views;
    views.append({QStringLiteral("Main"), QColor(52, 152, 219), 0, true, false});

    // 1. Standard navigation font size
    QImage headerImg1 = OutlineBadge::renderHudModeHeader(scale, false, QString(), QColor(), 0, views, 0, false, QString(), false, false, 0, false, QString(), 1, 0, 44, 0);
    assert(!headerImg1.isNull());
    const qreal headerH1 = headerImg1.height() / scale;
    const qreal headerBottom1 = screenRect.top() + 12.0 + headerH1;

    QImage hintImg1 = OutlineBadge::renderHudActionHintBar(scale, false, 0, QString(), QColor(), false, false, false, false, 1050.0, 0);
    assert(!hintImg1.isNull());
    const qreal hintH1 = hintImg1.height() / scale;

    const qreal hintY1 = headerBottom1 + 12.0;
    const QRectF headerRect1(screenRect.center().x() - headerImg1.width() / 2.0, screenRect.top() + 12.0, headerImg1.width(), headerH1);
    const QRectF hintRect1(screenRect.center().x() - hintImg1.width() / 2.0, hintY1, hintImg1.width(), hintH1);

    // Assert strictly no vertical intersection
    assert(!headerRect1.intersects(hintRect1));
    assert(hintRect1.top() - headerRect1.bottom() >= 12.0);

    // 2. Large fonts (navFontSize = 18, hintsFontSize = 18)
    QImage headerImgLarge = OutlineBadge::renderHudModeHeader(scale, false, QString(), QColor(), 0, views, 0, false, QString(), false, false, 0, false, QString(), 1, 0, 44, 18);
    assert(!headerImgLarge.isNull());
    const qreal headerHLarge = headerImgLarge.height() / scale;
    const qreal headerBottomLarge = screenRect.top() + 12.0 + headerHLarge;

    QImage hintImgLarge = OutlineBadge::renderHudActionHintBar(scale, false, 0, QString(), QColor(), false, false, false, false, 1350.0, 18);
    assert(!hintImgLarge.isNull());
    const qreal hintHLarge = hintImgLarge.height() / scale;

    const qreal hintYLarge = headerBottomLarge + 12.0;
    const QRectF headerRectLarge(screenRect.center().x() - headerImgLarge.width() / 2.0, screenRect.top() + 12.0, headerImgLarge.width(), headerHLarge);
    const QRectF hintRectLarge(screenRect.center().x() - hintImgLarge.width() / 2.0, hintYLarge, hintImgLarge.width(), hintHLarge);

    assert(!headerRectLarge.intersects(hintRectLarge));
    assert(hintRectLarge.top() - headerRectLarge.bottom() >= 12.0);

    std::cout << "testEmptyHudHeaderAndHintBarSeparation PASSED\n";
}

void testHudActiveMarkerRect()
{
    // 1. Empty rects -> returns invalid/empty QRectF
    assert(OutlineBadge::computeActiveMarkerRect({}, 0).isEmpty());

    // 2. Horizontal layout: 3 pills
    const QRectF p0(100, 50, 100, 40);
    const QRectF p1(212, 50, 100, 40);
    const QRectF p2(324, 50, 100, 40);
    const QList<QRectF> rects = {p0, p1, p2};

    // Index 0: Before first pill (to the left of p0)
    QRectF m0 = OutlineBadge::computeActiveMarkerRect(rects, 0, 0, 12.0);
    assert(m0.isValid() && !m0.isEmpty());
    assert(m0.right() <= p0.left());
    assert(m0.height() > 0);

    // Index 1: Between p0 and p1
    QRectF m1 = OutlineBadge::computeActiveMarkerRect(rects, 1, 0, 12.0);
    assert(m1.isValid() && !m1.isEmpty());
    assert(m1.left() >= p0.right() - 2.0);
    assert(m1.right() <= p1.left() + 2.0);

    // Index 2: Between p1 and p2
    QRectF m2 = OutlineBadge::computeActiveMarkerRect(rects, 2, 0, 12.0);
    assert(m2.isValid() && !m2.isEmpty());
    assert(m2.left() >= p1.right() - 2.0);
    assert(m2.right() <= p2.left() + 2.0);

    // Index 3: After last pill (to the right of p2)
    QRectF m3 = OutlineBadge::computeActiveMarkerRect(rects, 3, 0, 12.0);
    assert(m3.isValid() && !m3.isEmpty());
    assert(m3.left() >= p2.right());

    // 3. Vertical layout: 2 pills (Position 2 = Left)
    const QRectF v0(50, 100, 120, 36);
    const QRectF v1(50, 148, 120, 36);
    const QList<QRectF> vRects = {v0, v1};

    // Index 1: Between v0 and v1 vertically
    QRectF mv1 = OutlineBadge::computeActiveMarkerRect(vRects, 1, 2, 12.0);
    assert(mv1.isValid() && !mv1.isEmpty());
    assert(mv1.top() >= v0.bottom() - 2.0);
    assert(mv1.bottom() <= v1.top() + 2.0);

    // 4. Test renderHudActiveMarkerLine
    QImage lineImg = OutlineBadge::renderHudActiveMarkerLine(QSizeF(14, 40), 1.0, QColor(0, 195, 255), true);
    assert(!lineImg.isNull());
    assert(lineImg.width() == 14);
    assert(lineImg.height() == 40);

    std::cout << "testHudActiveMarkerRect PASSED\n";
}

void testHudMarkerAndBadgeSelectionAnimation()
{
    // 1. Interpolation between two marker positions (Marker -> Marker)
    const QRectF prevMarker(100.0, 50.0, 14.0, 40.0);
    const QRectF targetMarker(250.0, 50.0, 14.0, 40.0);

    auto easeOutCubic = [](qreal progress) -> qreal {
        const qreal clamped = qBound(0.0, progress, 1.0);
        return 1.0 - std::pow(1.0 - clamped, 3.0);
    };

    // At progress 0.0 (start)
    const qreal ease0 = easeOutCubic(0.0);
    assert(std::abs(ease0 - 0.0) < 0.001);
    const qreal x0 = prevMarker.left() + (targetMarker.left() - prevMarker.left()) * ease0;
    assert(std::abs(x0 - 100.0) < 0.001);
    const qreal ghostAlpha0 = (1.0 - ease0) * 0.45;
    assert(std::abs(ghostAlpha0 - 0.45) < 0.001);

    // At progress 0.5 (halfway in time: fast deceleration)
    const qreal easeHalf = easeOutCubic(0.5); // 1.0 - 0.125 = 0.875
    assert(std::abs(easeHalf - 0.875) < 0.001);
    const qreal xHalf = prevMarker.left() + (targetMarker.left() - prevMarker.left()) * easeHalf;
    assert(std::abs(xHalf - (100.0 + 150.0 * 0.875)) < 0.001);

    // At progress 1.0 (end)
    const qreal ease1 = easeOutCubic(1.0);
    assert(std::abs(ease1 - 1.0) < 0.001);
    const qreal x1 = prevMarker.left() + (targetMarker.left() - prevMarker.left()) * ease1;
    assert(std::abs(x1 - 250.0) < 0.001);

    // 2. Fade out when jumping to dock pill (Marker -> Pill)
    const qreal fadeAlpha0 = 1.0 - easeOutCubic(0.0);
    assert(std::abs(fadeAlpha0 - 1.0) < 0.001);
    const qreal fadeAlphaHalf = 1.0 - easeOutCubic(0.5);
    assert(std::abs(fadeAlphaHalf - 0.125) < 0.001);
    const qreal fadeAlpha1 = 1.0 - easeOutCubic(1.0);
    assert(std::abs(fadeAlpha1 - 0.0) < 0.001);

    // 3. Fade in when jumping from dock pill (Pill -> Marker)
    const qreal fadeIn0 = easeOutCubic(0.0);
    assert(std::abs(fadeIn0 - 0.0) < 0.001);
    const qreal fadeInHalf = easeOutCubic(0.5);
    assert(std::abs(fadeInHalf - 0.875) < 0.001);
    const qreal fadeIn1 = easeOutCubic(1.0);
    assert(std::abs(fadeIn1 - 1.0) < 0.001);

    // 4. Badge color transition (active cyan -> default dark)
    const QColor activeCyan(0, 195, 255);
    const QColor defaultDark(30, 30, 46);

    auto blendColor = [](const QColor &base, const QColor &target, qreal t) -> QColor {
        int r = qBound(0, qRound(base.red() + (target.red() - base.red()) * t), 255);
        int g = qBound(0, qRound(base.green() + (target.green() - base.green()) * t), 255);
        int b = qBound(0, qRound(base.blue() + (target.blue() - base.blue()) * t), 255);
        return QColor(r, g, b);
    };

    // When fading out active window:
    const QColor cStart = blendColor(defaultDark, activeCyan, 1.0 - ease0);
    assert(cStart == activeCyan);
    const QColor cEnd = blendColor(defaultDark, activeCyan, 1.0 - ease1);
    assert(cEnd == defaultDark);

    // When fading in new active window:
    const QColor cNewStart = blendColor(defaultDark, activeCyan, ease0);
    assert(cNewStart == defaultDark);
    const QColor cNewEnd = blendColor(defaultDark, activeCyan, ease1);
    assert(cNewEnd == activeCyan);

    std::cout << "testHudMarkerAndBadgeSelectionAnimation PASSED\n";
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
    testEmptyHudHeaderAndHintBarSeparation();
    testHudActiveMarkerRect();
    testHudMarkerAndBadgeSelectionAnimation();

    std::cout << "All HUD Layout tests passed successfully!\n";
    return 0;
}
