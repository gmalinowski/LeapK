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
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QSet>
#include <algorithm>
#include "ambilightalgorithm.h"

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

inline QColor extractSmartAccentFromImage(const QImage &image,
                                         qreal saturationLeveling = 0.65,
                                         qreal fallbackStrength = 0.50,
                                         qreal minBrightness = 0.25,
                                         qreal maxBrightness = 0.85)
{
    if (image.isNull()) {
        return QColor();
    }

    const QImage scaled = (image.width() == 32 && image.height() == 32 && image.format() == QImage::Format_RGBA8888)
        ? image
        : image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);

    OutlineAmbilight::Settings settings;
    settings.mode = OutlineAmbilight::Mode::Smart;
    settings.saturationLeveling = saturationLeveling;
    settings.fallbackStrength = fallbackStrength;
    settings.minBrightness = minBrightness;
    settings.maxBrightness = maxBrightness;
    settings.ensureContrast = false;
    settings.sampleWidth = scaled.width();
    settings.sampleHeight = scaled.height();

    return OutlineAmbilight::processRgbaSamples(scaled.constBits(), scaled.width() * scaled.height(), settings);
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

inline qreal computeBadgeWidth(qreal baseHeight, const QString &customLabel = QString())
{
    const qreal baseW = qRound(baseHeight * 1.35);
    const QString cleanLabel = customLabel.trimmed();
    if (cleanLabel.isEmpty()) {
        return baseW;
    }

    QFont labelFont(QStringLiteral("sans-serif"));
    labelFont.setPixelSize(qMax(8, qRound(baseHeight * 0.44)));
    labelFont.setBold(true);
    QFontMetrics fm(labelFont);

    QString displayLabel = cleanLabel;
    const int maxLabelW = 160;
    if (fm.horizontalAdvance(displayLabel) > maxLabelW) {
        displayLabel = fm.elidedText(displayLabel, Qt::ElideRight, maxLabelW);
    }

    const int labelW = fm.horizontalAdvance(displayLabel);
    return baseW + labelW + 18.0;
}

inline QImage renderBadgeImage(const QChar &letter, const QColor &bgColor, const QColor &textColor = QColor(),
                               bool isJumpMode = false, int baseSize = 28, qreal scale = 1.0,
                               const QString &customLabel = QString())
{
    const int effectiveBaseHeight = isJumpMode ? qRound(baseSize * 1.25) : baseSize;
    const QString cleanLabel = customLabel.trimmed();
    const bool hasLabel = !cleanLabel.isEmpty();

    QFont labelFont(QStringLiteral("sans-serif"));
    labelFont.setPixelSize(qMax(8, qRound(effectiveBaseHeight * 0.44 * scale)));
    labelFont.setBold(true);
    QFontMetrics fmLabel(labelFont);

    QString displayLabel = cleanLabel;
    int labelTextWidth = 0;
    if (hasLabel) {
        const int maxLabelW = qRound(160 * scale);
        if (fmLabel.horizontalAdvance(displayLabel) > maxLabelW) {
            displayLabel = fmLabel.elidedText(displayLabel, Qt::ElideRight, maxLabelW);
        }
        labelTextWidth = fmLabel.horizontalAdvance(displayLabel);
    }

    const int effectiveLetterWidth = qRound(effectiveBaseHeight * 1.35);
    const int effectiveBaseWidth = hasLabel
        ? effectiveLetterWidth + qRound((labelTextWidth + 18 * scale) / scale)
        : effectiveLetterWidth;

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

    QFont font(QStringLiteral("sans-serif"));
    font.setBold(true);
    const int pixelSize = qMax(8, qRound(effectiveBaseHeight * 0.52 * scale));
    font.setPixelSize(pixelSize);
    p.setFont(font);
    p.setPen(effectiveTextColor);

    if (hasLabel) {
        const qreal letterW = qRound(effectiveLetterWidth * scale);
        const QRectF letterRect(margin, margin, letterW - margin, h - 2 * margin);
        p.drawText(letterRect, Qt::AlignCenter, QString(letter.toUpper()));

        // Vertical divider
        const qreal sepX = margin + letterW - 2.0 * scale;
        const QColor sepCol = isLightBg ? QColor(0, 0, 0, 70) : QColor(255, 255, 255, 90);
        p.setPen(QPen(sepCol, 1.2 * scale));
        p.drawLine(QPointF(sepX, margin + 4.0 * scale), QPointF(sepX, h - margin - 4.0 * scale));

        // Label text
        const qreal labelX = sepX + 6.0 * scale;
        const QRectF labelRect(labelX, margin, w - margin - labelX, h - 2 * margin);
        p.setFont(labelFont);
        p.setPen(effectiveTextColor);
        p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, displayLabel);
    } else {
        p.drawText(rect, Qt::AlignCenter, QString(letter.toUpper()));
    }

    p.end();
    return img;
}

