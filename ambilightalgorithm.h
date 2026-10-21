// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QtGlobal>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace OutlineAmbilight
{

struct Settings
{
    qreal saturationBoost = 1.3;
    qreal minBrightness = 0.25; // 0.0 - 1.0 (25%)
    qreal maxBrightness = 0.85; // 0.0 - 1.0 (85%)
};

/**
 * Computes an adaptive ambilight color from a raw RGBA buffer of sampled window pixels.
 * Uses weighted additive RGB light mixing to prevent erratic hue jumps when a window
 * contains many diverse colorful elements, with perceptual saturation boosting and
 * contrast clamping.
 */
inline QColor processRgbaSamples(const uint8_t *rgba, int pixelCount, const Settings &settings = Settings())
{
    if (!rgba || pixelCount <= 0) {
        return QColor(61, 174, 233);
    }

    qreal sumR = 0.0;
    qreal sumG = 0.0;
    qreal sumB = 0.0;
    qreal totalWeight = 0.0;
    int validPixels = 0;

    for (int i = 0; i < pixelCount; ++i) {
        const int idx = i * 4;
        const int r = rgba[idx + 0];
        const int g = rgba[idx + 1];
        const int b = rgba[idx + 2];
        const int a = rgba[idx + 3];

        if (a < 25) {
            continue; // Skip transparent pixels
        }

        const QColor col(r, g, b);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        col.getHslF(&h, &s, &l);
        validPixels++;

        // Balanced perceptual weighting:
        // Saturated colors have priority over dull gray canvas, but in an additive
        // blend rather than polar coordinates to keep multi-colored windows stable.
        qreal w = s + 0.15;

        // Down-weight extreme backgrounds (pitch black or pure white)
        if (l < 0.08) {
            w *= std::max(0.05, static_cast<double>(l / 0.08));
        } else if (l > 0.92 && s < 0.15) {
            w *= std::max(0.05, static_cast<double>((1.0 - l) / 0.08));
        }

        sumR += w * r;
        sumG += w * g;
        sumB += w * b;
        totalWeight += w;
    }

    if (validPixels == 0 || totalWeight <= 0.0) {
        return QColor(61, 174, 233);
    }

    const int avgR = qBound(0, qRound(sumR / totalWeight), 255);
    const int avgG = qBound(0, qRound(sumG / totalWeight), 255);
    const int avgB = qBound(0, qRound(sumB / totalWeight), 255);

    QColor avgColor(avgR, avgG, avgB);
    float h = 0.0f, s = 0.0f, l = 0.0f;
    avgColor.getHslF(&h, &s, &l);

    // Monochrome / grayscale fallback
    if (s < 0.03f) {
        const qreal clampedL = qBound<qreal>(settings.minBrightness, l, settings.maxBrightness);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    // Apply saturation boost
    const qreal boostedS = qBound<qreal>(0.0, s * settings.saturationBoost, 1.0);

    // Apply lightness clamp
    const qreal clampedL = qBound<qreal>(settings.minBrightness, l, settings.maxBrightness);

    return QColor::fromHslF(qBound<qreal>(0.0, h, 1.0), boostedS, clampedL);
}

} // namespace OutlineAmbilight
