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

} // namespace OutlineGeometry
