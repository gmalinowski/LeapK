/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRandomGenerator>
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

inline qreal perceivedLuminance(const QColor &c)
{
    // Perceived luminance using ITU-R BT.601 coefficients
    return 0.299 * c.redF() + 0.587 * c.greenF() + 0.114 * c.blueF();
}

inline QColor contrastingTextColor(const QColor &bgColor)
{
    // Threshold ~0.53 corresponds to ~135 on a 0-255 scale.
    // If background is light (luminance > 0.53), black text ensures sharp contrast.
    // If background is dark (luminance <= 0.53), white text ensures sharp contrast.
    return (perceivedLuminance(bgColor) > 0.53) ? QColor(0, 0, 0) : QColor(255, 255, 255);
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

inline QColor generateRandomMarkColor(const QList<QColor> &usedColors = {})
{
    static const QList<QColor> palette = {
        QColor(QStringLiteral("#e74c3c")), // Crimson Red
        QColor(QStringLiteral("#3498db")), // Sky Blue
        QColor(QStringLiteral("#2ecc71")), // Emerald Green
        QColor(QStringLiteral("#f1c40f")), // Sunflower Yellow
        QColor(QStringLiteral("#9b59b6")), // Amethyst Purple
        QColor(QStringLiteral("#e67e22")), // Tangerine Orange
        QColor(QStringLiteral("#1abc9c")), // Turquoise
        QColor(QStringLiteral("#ff3366")), // Hot Pink
        QColor(QStringLiteral("#00d2d3")), // Cyan
        QColor(QStringLiteral("#5f27cd")), // Deep Violet
        QColor(QStringLiteral("#ff9f43")), // Amber
        QColor(QStringLiteral("#10ac84")), // Aqua Mint
        QColor(QStringLiteral("#ee5253")), // Coral Red
        QColor(QStringLiteral("#54a0ff")), // Neon Blue
        QColor(QStringLiteral("#1dd1a1")), // Bright Teal
        QColor(QStringLiteral("#feca57"))  // Warm Yellow
    };

    QList<QColor> available;
    for (const QColor &col : palette) {
        if (!usedColors.contains(col)) {
            available.append(col);
        }
    }

    if (!available.isEmpty()) {
        const int idx = QRandomGenerator::global()->bounded(available.size());
        return available.at(idx);
    }

    const int hue = QRandomGenerator::global()->bounded(360);
    const int sat = 210 + QRandomGenerator::global()->bounded(45);
    const int val = 220 + QRandomGenerator::global()->bounded(35);
    return QColor::fromHsv(hue, sat, val);
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

inline QImage renderBadgeImage(const QChar &letter, const QColor &bgColor, const QColor &textColor = QColor(),
                               bool isJumpMode = false, int baseSize = 28, qreal scale = 1.0)
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

    // Contrasting text color & border
    const QColor effectiveTextColor = textColor.isValid() ? textColor : contrastingTextColor(bgColor);
    const bool isLightBg = (effectiveTextColor == Qt::black) || (perceivedLuminance(bgColor) > 0.53);

    // Background pill
    p.setBrush(bgColor);
    const QColor borderColor = isLightBg
        ? (isJumpMode ? QColor(0, 0, 0, 180) : QColor(0, 0, 0, 120))
        : (isJumpMode ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 170));
    p.setPen(QPen(borderColor, isJumpMode ? (2.0 * scale) : (1.2 * scale)));
    p.drawRoundedRect(rect, radius, radius);

    // Letter
    QFont font(QStringLiteral("sans-serif"));
    font.setBold(true);
    const int pixelSize = qMax(8, qRound(effectiveBaseHeight * 0.52 * scale));
    font.setPixelSize(pixelSize);
    p.setFont(font);
    p.setPen(effectiveTextColor);
    p.drawText(rect, Qt::AlignCenter, QString(letter.toUpper()));

    p.end();
    return img;
}

