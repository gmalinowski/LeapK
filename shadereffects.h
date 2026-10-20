// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QtGlobal>
#include <cmath>

namespace OutlineShader
{

inline qreal computeBreatheFactor(qreal elapsedSec, bool enabled, qreal speed, int intensity)
{
    if (!enabled) {
        return 1.0;
    }
    const qreal depth = qBound(10, intensity, 100) / 100.0;
    const qreal wave = 0.5 + 0.5 * std::sin(2.0 * M_PI * speed * elapsedSec);
    return 1.0 - depth * (1.0 - wave);
}

inline qreal computeNeonFactor(qreal elapsedSec, bool enabled, qreal interval, qreal duration, int intensity)
{
    if (!enabled) {
        return 1.0;
    }
    const qreal validInterval = qMax<qreal>(0.5, interval);
    const qreal validDuration = qBound<qreal>(0.02, duration, validInterval * 0.95);
    const qreal phase = std::fmod(elapsedSec, validInterval);

    if (phase < validDuration) {
        const qreal t = elapsedSec;
        const qreal rawNoise = std::sin(t * 110.0) * 0.5 + std::cos(t * 67.0) * 0.3 + std::sin(t * 193.0) * 0.2;
        const qreal depth = qBound(10, intensity, 100) / 100.0;

        if (rawNoise < -0.2) {
            return qMax<qreal>(0.05, 1.0 - depth);
        } else if (rawNoise < 0.25) {
            return qMax<qreal>(0.1, 1.0 - depth * 0.6);
        } else {
            return 1.0 - depth * 0.15 * std::abs(rawNoise);
        }
    }

    return 1.0;
}

inline qreal computeFlashFactor(qreal elapsedSec, bool enabled, qreal interval, qreal duration)
{
    if (!enabled) {
        return 0.0;
    }
    const qreal validInterval = qMax<qreal>(0.5, interval);
    const qreal validDuration = qBound<qreal>(0.05, duration, 1.0);
    const qreal phase = std::fmod(elapsedSec, validInterval);
    if (phase < validDuration) {
        const qreal progress = phase / validDuration;
        return 0.8 * std::exp(-4.0 * progress);
    }
    return 0.0;
}

inline qreal computeGlitchFactor(qreal elapsedSec, bool enabled, bool anomalyEnabled,
                                 qreal speed, int frequency, int intensity)
{
    if (!enabled) {
        return 1.0;
    }
    const qreal intensityNorm = qBound(10, intensity, 100) / 100.0;
    if (!anomalyEnabled) {
        const qreal shimmer = 0.5 + 0.5 * std::sin(elapsedSec * (3.0 * speed));
        return 1.0 + intensityNorm * 0.06 * (shimmer - 0.5);
    }
    const qreal anomalyPhase = elapsedSec * (0.35 + 0.2 * frequency);
    const qreal beat = std::sin(anomalyPhase) * std::sin(anomalyPhase * 0.618 + 1.5);
    if (beat > 0.5) {
        const qreal surge = (beat - 0.5) / 0.5;
        const qreal microJitter = 0.5 + 0.5 * std::sin(elapsedSec * (16.0 * speed));
        const qreal depth = intensityNorm * 0.15;
        return 1.0 + depth * surge * (0.7 + 0.3 * microJitter);
    }
    return 1.0;
}

} // namespace OutlineShader
