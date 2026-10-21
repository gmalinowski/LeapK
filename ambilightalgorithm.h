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
    Smart = 0,    // Smart Adaptive (Hybrid): Dominant accent with automatic vibrancy boost when matching average tone
    Dominant = 1, // Dominant accent: Decisively locks onto the most prominent vibrant accent
    Balanced = 2  // Balanced average: Smooth additive light mixing of all window colors
};

struct Settings
{
    Mode mode = Mode::Smart;
    qreal saturationLeveling = 0.65; // Target saturation level [0.0, 1.0] (default 65%)
    qreal fallbackStrength = 0.50;   // Fallback contrast differentiation strength [0.0, 1.0] (default 50%)
    qreal minBrightness = 0.25;      // 0.0 - 1.0 (25%)
    qreal maxBrightness = 0.85;      // 0.0 - 1.0 (85%)
    float previousHue = -1.0f;       // For hysteresis/stickiness in Dominant mode [0.0, 1.0]
};

/**
 * Calculates a high-contrast monochrome outline luminance based on window lightness.
 * Ensures grayscale windows are always crisp and clearly defined:
 * - Dark windows (e.g. dark mode, dark terminal) receive a bright, crisp silver outline.
 * - Light windows (e.g. white PDF, document) receive a sleek, dark charcoal outline.
 * - Settings.fallbackStrength scales the separation: 0.0 = moderate, 1.0 = bold distinction.
 */
inline qreal getHighContrastMonochromeLuminance(qreal windowL, qreal minB, qreal maxB, qreal fallbackStrength = 0.50)
{
    const qreal fs = qBound<qreal>(0.0, fallbackStrength, 1.0);
    qreal targetL;
    if (windowL < 0.55) {
        // Dark to mid-dark window -> bright silver outline (e.g. 0.70 - 0.95)
        const qreal base = maxB - (0.15 * (1.0 - fs) * (windowL / 0.55));
        targetL = qMax<qreal>(0.70, base);
    } else {
        // Mid-light to bright window -> dark charcoal outline (e.g. 0.15 - 0.30)
        const qreal base = minB + (0.15 * (1.0 - fs) * ((1.0 - windowL) / 0.45));
        targetL = qMin<qreal>(0.30, base);
    }
    return qBound<qreal>(minB, targetL, maxB);
}

/**
 * Normalizes saturation towards settings.saturationLeveling:
 * - Low saturation below achromatic threshold (s < 0.22) is smoothly attenuated to pure monochrome
 *   (s = 0.0), completely preventing cool slate/gray themes (like KDE Breeze, Firefox, VS Code)
 *   from producing an accidental grayish-violet or blue cast.
 * - Real chromatic pastels (s >= 0.22) are gently boosted towards target level.
 * - Harsh neon colors (s > 0.70) are softly compressed towards target level.
 */
inline qreal applySaturationLeveling(qreal s, qreal target)
{
    // Achromatic deadband: suppress accidental tint in gray/slate themes
    if (s < 0.22) {
        if (s < 0.10) {
            return 0.0;
        }
        const qreal t = (s - 0.10) / 0.12;
        return s * (t * t * (3.0 - 2.0 * t)); // smooth cubic Hermite falloff into pure monochrome
    }
    const qreal weight = qBound<qreal>(0.0, (s - 0.22) / 0.18, 1.0);
    const qreal leveled = s + weight * 0.5 * (target - s);
    return qBound<qreal>(0.0, leveled, 1.0);
}

/**
 * Balanced Average mode:
 * Uses weighted additive RGB light mixing to prevent erratic hue jumps when a window
 * contains many diverse colorful elements, with perceptual saturation leveling and
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

    // Monochrome / grayscale fallback (including slate and cool gray themes)
    if (s < 0.22f) {
        const qreal clampedL = getHighContrastMonochromeLuminance(l, settings.minBrightness, settings.maxBrightness, settings.fallbackStrength);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    // Apply saturation leveling
    const qreal leveledS = applySaturationLeveling(s, settings.saturationLeveling);

    // If leveled saturation falls into the achromatic range, treat as pure monochrome
    if (leveledS < 0.02) {
        const qreal clampedL = getHighContrastMonochromeLuminance(l, settings.minBrightness, settings.maxBrightness, settings.fallbackStrength);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    // Apply lightness clamp
    const qreal clampedL = qBound<qreal>(settings.minBrightness, l, settings.maxBrightness);

    return QColor::fromHslF(qBound<qreal>(0.0, h, 1.0), leveledS, clampedL);
}

/**
 * Dominant Accent mode:
 * Discretizes window colors into 24 chromatic hue clusters and scores them by
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

        // Skip near-monochrome canvas (gray, slate, charcoal) and extreme black/white for chromatic binning
        if (s < 0.22f || l < 0.08f || (l > 0.93f && s < 0.28f)) {
            continue;
        }

        int binIdx = static_cast<int>(h * kNumBins) % kNumBins;
        if (binIdx < 0) {
            binIdx += kNumBins;
        }

        // Scoring: strongly emphasize saturated accent/brand colors (s^1.8)
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
    // fallback to high-contrast neutral tone.
    if (bestBin < 0 || maxScore < 0.12) {
        const qreal avgL = unweightedLightnessSum / validPixels;
        const qreal clampedL = getHighContrastMonochromeLuminance(avgL, settings.minBrightness, settings.maxBrightness, settings.fallbackStrength);
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
        const qreal clampedL = getHighContrastMonochromeLuminance(avgL, settings.minBrightness, settings.maxBrightness, settings.fallbackStrength);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    const int avgR = qBound(0, qRound(clusterR / clusterW), 255);
    const int avgG = qBound(0, qRound(clusterG / clusterW), 255);
    const int avgB = qBound(0, qRound(clusterB / clusterW), 255);

    QColor winningColor(avgR, avgG, avgB);
    float h = 0.0f, s = 0.0f, l = 0.0f;
    winningColor.getHslF(&h, &s, &l);

    // If the winning cluster itself has low saturation, it is not a genuine chromatic accent
    if (s < 0.25f) {
        const qreal avgL = unweightedLightnessSum / validPixels;
        const qreal clampedL = getHighContrastMonochromeLuminance(avgL, settings.minBrightness, settings.maxBrightness, settings.fallbackStrength);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    const qreal leveledS = applySaturationLeveling(s, settings.saturationLeveling);
    if (leveledS < 0.02) {
        const qreal avgL = unweightedLightnessSum / validPixels;
        const qreal clampedL = getHighContrastMonochromeLuminance(avgL, settings.minBrightness, settings.maxBrightness, settings.fallbackStrength);
        return QColor::fromHslF(0.0, 0.0, clampedL);
    }

    const qreal clampedL = qBound<qreal>(settings.minBrightness, l, settings.maxBrightness);

    return QColor::fromHslF(qBound<qreal>(0.0, h, 1.0), leveledS, clampedL);
}

/**
 * Smart Adaptive (Hybrid) mode:
 * Evaluates both the dominant accent and the balanced average of the window content.
 * When the dominant accent is very similar in hue and tone to the balanced average
 * (indicating a homogeneous window or that the whole window content shares that color),
 * it dynamically modifies saturation and contrast so the outline pops out vibrantly instead
 * of blending into the window. The strength of this differentiation is controlled by
 * settings.fallbackStrength (0%: subtle/identical, 100%: strong contrast & vibrance pop).
 */