inline QImage renderOffscreenTargetPill(const QChar &letter,
                                        const QColor &bgColor,
                                        const QColor &textColor,
                                        const QString &locationText,
                                        const QString &caption,
                                        int badgeSize,
                                        qreal scale)
{
    const int pillH = qMax(32, qRound(badgeSize * 1.35 * scale));
    const int badgeDim = pillH - qRound(8 * scale);
    const int padX = qRound(8 * scale);
    const int gapX = qRound(8 * scale);

    QFont locFont(QStringLiteral("sans-serif"));
    locFont.setPixelSize(qMax(10, qRound(11 * scale)));
    locFont.setBold(true);

    QFont capFont(QStringLiteral("sans-serif"));
    capFont.setPixelSize(qMax(9, qRound(10 * scale)));
    capFont.setBold(false);

    QFontMetrics fmLoc(locFont);
    QFontMetrics fmCap(capFont);

    QString displayCap = caption;
    if (fmCap.horizontalAdvance(displayCap) > qRound(180 * scale)) {
        displayCap = fmCap.elidedText(displayCap, Qt::ElideRight, qRound(180 * scale));
    }

    const int locWidth = fmLoc.horizontalAdvance(locationText);
    const int capWidth = displayCap.isEmpty() ? 0 : fmCap.horizontalAdvance(displayCap);
    const int textWidth = qMax(locWidth, capWidth);

    const int pillW = padX + badgeDim + gapX + textWidth + padX;

    QImage img(pillW, pillH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal cornerRadius = 8.0 * scale;
    const QRectF shadowRect(1.0, 2.0 * scale, pillW - 2.0, pillH - 2.0);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 140));
    p.drawRoundedRect(shadowRect, cornerRadius, cornerRadius);

    const QRectF pillRect(0.5, 0.5, pillW - 1.0, pillH - 1.0);
    QPainterPath bgPath;
    bgPath.addRoundedRect(pillRect, cornerRadius, cornerRadius);
    p.fillPath(bgPath, QColor(22, 25, 29, 235));

    QColor borderColor = bgColor;
    borderColor.setAlpha(160);
    p.setPen(QPen(borderColor, 1.5 * scale));
    p.drawPath(bgPath);

    const QColor effectiveTextColor = textColor.isValid() ? textColor : contrastingTextColor(bgColor);
    const bool isLightBg = (effectiveTextColor == Qt::black) || (perceivedLuminance(bgColor) > 0.53);

    const QRectF badgeRect(padX, (pillH - badgeDim) / 2.0, badgeDim, badgeDim);
    QPainterPath bPath;
    bPath.addRoundedRect(badgeRect, 6.0 * scale, 6.0 * scale);
    p.fillPath(bPath, bgColor);
    const QColor innerBorder = isLightBg
        ? QColor(0, 0, 0, 160)
        : QColor(255, 255, 255, 200);
    p.setPen(QPen(innerBorder, 1.0 * scale));
    p.drawPath(bPath);

    QFont letterFont(QStringLiteral("sans-serif"));
    letterFont.setPixelSize(qRound(badgeDim * 0.62));
    letterFont.setBold(true);
    p.setFont(letterFont);
    p.setPen(effectiveTextColor);
    p.drawText(badgeRect, Qt::AlignCenter, QString(letter.toUpper()));

    const int textLeft = padX + badgeDim + gapX;
    if (displayCap.isEmpty()) {
        const QRectF locRect(textLeft, 0, textWidth, pillH);
        p.setFont(locFont);
        p.setPen(Qt::white);
        p.drawText(locRect, Qt::AlignVCenter | Qt::AlignLeft, locationText);
    } else {
        const qreal midY = pillH / 2.0;
        const QRectF locRect(textLeft, qRound(midY - fmLoc.height() * 0.95), textWidth, fmLoc.height());
        const QRectF capRect(textLeft, qRound(midY + fmCap.height() * 0.05), textWidth, fmCap.height());

        p.setFont(locFont);
        p.setPen(QColor(241, 196, 15));
        p.drawText(locRect, Qt::AlignLeft | Qt::AlignVCenter, locationText);

        p.setFont(capFont);
        p.setPen(QColor(220, 225, 230));
        p.drawText(capRect, Qt::AlignLeft | Qt::AlignVCenter, displayCap);
    }

    p.end();
    return img;
}

} // namespace OutlineBadge
