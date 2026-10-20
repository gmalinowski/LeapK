// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <cmath>

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

} // namespace OutlineColor
