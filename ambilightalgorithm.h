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
    bool ensureContrast = true;      // Guarantees contrast against window perimeter/borders & desktop background
    int sampleWidth = 32;            // Grid width of the sampled RGBA buffer
    int sampleHeight = 32;           // Grid height of the sampled RGBA buffer
    const uint8_t *outsideRgba = nullptr; // Samples immediately outside the window (wallpaper / desktop background)
    int outsidePixelCount = 0;            // Number of sampled pixels in outsideRgba
};

/**
 * Calculates a high-contrast monochrome outline luminance based on window lightness.
 * Used exclusively for Balanced Average mode when reflecting neutral ambient lighting:
 * - Dark windows receive a bright, crisp silver outline.
 * - Light windows receive a sleek, dark charcoal outline.
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
        targetL = qMin<qreal>(0.28, base);
    }
    return qBound<qreal>(minB, targetL, maxB);
}

/**
 * Normalizes saturation towards settings.saturationLeveling:
 * - Low saturation below achromatic threshold (s < 0.22) is smoothly attenuated to pure monochrome
 *   (s = 0.0), completely preventing cool slate/gray themes (like KDE Breeze, Firefox, VS Code)
 *   from producing an accidental grayish-violet or blue cast.
 * - Real chromatic pastels (s >= 0.22) are boosted towards target level.
 * - Harsh neon colors (s > target) are softly compressed towards target level.
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
    const qreal blendRate = (s > target) ? 0.5 : 0.85;
    const qreal leveled = s + weight * blendRate * (target - s);
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
 * Resolves a vibrant chromatic fallback accent color when a window has no detected color
 * (e.g. monochrome dark or white windows, black-and-white text documents, terminals):
 * - Black, white, and shades of gray are completely excluded in Accent/Smart modes.
 * - Starts from candidate hue (settings.previousHue if valid, or KDE Breeze Blue 0.555).
 * - If settings.ensureContrast is enabled: checks contrast in BOTH directions:
 *     1. Inside the window: samples the window perimeter (decorations/frame/edges).
 *     2. Outside the window: samples the desktop background / wallpaper surrounding the window.
 *   If the candidate hue clashes with either side, shifts the hue (by +0.25, up to 4 attempts)
 *   until a high-contrast hue is found. Tunes lightness for maximum contrast against both sides.
 * - If settings.ensureContrast is disabled: uses candidate hue directly without perimeter shifting.
 */
