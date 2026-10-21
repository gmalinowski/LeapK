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

    // 2. Solid pure red pixels (both Dominant and Balanced)
    {
        std::vector<uint8_t> pixels(64 * 4);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }
        for (const auto mode : {Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            check(c.isValid(), "pure red valid");
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            check(h < 0.05f || h > 0.95f, "pure red hue near 0");
            checkNear(s, 1.0, 0.01, "pure red saturation 1.0");
            checkNear(l, 0.5, 0.05, "pure red lightness near 0.5");
        }
    }

    // 3. Vivaldi Dominant scoring: Vibrant accent (10% blue) over large white canvas (90% white)
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
        const QColor c = processRgbaSamples(pixels.data(), 100, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        // Blue hue in HSL is around 210 degrees = 210/360 = 0.583
        check(h >= 0.50f && h <= 0.65f, "vibrant blue accent prioritized over 90% white canvas in dominant mode");
        check(s > 0.6f, "dominant accent saturation retained");
    }

    // 4. Pure black window: clamped to minBrightness (both modes)
    {
        std::vector<uint8_t> pixels(64 * 4, 0);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 3] = 255; // Alpha
        }
        for (const auto mode : {Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            settings.minBrightness = 0.25;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(l, 0.25, 0.01, "black clamped to minBrightness 0.25");
            check(s < 0.05f, "black has no saturation");
        }
    }

    // 5. Pure white window: clamped to maxBrightness (both modes)
    {
        std::vector<uint8_t> pixels(64 * 4, 255);
        for (const auto mode : {Mode::Dominant, Mode::Balanced}) {
            Settings settings;
            settings.mode = mode;
            settings.maxBrightness = 0.85;
            const QColor c = processRgbaSamples(pixels.data(), 64, settings);
            float h = 0.0f, s = 0.0f, l = 0.0f;
            c.getHslF(&h, &s, &l);
            checkNear(l, 0.85, 0.01, "white clamped to maxBrightness 0.85");
            check(s < 0.05f, "white has no saturation");
        }
    }

    // 6. Saturation boost check
    {
        // Pastel orange: R=200, G=150, B=130
        std::vector<uint8_t> pixels(64 * 4);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 0] = 200;
            pixels[i * 4 + 1] = 150;
            pixels[i * 4 + 2] = 130;
            pixels[i * 4 + 3] = 255;
        }
        Settings normal;
        normal.mode = Mode::Balanced;
        normal.saturationBoost = 1.0;
        const QColor cNormal = processRgbaSamples(pixels.data(), 64, normal);

        Settings boosted;
        boosted.mode = Mode::Balanced;
        boosted.saturationBoost = 1.5;
        const QColor cBoosted = processRgbaSamples(pixels.data(), 64, boosted);

        float h1 = 0.0f, s1 = 0.0f, l1 = 0.0f;
        float h2 = 0.0f, s2 = 0.0f, l2 = 0.0f;
        cNormal.getHslF(&h1, &s1, &l1);
        cBoosted.getHslF(&h2, &s2, &l2);

        checkNear(s2, s1 * 1.5, 0.02, "saturation boost scaling");
        checkNear(h1, h2, 0.02, "hue preserved under boost");
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

    // 9. Dominant mode (Vivaldi-style): Strongly locks onto dominant accent among multiple colors
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

    std::printf("ambilight_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
