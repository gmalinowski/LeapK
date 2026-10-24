/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QSet>
#include <algorithm>

namespace OutlineBadge
{

enum class BadgePosition {
    TopLeft = 0,
    Top = 1,
    TopRight = 2,
    Left = 3,
    Center = 4,
    Right = 5,
    BottomLeft = 6,
    Bottom = 7,
    BottomRight = 8
};

inline const QString &preferredLetterPool()
{
    static const QString pool = QStringLiteral("ASDFJKLQWERTYUIOPZXCVBNM123456789");
    return pool;
}

inline QChar findNextAvailableLetter(const QList<QChar> &usedLetters)
{
    QSet<QChar> usedSet;
    for (const QChar &ch : usedLetters) {
        usedSet.insert(ch.toUpper());
    }

    const QString &pool = preferredLetterPool();
    for (int i = 0; i < pool.length(); ++i) {
        const QChar candidate = pool.at(i);
        if (!usedSet.contains(candidate)) {
            return candidate;
        }
    }
    return QLatin1Char('?');
}

inline QRectF computeBadgeRect(const QRectF &geo, int position, qreal badgeWidth, qreal badgeHeight, qreal margin)
{
    qreal x = geo.left() + margin;
    qreal y = geo.top() + margin;

    switch (position) {
    case 0: // Top-Left
        x = geo.left() + margin;
        y = geo.top() + margin;
        break;
    case 1: // Top
        x = geo.left() + (geo.width() - badgeWidth) / 2.0;
        y = geo.top() + margin;
        break;
    case 2: // Top-Right
        x = geo.right() - margin - badgeWidth;
        y = geo.top() + margin;
        break;
    case 3: // Left
        x = geo.left() + margin;
        y = geo.top() + (geo.height() - badgeHeight) / 2.0;
        break;
    case 4: // Center
        x = geo.left() + (geo.width() - badgeWidth) / 2.0;
        y = geo.top() + (geo.height() - badgeHeight) / 2.0;
        break;
    case 5: // Right
        x = geo.right() - margin - badgeWidth;
        y = geo.top() + (geo.height() - badgeHeight) / 2.0;
        break;
    case 6: // Bottom-Left
        x = geo.left() + margin;
        y = geo.bottom() - margin - badgeHeight;
        break;
    case 7: // Bottom
        x = geo.left() + (geo.width() - badgeWidth) / 2.0;
        y = geo.bottom() - margin - badgeHeight;
        break;
    case 8: // Bottom-Right
        x = geo.right() - margin - badgeWidth;
        y = geo.bottom() - margin - badgeHeight;
        break;
    default:
        x = geo.left() + margin;
        y = geo.top() + margin;
        break;
    }

    if (geo.width() >= badgeWidth) {
        x = std::max(geo.left(), std::min(x, geo.right() - badgeWidth));
    }
    if (geo.height() >= badgeHeight) {
        y = std::max(geo.top(), std::min(y, geo.bottom() - badgeHeight));
    }

    return QRectF(x, y, badgeWidth, badgeHeight);
}

inline QImage renderBadgeImage(const QChar &letter, const QColor &bgColor, const QColor &textColor,
                               bool isJumpMode, int baseSize = 28, qreal scale = 1.0)
{
    const int effectiveBaseHeight = isJumpMode ? qRound(baseSize * 1.25) : baseSize;
    const int effectiveBaseWidth = qRound(effectiveBaseHeight * 1.35);
    const int w = qMax(1, qRound(effectiveBaseWidth * scale));
    const int h = qMax(1, qRound(effectiveBaseHeight * scale));

    QImage img(w, h, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal margin = 2.0 * scale;
    const QRectF rect(margin, margin, w - 2 * margin, h - 2 * margin);
    const qreal radius = qMax(3.0, (effectiveBaseHeight * 0.22) * scale);

    // Drop shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, isJumpMode ? 150 : 100));
    p.drawRoundedRect(rect.translated(0, 2.0 * scale), radius, radius);

    // Background pill
    p.setBrush(bgColor);
    const QColor borderColor = isJumpMode ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 170);
    p.setPen(QPen(borderColor, isJumpMode ? (2.0 * scale) : (1.2 * scale)));
    p.drawRoundedRect(rect, radius, radius);

    // Letter
    QFont font(QStringLiteral("sans-serif"));
    font.setBold(true);
    const int pixelSize = qMax(8, qRound(effectiveBaseHeight * 0.52 * scale));
    font.setPixelSize(pixelSize);
    p.setFont(font);
    p.setPen(textColor);
    p.drawText(rect, Qt::AlignCenter, QString(letter.toUpper()));

    p.end();
    return img;
}

} // namespace OutlineBadge
