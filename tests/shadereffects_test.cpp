// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#include "shadereffects.h"
#include <cstdio>
#include <cmath>

int main()
{
    using namespace OutlineShader;
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

    // 1. Breathe / Pulsing
    {
        // When disabled, always 1.0
        checkNear(computeBreatheFactor(0.0, false, 0.5, 40), 1.0, 0.0001, "breathe disabled t=0");
        checkNear(computeBreatheFactor(1.23, false, 2.0, 80), 1.0, 0.0001, "breathe disabled t=1.23");

        // When enabled with intensity 40% (depth = 0.40)
        const qreal speed = 1.0; // 1 Hz
        // At t=0: sin(0) = 0 -> wave = 0.5 -> factor = 1.0 - 0.40 * 0.5 = 0.80
        checkNear(computeBreatheFactor(0.0, true, speed, 40), 0.80, 0.001, "breathe midpoint t=0");

        // At peak: 2*PI*speed*t = PI/2 => t = 0.25 -> sin = 1.0 -> wave = 1.0 -> factor = 1.0
        checkNear(computeBreatheFactor(0.25, true, speed, 40), 1.00, 0.001, "breathe peak t=0.25");

        // At trough: 2*PI*speed*t = 3*PI/2 => t = 0.75 -> sin = -1.0 -> wave = 0.0 -> factor = 0.60
        checkNear(computeBreatheFactor(0.75, true, speed, 40), 0.60, 0.001, "breathe trough t=0.75");

        // Verify across 200 time steps that bounds are strictly respected
        bool allWithinBounds = true;
        for (int i = 0; i < 200; ++i) {
            const qreal t = i * 0.02;
            const qreal factor = computeBreatheFactor(t, true, speed, 40);
            if (factor < 0.5999 || factor > 1.0001) {
                allWithinBounds = false;
                break;
            }
        }
        check(allWithinBounds, "breathe factor always strictly within [1.0 - depth, 1.0]");
    }

    // 2. Neon Voltage Flicker
    {
        // When disabled, always 1.0
        checkNear(computeNeonFactor(0.1, false, 3.0, 0.25, 30), 1.0, 0.0001, "neon disabled");

        // Outside duration, factor is exactly 1.0
        // interval = 3.0, duration = 0.25. At t = 0.50 (outside duration):
        checkNear(computeNeonFactor(0.50, true, 3.0, 0.25, 30), 1.0, 0.0001, "neon quiet period");
        checkNear(computeNeonFactor(2.50, true, 3.0, 0.25, 30), 1.0, 0.0001, "neon quiet period 2");

        // Inside duration, factor is <= 1.0 and >= 0.05
        bool validFlickerValues = true;
        for (int i = 1; i <= 20; ++i) {
            const qreal t = i * 0.01; // between 0.01 and 0.20
            const qreal factor = computeNeonFactor(t, true, 3.0, 0.25, 30);
            if (factor < 0.05 || factor > 1.0001) {
                validFlickerValues = false;
                break;
            }
        }
        check(validFlickerValues, "neon factor during flicker episode within [0.05, 1.0]");
    }

    // 3. Flash / Strobe
    {
        // When disabled, always 0.0
        checkNear(computeFlashFactor(0.0, false, 4.0, 0.15), 0.0, 0.0001, "flash disabled");

        // At start of flash (phase = 0), factor is peak boost = 0.8
        checkNear(computeFlashFactor(0.0, true, 4.0, 0.15), 0.8, 0.001, "flash peak t=0");
        checkNear(computeFlashFactor(4.0, true, 4.0, 0.15), 0.8, 0.001, "flash peak t=4.0 (cycle 2)");

        // Outside flash duration (e.g. t = 1.0 with interval 4.0, duration 0.15)
        checkNear(computeFlashFactor(1.0, true, 4.0, 0.15), 0.0, 0.0001, "flash quiet period");

        // Exponential decay: at progress = 0.5 (t = 0.075), factor = 0.8 * exp(-2.0) approx 0.108
        const qreal expectedHalf = 0.8 * std::exp(-2.0);
        checkNear(computeFlashFactor(0.075, true, 4.0, 0.15), expectedHalf, 0.01, "flash exponential decay");
    }

    // 4. Futuristic Hologram / Glitch
    {
        // When disabled, always 1.0
        checkNear(computeGlitchFactor(1.5, false, false, 1.0, 3, 50), 1.0, 0.0001, "glitch disabled");

        // Continuous hologram (anomalyEnabled = false): subtle micro-shimmer bounded within +/- 0.03
        bool shimmerBounded = true;
        for (int i = 0; i < 100; ++i) {
            const qreal t = i * 0.05;
            const qreal factor = computeGlitchFactor(t, true, false, 1.0, 3, 50);
            // 50% intensity -> intensityNorm = 0.5 -> max variation = 0.5 * 0.06 * 0.5 = 0.015
            if (factor < 0.98 || factor > 1.02) {
                shimmerBounded = false;
                break;
            }
        }
        check(shimmerBounded, "continuous hologram shimmer is subtle and bounded");

        // Glitch with disturbances (anomalyEnabled = true)
        bool anomalyFactorValid = true;
        for (int i = 0; i < 100; ++i) {
            const qreal t = i * 0.1;
            const qreal factor = computeGlitchFactor(t, true, true, 1.0, 3, 50);
            if (factor < 0.999) { // never drops below 1.0
                anomalyFactorValid = false;
                break;
            }
        }
        check(anomalyFactorValid, "glitch anomaly factor is always >= 1.0");
    }

    std::printf("shadereffects_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
