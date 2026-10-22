// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <cmath>
#include <algorithm>

namespace OutlineColor
{

struct ColorStop
{
    QColor color;
    qreal holdTime = 3.0;

    bool operator==(const ColorStop &other) const
    {
        return color == other.color && qFuzzyCompare(holdTime, other.holdTime);
    }
};

enum class EasingMode
{
    EaseInOut = 0,
    Linear = 1,
    EaseIn = 2,
    EaseOut = 3
};

inline qreal evaluateEasing(qreal progress, int easingMode)
{
    const qreal p = qBound<qreal>(0.0, progress, 1.0);
    switch (easingMode) {
    case 0: // Ease In Out (Smoothstep)
        return p * p * (3.0 - 2.0 * p);
    case 1: // Linear
        return p;
    case 2: // Ease In
        return p * p;
    case 3: // Ease Out
        return p * (2.0 - p);
    default:
        return p;
    }
}

inline QList<ColorStop> parseColorStops(const QString &str)
{
    QList<ColorStop> result;
    const QStringList parts = str.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QStringList tokens = part.split(QLatin1Char(':'));
        if (tokens.size() >= 1) {
            const QColor col(tokens[0].trimmed());
            qreal hold = 3.0;
            if (tokens.size() >= 2) {
                bool ok = false;
                const qreal val = tokens[1].trimmed().toDouble(&ok);
                if (ok && val > 0.0) {
                    hold = val;
                }
            }
            if (col.isValid()) {
                result.append({col, hold});
            }
        }
    }
    return result;
}

inline QColor interpolateColors(const QColor &c1, const QColor &c2, qreal progress, int easingMode)
{
    const qreal f = evaluateEasing(progress, easingMode);
    const int r = qRound(c1.red() + (c2.red() - c1.red()) * f);
    const int g = qRound(c1.green() + (c2.green() - c1.green()) * f);
    const int b = qRound(c1.blue() + (c2.blue() - c1.blue()) * f);
    const int a = qRound(c1.alpha() + (c2.alpha() - c1.alpha()) * f);
    return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), qBound(0, a, 255));
}

inline qreal computeRecentWindowOpacity(int recentIdx, int totalCount, int lastWindowOpacityPercent)
{
    const qreal minOpacity = qBound<qreal>(0.01, lastWindowOpacityPercent / 100.0, 1.0);
    const int totalSteps = std::max(1, totalCount);
    const int step = std::clamp(recentIdx + 1, 1, totalSteps);
    const qreal stepRatio = static_cast<qreal>(step) / static_cast<qreal>(totalSteps);
    const qreal interpolatedOpacity = 1.0 - stepRatio * (1.0 - minOpacity);
    return qBound<qreal>(0.0, interpolatedOpacity, 1.0);
}

inline QColor applyRecentWindowStyling(const QColor &color, int recentIdx, int totalCount, int hueShiftDeg, int lastWindowSaturationPercent, bool isCustomColor = false)
{
    if (hueShiftDeg == 0 && lastWindowSaturationPercent >= 100) {
        return color;
    }
    float h = 0.0f, s = 0.0f, l = 0.0f, a = 0.0f;
    color.getHslF(&h, &s, &l, &a);

    if (hueShiftDeg != 0 && h >= 0.0f && s > 0.05f) {
        const int hueSteps = isCustomColor ? recentIdx : (recentIdx + 1);
        if (hueSteps > 0) {
            const float shift = float(hueSteps * hueShiftDeg) / 360.0f;
            h = std::fmod(h + shift + 10.0f, 1.0f);
        }
    }

    if (lastWindowSaturationPercent < 100) {
        const int totalSteps = std::max(1, totalCount);
        const int step = std::clamp(recentIdx + 1, 1, totalSteps);
        const float stepRatio = static_cast<float>(step) / static_cast<float>(totalSteps);
        const float minSatRatio = qBound<float>(0.0f, static_cast<float>(lastWindowSaturationPercent) / 100.0f, 1.0f);
        const float satMultiplier = 1.0f - stepRatio * (1.0f - minSatRatio);
        s = qBound<float>(0.0f, s * satMultiplier, 1.0f);
    }

    return QColor::fromHslF(h < 0.0f ? 0.0f : h, s, l, a);
}

} // namespace OutlineColor
