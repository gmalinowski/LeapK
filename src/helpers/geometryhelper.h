// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QVector2D>
#include <QtGlobal>
#include <QtMath>

namespace OutlineGeometry
{

inline void buildRoundedRectStrip(const QRectF &outer, qreal outerRadius,
                                  const QRectF &inner, qreal innerRadius,
                                  qreal scale, QList<QVector2D> &verts)
{
    if (outer.width() <= 0.0 || outer.height() <= 0.0
        || inner.width() <= 0.0 || inner.height() <= 0.0) {
        return;
    }

    constexpr int cornerSegments = 8;

    // Clamp radii so they don't exceed half the rect dimensions
    outerRadius = qBound(0.0, outerRadius, qMin(outer.width(), outer.height()) / 2.0);
    innerRadius = qBound(0.0, innerRadius, qMin(inner.width(), inner.height()) / 2.0);

    const QPointF innerCenters[4] = {
        {inner.x() + innerRadius, inner.y() + innerRadius},             // top-left
        {inner.right() - innerRadius, inner.y() + innerRadius},         // top-right
        {inner.right() - innerRadius, inner.bottom() - innerRadius},    // bottom-right
        {inner.x() + innerRadius, inner.bottom() - innerRadius},        // bottom-left
    };
    const QPointF outerCenters[4] = {
        {outer.x() + outerRadius, outer.y() + outerRadius},             // top-left
        {outer.right() - outerRadius, outer.y() + outerRadius},         // top-right
        {outer.right() - outerRadius, outer.bottom() - outerRadius},    // bottom-right
        {outer.x() + outerRadius, outer.bottom() - outerRadius},        // bottom-left
    };

    // Start angles for each corner in screen coordinates (Y points down).
    // Going clockwise: top-left, top-right, bottom-right, bottom-left.
    const qreal startAngles[4] = {M_PI, 1.5 * M_PI, 0.0, 0.5 * M_PI};

    struct PointPair {
        QPointF outer, inner;
    };
    QList<PointPair> ring;
    ring.reserve(4 * (cornerSegments + 1) + 1);

    for (int corner = 0; corner < 4; ++corner) {
        const qreal startAngle = startAngles[corner];
        for (int i = 0; i <= cornerSegments; ++i) {
            const qreal angle = startAngle + (M_PI / 2.0) * i / cornerSegments;
            const qreal cosA = qCos(angle);
            const qreal sinA = qSin(angle);

            PointPair pp;
            pp.inner = innerCenters[corner] + QPointF(innerRadius * cosA, innerRadius * sinA);
            pp.outer = outerCenters[corner] + QPointF(outerRadius * cosA, outerRadius * sinA);
            ring.append(pp);
        }
    }

    // Close the ring by repeating the first pair
    ring.append(ring.first());

    // Convert the ring into triangles (2 per segment)
    verts.reserve(verts.size() + (ring.size() - 1) * 6);
    for (int i = 0; i < ring.size() - 1; ++i) {
        const auto &a = ring[i];
        const auto &b = ring[i + 1];

        // Triangle 1: a.outer, b.outer, a.inner
        verts.append(QVector2D(a.outer.x() * scale, a.outer.y() * scale));
        verts.append(QVector2D(b.outer.x() * scale, b.outer.y() * scale));
        verts.append(QVector2D(a.inner.x() * scale, a.inner.y() * scale));

        // Triangle 2: b.outer, b.inner, a.inner
        verts.append(QVector2D(b.outer.x() * scale, b.outer.y() * scale));
        verts.append(QVector2D(b.inner.x() * scale, b.inner.y() * scale));
        verts.append(QVector2D(a.inner.x() * scale, a.inner.y() * scale));
    }
}

inline qreal computeRecentWindowWidth(int recentIdx, qreal baseWidth, int widthReductionPx)
{
    if (widthReductionPx <= 0) {
        return baseWidth;
    }
    const int step = std::max(1, recentIdx + 1);
    return std::max<qreal>(1.0, baseWidth - static_cast<qreal>(step * widthReductionPx));
}

inline qreal computeRoundedRectPerimeter(const QRectF &rect, qreal radius, qreal scale)
{
    const qreal w = rect.width() * scale;
    const qreal h = rect.height() * scale;
    if (w <= 0.0 || h <= 0.0) {
        return 0.0;
    }
    const qreal r = qBound<qreal>(0.0, radius * scale, qMin(w, h) / 2.0);
    const qreal w0 = std::max<qreal>(0.0, w / 2.0 - r);
    const qreal h0 = std::max<qreal>(0.0, h / 2.0 - r);
    return 4.0 * w0 + 4.0 * h0 + 2.0 * M_PI * r;
}

inline void computeRecentWindowDashPattern(int recentIdx, qreal perimeter, qreal scale,
                                           qreal &effectiveDashLength, qreal &effectiveDashPeriod,
                                           int gapGrowthPx = 6)
{
    const qreal baseDash = 12.0 * scale;
    const qreal baseGap = 6.0 * scale;
    const qreal gapStep = std::max(0, gapGrowthPx) * scale;
    const int step = std::max(1, recentIdx + 1);

    const qreal dashLength = baseDash;
    const qreal gapLength = baseGap + step * gapStep;
    const qreal period = dashLength + gapLength;

    if (perimeter <= 0.0 || period <= 0.0) {
        effectiveDashLength = dashLength;
        effectiveDashPeriod = period;
        return;
    }

    const qreal count = std::max<qreal>(1.0, std::round(perimeter / period));
    effectiveDashPeriod = perimeter / count;
    const qreal ratio = effectiveDashPeriod / period;
    effectiveDashLength = dashLength * ratio;
}

inline void computeCustomDashPattern(qreal dashLengthPx, qreal gapLengthPx, qreal perimeter, qreal scale,
                                     qreal &effectiveDashLength, qreal &effectiveDashPeriod)
{
    const qreal dashLength = std::max<qreal>(1.0, dashLengthPx) * scale;
    const qreal gapLength = std::max<qreal>(1.0, gapLengthPx) * scale;
    const qreal period = dashLength + gapLength;

    if (perimeter <= 0.0 || period <= 0.0) {
        effectiveDashLength = dashLength;
        effectiveDashPeriod = period;
        return;
    }

    const qreal count = std::max<qreal>(1.0, std::round(perimeter / period));
    effectiveDashPeriod = perimeter / count;
    const qreal ratio = effectiveDashPeriod / period;
    effectiveDashLength = dashLength * ratio;
}

inline qreal computePerimeterDistance(const QPointF &pt, const QRectF &rect, qreal radius, qreal scale)
{
    const qreal w = rect.width() * scale;
    const qreal h = rect.height() * scale;
    if (w <= 0.0 || h <= 0.0) {
        return 0.0;
    }
    const qreal r = qBound<qreal>(0.0, radius * scale, qMin(w, h) / 2.0);
    const QPointF center(rect.center().x() * scale, rect.center().y() * scale);
    const QPointF q = pt - center;
    const qreal w0 = std::max<qreal>(0.0, w / 2.0 - r);
    const qreal h0 = std::max<qreal>(0.0, h / 2.0 - r);
    const qreal Lx = 2.0 * w0;
    const qreal Ly = 2.0 * h0;
    const qreal Lc = 0.5 * M_PI * r;

    if (q.x() > w0 && q.y() < -h0) {
        const qreal angle = std::atan2(q.x() - w0, -(q.y() + h0));
        return Lx + r * angle;
    }
    if (q.x() > w0 && q.y() > h0) {
        const qreal angle = std::atan2(q.y() - h0, q.x() - w0);
        return Lx + Lc + Ly + r * angle;
    }
    if (q.x() < -w0 && q.y() > h0) {
        const qreal angle = std::atan2(-(q.x() + w0), q.y() - h0);
        return 2.0 * Lx + 2.0 * Lc + Ly + r * angle;
    }
    if (q.x() < -w0 && q.y() < -h0) {
        const qreal angle = std::atan2(-(q.y() + h0), -(q.x() + w0));
        return 2.0 * Lx + 3.0 * Lc + 2.0 * Ly + r * angle;
    }

    if (q.y() < -h0) {
        return std::clamp(q.x() + w0, 0.0, Lx);
    }
    if (q.x() > w0) {
        return Lx + Lc + std::clamp(q.y() + h0, 0.0, Ly);
    }
    if (q.y() > h0) {
        return Lx + Lc + Ly + Lc + std::clamp(w0 - q.x(), 0.0, Lx);
    }
    return 2.0 * Lx + 3.0 * Lc + Ly + std::clamp(h0 - q.y(), 0.0, Ly);
}

} // namespace OutlineGeometry