inline QColor resolveFallbackAccent(const uint8_t *rgba, int width, int height, const Settings &settings)
{
    float candidateH = (settings.previousHue >= 0.0f && settings.previousHue <= 1.0f) ? settings.previousHue : 0.555f;
    float perimH = 0.0f, perimS = 0.0f, perimL = 0.5f;
    bool insideHasColor = false;

    if (settings.ensureContrast && rgba && width >= 4 && height >= 4) {
        qreal sumR = 0.0, sumG = 0.0, sumB = 0.0;
        int totalCount = 0;
        const int ringThickness = std::max(1, std::min(width, height) / 8);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const bool isTop = (y >= height - ringThickness);
                const bool isBottom = (y < ringThickness);
                const bool isLeft = (x < ringThickness);
                const bool isRight = (x >= width - ringThickness);

                if (isTop || isBottom || isLeft || isRight) {
                    const int idx = (y * width + x) * 4;
                    const int r = rgba[idx + 0];
                    const int g = rgba[idx + 1];
                    const int b = rgba[idx + 2];
                    const int a = rgba[idx + 3];
                    if (a >= 25) {
                        sumR += r;
                        sumG += g;
                        sumB += b;
                        totalCount++;
                    }
                }
            }
        }

        if (totalCount > 0) {
            const int avgR = qBound(0, qRound(sumR / totalCount), 255);
            const int avgG = qBound(0, qRound(sumG / totalCount), 255);
            const int avgB = qBound(0, qRound(sumB / totalCount), 255);
            const QColor perimColor(avgR, avgG, avgB);
            perimColor.getHslF(&perimH, &perimS, &perimL);

            const qreal chroma = (std::max({avgR, avgG, avgB}) - std::min({avgR, avgG, avgB})) / 255.0;
            if (perimS >= 0.18f && chroma >= 0.08) {
                insideHasColor = true;
            }
        }
    }

    // Analyze outside perimeter (desktop wallpaper / background behind window outline)
    float outH = 0.0f, outS = 0.0f, outL = 0.5f;
    bool outHasColor = false;
    bool hasOutside = false;

    if (settings.ensureContrast && settings.outsideRgba && settings.outsidePixelCount > 0) {
        qreal sumR = 0.0, sumG = 0.0, sumB = 0.0;
        int totalCount = 0;
        for (int i = 0; i < settings.outsidePixelCount; ++i) {
            const int idx = i * 4;
            const int r = settings.outsideRgba[idx + 0];
            const int g = settings.outsideRgba[idx + 1];
            const int b = settings.outsideRgba[idx + 2];
            const int a = settings.outsideRgba[idx + 3];
            if (a >= 25) {
                sumR += r;
                sumG += g;
                sumB += b;
                totalCount++;
            }
        }
        if (totalCount > 0) {
            hasOutside = true;
            const int avgR = qBound(0, qRound(sumR / totalCount), 255);
            const int avgG = qBound(0, qRound(sumG / totalCount), 255);
            const int avgB = qBound(0, qRound(sumB / totalCount), 255);
            const QColor outColor(avgR, avgG, avgB);
            outColor.getHslF(&outH, &outS, &outL);

            const qreal chroma = (std::max({avgR, avgG, avgB}) - std::min({avgR, avgG, avgB})) / 255.0;
            if (outS >= 0.18f && chroma >= 0.08) {
                outHasColor = true;
            }
        }
    }

    // Shift candidate hue if it clashes with either inside (window) or outside (wallpaper)
    if (settings.ensureContrast && (insideHasColor || outHasColor)) {
        const auto clashesWith = [](float cand, float targetH, bool active) {
            if (!active) return false;
            qreal diff = std::abs(cand - targetH);
            if (diff > 0.5) diff = 1.0 - diff;
            return (diff < 0.15);
        };

        for (int attempt = 0; attempt < 4; ++attempt) {
            if (!clashesWith(candidateH, perimH, insideHasColor) &&
                !clashesWith(candidateH, outH, outHasColor)) {
                break; // Achieved contrast with both inside and outside
            }
            candidateH = std::fmod(candidateH + 0.25f, 1.0f);
        }
    }

    const qreal targetS = qBound<qreal>(0.75, qMax<qreal>(settings.saturationLeveling, 0.85), 1.0);
    qreal targetL = 0.50;

    if (!hasOutside) {
        targetL = (perimL >= 0.50) ? qBound<qreal>(settings.minBrightness, 0.40, 0.44)
                                   : qBound<qreal>(0.68, settings.maxBrightness, 0.78);
    } else {
        if (perimL >= 0.50 && outL >= 0.50) {
            // Both inside and outside are light -> deep, dark contrasting tone
            targetL = qBound<qreal>(settings.minBrightness, 0.40, 0.44);
        } else if (perimL < 0.50 && outL < 0.50) {
            // Both inside and outside are dark -> luminous bright neon tone
            targetL = qBound<qreal>(0.68, settings.maxBrightness, 0.78);
        } else {
            // One side light, one side dark -> midtone pop that stands out on both
            targetL = qBound<qreal>(settings.minBrightness, 0.56, settings.maxBrightness);
        }
    }

    return QColor::fromHslF(qBound<qreal>(0.0, candidateH, 1.0), targetS, targetL);
}

/**
 * Dominant Accent mode:
 * Discretizes window colors into 24 chromatic hue clusters and scores them by
 * saturation and midtone presence. The most prominent vibrant accent wins decisively,
 * producing aggressive, punchy accent tracking without diluting into muddy averages.
 * Grayscale (black, gray, white) is excluded: an accent is always a vibrant chromatic color.
 */
