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
 * Uses saturation-weighted circular hue averaging and lightness clamping inspired by Vivaldi's
 * adaptive theme color scoring.
 */
inline QColor processRgbaSamples(const uint8_t *rgba, int pixelCount, const Settings &settings = Settings())
{
    if (!rgba || pixelCount <= 0) {
        return QColor(61, 174, 233);
    }

    qreal sumCos = 0.0;
    qreal sumSin = 0.0;
    qreal sumSaturation = 0.0;
    qreal sumLightness = 0.0;
    qreal totalWeight = 0.0;

    qreal unweightedLightnessSum = 0.0;
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

        unweightedLightnessSum += l;
        validPixels++;

        // Vivaldi-inspired scoring:
        // Strongly favor saturated brand/accent colors over large plain background canvas.
        // Penalize extreme near-black (< 0.08) and extreme near-white (> 0.92, s < 0.15).
        qreal w = std::pow(s, 1.4) + 0.01;

        if (l < 0.08) {
            w *= std::max(0.01, static_cast<double>(l / 0.08));
        } else if (l > 0.92 && s < 0.15) {
            w *= std::max(0.01, static_cast<double>((1.0 - l) / 0.08));
        }

        if (h >= 0.0) {
            const qreal angle = h * 2.0 * M_PI;
            sumCos += w * std::cos(angle);
            sumSin += w * std::sin(angle);
        }

        sumSaturation += w * s;
        sumLightness += w * l;
        totalWeight += w;
    }

    if (validPixels == 0 || totalWeight <= 0.0) {
        return QColor(61, 174, 233);
    }

    const qreal avgSaturation = sumSaturation / totalWeight;

    // Monochrome / grayscale fallback
    if (avgSaturation < 0.04) {
        const qreal avgL = unweightedLightnessSum / validPixels;
        const qreal clampedL = qBound<qreal>(settings.minBrightness, avgL, settings.maxBrightness);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    // Circular mean hue
    qreal avgH = std::atan2(sumSin, sumCos) / (2.0 * M_PI);
    if (avgH < 0.0) {
        avgH += 1.0;
    }

    // Apply saturation boost
    const qreal boostedS = qBound<qreal>(0.0, avgSaturation * settings.saturationBoost, 1.0);

    // Apply lightness clamp
    const qreal avgL = sumLightness / totalWeight;
    const qreal clampedL = qBound<qreal>(settings.minBrightness, avgL, settings.maxBrightness);

    return QColor::fromHslF(qBound<qreal>(0.0, avgH, 1.0), boostedS, clampedL);
}

} // namespace OutlineAmbilight
