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

    // 2. Solid pure red pixels
    {
        std::vector<uint8_t> pixels(64 * 4);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 0] = 255;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = 0;
            pixels[i * 4 + 3] = 255;
        }
        const QColor c = processRgbaSamples(pixels.data(), 64);
        check(c.isValid(), "pure red valid");
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        check(h < 0.05f || h > 0.95f, "pure red hue near 0");
        checkNear(s, 1.0, 0.01, "pure red saturation 1.0");
        checkNear(l, 0.5, 0.05, "pure red lightness near 0.5");
    }

    // 3. Vivaldi scoring: Vibrant accent (10% blue) over large white canvas (90% white)
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
        const QColor c = processRgbaSamples(pixels.data(), 100);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        // Blue hue in HSL is around 210 degrees = 210/360 = 0.583
        check(h >= 0.50f && h <= 0.65f, "vibrant blue accent prioritized over 90% white canvas");
        check(s > 0.5f, "accent saturation retained");
    }

    // 4. Pure black window: clamped to minBrightness
    {
        std::vector<uint8_t> pixels(64 * 4, 0);
        for (size_t i = 0; i < 64; ++i) {
            pixels[i * 4 + 3] = 255; // Alpha
        }
        Settings settings;
        settings.minBrightness = 0.25;
        const QColor c = processRgbaSamples(pixels.data(), 64, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        checkNear(l, 0.25, 0.01, "black clamped to minBrightness 0.25");
        check(s < 0.05f, "black has no saturation");
    }

    // 5. Pure white window: clamped to maxBrightness
    {
        std::vector<uint8_t> pixels(64 * 4, 255);
        Settings settings;
        settings.maxBrightness = 0.85;
        const QColor c = processRgbaSamples(pixels.data(), 64, settings);
        float h = 0.0f, s = 0.0f, l = 0.0f;
        c.getHslF(&h, &s, &l);
        checkNear(l, 0.85, 0.01, "white clamped to maxBrightness 0.85");
        check(s < 0.05f, "white has no saturation");
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
        normal.saturationBoost = 1.0;
        const QColor cNormal = processRgbaSamples(pixels.data(), 64, normal);

        Settings boosted;
        boosted.saturationBoost = 1.5;
        const QColor cBoosted = processRgbaSamples(pixels.data(), 64, boosted);

        float h1 = 0.0f, s1 = 0.0f, l1 = 0.0f;
        float h2 = 0.0f, s2 = 0.0f, l2 = 0.0f;
        cNormal.getHslF(&h1, &s1, &l1);
        cBoosted.getHslF(&h2, &s2, &l2);

        checkNear(s2, s1 * 1.5, 0.02, "saturation boost scaling");
        checkNear(h1, h2, 0.02, "hue preserved under boost");
    }

    std::printf("ambilight_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
