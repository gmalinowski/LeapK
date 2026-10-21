// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ambilightalgorithm.h"
#include <cstdio>
#include <cmath>
#include <vector>

int main()
{
    using namespace OutlineAmbilight;
    int failures = 0;
    int checks = 0;

    const auto check = [&](bool condition, const char *name) {
        ++checks;
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };

    const auto checkNear = [&](qreal actual, qreal expected, qreal tol, const char *name) {
        ++checks;
        if (std::abs(actual - expected) > tol) {
            std::fprintf(stderr, "FAIL: %s (got %f, expected %f)\n", name, actual, expected);
            ++failures;
        }
    };

    // 1. Null / empty input
    {
        const QColor fallback = processRgbaSamples(nullptr, 0);
        check(fallback.isValid(), "null fallback is valid");
    }

    // 2. Solid pure red pixels (Smart, Dominant and Balanced)
    {
        std::vector<uint8_t> pixels(64 * 4);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }
        for (const auto mode : {Mode::Smart, Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            settings.saturationLeveling = 1.0;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            check(c.isValid(), "pure red valid");
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            check(h < 0.05f || h > 0.95f, "pure red hue near 0");
            checkNear(s, 1.0, 0.01, "pure red saturation 1.0");
            checkNear(l, 0.5, 0.05, "pure red lightness near 0.5");
        }
    }

    // 3. Dominant scoring: Vibrant accent (10% blue) over large white canvas (90% white)
    {
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 90; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 255;
            pixels[i * 4 + 2] = 255;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 90; i < 100; ++i) {
            pixels[i * 4 + 0] = 0;
            pixels[i * 4 + 1] = 120;
            pixels[i * 4 + 2] = 255;
            pixels[i * 4 + 3] = 255;
        }
        Settings settings;
        settings.mode = Mode::Dominant;
        settings.saturationLeveling = 0.85;
        const QColor c = processRgbaSamples(pixels.data(), 100, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        // Blue hue in HSL is around 210 degrees = 210/360 = 0.583
        check(h >= 0.50f && h <= 0.65f, "vibrant blue accent prioritized over 90% white canvas in dominant mode");
        check(s > 0.6f, "dominant accent saturation retained");
    }

    // 4. Pure black window: high-contrast silver outline (all modes)
    {
        std::vector<uint8_t> pixels(64 * 4, 0);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 3] = 255; // Alpha
        }
        for (const auto mode : {Mode::Smart, Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            settings.minBrightness = 0.25;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(l, 0.85, 0.01, "black window gets high-contrast bright silver outline 0.85");
            check(s < 0.05f, "black has no saturation");
        }
    }

    // 5. Pure white window: high-contrast dark charcoal outline (all modes)
    {
        std::vector<uint8_t> pixels(64 * 4, 255);
        for (const auto mode : {Mode::Smart, Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            settings.minBrightness = 0.25;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(l, 0.25, 0.01, "white window gets high-contrast dark charcoal outline 0.25");
            check(s < 0.05f, "white has no saturation");
        }
    }

    // 6. Saturation leveling: softens harsh neon and boosts pale pastel towards target
    {
        // 6a. High saturation (1.0) leveled towards 0.60 -> tones down
        const qreal sHigh = 1.0;
        const qreal leveledHigh = applySaturationLeveling(sHigh, 0.60);
        check(leveledHigh < 1.0 && leveledHigh >= 0.75, "saturation leveling tones down harsh neon saturation");

        // 6b. Weak pastel saturation (0.25) leveled towards 0.65 -> boosts up
        const qreal sLow = 0.25;
        const qreal leveledLow = applySaturationLeveling(sLow, 0.65);
        check(leveledLow > 0.25 && leveledLow <= 0.55, "saturation leveling boosts weak pale saturation");

        // 6c. Target match preserved
        const qreal sTarget = 0.65;
        const qreal leveledTarget = applySaturationLeveling(sTarget, 0.65);
        checkNear(leveledTarget, 0.65, 0.01, "saturation leveling preserves target level");

        // 6d. Neutral monochrome (0.0) untouched
        const qreal sZero = 0.0;
        checkNear(applySaturationLeveling(sZero, 0.65), 0.0, 0.001, "saturation leveling leaves zero saturation untouched");

        // 6e. Sub-threshold cool slate tint suppressed into pure neutral
        const qreal sSlate = 0.05;
        checkNear(applySaturationLeveling(sSlate, 0.65), 0.0, 0.001, "sub-threshold slate tint flattened to pure monochrome");
    }

    // 7. Balanced mode: Multi-colored elements blend into harmonious composite (Magenta/Purple)
    {
        // 50% pure red, 50% pure blue
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 50; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 50; i < 100; ++i) {
            pixels[i * 4 + 0] = 0;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 255;
            pixels[i * 4 + 3] = 255;
        }
        Settings settings;
        settings.mode = Mode::Balanced;
        settings.saturationLeveling = 0.85;
        const QColor c = processRgbaSamples(pixels.data(), 100, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        // Magenta / Purple is around 300 degrees = 300 / 360 = 0.833
        check(h >= 0.75f && h <= 0.90f, "balanced: red + blue blends into stable magenta hue");
        check(s > 0.5f, "balanced: magenta blend has rich saturation");
    }

    // 8. Balanced mode: Opposing colors produce stable neutral composite without jumping
    {
        // 50% red, 50% cyan
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 50; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 50; i < 100; ++i) {
            pixels[i * 4 + 0] = 0;
            pixels[i * 4 + 1] = 255;
            pixels[i * 4 + 2] = 255;
            pixels[i * 4 + 3] = 255;
        }
        Settings settings;
        settings.mode = Mode::Balanced;
        const QColor c = processRgbaSamples(pixels.data(), 100, settings);
        check(c.isValid(), "balanced: opposing colors blend stably");
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        check(s < 0.1f, "balanced: opposing complementary colors neutralize cleanly");
    }

    // 9. Dominant mode: Strongly locks onto dominant accent among multiple colors
    {
        // 80% dark background (RGB 20, 20, 20)
        // 15% Spotify Neon Green (RGB 30, 215, 96) -> H ~ 141 deg (0.39)
        // 5% Dull Blue (RGB 40, 80, 140)
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 80; ++i) {
            pixels[i * 4 + 0] = 20;
            pixels[i * 4 + 1] = 20;
            pixels[i * 4 + 2] = 20;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 80; i < 95; ++i) {
            pixels[i * 4 + 0] = 30;
            pixels[i * 4 + 1] = 215;
            pixels[i * 4 + 2] = 96;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 95; i < 100; ++i) {
            pixels[i * 4 + 0] = 40;
            pixels[i * 4 + 1] = 80;
            pixels[i * 4 + 2] = 140;
            pixels[i * 4 + 3] = 255;
        }

        Settings settings;
        settings.mode = Mode::Dominant;
        settings.saturationLeveling = 0.85;
        const QColor c = processRgbaSamples(pixels.data(), 100, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        // Green hue: 141 / 360 ~ 0.392
        check(h >= 0.32f && h <= 0.45f, "dominant: Spotify green accent decisively wins over dull blue and dark canvas");
        check(s > 0.6f, "dominant: retains rich accent saturation");
    }

    // 10. Dominant mode inertia / hysteresis prevents flip-flopping between close candidates
    {
        // 50% Red, 50% Blue
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 50; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 50; i < 100; ++i) {
            pixels[i * 4 + 0] = 0;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 255;
            pixels[i * 4 + 3] = 255;
        }

        Settings settingsWithBlue;
        settingsWithBlue.mode = Mode::Dominant;
        settingsWithBlue.previousHue = 240.0f / 360.0f; // Blue hue
        const QColor cBlue = processRgbaSamples(pixels.data(), 100, settingsWithBlue);
        float hb = 0.0f, sb = 0.0f, lb = 0.0f;
        cBlue.getHslF(&hb, &sb, &lb);
        check(hb >= 0.60f && hb <= 0.72f, "dominant inertia: holds previous blue hue on equal competition");

        Settings settingsWithRed;
        settingsWithRed.mode = Mode::Dominant;
        settingsWithRed.previousHue = 0.0f; // Red hue
        const QColor cRed = processRgbaSamples(pixels.data(), 100, settingsWithRed);
        float hr = 0.0f, sr = 0.0f, lr = 0.0f;
        cRed.getHslF(&hr, &sr, &lr);
        check(hr <= 0.05f || hr >= 0.95f, "dominant inertia: holds previous red hue on equal competition");
    }

    // 11. Smart Adaptive mode: Fallback distinction strength scaling
    {
        // Homogeneous pastel blue window (RGB 100, 150, 220)
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 100; ++i) {
            pixels[i * 4 + 0] = 100;
            pixels[i * 4 + 1] = 150;
            pixels[i * 4 + 2] = 220;
            pixels[i * 4 + 3] = 255;
        }

        Settings dominantSettings;
        dominantSettings.mode = Mode::Dominant;
        const QColor cDom = processRgbaSamples(pixels.data(), 100, dominantSettings);

        // Fallback strength 0% (subtle - identical to dominant)
        Settings smartSubtle;
        smartSubtle.mode = Mode::Smart;
        smartSubtle.fallbackStrength = 0.0;
        const QColor cSubtle = processRgbaSamples(pixels.data(), 100, smartSubtle);

        // Fallback strength 100% (bold distinction)
        Settings smartBold;
        smartBold.mode = Mode::Smart;
        smartBold.fallbackStrength = 1.0;
        const QColor cBold = processRgbaSamples(pixels.data(), 100, smartBold);

        float hd = 0.0f, sd = 0.0f, ld = 0.0f;
        float hs = 0.0f, ss = 0.0f, ls = 0.0f;
        float hb = 0.0f, sb = 0.0f, lb = 0.0f;
        cDom.getHslF(&hd, &sd, &ld);
        cSubtle.getHslF(&hs, &ss, &ls);
        cBold.getHslF(&hb, &sb, &lb);

        checkNear(sd, ss, 0.01, "fallback 0%: saturation matches dominant (subtle)");
        check(sb > sd, "fallback 100%: saturation boosted significantly above dominant");
    }

    // 12. Smart Adaptive mode: Retains dominant accent on contrasting backgrounds
    {
        // 80% dark background, 20% vibrant red button
        std::vector<uint8_t> pixels(100 * 4);
        for (int i = 0; i < 80; ++i) {
            pixels[i * 4 + 0] = 30;
            pixels[i * 4 + 1] = 30;
            pixels[i * 4 + 2] = 30;
            pixels[i * 4 + 3] = 255;
        }
        for (int i = 80; i < 100; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }

        Settings smartSettings;
        smartSettings.mode = Mode::Smart;
        smartSettings.saturationLeveling = 0.85;
        const QColor cSmart = processRgbaSamples(pixels.data(), 100, smartSettings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        cSmart.getHslF(&h, &s, &l);
        check(h < 0.05f || h > 0.95f, "smart: locks onto dominant red accent on contrasting background");
        check(s > 0.6f, "smart: rich saturation on contrasting accent");
    }

    // 13. Preset presets sanity checks
    {
        // Balanced (Default) matches default Settings
        Settings defSettings;
        check(defSettings.mode == Mode::Smart, "preset balanced: smart mode");
        checkNear(defSettings.saturationLeveling, 0.65, 0.01, "preset balanced: saturation leveling 0.65");
        checkNear(defSettings.fallbackStrength, 0.50, 0.01, "preset balanced: fallback strength 0.50");
        checkNear(defSettings.minBrightness, 0.25, 0.01, "preset balanced: min brightness 0.25");
        checkNear(defSettings.maxBrightness, 0.85, 0.01, "preset balanced: max brightness 0.85");
    }

    // 14. Grayscale slate dark-theme window (e.g. KDE Breeze Dark #232629 / Firefox #2b2a33)
    {
        // Dark slate window (R=35, G=38, B=41) -> cool gray with ~4.6% accidental blue tint
        std::vector<uint8_t> pixels(64 * 4);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 0] = 35;
            pixels[i * 4 + 1] = 38;
            pixels[i * 4 + 2] = 41;
            pixels[i * 4 + 3] = 255;
        }
        for (const auto mode : {Mode::Smart, Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(s, 0.0, 0.02, "slate dark theme has zero saturation (no blue/violet cast)");
            check(l >= 0.70f, "dark slate window gets bright, clearly visible silver outline");
        }
    }

    std::printf("ambilight_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
