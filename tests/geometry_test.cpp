// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#include "geometryhelper.h"
#include <cstdio>
#include <cmath>

int main()
{
    using namespace OutlineGeometry;
    int failures = 0;
    int checks = 0;

    const auto check = [&](bool condition, const char *name) {
        ++checks;
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };

    // 1. Degenerate inputs
    {
        QList<QVector2D> verts;
        buildRoundedRectStrip(QRectF(0, 0, 0, 100), 10, QRectF(0, 0, 0, 80), 5, 1.0, verts);
        check(verts.isEmpty(), "zero width produces empty vertices");

        verts.clear();
        buildRoundedRectStrip(QRectF(0, 0, 100, -10), 10, QRectF(0, 0, 80, -10), 5, 1.0, verts);
        check(verts.isEmpty(), "negative height produces empty vertices");
    }

    // 2. Normal rounded rect strip generation
    {
        QList<QVector2D> verts;
        const QRectF outer(0, 0, 400, 300);
        const QRectF inner(10, 10, 380, 280);
        buildRoundedRectStrip(outer, 12, inner, 6, 1.0, verts);

        check(!verts.isEmpty(), "normal rect produces non-empty vertices");
        check(verts.size() % 3 == 0, "vertices form valid triangles (divisible by 3)");
        // 4 corners * 8 segments + 4 joints = 36 segments. 36 * 2 triangles * 3 verts = 216 verts.
        check(verts.size() == 216, "vertex count exactly matches strip tessellation (216)");

        // Verify bounding box of all vertices
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
        for (const auto &v : verts) {
            minX = std::min(minX, v.x());
            maxX = std::max(maxX, v.x());
            minY = std::min(minY, v.y());
            maxY = std::max(maxY, v.y());
        }

        check(minX >= -0.01f && maxX <= 400.01f, "all X coordinates lie within outer rect width");
        check(minY >= -0.01f && maxY <= 300.01f, "all Y coordinates lie within outer rect height");
    }

    // 3. Radius clamping (radius > half of dimension)
    {
        QList<QVector2D> verts;
        // height is 40, so half is 20. Requesting radius 100 must be safely clamped to 20.
        const QRectF outer(0, 0, 200, 40);
        const QRectF inner(5, 5, 190, 30);
        buildRoundedRectStrip(outer, 100, inner, 50, 1.0, verts);

        check(verts.size() == 216, "clamped radii still produce valid 216 vertices");

        bool hasNanOrInf = false;
        for (const auto &v : verts) {
            if (std::isnan(v.x()) || std::isnan(v.y()) || std::isinf(v.x()) || std::isinf(v.y())) {
                hasNanOrInf = true;
                break;
            }
        }
        check(!hasNanOrInf, "no NaN or Inf generated during radius clamping");
    }

    // 4. Scaling
    {
        QList<QVector2D> verts1;
        QList<QVector2D> verts2;
        const QRectF outer(10, 10, 100, 80);
        const QRectF inner(15, 15, 90, 70);

        buildRoundedRectStrip(outer, 5, inner, 2, 1.0, verts1);
        buildRoundedRectStrip(outer, 5, inner, 2, 2.5, verts2);

        check(verts1.size() == verts2.size(), "scaled verts have identical count");
        bool correctlyScaled = true;
        for (int i = 0; i < verts1.size(); ++i) {
            if (std::abs(verts1[i].x() * 2.5f - verts2[i].x()) > 0.001f ||
                std::abs(verts1[i].y() * 2.5f - verts2[i].y()) > 0.001f) {
                correctlyScaled = false;
                break;
            }
        }
        check(correctlyScaled, "scale factor multiplies all vertex coordinates accurately");
    }

    // 5. Recent window width reduction
    {
        // 0 px reduction keeps base width unchanged
        check(std::abs(computeRecentWindowWidth(0, 5.0, 0) - 5.0) < 0.001, "0px reduction on window 0 keeps base width");
        check(std::abs(computeRecentWindowWidth(2, 5.0, 0) - 5.0) < 0.001, "0px reduction on window 2 keeps base width");

        // 2 px reduction on 5 px base width:
        // Window 1 (recentIdx = 0): step 1 -> 5 - 2 = 3 px
        check(std::abs(computeRecentWindowWidth(0, 5.0, 2) - 3.0) < 0.001, "window 0 reduced by 2px (5 -> 3px)");
        // Window 2 (recentIdx = 1): step 2 -> 5 - 4 = 1 px
        check(std::abs(computeRecentWindowWidth(1, 5.0, 2) - 1.0) < 0.001, "window 1 reduced by 4px (5 -> 1px)");
        // Window 3 (recentIdx = 2): step 3 -> 5 - 6 = -1 -> clamped at 1 px
        check(std::abs(computeRecentWindowWidth(2, 5.0, 2) - 1.0) < 0.001, "window 2 clamped at minimum 1px");

        // Edge case: base width is already 1 px -> stays 1 px
        check(std::abs(computeRecentWindowWidth(0, 1.0, 2) - 1.0) < 0.001, "1px base width stays 1px");

        // Safety clamp on negative index
        check(std::abs(computeRecentWindowWidth(-1, 5.0, 2) - 3.0) < 0.001, "negative index clamped to step 1");
    }

    // 6. Rounded rect perimeter
    {
        const QRectF rect(0, 0, 100, 100);
        // Radius 0 -> sharp 100x100 square perimeter = 400
        check(std::abs(computeRoundedRectPerimeter(rect, 0.0, 1.0) - 400.0) < 0.001, "sharp square perimeter is 400");
        // Radius 50 -> circle radius 50 perimeter = 2 * PI * 50 = 100 * PI ~ 314.159
        check(std::abs(computeRoundedRectPerimeter(rect, 50.0, 1.0) - (100.0 * M_PI)) < 0.001, "circle perimeter is 100*PI");
        // Scale 2.0 -> square perimeter = 800
        check(std::abs(computeRoundedRectPerimeter(rect, 0.0, 2.0) - 800.0) < 0.001, "scaled square perimeter is 800");
        // Degenerate
        check(computeRoundedRectPerimeter(QRectF(0, 0, 0, 100), 5.0, 1.0) == 0.0, "degenerate rect has 0 perimeter");
    }

    // 7. Recent window dash pattern progression
    {
        const qreal perimeter = 1000.0;
        const qreal scale = 1.0;

        qreal dash0 = 0.0, period0 = 0.0;
        computeRecentWindowDashPattern(0, perimeter, scale, dash0, period0);
        const qreal gap0 = period0 - dash0;

        qreal dash1 = 0.0, period1 = 0.0;
        computeRecentWindowDashPattern(1, perimeter, scale, dash1, period1);
        const qreal gap1 = period1 - dash1;

        qreal dash2 = 0.0, period2 = 0.0;
        computeRecentWindowDashPattern(2, perimeter, scale, dash2, period2);
        const qreal gap2 = period2 - dash2;

        qreal dash3 = 0.0, period3 = 0.0;
        computeRecentWindowDashPattern(3, perimeter, scale, dash3, period3);
        const qreal gap3 = period3 - dash3;

        // Gap must strictly increase for older windows
        check(gap0 > 0.0, "window 0 gap is positive");
        check(gap1 > gap0, "window 1 gap is larger than window 0");
        check(gap2 > gap1, "window 2 gap is larger than window 1");
        check(gap3 > gap2, "window 3 gap is larger than window 2");

        // Dash length stays close to base dash length (~12px)
        check(std::abs(dash0 - 12.0) < 1.0, "window 0 dash length close to 12px");
        check(std::abs(dash1 - 12.0) < 1.0, "window 1 dash length close to 12px");
        check(std::abs(dash2 - 12.0) < 1.0, "window 2 dash length close to 12px");

        // Period must divide perimeter into an integer number of repetitions
        const qreal count0 = perimeter / period0;
        check(std::abs(count0 - std::round(count0)) < 1e-4, "period0 evenly divides perimeter");
        const qreal count1 = perimeter / period1;
        check(std::abs(count1 - std::round(count1)) < 1e-4, "period1 evenly divides perimeter");

        // High-DPI scale (2.0) doubles dash and gap lengths
        qreal dashScale2 = 0.0, periodScale2 = 0.0;
        computeRecentWindowDashPattern(0, perimeter * 2.0, 2.0, dashScale2, periodScale2);
        check(std::abs(dashScale2 - 24.0) < 2.0, "2x scale doubles base dash to ~24px");

        // Configurable gap growth (gapGrowthPx = 0 -> constant gaps)
        qreal cDash0 = 0.0, cPeriod0 = 0.0;
        computeRecentWindowDashPattern(0, perimeter, 1.0, cDash0, cPeriod0, 0);
        qreal cDash2 = 0.0, cPeriod2 = 0.0;
        computeRecentWindowDashPattern(2, perimeter, 1.0, cDash2, cPeriod2, 0);
        check(std::abs((cPeriod0 - cDash0) - (cPeriod2 - cDash2)) < 0.1, "0px gap growth yields constant gap between windows");

        // Fast gap growth (gapGrowthPx = 12 -> 12px jump per step)
        qreal fDash0 = 0.0, fPeriod0 = 0.0;
        computeRecentWindowDashPattern(0, perimeter, 1.0, fDash0, fPeriod0, 12);
        qreal fDash1 = 0.0, fPeriod1 = 0.0;
        computeRecentWindowDashPattern(1, perimeter, 1.0, fDash1, fPeriod1, 12);
        check((fPeriod1 - fDash1) - (fPeriod0 - fDash0) > 10.0, "12px gap growth increases gap by ~12px per window");

        // Gap growth takes effect immediately on window 1 (recentIdx = 0)
        qreal g0Dash = 0.0, g0Period = 0.0;
        computeRecentWindowDashPattern(0, perimeter, 1.0, g0Dash, g0Period, 0);
        qreal g6Dash = 0.0, g6Period = 0.0;
        computeRecentWindowDashPattern(0, perimeter, 1.0, g6Dash, g6Period, 6);
        qreal g12Dash = 0.0, g12Period = 0.0;
        computeRecentWindowDashPattern(0, perimeter, 1.0, g12Dash, g12Period, 12);

        check((g6Period - g6Dash) > (g0Period - g0Dash) + 4.0, "gap growth affects window 1 gap immediately (6px vs 0px)");
        check((g12Period - g12Dash) > (g6Period - g6Dash) + 4.0, "gap growth affects window 1 gap immediately (12px vs 6px)");
    }

    // 8. Perimeter distance continuity around rounded rect
    {
        const QRectF rect(0, 0, 200, 100);
        const qreal radius = 10.0;
        const qreal scale = 1.0;
        const qreal totalPerim = computeRoundedRectPerimeter(rect, radius, scale);

        // Top edge start (-w0, -h0) -> near (10, 0)
        const qreal sTopLeft = computePerimeterDistance(QPointF(10, 0), rect, radius, scale);
        check(std::abs(sTopLeft) < 1.0, "start of top edge is near s=0");

        // Top edge end (w0, -h0) -> (190, 0)
        const qreal sTopRight = computePerimeterDistance(QPointF(190, 0), rect, radius, scale);
        check(sTopRight > sTopLeft, "perimeter distance increases along top edge");

        // Right edge middle -> (200, 50)
        const qreal sRightMid = computePerimeterDistance(QPointF(200, 50), rect, radius, scale);
        check(sRightMid > sTopRight, "perimeter distance increases along right edge");

        // Bottom edge middle -> (100, 100)
        const qreal sBottomMid = computePerimeterDistance(QPointF(100, 100), rect, radius, scale);
        check(sBottomMid > sRightMid, "perimeter distance increases along bottom edge");

        // Left edge middle -> (0, 50)
        const qreal sLeftMid = computePerimeterDistance(QPointF(0, 50), rect, radius, scale);
        check(sLeftMid > sBottomMid, "perimeter distance increases along left edge");
        check(sLeftMid < totalPerim, "perimeter distance before wrap is less than total perimeter");
    }

    // 9. Custom dash pattern (e.g. for marked windows)
    {
        const qreal perimeter = 1000.0;
        const qreal scale = 1.0;

        qreal dashA = 0.0, periodA = 0.0;
        computeCustomDashPattern(12.0, 4.0, perimeter, scale, dashA, periodA);
        const qreal gapA = periodA - dashA;

        qreal dashB = 0.0, periodB = 0.0;
        computeCustomDashPattern(12.0, 16.0, perimeter, scale, dashB, periodB);
        const qreal gapB = periodB - dashB;

        check(gapA > 2.0 && gapA < 6.0, "gap for 4px config is near 4px");
        check(gapB > 12.0 && gapB < 20.0, "gap for 16px config is near 16px");
        check(gapB > gapA + 8.0, "16px gap is strictly larger than 4px gap");

        // Perimeter must be integer multiple of period
        const qreal countA = perimeter / periodA;
        check(std::abs(countA - std::round(countA)) < 0.001, "countA divides perimeter evenly");
        const qreal countB = perimeter / periodB;
        check(std::abs(countB - std::round(countB)) < 0.001, "countB divides perimeter evenly");
    }

    std::printf("geometry_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