inline QColor processSmartAdaptive(const uint8_t *rgba, int pixelCount, const Settings &settings)
{
    Settings subSettings = settings;

    // 1. Calculate dominant accent
    subSettings.mode = Mode::Dominant;
    const QColor dom = processDominantAccent(rgba, pixelCount, subSettings);

    // 2. Calculate balanced average
    subSettings.mode = Mode::Balanced;
    const QColor bal = processBalancedAverage(rgba, pixelCount, subSettings);

    float hd = 0.0f, sd = 0.0f, ld = 0.0f;
    dom.getHslF(&hd, &sd, &ld);

    float hb = 0.0f, sb = 0.0f, lb = 0.0f;
    bal.getHslF(&hb, &sb, &lb);

    // If dominant color is monochrome or near-monochrome, or if the whole window is achromatic,
    // return high-contrast monochrome
    if (sd < 0.10f || (sb < 0.10f && sd < 0.25f)) {
        return dom;
    }

    // Circular hue difference [0.0, 0.5]
    qreal hueDiff = std::abs(hd - hb);
    if (hueDiff > 0.5) {
        hueDiff = 1.0 - hueDiff;
    }

    const qreal lightDiff = std::abs(ld - lb);
    const qreal satDiff = std::abs(sd - sb);

    // Circular hue similarity: 1.0 when identical, decays to 0.0 at 60 degrees (0.166)
    const qreal hueSimilarity = std::max(0.0, 1.0 - (hueDiff / 0.166));

    // Tone similarity in lightness and saturation
    const qreal toneSimilarity = std::max(0.0, 1.0 - (lightDiff * 0.8 + satDiff * 0.4));

    // Overall similarity factor [0.0, 1.0]
    const qreal similarity = qBound<qreal>(0.0, hueSimilarity * toneSimilarity, 1.0);

    // When dominant accent aligns closely with the window average, apply fallback contrast differentiation
    if (similarity > 0.25 && settings.fallbackStrength > 0.001) {
        const qreal extraBoost = 1.0 + (0.60 * settings.fallbackStrength) * similarity;
        const qreal boostedS = qBound<qreal>(0.0, sd * extraBoost, 1.0);

        // Optimize lightness contrast against homogeneous backgrounds
        qreal adjustedL = ld;
        if (adjustedL > 0.65) {
            const qreal lightShift = (adjustedL - settings.minBrightness) * (0.50 * settings.fallbackStrength) * similarity;
            adjustedL = qMax<qreal>(settings.minBrightness, adjustedL - lightShift);
        } else if (adjustedL < 0.45) {
            const qreal lightShift = (settings.maxBrightness - adjustedL) * (0.60 * settings.fallbackStrength) * similarity;
            adjustedL = qMin<qreal>(settings.maxBrightness, adjustedL + lightShift);
        }

        const qreal clampedL = qBound<qreal>(settings.minBrightness, adjustedL, settings.maxBrightness);
        return QColor::fromHslF(qBound<qreal>(0.0, hd, 1.0), boostedS, clampedL);
    }

    return dom;
}

/**
 * Computes an adaptive ambilight color from a raw RGBA buffer of sampled window pixels.
 * Dispatches to Smart Adaptive (Hybrid), Dominant Accent, or Balanced Average mode based on settings.
 */
inline QColor processRgbaSamples(const uint8_t *rgba, int pixelCount, const Settings &settings = Settings())
{
    switch (settings.mode) {
    case Mode::Smart:
        return processSmartAdaptive(rgba, pixelCount, settings);
    case Mode::Balanced:
        return processBalancedAverage(rgba, pixelCount, settings);
    case Mode::Dominant:
    default:
        return processDominantAccent(rgba, pixelCount, settings);
    }
}

} // namespace OutlineAmbilight