inline QColor processDominantAccent(const uint8_t *rgba, int pixelCount, const Settings &settings)
{
    if (!rgba || pixelCount <= 0) {
        return QColor(61, 174, 233);
    }

    int gridW = settings.sampleWidth;
    int gridH = settings.sampleHeight;
    if (gridW <= 0 || gridH <= 0 || gridW * gridH != pixelCount) {
        gridW = static_cast<int>(std::sqrt(pixelCount));
        gridH = (gridW > 0) ? (pixelCount / gridW) : 0;
    }

    constexpr int kNumBins = 24;
    struct Bucket {
        qreal score = 0.0;
        qreal sumR = 0.0;
        qreal sumG = 0.0;
        qreal sumB = 0.0;
        qreal totalWeight = 0.0;
        int count = 0;
        int interiorCount = 0;
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
        float hc = 0.0f, sc = 0.0f, lc = 0.0f;
        col.getHslF(&hc, &sc, &lc);
        validPixels++;
        unweightedLightnessSum += lc;

        // Skip near-monochrome canvas (gray, slate, charcoal) and extreme black/white for chromatic binning
        if (sc < 0.22f || lc < 0.08f || (lc > 0.93f && sc < 0.28f)) {
            continue;
        }

        int binIdx = static_cast<int>(hc * kNumBins) % kNumBins;
        if (binIdx < 0) {
            binIdx += kNumBins;
        }

        // Scoring: strongly emphasize saturated accent/brand colors (s^1.8)
        qreal weight = std::pow(static_cast<double>(sc), 1.8);

        // Midtone preference (penalize extreme brightness)
        if (lc < 0.20f) {
            weight *= std::max(0.1, static_cast<double>((lc - 0.08f) / 0.12f));
        } else if (lc > 0.80f) {
            weight *= std::max(0.1, static_cast<double>((0.94f - lc) / 0.14f));
        }

        const int x = (gridW > 0) ? (i % gridW) : 0;
        const int y = (gridW > 0) ? (i / gridW) : 0;
        const int ringThickness = std::max(1, std::min(gridW, gridH) / 8);
        const bool isPerimeter = (x < ringThickness || x >= gridW - ringThickness ||
                                  y < ringThickness || y >= gridH - ringThickness);

        buckets[binIdx].score += weight;
        buckets[binIdx].sumR += weight * r;
        buckets[binIdx].sumG += weight * g;
        buckets[binIdx].sumB += weight * b;
        buckets[binIdx].totalWeight += weight;
        buckets[binIdx].count++;
        if (!isPerimeter) {
            buckets[binIdx].interiorCount++;
        }
    }

    if (validPixels == 0) {
        return resolveFallbackAccent(rgba, gridW, gridH, settings);
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
        smoothedScores[prevBin] *= 1.50;
    }

    int bestBin = -1;
    qreal maxScore = 0.0;
    int bestCount = 0;
    for (int i = 0; i < kNumBins; ++i) {
        if (smoothedScores[i] > maxScore) {
            maxScore = smoothedScores[i];
            bestBin = i;
            bestCount = buckets[i].count;
        }
    }

    // An accent is dominant if it represents a noticeable visual element of the window.
    // Stray single-pixel noise is filtered out, but real UI accents like red underlines,
    // colored buttons, badges, links, and image content are faithfully captured.
    const int minPixelQuorum = std::max(3, validPixels / 250); // ~0.4% (e.g. 4 pixels in 32x32)
    const qreal minScoreQuorum = std::max(1.8, validPixels * 0.002);

    // If no meaningful chromatic accent was found in the window content:
    // In Dominant mode, black, gray, and white are excluded.
    // If chromatic pixels exist exclusively on the window perimeter (the outer frame/decorations)
    // while the interior page content is monochrome, the window has no content accent to detect.
    // Fallback to a vibrant chromatic accent starting from candidate hue, shifting if clash occurs.
    const bool isPerimeterDecorationOnly = (gridW >= 12 && gridH >= 12 && bestBin >= 0 && buckets[bestBin].interiorCount == 0);
    if (bestBin < 0 || maxScore < minScoreQuorum || bestCount < minPixelQuorum || isPerimeterDecorationOnly) {
        return resolveFallbackAccent(rgba, gridW, gridH, settings);
    }

    // Reconstruct representative color from the winning bin and its immediate neighbors
    const int prev = (bestBin + kNumBins - 1) % kNumBins;
    const int next = (bestBin + 1) % kNumBins;
    const qreal clusterR = buckets[bestBin].sumR + 0.5 * (buckets[prev].sumR + buckets[next].sumR);
    const qreal clusterG = buckets[bestBin].sumG + 0.5 * (buckets[prev].sumG + buckets[next].sumG);
    const qreal clusterB = buckets[bestBin].sumB + 0.5 * (buckets[prev].sumB + buckets[next].sumB);
    const qreal clusterW = buckets[bestBin].totalWeight + 0.5 * (buckets[prev].totalWeight + buckets[next].totalWeight);

    if (clusterW <= 0.0) {
        return resolveFallbackAccent(rgba, gridW, gridH, settings);
    }

    const int avgR = qBound(0, qRound(clusterR / clusterW), 255);
    const int avgG = qBound(0, qRound(clusterG / clusterW), 255);
    const int avgB = qBound(0, qRound(clusterB / clusterW), 255);

    QColor winningColor(avgR, avgG, avgB);
    float h = 0.0f, s = 0.0f, l = 0.0f;
    winningColor.getHslF(&h, &s, &l);

    // If the winning cluster itself has low saturation, it is not a genuine chromatic accent
    if (s < 0.25f) {
        return resolveFallbackAccent(rgba, gridW, gridH, settings);
    }

    // Ensure detected chromatic accents are rich and vivid (at least 75% saturation target)
    const qreal targetS = qBound<qreal>(0.75, qMax<qreal>(settings.saturationLeveling, 0.85), 1.0);
    const qreal leveledS = applySaturationLeveling(s, targetS);
    const qreal finalS = qBound<qreal>(0.0, qMax<qreal>(leveledS, 0.75), 1.0);
    const qreal clampedL = qBound<qreal>(settings.minBrightness, l, settings.maxBrightness);

    return QColor::fromHslF(qBound<qreal>(0.0, h, 1.0), finalS, clampedL);
}