inline QImage renderOffscreenTargetPill(const QChar &letter,
                                        const QColor &bgColor,
                                        const QColor &textColor,
                                        const QString &locationText,
                                        const QString &caption,
                                        int badgeSize,
                                        qreal scale,
                                        const QString &customLabel = QString())
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

    const QString cleanLabel = customLabel.trimmed();
    const bool hasCustomLabel = !cleanLabel.isEmpty();

    QString line1Text;
    QString line2Text;
    int textWidth = 0;

    QFont line1Font = locFont;
    QFont line2Font = capFont;
    QColor line1Color = QColor(241, 196, 15);
    QColor line2Color = QColor(220, 225, 230);

    if (hasCustomLabel) {
        line1Font = locFont;
        line1Font.setPixelSize(qMax(10, qRound(11.5 * scale)));
        line1Font.setBold(true);
        line1Color = Qt::white;

        line2Font = capFont;
        line2Color = QColor(241, 196, 15);

        QFontMetrics fmLine1(line1Font);
        QFontMetrics fmLine2(line2Font);

        line1Text = cleanLabel;
        if (fmLine1.horizontalAdvance(line1Text) > qRound(220 * scale)) {
            line1Text = fmLine1.elidedText(line1Text, Qt::ElideRight, qRound(220 * scale));
        }

        line2Text = locationText;
        if (!caption.trimmed().isEmpty()) {
            line2Text += QStringLiteral(" · ") + caption.trimmed();
        }
        if (fmLine2.horizontalAdvance(line2Text) > qRound(240 * scale)) {
            line2Text = fmLine2.elidedText(line2Text, Qt::ElideRight, qRound(240 * scale));
        }

        const int w1 = fmLine1.horizontalAdvance(line1Text);
        const int w2 = fmLine2.horizontalAdvance(line2Text);
        textWidth = qMax(w1, w2);
    } else {
        QString displayCap = caption;
        if (fmCap.horizontalAdvance(displayCap) > qRound(180 * scale)) {
            displayCap = fmCap.elidedText(displayCap, Qt::ElideRight, qRound(180 * scale));
        }

        line1Text = locationText;
        line2Text = displayCap;

        const int locWidth = fmLoc.horizontalAdvance(line1Text);
        const int capWidth = line2Text.isEmpty() ? 0 : fmCap.horizontalAdvance(line2Text);
        textWidth = qMax(locWidth, capWidth);
    }

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
    if (line2Text.isEmpty()) {
        const QRectF locRect(textLeft, 0, textWidth, pillH);
        p.setFont(line1Font);
        p.setPen(Qt::white);
        p.drawText(locRect, Qt::AlignVCenter | Qt::AlignLeft, line1Text);
    } else {
        QFontMetrics fm1(line1Font);
        QFontMetrics fm2(line2Font);
        const qreal midY = pillH / 2.0;
        const QRectF r1(textLeft, qRound(midY - fm1.height() * 0.95), textWidth, fm1.height());
        const QRectF r2(textLeft, qRound(midY + fm2.height() * 0.05), textWidth, fm2.height());

        p.setFont(line1Font);
        p.setPen(line1Color);
        p.drawText(r1, Qt::AlignLeft | Qt::AlignVCenter, line1Text);

        p.setFont(line2Font);
        p.setPen(line2Color);
        p.drawText(r2, Qt::AlignLeft | Qt::AlignVCenter, line2Text);
    }

    p.end();
    return img;
}

