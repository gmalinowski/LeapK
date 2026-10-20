// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#include "colortransition.h"
#include <cstdio>
#include <cmath>

int main()
{
    using namespace OutlineColor;
    int failures = 0;
    int checks = 0;

    const auto check = [&](bool condition, const char *name) {
        ++checks;
        if (!condition) {
            std::fprintf(stderr, "FAIL: %s\n", name);
            ++failures;
        }
    };

    const auto checkEqualColor = [&](const QColor &c1, const QColor &c2, const char *name) {
        ++checks;
        if (c1 != c2) {
            std::fprintf(stderr, "FAIL: %s (got %s, expected %s)\n",
                         name, qPrintable(c1.name(QColor::HexArgb)), qPrintable(c2.name(QColor::HexArgb)));
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

    // 1. Parsing tests
    {
        const auto stops = parseColorStops(QStringLiteral("#3daee9:3.0;#2ecc71:2.5;#9b59b6:1.0"));
        check(stops.size() == 3, "parse 3 stops size");
        if (stops.size() == 3) {
            check(stops[0].color == QColor("#3daee9"), "stop 0 color");
            checkNear(stops[0].holdTime, 3.0, 0.001, "stop 0 hold");
            check(stops[1].color == QColor("#2ecc71"), "stop 1 color");
            checkNear(stops[1].holdTime, 2.5, 0.001, "stop 1 hold");
            check(stops[2].color == QColor("#9b59b6"), "stop 2 color");
            checkNear(stops[2].holdTime, 1.0, 0.001, "stop 2 hold");
        }
    }

    {
        // Alpha channel hex parsing (#AARRGGBB)
        const auto stops = parseColorStops(QStringLiteral("#803daee9:2.0;#ff2ecc71:1.5"));
        check(stops.size() == 2, "parse stops with alpha channel");
        if (stops.size() == 2) {
            checkEqualColor(stops[0].color, QColor("#803daee9"), "stop 0 alpha color");
            check(stops[0].color.alpha() == 128, "stop 0 alpha value 128");
            checkNear(stops[0].holdTime, 2.0, 0.001, "stop 0 hold time");
            checkEqualColor(stops[1].color, QColor("#ff2ecc71"), "stop 1 alpha color");
            check(stops[1].color.alpha() == 255, "stop 1 alpha value 255");
        }
    }

    {
        // Default hold time when omitted
        const auto stops = parseColorStops(QStringLiteral("#ff0000;#00ff00"));
        check(stops.size() == 2, "parse stops without hold time");
        if (stops.size() == 2) {
            checkNear(stops[0].holdTime, 3.0, 0.001, "default hold stop 0");
            checkNear(stops[1].holdTime, 3.0, 0.001, "default hold stop 1");
        }
    }

    {
        // Whitespace and trailing semicolon handling
        const auto stops = parseColorStops(QStringLiteral("  #112233:1.5 ;   #445566:4.0 ;  "));
        check(stops.size() == 2, "parse with whitespace");
    }

    {
        // Ignore invalid colors while keeping valid ones
        const auto stops = parseColorStops(QStringLiteral("#123456:2.0;not-a-color:3.0;#654321:1.0"));
        check(stops.size() == 2, "filter invalid color tokens");
    }

    {
        // Empty string
        const auto stops = parseColorStops(QString());
        check(stops.isEmpty(), "empty color string");
    }

    {
        // Invalid or negative hold time fallback to 3.0
        const auto stops = parseColorStops(QStringLiteral("#ff0000:-1.0;#00ff00:0.0;#0000ff:abc"));
        check(stops.size() == 3, "negative or invalid hold time size");
        if (stops.size() == 3) {
            checkNear(stops[0].holdTime, 3.0, 0.001, "negative hold fallback");
            checkNear(stops[1].holdTime, 3.0, 0.001, "zero hold fallback");
            checkNear(stops[2].holdTime, 3.0, 0.001, "nan hold fallback");
        }
    }

    // 2. Easing functions
    {
        // Boundary conditions for all easing modes
        for (int mode = 0; mode <= 3; ++mode) {
            checkNear(evaluateEasing(0.0, mode), 0.0, 0.001, "easing start is 0");
            checkNear(evaluateEasing(1.0, mode), 1.0, 0.001, "easing end is 1");
            checkNear(evaluateEasing(-0.5, mode), 0.0, 0.001, "easing clamp min");
            checkNear(evaluateEasing(1.5, mode), 1.0, 0.001, "easing clamp max");
        }

        // Ease In Out (Smoothstep): f(x) = x^2 * (3 - 2x)
        checkNear(evaluateEasing(0.5, 0), 0.5, 0.001, "smoothstep midpoint");
        checkNear(evaluateEasing(0.25, 0), 0.15625, 0.001, "smoothstep quarter");
        checkNear(evaluateEasing(0.75, 0), 0.84375, 0.001, "smoothstep three-quarters");

        // Linear: f(x) = x
        checkNear(evaluateEasing(0.3, 1), 0.3, 0.001, "linear 0.3");
        checkNear(evaluateEasing(0.7, 1), 0.7, 0.001, "linear 0.7");

        // Ease In: f(x) = x^2
        checkNear(evaluateEasing(0.5, 2), 0.25, 0.001, "ease in midpoint");

        // Ease Out: f(x) = x * (2 - x)
        checkNear(evaluateEasing(0.5, 3), 0.75, 0.001, "ease out midpoint");
    }

    // 3. Color Interpolation
    {
        const QColor red(255, 0, 0, 255);
        const QColor blue(0, 0, 255, 255);

        // Linear interpolation midpoint
        const QColor midLinear = interpolateColors(red, blue, 0.5, 1);
        checkEqualColor(midLinear, QColor(128, 0, 128, 255), "linear midpoint red to blue");

        // Progress 0 and 1
        checkEqualColor(interpolateColors(red, blue, 0.0, 0), red, "interpolate p=0 is c1");
        checkEqualColor(interpolateColors(red, blue, 1.0, 0), blue, "interpolate p=1 is c2");

        // Alpha channel interpolation
        const QColor alpha1(100, 100, 100, 50);
        const QColor alpha2(100, 100, 100, 250);
        const QColor midAlpha = interpolateColors(alpha1, alpha2, 0.5, 1);
        check(midAlpha.alpha() == 150, "alpha channel interpolation");
    }

    std::printf("colortransition_test: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
