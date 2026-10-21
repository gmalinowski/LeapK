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

enum class Mode
{
    Dominant = 0, // Dominant accent (Vivaldi-style aggressive tracking)
    Balanced = 1  // Balanced average (smooth additive light mixing)
};

struct Settings
{
    Mode mode = Mode::Dominant;
    qreal saturationBoost = 1.3;
    qreal minBrightness = 0.25; // 0.0 - 1.0 (25%)
    qreal maxBrightness = 0.85; // 0.0 - 1.0 (85%)
    float previousHue = -1.0f;  // For hysteresis/stickiness in Dominant mode [0.0, 1.0]
};

/**
 * Balanced Average mode:
 * Uses weighted additive RGB light mixing to prevent erratic hue jumps when a window
 * contains many diverse colorful elements, with perceptual saturation boosting and
 * contrast clamping.
 */
inline QColor processBalancedAverage(const uint8_t *rgba, int pixelCount, const Settings &settings)
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

/**
 * Dominant Accent mode (Vivaldi-style):
 * Discretizes window colors into 24 chromatic hue clusters and heavily scores them by
 * saturation and midtone presence. The most prominent vibrant accent wins decisively,
 * producing aggressive, punchy accent tracking without diluting into muddy averages.
 */
inline QColor processDominantAccent(const uint8_t *rgba, int pixelCount, const Settings &settings)
{
    if (!rgba || pixelCount <= 0) {
        return QColor(61, 174, 233);
    }

    constexpr int kNumBins = 24;
    struct Bucket {
        qreal score = 0.0;
        qreal sumR = 0.0;
        qreal sumG = 0.0;
        qreal sumB = 0.0;
        qreal totalWeight = 0.0;
        int count = 0;
    };

    Bucket buckets[kNumBins];
    qreal unweightedLightnessSum = 0.0;
    int validPixels = 0;

    for (int i = 0; i < pixelCount; ++i) {
        const int idx = i * 4;
        const int r = rgba[idx + 0];
        const int g = rgba[idx + 1];
        const int b = rgba[idx + 2];
        const int a = rgba[idx + 3];

        if (a < 25) {
            continue;
        }

        const QColor col(r, g, b);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        col.getHslF(&h, &s, &l);
        validPixels++;
        unweightedLightnessSum += l;

        // Skip near-monochrome canvas or extreme black/white for chromatic binning
        if (s < 0.10f || l < 0.08f || (l > 0.94f && s < 0.20f)) {
            continue;
        }

        int binIdx = static_cast<int>(h * kNumBins) % kNumBins;
        if (binIdx < 0) {
            binIdx += kNumBins;
        }

        // Vivaldi scoring: strongly emphasize saturated accent/brand colors (s^1.8)
        qreal w = std::pow(static_cast<double>(s), 1.8);

        // Midtone preference (penalize extreme brightness)
        if (l < 0.20f) {
            w *= std::max(0.1, static_cast<double>((l - 0.08f) / 0.12f));
        } else if (l > 0.80f) {
            w *= std::max(0.1, static_cast<double>((0.94f - l) / 0.14f));
        }

        buckets[binIdx].score += w;
        buckets[binIdx].sumR += w * r;
        buckets[binIdx].sumG += w * g;
        buckets[binIdx].sumB += w * b;
        buckets[binIdx].totalWeight += w;
        buckets[binIdx].count++;
    }

    if (validPixels == 0) {
        return QColor(61, 174, 233);
    }

    // Circular 3-tap smoothing to avoid edge artifacts between adjacent hue bins
    qreal smoothedScores[kNumBins];
    for (int i = 0; i < kNumBins; ++i) {
        const int prev = (i + kNumBins - 1) % kNumBins;
        const int next = (i + 1) % kNumBins;
        smoothedScores[i] = buckets[i].score * 0.5 + (buckets[prev].score + buckets[next].score) * 0.25;
    }

    // Apply inertia bonus to the previously active hue if specified (prevents jitter between close candidates)
    if (settings.previousHue >= 0.0f && settings.previousHue <= 1.0f) {
        int prevBin = static_cast<int>(settings.previousHue * kNumBins) % kNumBins;
        if (prevBin < 0) prevBin += kNumBins;
        smoothedScores[prevBin] *= 1.15;
    }

    int bestBin = -1;
    qreal maxScore = 0.0;
    for (int i = 0; i < kNumBins; ++i) {
        if (smoothedScores[i] > maxScore) {
            maxScore = smoothedScores[i];
            bestBin = i;
        }
    }

    // If no meaningful chromatic accent was found (e.g. grayscale document or terminal),
    // fallback to contrast-clamped neutral tone.
    if (bestBin < 0 || maxScore < 0.05) {
        const qreal avgL = unweightedLightnessSum / validPixels;
        const qreal clampedL = qBound<qreal>(settings.minBrightness, avgL, settings.maxBrightness);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    // Reconstruct representative color from the winning bin and its immediate neighbors
    const int prev = (bestBin + kNumBins - 1) % kNumBins;
    const int next = (bestBin + 1) % kNumBins;
    const qreal clusterR = buckets[bestBin].sumR + 0.5 * (buckets[prev].sumR + buckets[next].sumR);
    const qreal clusterG = buckets[bestBin].sumG + 0.5 * (buckets[prev].sumG + buckets[next].sumG);
    const qreal clusterB = buckets[bestBin].sumB + 0.5 * (buckets[prev].sumB + buckets[next].sumB);
    const qreal clusterW = buckets[bestBin].totalWeight + 0.5 * (buckets[prev].totalWeight + buckets[next].totalWeight);

    if (clusterW <= 0.0) {
        const qreal avgL = unweightedLightnessSum / validPixels;
        const qreal clampedL = qBound<qreal>(settings.minBrightness, avgL, settings.maxBrightness);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    const int avgR = qBound(0, qRound(clusterR / clusterW), 255);
    const int avgG = qBound(0, qRound(clusterG / clusterW), 255);
    const int avgB = qBound(0, qRound(clusterB / clusterW), 255);

    QColor winningColor(avgR, avgG, avgB);
    float h = 0.0f, s = 0.0f, l = 0.0f;
    winningColor.getHslF(&h, &s, &l);

    const qreal boostedS = qBound<qreal>(0.0, s * settings.saturationBoost, 1.0);
    const qreal clampedL = qBound<qreal>(settings.minBrightness, l, settings.maxBrightness);

    return QColor::fromHslF(qBound<qreal>(0.0, h, 1.0), boostedS, clampedL);
}

/**
 * Computes an adaptive ambilight color from a raw RGBA buffer of sampled window pixels.
 * Dispatches to Dominant Accent (Vivaldi-style) or Balanced Average mode based on settings.
 */
inline QColor processRgbaSamples(const uint8_t *rgba, int pixelCount, const Settings &settings = Settings())
{
    if (settings.mode == Mode::Balanced) {
        return processBalancedAverage(rgba, pixelCount, settings);
    }
    return processDominantAccent(rgba, pixelCount, settings);
}

} // namespace OutlineAmbilight
