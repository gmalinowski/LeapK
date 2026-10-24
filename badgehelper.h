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
#include <QString>
#include <QStringList>
#include <QSet>

namespace OutlineBadge
{

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

inline QImage renderBadgeImage(const QChar &letter, const QColor &bgColor, const QColor &textColor,
                               bool isJumpMode, qreal scale = 1.0)
{
    const int baseWidth = isJumpMode ? 46 : 36;
    const int baseHeight = isJumpMode ? 34 : 26;
    const int w = qMax(1, qRound(baseWidth * scale));
    const int h = qMax(1, qRound(baseHeight * scale));

    QImage img(w, h, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal margin = 2.0 * scale;
    const QRectF rect(margin, margin, w - 2 * margin, h - 2 * margin);
    const qreal radius = (isJumpMode ? 8.0 : 6.0) * scale;

    // Drop shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, isJumpMode ? 140 : 90));
    p.drawRoundedRect(rect.translated(0, 2.0 * scale), radius, radius);

    // Background pill
    p.setBrush(bgColor);
    const QColor borderColor = isJumpMode ? QColor(255, 255, 255, 220) : QColor(255, 255, 255, 160);
    p.setPen(QPen(borderColor, isJumpMode ? (2.0 * scale) : (1.2 * scale)));
    p.drawRoundedRect(rect, radius, radius);

    // Letter
    QFont font(QStringLiteral("sans-serif"));
    font.setBold(true);
    font.setPixelSize(qMax(8, qRound((isJumpMode ? 18.0 : 13.0) * scale)));
    p.setFont(font);
    p.setPen(textColor);
    p.drawText(rect, Qt::AlignCenter, QString(letter.toUpper()));

    p.end();
    return img;
}

} // namespace OutlineBadge
