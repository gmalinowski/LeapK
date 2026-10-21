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
            settings.ensureContrast = false;
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

    // 4. Pure black window: Balanced gives monochrome silver; Smart/Dominant give rich chromatic accent
    {
        std::vector<uint8_t> pixels(64 * 4, 0);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 3] = 255; // Alpha
        }

        // 4a. Balanced mode: pure monochrome silver outline
        {
            Settings settings;
            settings.mode = Mode::Balanced;
            settings.minBrightness = 0.25;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(l, 0.85, 0.01, "black window in balanced mode gets bright silver outline 0.85");
            check(s < 0.05f, "black window in balanced mode has no saturation");
        }

        // 4b. Smart & Dominant modes: grayscale excluded, always vibrant chromatic accent
        for (const auto mode : {Mode::Smart, Mode::Dominant}) {
            Settings settings;
            settings.mode = mode;
            settings.minBrightness = 0.25;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            check(s >= 0.75f, "black window in accent mode produces rich chromatic accent (s >= 0.75)");
            check(l >= 0.65f, "black window produces luminous outline (l >= 0.65)");
        }
    }

    // 5. Pure white window: Balanced gives monochrome charcoal; Smart/Dominant give rich chromatic accent
    {
        std::vector<uint8_t> pixels(64 * 4, 255);

        // 5a. Balanced mode: pure monochrome dark charcoal outline
        {
            Settings settings;
            settings.mode = Mode::Balanced;
            settings.minBrightness = 0.25;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(l, 0.25, 0.01, "white window in balanced mode gets dark charcoal outline 0.25");
            check(s < 0.05f, "white window in balanced mode has no saturation");
        }

        // 5b. Smart & Dominant modes: grayscale excluded, always deep chromatic accent
        for (const auto mode : {Mode::Smart, Mode::Dominant}) {
            Settings settings;
            settings.mode = mode;
            settings.minBrightness = 0.25;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            check(s >= 0.75f, "white window in accent mode produces rich chromatic accent (s >= 0.75)");
            check(l <= 0.45f, "white window produces deep contrasting outline (l <= 0.45)");
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
        check(defSettings.ensureContrast == true, "preset balanced: ensure contrast true");

        // True Ambilight (Video / Cinema) settings & cinematic video simulation
        Settings videoSettings;
        videoSettings.mode = Mode::Balanced;
        videoSettings.saturationLeveling = 0.75;
        videoSettings.fallbackStrength = 0.50;
        videoSettings.minBrightness = 0.15;
        videoSettings.maxBrightness = 0.90;
        videoSettings.ensureContrast = false;
        check(videoSettings.mode == Mode::Balanced, "preset video: balanced average mode");
        check(videoSettings.ensureContrast == false, "preset video: ensure contrast false (ambient movie lighting)");
        checkNear(videoSettings.saturationLeveling, 0.75, 0.01, "preset video: saturation leveling 0.75");
        checkNear(videoSettings.minBrightness, 0.15, 0.01, "preset video: min brightness 0.15");
        checkNear(videoSettings.maxBrightness, 0.90, 0.01, "preset video: max brightness 0.90");

        // Simulate 16:9 movie with 21:9 aspect ratio (black letterbox bars on top/bottom) and warm sunset scene
        std::vector<uint8_t> moviePixels(100 * 4);
        // Top 20 pixels: pure black letterbox (R=0, G=0, B=0)
        for (int i = 0; i < 20; ++i) {
            moviePixels[i * 4 + 0] = 0;
            moviePixels[i * 4 + 1] = 0;
            moviePixels[i * 4 + 2] = 0;
            moviePixels[i * 4 + 3] = 255;
        }
        // Middle 60 pixels: vibrant warm sunset scene (R=230, G=110, B=20)
        for (int i = 20; i < 80; ++i) {
            moviePixels[i * 4 + 0] = 230;
            moviePixels[i * 4 + 1] = 110;
            moviePixels[i * 4 + 2] = 20;
            moviePixels[i * 4 + 3] = 255;
        }
        // Bottom 20 pixels: pure black letterbox (R=0, G=0, B=0)
        for (int i = 80; i < 100; ++i) {
            moviePixels[i * 4 + 0] = 0;
            moviePixels[i * 4 + 1] = 0;
            moviePixels[i * 4 + 2] = 0;
            moviePixels[i * 4 + 3] = 255;
        }

        const QColor cVideo = processRgbaSamples(moviePixels.data(), 100, videoSettings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        cVideo.getHslF(&h, &s, &l);
        // Orange hue range is ~0.04 to 0.10
        check(h >= 0.04f && h <= 0.12f, "video preset: captures warm sunset orange hue despite letterbox bars");
        check(s >= 0.70f, "video preset: high saturation leveling produces vivid ambient color");
        check(l >= 0.40f && l <= 0.60f, "video preset: balanced lightness avoids black bar dilution");
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

        // 14a. Balanced mode: strictly monochrome fallback
        {
            Settings settings;
            settings.mode = Mode::Balanced;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(s, 0.0, 0.02, "slate dark theme in balanced mode has zero saturation (no blue/violet cast)");
            check(l >= 0.70f, "dark slate window in balanced mode gets bright silver outline");
        }

        // 14b. Smart & Dominant modes: grayscale excluded, always vibrant chromatic accent
        for (const auto mode : {Mode::Smart, Mode::Dominant}) {
            Settings settings;
            settings.mode = mode;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            check(s >= 0.75f, "dark slate window in accent mode gets vibrant chromatic accent (s >= 0.75)");
            check(l >= 0.65f, "dark slate window in accent mode gets luminous outline");
        }
    }

    // 15. High Contrast preset: slate dark window and light window in Balanced mode
    {
        Settings highContrastSettings;
        highContrastSettings.mode = Mode::Balanced;
        highContrastSettings.saturationLeveling = 0.75;
        highContrastSettings.fallbackStrength = 0.90;
        highContrastSettings.minBrightness = 0.20;
        highContrastSettings.maxBrightness = 0.92;

        // 15a. Dark slate window (R=45, G=50, B=60 - Nord/Slate background with ~14% blue tint)
        std::vector<uint8_t> darkSlate(64 * 4);
        for (size_t i = 0; i < 64; ++i) {
            darkSlate[i * 4 + 0] = 45;
            darkSlate[i * 4 + 1] = 50;
            darkSlate[i * 4 + 2] = 60;
            darkSlate[i * 4 + 3] = 255;
        }
        const QColor cDark = processRgbaSamples(darkSlate.data(), 64, highContrastSettings);
        float hd = 0.0f, sd = 0.0f, ld = 0.0f;
        cDark.getHslF(&hd, &sd, &ld);
        checkNear(sd, 0.0, 0.001, "high contrast: dark slate window strictly monochrome (zero blue cast)");
        check(ld >= 0.88f, "high contrast: dark slate window produces luminous bright silver outline (L >= 0.88)");

        // 15b. Bright white window (R=250, G=250, B=250)
        std::vector<uint8_t> lightWin(64 * 4, 250);
        const QColor cLight = processRgbaSamples(lightWin.data(), 64, highContrastSettings);
        float hl = 0.0f, sl = 0.0f, ll = 0.0f;
        cLight.getHslF(&hl, &sl, &ll);
        checkNear(sl, 0.0, 0.001, "high contrast: bright window strictly monochrome");
        check(ll <= 0.25f, "high contrast: bright window produces deep crisp charcoal outline (L <= 0.25)");
    }

    // 16. Perimeter contrast enforcement (Ensure edge contrast)
    {
        // 16a. White window (R=250, G=250, B=250) with black titlebar on top edge (R=20, G=20, B=20)
        constexpr int kGridW = 16;
        constexpr int kGridH = 16;
        std::vector<uint8_t> whiteWithDarkHeader(kGridW * kGridH * 4);
        for (int y = 0; y < kGridH; ++y) {
            for (int x = 0; x < kGridW; ++x) {
                const int idx = (y * kGridW + x) * 4;
                const uint8_t val = (y >= kGridH - 2) ? 20 : 250;
                whiteWithDarkHeader[idx + 0] = val;
                whiteWithDarkHeader[idx + 1] = val;
                whiteWithDarkHeader[idx + 2] = val;
                whiteWithDarkHeader[idx + 3] = 255;
            }
        }

        // In Balanced mode: dark charcoal outline
        Settings contrastOn;
        contrastOn.mode = Mode::Balanced;
        contrastOn.ensureContrast = true;
        contrastOn.sampleWidth = kGridW;
        contrastOn.sampleHeight = kGridH;
        const QColor cOn = processRgbaSamples(whiteWithDarkHeader.data(), kGridW * kGridH, contrastOn);
        float hon = 0.0f, son = 0.0f, lon = 0.0f;
        cOn.getHslF(&hon, &son, &lon);
        check(lon <= 0.30f, "ensure contrast: white window with dark titlebar maintains crisp dark charcoal outline in balanced mode");
        checkNear(son, 0.0, 0.01, "ensure contrast: white window maintains clean monochrome in balanced mode");

        // In Smart mode: deep chromatic accent contrasting against white page
        Settings contrastSmart;
        contrastSmart.mode = Mode::Smart;
        contrastSmart.ensureContrast = true;
        contrastSmart.sampleWidth = kGridW;
        contrastSmart.sampleHeight = kGridH;
        const QColor cSmart = processRgbaSamples(whiteWithDarkHeader.data(), kGridW * kGridH, contrastSmart);
        float hsm = 0.0f, ssm = 0.0f, lsm = 0.0f;
        cSmart.getHslF(&hsm, &ssm, &lsm);
        check(ssm >= 0.75f, "ensure contrast: white window in smart mode produces rich chromatic accent (not gray/white)");
        check(lsm <= 0.45f, "ensure contrast: white window in smart mode produces deep contrasting lightness (l <= 0.45)");

        // 16b. Solid mid-green window (RGB 35, 145, 60)
        // Without contrast enforcement: outline is green (lightness ~0.35, same as window).
        // With contrast enforcement: outline pops into bright neon lime/mint (lightness >= 0.70, saturation >= 0.75).
        std::vector<uint8_t> greenWindow(kGridW * kGridH * 4);
        for (int i = 0; i < kGridW * kGridH; ++i) {
            greenWindow[i * 4 + 0] = 35;
            greenWindow[i * 4 + 1] = 145;
            greenWindow[i * 4 + 2] = 60;
            greenWindow[i * 4 + 3] = 255;
        }

        const QColor cGreenOn = processRgbaSamples(greenWindow.data(), kGridW * kGridH, contrastOn);
        float hgOn = 0.0f, sgOn = 0.0f, lgOn = 0.0f;
        cGreenOn.getHslF(&hgOn, &sgOn, &lgOn);
        check(lgOn >= 0.70f, "ensure contrast: green window outline pops into luminous bright neon green (L >= 0.70)");
        check(sgOn >= 0.75f, "ensure contrast: green window outline has rich vivid saturation");
    }

    // 17. GitHub dark mode with tiny file tree icons and language bar
    // Verifies that isolated single-pixel noise (e.g. 1-2 pixels) does NOT trigger
    // erratic color jumping to orange or blue, and remains stable luminous chromatic accent.
    {
        constexpr int kGrid = 32;
        constexpr int kTotal = kGrid * kGrid;
        std::vector<uint8_t> githubDark(kTotal * 4);

        // Fill background with GitHub dark #0d1117 (R=13, G=17, B=23)
        for (int i = 0; i < kTotal; ++i) {
            githubDark[i * 4 + 0] = 13;
            githubDark[i * 4 + 1] = 17;
            githubDark[i * 4 + 2] = 23;
            githubDark[i * 4 + 3] = 255;
        }

        // Add 80 pixels of white text (R=230, G=230, B=230)
        for (int i = 100; i < 180; ++i) {
            githubDark[i * 4 + 0] = 230;
            githubDark[i * 4 + 1] = 230;
            githubDark[i * 4 + 2] = 230;
        }

        // Add 2 pixels of stray icon noise (R=230, G=100, B=30)
        for (int i = 300; i < 302; ++i) {
            githubDark[i * 4 + 0] = 230;
            githubDark[i * 4 + 1] = 100;
            githubDark[i * 4 + 2] = 30;
        }

        for (const auto mode : {Mode::Smart, Mode::Dominant}) {
            Settings settings;
            settings.mode = mode;
            settings.ensureContrast = true;
            settings.sampleWidth = kGrid;
            settings.sampleHeight = kGrid;
            settings.previousHue = 0.555f; // KDE Blue

            const QColor c = processRgbaSamples(githubDark.data(), kTotal, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);

            check(l >= 0.65f, "github dark: outline is luminous accent (l >= 0.65)");
            check(s >= 0.75f, "github dark: tiny noise rejected, outline maintains stable rich chromatic accent");
            check(h >= 0.50f && h <= 0.65f, "github dark: holds stable blue accent without jumping to orange noise");
        }
    }

    // 18. White webpage with black text and red underlines / accents
    // Verifies that red underlines/accents are faithfully captured and produce a rich, deep red outline
    // that contrasts clearly against the white webpage (NOT turned into white).
    {
        constexpr int kGrid = 32;
        constexpr int kTotal = kGrid * kGrid;
        std::vector<uint8_t> whiteWithRed(kTotal * 4);

        // Fill background with white (R=250, G=250, B=250)
        for (int i = 0; i < kTotal; ++i) {
            whiteWithRed[i * 4 + 0] = 250;
            whiteWithRed[i * 4 + 1] = 250;
            whiteWithRed[i * 4 + 2] = 250;
            whiteWithRed[i * 4 + 3] = 255;
        }

        // Add black text (R=20, G=20, B=20) across 150 pixels
        for (int i = 100; i < 250; ++i) {
            whiteWithRed[i * 4 + 0] = 20;
            whiteWithRed[i * 4 + 1] = 20;
            whiteWithRed[i * 4 + 2] = 20;
        }

        // Add 6 pixels of bright red underlines (R=235, G=25, B=25)
        for (int i = 260; i < 266; ++i) {
            whiteWithRed[i * 4 + 0] = 235;
            whiteWithRed[i * 4 + 1] = 25;
            whiteWithRed[i * 4 + 2] = 25;
        }

        Settings settings;
        settings.mode = Mode::Smart;
        settings.ensureContrast = true;
        settings.sampleWidth = kGrid;
        settings.sampleHeight = kGrid;

        const QColor c = processRgbaSamples(whiteWithRed.data(), kTotal, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);

        check(h <= 0.05f || h >= 0.95f, "white with red underlines: captures red hue");
        check(s >= 0.75f, "white with red underlines: rich red saturation >= 0.75");
        check(l <= 0.46f, "white with red underlines: deep red lightness <= 0.46 for strong contrast against white page");
    }

    // 19. Google Images with green thumbnails on white background
    // Verifies that green search results produce a rich, vibrant green outline (NOT washed-out pale mint/white).
    {
        constexpr int kGrid = 32;
        constexpr int kTotal = kGrid * kGrid;
        std::vector<uint8_t> googleImagesGreen(kTotal * 4);

        // 60% white margins and search header (R=250, G=250, B=250)
        for (int i = 0; i < 600; ++i) {
            googleImagesGreen[i * 4 + 0] = 250;
            googleImagesGreen[i * 4 + 1] = 250;
            googleImagesGreen[i * 4 + 2] = 250;
            googleImagesGreen[i * 4 + 3] = 255;
        }

        // 40% green image photos (RGB 45, 140, 55)
        for (int i = 600; i < kTotal; ++i) {
            googleImagesGreen[i * 4 + 0] = 45;
            googleImagesGreen[i * 4 + 1] = 140;
            googleImagesGreen[i * 4 + 2] = 55;
            googleImagesGreen[i * 4 + 3] = 255;
        }

        Settings settings;
        settings.mode = Mode::Smart;
        settings.ensureContrast = true;
        settings.sampleWidth = kGrid;
        settings.sampleHeight = kGrid;

        const QColor c = processRgbaSamples(googleImagesGreen.data(), kTotal, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);

        check(h >= 0.28f && h <= 0.42f, "google images green: captures green hue");
        check(s >= 0.75f, "google images green: vibrant rich saturation >= 0.75 (not washed-out)");
        check(l <= 0.46f, "google images green: deep green lightness <= 0.46 (not pale mint/white)");
    }

    // 20. Hue shifting on perimeter clash (when window has no color to detect)
    // Verifies that when candidate hue matches the perimeter color:
    // - With ensureContrast = true: shifts hue by +0.25 (bounded to 4 steps) until contrast is achieved.
    // - With ensureContrast = false: candidate hue is kept without shifting.
    {
        constexpr int kGrid = 16;
        constexpr int kTotal = kGrid * kGrid;
        std::vector<uint8_t> greenBorderWindow(kTotal * 4);

        // Fill entire window with pure dark gray (monochrome content)
        for (int i = 0; i < kTotal; ++i) {
            greenBorderWindow[i * 4 + 0] = 30;
            greenBorderWindow[i * 4 + 1] = 30;
            greenBorderWindow[i * 4 + 2] = 30;
            greenBorderWindow[i * 4 + 3] = 255;
        }

        // Paint perimeter with green (RGB 35, 145, 60 -> H ~ 0.38)
        for (int y = 0; y < kGrid; ++y) {
            for (int x = 0; x < kGrid; ++x) {
                if (y < 2 || y >= kGrid - 2 || x < 2 || x >= kGrid - 2) {
                    const int idx = (y * kGrid + x) * 4;
                    greenBorderWindow[idx + 0] = 35;
                    greenBorderWindow[idx + 1] = 145;
                    greenBorderWindow[idx + 2] = 60;
                }
            }
        }

        // 20a. With ensureContrast = true: candidate hue 0.38 (green) clashes with perimeter, so it must shift
        Settings shiftOn;
        shiftOn.mode = Mode::Dominant;
        shiftOn.ensureContrast = true;
        shiftOn.previousHue = 0.38f; // Green
        shiftOn.sampleWidth = kGrid;
        shiftOn.sampleHeight = kGrid;

        const QColor cShifted = processRgbaSamples(greenBorderWindow.data(), kTotal, shiftOn);
        float hs = 0.0f, ss = 0.0f, ls = 0.0f;
        cShifted.getHslF(&hs, &ss, &ls);

        qreal diff = std::abs(hs - 0.38f);
        if (diff > 0.5) diff = 1.0 - diff;
        check(diff >= 0.15, "hue shifting: shifts hue away from green perimeter clash (diff >= 0.15)");
        check(ss >= 0.75f, "hue shifting: shifted outline remains a rich vibrant accent");

        // 20b. With ensureContrast = false: candidate hue 0.38 is preserved without shifting
        Settings shiftOff;
        shiftOff.mode = Mode::Dominant;
        shiftOff.ensureContrast = false;
        shiftOff.previousHue = 0.38f; // Green
        shiftOff.sampleWidth = kGrid;
        shiftOff.sampleHeight = kGrid;

        const QColor cNoShift = processRgbaSamples(greenBorderWindow.data(), kTotal, shiftOff);
        float hns = 0.0f, sns = 0.0f, lns = 0.0f;
        cNoShift.getHslF(&hns, &sns, &lns);

        qreal diffNoShift = std::abs(hns - 0.38f);
        if (diffNoShift > 0.5) diffNoShift = 1.0 - diffNoShift;
        check(diffNoShift < 0.05, "hue shifting off: preserves candidate hue when ensureContrast is false");
    }

    // 21. Two-sided perimeter contrast check (inside window edge + outside desktop wallpaper)
    // Verifies that the outline checks contrast in BOTH directions:
    // - From outside the window (wallpaper / desktop background)
    // - From inside the window (window border / titlebar)
    {
        constexpr int kGrid = 16;
        constexpr int kTotal = kGrid * kGrid;
        std::vector<uint8_t> darkWindow(kTotal * 4);
        for (int i = 0; i < kTotal; ++i) {
            darkWindow[i * 4 + 0] = 30;
            darkWindow[i * 4 + 1] = 30;
            darkWindow[i * 4 + 2] = 30;
            darkWindow[i * 4 + 3] = 255;
        }

        // Simulate 32x8 outside strip sampled from a rich blue desktop wallpaper (RGB 20, 100, 220 -> H ~ 0.59)
        constexpr int kOutPixels = 32 * 8;
        std::vector<uint8_t> blueWallpaper(kOutPixels * 4);
        for (int i = 0; i < kOutPixels; ++i) {
            blueWallpaper[i * 4 + 0] = 20;
            blueWallpaper[i * 4 + 1] = 100;
            blueWallpaper[i * 4 + 2] = 220;
            blueWallpaper[i * 4 + 3] = 255;
        }

        // 21a. Dark window on blue wallpaper with ensureContrast = true:
        // Candidate hue is Blue (0.58). It clashes with the blue wallpaper outside the window,
        // so it must shift hue away from blue to avoid blending into the wallpaper!
        Settings contrastOn;
        contrastOn.mode = Mode::Dominant;
        contrastOn.ensureContrast = true;
        contrastOn.previousHue = 0.58f; // Blue
        contrastOn.sampleWidth = kGrid;
        contrastOn.sampleHeight = kGrid;
        contrastOn.outsideRgba = blueWallpaper.data();
        contrastOn.outsidePixelCount = kOutPixels;

        const QColor cOn = processRgbaSamples(darkWindow.data(), kTotal, contrastOn);
        float hon = 0.0f, son = 0.0f, lon = 0.0f;
        cOn.getHslF(&hon, &son, &lon);

        qreal diffOn = std::abs(hon - 0.58f);
        if (diffOn > 0.5) diffOn = 1.0 - diffOn;
        check(diffOn >= 0.15, "two-sided contrast: shifts hue away from blue wallpaper clash (diff >= 0.15)");
        check(son >= 0.75f, "two-sided contrast: retains rich vibrant accent saturation");
        check(lon >= 0.65f, "two-sided contrast: luminous outline on dark window and wallpaper");

        // 21b. Dark window on blue wallpaper with ensureContrast = false:
        // When ensureContrast is false, hue is preserved without shifting.
        Settings contrastOff = contrastOn;
        contrastOff.ensureContrast = false;

        const QColor cOff = processRgbaSamples(darkWindow.data(), kTotal, contrastOff);
        float hoff = 0.0f, soff = 0.0f, loff = 0.0f;
        cOff.getHslF(&hoff, &soff, &loff);

        qreal diffOff = std::abs(hoff - 0.58f);
        if (diffOff > 0.5) diffOff = 1.0 - diffOff;
        check(diffOff < 0.05, "two-sided contrast off: preserves candidate hue when ensureContrast is false");

        // 21c. Green window border inside, blue wallpaper outside:
        // Outline must shift to a color that contrasts with BOTH the green window AND the blue wallpaper!
        std::vector<uint8_t> greenBorderWindow = darkWindow;
        for (int y = 0; y < kGrid; ++y) {
            for (int x = 0; x < kGrid; ++x) {
                if (y < 2 || y >= kGrid - 2 || x < 2 || x >= kGrid - 2) {
                    const int idx = (y * kGrid + x) * 4;
                    greenBorderWindow[idx + 0] = 35;
                    greenBorderWindow[idx + 1] = 145;
                    greenBorderWindow[idx + 2] = 60;
                }
            }
        }

        Settings bothSides = contrastOn;
        bothSides.previousHue = 0.58f; // Start from blue
        const QColor cBoth = processRgbaSamples(greenBorderWindow.data(), kTotal, bothSides);
        float hb = 0.0f, sb = 0.0f, lb = 0.0f;
        cBoth.getHslF(&hb, &sb, &lb);

        qreal diffGreen = std::abs(hb - 0.38f);
        if (diffGreen > 0.5) diffGreen = 1.0 - diffGreen;
        qreal diffBlue = std::abs(hb - 0.58f);
        if (diffBlue > 0.5) diffBlue = 1.0 - diffBlue;
        check(diffGreen >= 0.15, "two-sided contrast: contrasts with green window inside (diff >= 0.15)");
        check(diffBlue >= 0.15, "two-sided contrast: contrasts with blue wallpaper outside (diff >= 0.15)");
    }

    // 22. Global perimeter contrast on static custom colors & KDE accent colors
    // Verifies that when Ensure Perimeter Contrast is active globally, static colors
    // (such as static green or system accent blue) dynamically adapt if they clash with
    // the window border or desktop wallpaper.
    {
        constexpr int kGrid = 16;
        constexpr int kTotal = kGrid * kGrid;
        std::vector<uint8_t> darkWindow(kTotal * 4, 30);
        for (int i = 0; i < kTotal; ++i) {
            darkWindow[i * 4 + 3] = 255;
        }

        constexpr int kOutPixels = 32 * 8;
        // Rich green wallpaper (RGB 30, 200, 50 -> H ~ 0.35)
        std::vector<uint8_t> greenWallpaper(kOutPixels * 4);
        for (int i = 0; i < kOutPixels; ++i) {
            greenWallpaper[i * 4 + 0] = 30;
            greenWallpaper[i * 4 + 1] = 200;
            greenWallpaper[i * 4 + 2] = 50;
            greenWallpaper[i * 4 + 3] = 255;
        }

        // Rich blue wallpaper (RGB 20, 100, 220 -> H ~ 0.59)
        std::vector<uint8_t> blueWallpaper(kOutPixels * 4);
        for (int i = 0; i < kOutPixels; ++i) {
            blueWallpaper[i * 4 + 0] = 20;
            blueWallpaper[i * 4 + 1] = 100;
            blueWallpaper[i * 4 + 2] = 220;
            blueWallpaper[i * 4 + 3] = 255;
        }

        // 22a. Static green outline on green wallpaper:
        // User sets custom color to green (H ~ 0.33), wallpaper is green (H ~ 0.35).
        // enforcePerimeterContrast must shift hue away so the outline does not blend in!
        const QColor staticGreen(0, 255, 0);
        Settings greenSettings;
        greenSettings.ensureContrast = true;
        greenSettings.sampleWidth = kGrid;
        greenSettings.sampleHeight = kGrid;
        greenSettings.outsideRgba = greenWallpaper.data();
        greenSettings.outsidePixelCount = kOutPixels;

        const QColor adjustedGreen = enforcePerimeterContrast(staticGreen, darkWindow.data(), kGrid, kGrid, greenSettings);
        float hg = 0.0f, sg = 0.0f, lg = 0.0f;
        adjustedGreen.getHslF(&hg, &sg, &lg);

        qreal diffWallpaper = std::abs(hg - 0.35f);
        if (diffWallpaper > 0.5) diffWallpaper = 1.0 - diffWallpaper;
        check(diffWallpaper >= 0.15, "global contrast: static green shifts away from green wallpaper (diff >= 0.15)");
        check(sg >= 0.75, "global contrast: shifted outline retains high saturation");

        // 22b. Static green on green wallpaper with ensureContrast = false:
        // Preserves static green without shifting.
        Settings noContrast = greenSettings;
        noContrast.ensureContrast = false;
        const QColor rawGreen = enforcePerimeterContrast(staticGreen, darkWindow.data(), kGrid, kGrid, noContrast);
        check(rawGreen == staticGreen, "global contrast disabled: static green is preserved exactly");

        // 22c. System accent blue on blue wallpaper:
        // KDE Highlight is blue (H ~ 0.58), wallpaper is blue.
        // enforcePerimeterContrast must shift hue away from blue!
        const QColor accentBlue(29, 153, 243);
        Settings blueSettings;
        blueSettings.ensureContrast = true;
        blueSettings.sampleWidth = kGrid;
        blueSettings.sampleHeight = kGrid;
        blueSettings.outsideRgba = blueWallpaper.data();
        blueSettings.outsidePixelCount = kOutPixels;

        const QColor adjustedBlue = enforcePerimeterContrast(accentBlue, darkWindow.data(), kGrid, kGrid, blueSettings);
        float hb = 0.0f, sb = 0.0f, lb = 0.0f;
        adjustedBlue.getHslF(&hb, &sb, &lb);

        qreal diffBlueWp = std::abs(hb - 0.58f);
        if (diffBlueWp > 0.5) diffBlueWp = 1.0 - diffBlueWp;
        check(diffBlueWp >= 0.15, "global contrast: system accent blue shifts away from blue wallpaper (diff >= 0.15)");

        // 22d. Window edge inside has green borders, wallpaper is neutral dark:
        // staticGreen clashes with window border inside: shifts hue away from green!
        std::vector<uint8_t> greenBorderWindow = darkWindow;
        for (int y = 0; y < kGrid; ++y) {
            for (int x = 0; x < kGrid; ++x) {
                if (y < 2 || y >= kGrid - 2 || x < 2 || x >= kGrid - 2) {
                    const int idx = (y * kGrid + x) * 4;
                    greenBorderWindow[idx + 0] = 30;
                    greenBorderWindow[idx + 1] = 220;
                    greenBorderWindow[idx + 2] = 40;
                }
            }
        }
        std::vector<uint8_t> neutralDarkWallpaper(kOutPixels * 4, 25);
        for (int i = 0; i < kOutPixels; ++i) neutralDarkWallpaper[i * 4 + 3] = 255;

        Settings insideSettings = greenSettings;
        insideSettings.outsideRgba = neutralDarkWallpaper.data();
        const QColor adjustedInside = enforcePerimeterContrast(staticGreen, greenBorderWindow.data(), kGrid, kGrid, insideSettings);
        float hi = 0.0f, si = 0.0f, li = 0.0f;
        adjustedInside.getHslF(&hi, &si, &li);

        qreal diffInside = std::abs(hi - 0.34f);
        if (diffInside > 0.5) diffInside = 1.0 - diffInside;
        const qreal lightDiffInside = std::abs(li - 0.49);
        check(lightDiffInside >= 0.20 || diffInside >= 0.15, "global contrast: static green contrasts with green window edge inside (diff >= 0.15 or lightDiff >= 0.20)");

        // 22e. Static color on dark window + dark wallpaper:
        // Luminous bright neon boost (L >= 0.65)
        const QColor redBase(200, 30, 30);
        const QColor adjustedDark = enforcePerimeterContrast(redBase, darkWindow.data(), kGrid, kGrid, insideSettings);
        float hd = 0.0f, sd = 0.0f, ld = 0.0f;
        adjustedDark.getHslF(&hd, &sd, &ld);
        check(ld >= 0.65, "global contrast: boosts lightness on dark window + dark wallpaper (L >= 0.65)");

        // 22f. Static pastel on white window + white wallpaper:
        // Rich accent darkening (L <= 0.45)
        std::vector<uint8_t> whiteWindow(kTotal * 4, 245);
        for (int i = 0; i < kTotal; ++i) whiteWindow[i * 4 + 3] = 255;
        std::vector<uint8_t> whiteWallpaper(kOutPixels * 4, 250);
        for (int i = 0; i < kOutPixels; ++i) whiteWallpaper[i * 4 + 3] = 255;

        Settings whiteSettings = greenSettings;
        whiteSettings.outsideRgba = whiteWallpaper.data();
        const QColor pastelRed(255, 180, 180);
        const QColor adjustedLight = enforcePerimeterContrast(pastelRed, whiteWindow.data(), kGrid, kGrid, whiteSettings);
        float hl = 0.0f, sl = 0.0f, ll = 0.0f;
        adjustedLight.getHslF(&hl, &sl, &ll);
        check(ll <= 0.45, "global contrast: deepens lightness on white window + white wallpaper (L <= 0.45)");
    }

    std::printf("ambilight_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