inline QList<QRectF> computeHudPillPositions(const QList<QSizeF> &pillSizes,
                                             const QRectF &screenRect,
                                             int hudPosition = 0,
                                             qreal margin = 24.0,
                                             qreal gap = 12.0)
{
    if (pillSizes.isEmpty()) {
        return {};
    }

    QList<QRectF> result;
    result.resize(pillSizes.size());

    const int clampedPos = qBound(0, hudPosition, 3);

    if (clampedPos == 0 || clampedPos == 1) {
        // Horizontal layout with row wrapping: 0 = Bottom (default), 1 = Top
        const qreal availWidth = qMax(60.0, screenRect.width() - 2.0 * margin);

        struct Row {
            QList<int> indices;
            qreal totalWidth = 0.0;
            qreal maxHeight = 0.0;
        };

        QList<Row> rows;
        Row currentRow;

        for (int i = 0; i < pillSizes.size(); ++i) {
            const QSizeF &sz = pillSizes.at(i);
            const qreal neededWidth = currentRow.indices.isEmpty()
                ? sz.width()
                : (currentRow.totalWidth + gap + sz.width());

            if (neededWidth <= availWidth || currentRow.indices.isEmpty()) {
                currentRow.indices.append(i);
                currentRow.totalWidth = neededWidth;
                currentRow.maxHeight = qMax(currentRow.maxHeight, sz.height());
            } else {
                rows.append(currentRow);
                currentRow = Row();
                currentRow.indices.append(i);
                currentRow.totalWidth = sz.width();
                currentRow.maxHeight = sz.height();
            }
        }
        if (!currentRow.indices.isEmpty()) {
            rows.append(currentRow);
        }

        qreal clusterHeight = 0.0;
        for (const Row &r : rows) {
            clusterHeight += r.maxHeight;
        }
        clusterHeight += (rows.size() - 1) * gap;

        qreal rowY = 0.0;
        if (clampedPos == 1) { // Top
            rowY = screenRect.top() + margin;
        } else { // 0 = Bottom
            rowY = screenRect.bottom() - margin - clusterHeight;
            if (rowY < screenRect.top() + margin) {
                rowY = screenRect.top() + margin;
            }
        }

        for (const Row &r : rows) {
            qreal rowStartX = screenRect.left() + (screenRect.width() - r.totalWidth) / 2.0;
            if (rowStartX < screenRect.left() + margin) {
                rowStartX = screenRect.left() + margin;
            }
            qreal currentX = rowStartX;
            for (int idx : r.indices) {
                const QSizeF &sz = pillSizes.at(idx);
                const qreal itemY = rowY + (r.maxHeight - sz.height()) / 2.0;
                result[idx] = QRectF(currentX, itemY, sz.width(), sz.height());
                currentX += sz.width() + gap;
            }
            rowY += r.maxHeight + gap;
        }
    } else {
        // Vertical layout with column wrapping: 2 = Left, 3 = Right
        const qreal availHeight = qMax(60.0, screenRect.height() - 2.0 * margin);

        struct Col {
            QList<int> indices;
            qreal totalHeight = 0.0;
            qreal maxWidth = 0.0;
        };

        QList<Col> cols;
        Col currentCol;

        for (int i = 0; i < pillSizes.size(); ++i) {
            const QSizeF &sz = pillSizes.at(i);
            const qreal neededHeight = currentCol.indices.isEmpty()
                ? sz.height()
                : (currentCol.totalHeight + gap + sz.height());

            if (neededHeight <= availHeight || currentCol.indices.isEmpty()) {
                currentCol.indices.append(i);
                currentCol.totalHeight = neededHeight;
                currentCol.maxWidth = qMax(currentCol.maxWidth, sz.width());
            } else {
                cols.append(currentCol);
                currentCol = Col();
                currentCol.indices.append(i);
                currentCol.totalHeight = sz.height();
                currentCol.maxWidth = sz.width();
            }
        }
        if (!currentCol.indices.isEmpty()) {
            cols.append(currentCol);
        }

        qreal clusterWidth = 0.0;
        for (const Col &c : cols) {
            clusterWidth += c.maxWidth;
        }
        clusterWidth += (cols.size() - 1) * gap;

        qreal colX = 0.0;
        if (clampedPos == 2) { // Left
            colX = screenRect.left() + margin;
        } else { // 3 = Right
            colX = screenRect.right() - margin - clusterWidth;
            if (colX < screenRect.left() + margin) {
                colX = screenRect.left() + margin;
            }
        }

        for (const Col &c : cols) {
            qreal colStartY = screenRect.top() + (screenRect.height() - c.totalHeight) / 2.0;
            if (colStartY < screenRect.top() + margin) {
                colStartY = screenRect.top() + margin;
            }
            qreal currentY = colStartY;
            for (int idx : c.indices) {
                const QSizeF &sz = pillSizes.at(idx);
                const qreal itemX = colX; // Left-aligned so letter badges align in a straight vertical column
                result[idx] = QRectF(itemX, currentY, sz.width(), sz.height());
                currentY += sz.height() + gap;
            }
            colX += c.maxWidth + gap;
        }
    }

    return result;
}

} // namespace OutlineBadge
