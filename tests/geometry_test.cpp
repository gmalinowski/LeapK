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

    std::printf("geometry_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