/**
 * Smart Adaptive (Hybrid) mode:
 * Evaluates both the dominant accent and the balanced average of the window content.
 * When the dominant accent is very similar in hue and tone to the balanced average
 * (indicating a homogeneous window or that the whole window content shares that color),
 * it dynamically modifies saturation and contrast so the outline pops out vibrantly instead
 * of blending into the window. The strength of this differentiation is controlled by
 * settings.fallbackStrength (0%: subtle/identical, 100%: strong contrast & vibrance pop).
 * Grayscale (black, gray, white) is excluded: always returns a rich chromatic color.
 */
inline QColor processSmartAdaptive(const uint8_t *rgba, int pixelCount, const Settings &settings)
{
    Settings subSettings = settings;

    // 1. Calculate dominant accent (always chromatic)
    subSettings.mode = Mode::Dominant;
    const QColor dom = processDominantAccent(rgba, pixelCount, subSettings);

    // 2. Calculate balanced average
    subSettings.mode = Mode::Balanced;
    const QColor bal = processBalancedAverage(rgba, pixelCount, subSettings);

    float hd = 0.0f, sd = 0.0f, ld = 0.0f;
    dom.getHslF(&hd, &sd, &ld);

    float hb = 0.0f, sb = 0.0f, lb = 0.0f;
    bal.getHslF(&hb, &sb, &lb);

    // If balanced average is achromatic, dominant accent stands on its own
    if (sb < 0.15f) {
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
    const qreal toneSimilarity = std::max(0.0, 1.0 - (lightDiff * 0.8 + satDiff * 0.4));
    const qreal similarity = qBound<qreal>(0.0, hueSimilarity * toneSimilarity, 1.0);

    // When dominant accent aligns closely with the window average, apply fallback contrast differentiation
    if (similarity > 0.25 && settings.fallbackStrength > 0.001) {
        const qreal extraBoost = 1.0 + (0.60 * settings.fallbackStrength) * similarity;
        const qreal boostedS = qBound<qreal>(0.75, sd * extraBoost, 1.0);

        // Optimize lightness contrast against homogeneous backgrounds:
        // When the window is overwhelmingly homogeneous in the same color,
        // push lightness away from the background lightness lb to ensure edge separation.
        qreal adjustedL = ld;
        if (lb > 0.60) {
            const qreal lightShift = (adjustedL - settings.minBrightness) * (0.50 * settings.fallbackStrength) * similarity;
            adjustedL = qMax<qreal>(settings.minBrightness, adjustedL - lightShift);
        } else if (lb < 0.45) {
            const qreal lightShift = (settings.maxBrightness - adjustedL) * (0.60 * settings.fallbackStrength) * similarity;
            adjustedL = qMin<qreal>(settings.maxBrightness, adjustedL + lightShift);
        }

        const qreal clampedL = qBound<qreal>(settings.minBrightness, adjustedL, settings.maxBrightness);
        return QColor::fromHslF(qBound<qreal>(0.0, hd, 1.0), boostedS, clampedL);
    }

    return dom;
}

/**
 * Analyzes the perimeter where the outline is drawn and ensures that targetColor
 * maintains high contrast against the window edges and borders:
 * - If the window has colored borders/titlebars matching targetColor, shifts hue (up to 4 steps)
 *   to guarantee unmistakable color separation.
 * - If perimeter is predominantly light (e.g. white web page), enforces deep, rich accent tone (L <= 0.45).
 * - If perimeter is predominantly dark (e.g. dark mode, terminal), enforces luminous neon tone (L >= 0.68).
 */
inline QColor enforcePerimeterContrast(const QColor &targetColor, const uint8_t *rgba, int width, int height, const Settings &settings)
{
    if (!settings.ensureContrast || !targetColor.isValid() || !rgba || width < 4 || height < 4) {
        return targetColor;
    }

    qreal sumR = 0.0, sumG = 0.0, sumB = 0.0;
    int totalCount = 0;

    const int ringThickness = std::max(1, std::min(width, height) / 8);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool isTop = (y >= height - ringThickness);
            const bool isBottom = (y < ringThickness);
            const bool isLeft = (x < ringThickness);
            const bool isRight = (x >= width - ringThickness);

            if (isTop || isBottom || isLeft || isRight) {
                const int idx = (y * width + x) * 4;
                const int r = rgba[idx + 0];
                const int g = rgba[idx + 1];
                const int b = rgba[idx + 2];
                const int a = rgba[idx + 3];
                if (a >= 25) {
                    sumR += r;
                    sumG += g;
                    sumB += b;
                    totalCount++;
                }
            }
        }
    }

    if (totalCount == 0) {
        return targetColor;
    }

    const int avgR = qBound(0, qRound(sumR / totalCount), 255);
    const int avgG = qBound(0, qRound(sumG / totalCount), 255);
    const int avgB = qBound(0, qRound(sumB / totalCount), 255);
    const QColor perimColor(avgR, avgG, avgB);

    float hp = 0.0f, sp = 0.0f, lp = 0.0f;
    perimColor.getHslF(&hp, &sp, &lp);

    float ht = 0.0f, st = 0.0f, lt = 0.0f;
    targetColor.getHslF(&ht, &st, &lt);

    const qreal insideChroma = (std::max({avgR, avgG, avgB}) - std::min({avgR, avgG, avgB})) / 255.0;
    const bool insideHasColor = (sp >= 0.18f && insideChroma >= 0.08);

    // Analyze outside perimeter (desktop wallpaper / background behind window outline)
    float outH = 0.0f, outS = 0.0f, outL = 0.5f;
    bool hasOutside = false;
    bool outHasColor = false;

    if (settings.outsideRgba && settings.outsidePixelCount > 0) {
        qreal sumOutR = 0.0, sumOutG = 0.0, sumOutB = 0.0;
        int outCount = 0;
        for (int i = 0; i < settings.outsidePixelCount; ++i) {
            const int idx = i * 4;
            const int r = settings.outsideRgba[idx + 0];
            const int g = settings.outsideRgba[idx + 1];
            const int b = settings.outsideRgba[idx + 2];
            const int a = settings.outsideRgba[idx + 3];
            if (a >= 25) {
                sumOutR += r;
                sumOutG += g;
                sumOutB += b;
                outCount++;
            }
        }
        if (outCount > 0) {
            hasOutside = true;
            const int avgOutR = qBound(0, qRound(sumOutR / outCount), 255);
            const int avgOutG = qBound(0, qRound(sumOutG / outCount), 255);
            const int avgOutB = qBound(0, qRound(sumOutB / outCount), 255);
            const QColor outColor(avgOutR, avgOutG, avgOutB);
            outColor.getHslF(&outH, &outS, &outL);

            const qreal outChroma = (std::max({avgOutR, avgOutG, avgOutB}) - std::min({avgOutR, avgOutG, avgOutB})) / 255.0;
            if (outS >= 0.18f && outChroma >= 0.08) {
                outHasColor = true;
            }
        }
    }

    const auto clashesWith = [](float cand, float targetH, bool active) {
        if (!active) return false;
        qreal diff = std::abs(cand - targetH);
        if (diff > 0.5) diff = 1.0 - diff;
        return (diff < 0.15);
    };

    // If outside wallpaper is chromatic and outline clashes with it, shift hue away from wallpaper
    if (st >= 0.20f && outHasColor && clashesWith(ht, outH, outHasColor)) {
        for (int attempt = 0; attempt < 4; ++attempt) {
            if (!clashesWith(ht, outH, outHasColor) && !clashesWith(ht, hp, insideHasColor)) {
                break;
            }
            ht = std::fmod(ht + 0.25f, 1.0f);
        }
    }

    // Two-sided lightness contrast when outside wallpaper is available
    if (hasOutside) {
        if (st >= 0.20f) {
            if (lp >= 0.50 && outL >= 0.50) {
                // Both inside and outside are light -> deep, rich accent tone
                lt = qBound<qreal>(settings.minBrightness, qMin<qreal>(lt, 0.42), 0.45);
                st = qMin<qreal>(1.0, qMax<qreal>(st, 0.82));
            } else if (lp < 0.50 && outL < 0.50) {
                // Both inside and outside are dark -> luminous bright neon tone
                lt = qBound<qreal>(0.65, qMax<qreal>(lt, 0.70), settings.maxBrightness);
                st = qMin<qreal>(1.0, qMax<qreal>(st, 0.85));
            } else {
                // One side light, one side dark -> vivid midtone pop contrasting with both
                lt = qBound<qreal>(settings.minBrightness, 0.56, settings.maxBrightness);
                st = qMin<qreal>(1.0, qMax<qreal>(st, 0.82));
            }
            return QColor::fromHslF(qBound<qreal>(0.0, ht, 1.0), st, lt);
        } else {
            // Monochrome outline
            if (lp >= 0.50 && outL >= 0.50) {
                const qreal darkL = qBound<qreal>(settings.minBrightness, 0.25, 0.28);
                return QColor::fromHslF(0.0, 0.0, darkL);
            } else if (lp < 0.50 && outL < 0.50) {
                const qreal brightL = qBound<qreal>(0.80, settings.maxBrightness, 1.0);
                return QColor::fromHslF(0.0, 0.0, brightL);
            } else {
                return QColor::fromHslF(0.0, 0.0, 0.55);
            }
        }
    }

    // Single-sided contrast (inside window edge only, when outside is not available)
    // Circular hue difference [0.0, 0.5]
    qreal hueDiff = std::abs(ht - hp);
    if (hueDiff > 0.5) {
        hueDiff = 1.0 - hueDiff;
    }
    const qreal lightDiff = std::abs(lt - lp);

    // Case 1: Perimeter is predominantly LIGHT / WHITE (lp >= 0.50)
    // A light outline against a white/light window edge is INVISIBLE.
    // Guarantee crisp contrast against light edges:
    // - Monochrome: crisp dark charcoal (L <= 0.28).
    // - Chromatic: deep, rich, punchy accent (L <= 0.45, S >= 0.80).
    if (lp >= 0.50) {
        if (st < 0.20) {
            if (lt > 0.35) {
                const qreal darkL = qBound<qreal>(settings.minBrightness, 0.25, 0.28);
                return QColor::fromHslF(0.0, 0.0, darkL);
            }
        } else {
            // Prevent chromatic outlines (like green on Google Images) from washing out into pale mint/white
            if (lt > 0.48) {
                const qreal richL = qBound<qreal>(settings.minBrightness, 0.40, 0.45);
                const qreal richS = qMin<qreal>(1.0, qMax<qreal>(st, 0.82));
                return QColor::fromHslF(qBound<qreal>(0.0, ht, 1.0), richS, richL);
            }
        }
        return targetColor;
    }

    // Case 2: Perimeter is predominantly DARK (lp < 0.50)
    // A dark outline against a dark window edge is INVISIBLE.
    // Guarantee bright contrast against dark edges:
    // - Monochrome: brilliant bright silver/white (L >= 0.85).
    // - Chromatic: luminous, vibrant neon pop (L >= 0.65, S >= 0.85).
    if (lp < 0.50) {
        if (st < 0.20) {
            if (lt < 0.65) {
                const qreal brightL = qBound<qreal>(0.80, settings.maxBrightness, 1.0);
                return QColor::fromHslF(0.0, 0.0, brightL);
            }
        } else {
            if (lt < 0.55) {
                const qreal brightL = qBound<qreal>(0.65, settings.maxBrightness, 0.85);
                const qreal brightS = qMin<qreal>(1.0, qMax<qreal>(st, 0.85));
                return QColor::fromHslF(qBound<qreal>(0.0, ht, 1.0), brightS, brightL);
            }
        }
    }

    // Case 3: Matching color clash on chromatic windows (e.g. green outline on green window)
    if (st >= 0.20 && sp >= 0.20 && hueDiff < 0.12 && lightDiff < 0.38) {
        if (lp < 0.55) {
            const qreal boostedL = qBound<qreal>(settings.minBrightness, qMax<qreal>(0.75, lp + 0.35), settings.maxBrightness);
            const qreal boostedS = qMin<qreal>(1.0, qMax<qreal>(st, 0.85));
            return QColor::fromHslF(qBound<qreal>(0.0, ht, 1.0), boostedS, boostedL);
        } else {
            const qreal darkL = qBound<qreal>(settings.minBrightness, qMin<qreal>(0.30, lp - 0.35), settings.maxBrightness);
            const qreal darkS = qMin<qreal>(1.0, qMax<qreal>(st, 0.80));
            return QColor::fromHslF(qBound<qreal>(0.0, ht, 1.0), darkS, darkL);
        }
    }

    return targetColor;
}

/**
 * Computes an adaptive ambilight color from a raw RGBA buffer of sampled window pixels.
 * Dispatches to Smart Adaptive (Hybrid), Dominant Accent, or Balanced Average mode based on settings.
 */
inline QColor processRgbaSamples(const uint8_t *rgba, int pixelCount, const Settings &settings = Settings())
{
    QColor rawColor;
    switch (settings.mode) {
    case Mode::Smart:
        rawColor = processSmartAdaptive(rgba, pixelCount, settings);
        break;
    case Mode::Balanced:
        rawColor = processBalancedAverage(rgba, pixelCount, settings);
        break;
    case Mode::Dominant:
    default:
        rawColor = processDominantAccent(rgba, pixelCount, settings);
        break;
    }

    if (settings.ensureContrast) {
        int w = settings.sampleWidth;
        int h = settings.sampleHeight;
        if (w <= 0 || h <= 0 || w * h != pixelCount) {
            w = static_cast<int>(std::sqrt(pixelCount));
            h = (w > 0) ? (pixelCount / w) : 0;
        }
        return enforcePerimeterContrast(rawColor, rgba, w, h, settings);
    }

    return rawColor;
}

} // namespace OutlineAmbilight
