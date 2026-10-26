/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
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

enum class HudMarkColorMode {
    FullMarkColor = 0,
    TintedMarkColor = 1,
    DarkWithBadge = 2
};

struct BadgeConfig {
    // Sizing
    int windowBadgeSize = 28;
    int jumpBadgeSize = 34;
    int hudBadgeSize = 36;
    int iconSize = 20;
    int thumbnailWidth = 56;
    int thumbnailHeight = 32;
    int margin = 0;

    // Appearance
    QColor defaultBgColor = QColor(35, 38, 41, 235);
    int cornerRadius = 6;
    int opacityPercent = 92;
    int hudMarkColorMode = 0; // 0 = Full mark color, 1 = Tinted mark color, 2 = Dark with badge

    // Features
    bool showIcon = false;
    bool showThumbnail = false;

    // Typography
    QString letterFontFamily;
    int letterFontSize = 0; // 0 = auto
    bool letterColorAuto = true;
    QColor letterColor = QColor(255, 255, 255);

    QString labelFontFamily;
    int labelFontSize = 0; // 0 = auto
    bool labelColorAuto = true;
    QColor labelUnmarkedColor = QColor(241, 196, 15);

    QString captionFontFamily;
    int captionFontSize = 0; // 0 = auto
    bool captionColorAuto = true;
    QColor captionColor = QColor(220, 225, 230);

    QString locationFontFamily;
    int locationFontSize = 0; // 0 = auto
    bool locationColorAuto = true;
    QColor locationColor = QColor(241, 196, 15);
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

inline qreal computeBadgeWidth(qreal baseHeight, const QString &customLabel = QString(),
                               bool showIcon = false, bool showThumbnail = false,
                               qreal thumbnailAspect = 1.6, int iconSize = 0,
                               int thumbWidth = 0, int labelFontSize = 0,
                               const QString &labelFontFamily = QString(),
                               int thumbHeight = 0)
{
    const qreal baseW = qRound(baseHeight * 1.35);
    qreal totalW = baseW;

    if (showIcon) {
        const int maxIconDim = qMax(12, qRound(baseHeight - 6.0));
        const int iconDim = (iconSize > 0) ? qMin(iconSize, maxIconDim) : qRound(baseHeight * 0.70);
        totalW += iconDim + 10.0;
    }

    if (showThumbnail) {
        const qreal maxThumbH = qMax(12.0, baseHeight - 6.0);
        const qreal effThumbH = (thumbHeight > 0) ? qMin(qreal(thumbHeight), maxThumbH) : maxThumbH;
        const qreal configAspect = (thumbHeight > 0 && thumbWidth > 0) ? (qreal(thumbWidth) / qreal(thumbHeight)) : thumbnailAspect;
        const qreal aspect = qBound(1.0, configAspect, 2.0);
        const qreal effThumbW = (thumbWidth > 0) ? qMin(qreal(thumbWidth), qreal(qRound(effThumbH * aspect))) : qreal(qRound(effThumbH * aspect));
        totalW += effThumbW + 10.0;
    }

    const QString cleanLabel = customLabel.trimmed();
    if (cleanLabel.isEmpty()) {
        return totalW;
    }

    const QString family = labelFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : labelFontFamily.trimmed();
    QFont labelFont(family);
    labelFont.setPixelSize((labelFontSize > 0) ? labelFontSize : qMax(8, qRound(baseHeight * 0.44)));
    labelFont.setBold(true);
    QFontMetrics fm(labelFont);

    QString displayLabel = cleanLabel;
    const int maxLabelW = 160;
    if (fm.horizontalAdvance(displayLabel) > maxLabelW) {
        displayLabel = fm.elidedText(displayLabel, Qt::ElideRight, maxLabelW);
    }

    const int labelW = fm.horizontalAdvance(displayLabel);
    return totalW + labelW + 18.0;
}

inline qreal computeBadgeWidth(const BadgeConfig &cfg, qreal baseHeight, const QString &customLabel = QString())
{
    return computeBadgeWidth(baseHeight, customLabel, cfg.showIcon, cfg.showThumbnail,
                             1.6, cfg.iconSize, cfg.thumbnailWidth, cfg.labelFontSize, cfg.labelFontFamily,
                             cfg.thumbnailHeight);
}

inline QImage renderBadgeImage(const BadgeConfig &cfg,
                               const QChar &letter,
                               const QColor &bgColor,
                               const QColor &textColor = QColor(),
                               bool isJumpMode = false,
                               int baseSize = 0,
                               qreal scale = 1.0,
                               const QString &customLabel = QString(),
                               const QIcon &icon = QIcon(),
                               const QImage &thumbnail = QImage())
{
    const int effectiveBaseHeight = (baseSize > 0)
        ? baseSize
        : (isJumpMode ? cfg.jumpBadgeSize : cfg.windowBadgeSize);

    const QString cleanLabel = customLabel.trimmed();
    const bool hasLabel = !cleanLabel.isEmpty();

    const bool hasIcon = cfg.showIcon && !icon.isNull();
    const int maxIconDim = qMax(12, effectiveBaseHeight - 6);
    const int iconDim = hasIcon
        ? ((cfg.iconSize > 0) ? qMin(cfg.iconSize, maxIconDim) : qRound(effectiveBaseHeight * 0.70))
        : 0;
    const int iconSectionW = hasIcon ? (iconDim + 10) : 0;

    const bool hasThumb = cfg.showThumbnail && !thumbnail.isNull();
    const int maxThumbH = qMax(12, effectiveBaseHeight - 6);
    const int thumbH = hasThumb
        ? ((cfg.thumbnailHeight > 0) ? qMin(cfg.thumbnailHeight, maxThumbH) : maxThumbH)
        : 0;
    const qreal rawThumbAspect = (cfg.thumbnailHeight > 0 && cfg.thumbnailWidth > 0)
        ? (qreal(cfg.thumbnailWidth) / qreal(cfg.thumbnailHeight))
        : ((hasThumb && thumbnail.height() > 0) ? (qreal(thumbnail.width()) / qreal(thumbnail.height())) : 1.6);
    const qreal thumbAspect = qBound(1.0, rawThumbAspect, 2.0);
    const int thumbW = hasThumb
        ? ((cfg.thumbnailWidth > 0) ? qMin(cfg.thumbnailWidth, qRound(thumbH * thumbAspect)) : qRound(thumbH * thumbAspect))
        : 0;
    const int thumbSectionW = hasThumb ? (thumbW + 10) : 0;

    const QString labelFam = cfg.labelFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.labelFontFamily.trimmed();
    QFont labelFont(labelFam);
    const int labelPx = (cfg.labelFontSize > 0) ? qRound(cfg.labelFontSize * scale) : qMax(8, qRound(effectiveBaseHeight * 0.44 * scale));
    labelFont.setPixelSize(labelPx);
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
    const int labelSectionW = hasLabel ? qRound((labelTextWidth + 18 * scale) / scale) : 0;

    const int effectiveLetterWidth = qRound(effectiveBaseHeight * 1.35);
    const int effectiveBaseWidth = effectiveLetterWidth + iconSectionW + thumbSectionW + labelSectionW;

    const int w = qMax(1, qRound(effectiveBaseWidth * scale));
    const int h = qMax(1, qRound(effectiveBaseHeight * scale));

    QImage img(w, h, QImage::Format_RGBA8888_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal margin = 2.0 * scale;
    const QRectF rect(margin, margin, w - 2 * margin, h - 2 * margin);
    const qreal radius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, cfg.cornerRadius * scale);

    // Drop shadow (rendered outside the pill so translucent pills remain vibrant)
    QPainterPath pillPath;
    pillPath.addRoundedRect(rect, radius, radius);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(rect.translated(0, 2.0 * scale), radius, radius);
    p.setPen(Qt::NoPen);
    p.fillPath(shadowPath.subtracted(pillPath), QColor(0, 0, 0, isJumpMode ? 150 : 100));

    // Background color with opacity
    QColor effectiveBg = bgColor.isValid() ? bgColor : cfg.defaultBgColor;
    effectiveBg.setAlphaF(qBound(0.1, effectiveBg.alphaF() * (cfg.opacityPercent / 100.0), 1.0));

    // Contrasting text color & border
    const QColor autoTextColor = contrastingTextColor(effectiveBg);
    const QColor effectiveTextColor = textColor.isValid() ? textColor : autoTextColor;
    const bool isLightBg = (effectiveTextColor == Qt::black) || (perceivedLuminance(effectiveBg) > 0.53);

    // Background pill
    p.setBrush(effectiveBg);
    const QColor borderColor = isLightBg
        ? (isJumpMode ? QColor(0, 0, 0, 180) : QColor(0, 0, 0, 120))
        : (isJumpMode ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 170));
    p.setPen(QPen(borderColor, isJumpMode ? (2.0 * scale) : (1.2 * scale)));
    p.drawRoundedRect(rect, radius, radius);

    const QColor sepCol = isLightBg ? QColor(0, 0, 0, 70) : QColor(255, 255, 255, 90);

    const QString letterFam = cfg.letterFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.letterFontFamily.trimmed();
    QFont font(letterFam);
    font.setBold(true);
    const int letterPx = (cfg.letterFontSize > 0) ? qRound(cfg.letterFontSize * scale) : qMax(8, qRound(effectiveBaseHeight * 0.52 * scale));
    font.setPixelSize(letterPx);
    p.setFont(font);
    const QColor effLetterColor = (cfg.letterColorAuto || !cfg.letterColor.isValid()) ? autoTextColor : cfg.letterColor;
    p.setPen(effLetterColor);

    const bool hasExtra = hasIcon || hasThumb || hasLabel;
    if (!hasExtra) {
        p.drawText(rect, Qt::AlignCenter, QString(letter.toUpper()));
    } else {
        qreal currentX = margin;

        // 1. Letter
        const qreal letterW = qRound(effectiveLetterWidth * scale);
        const QRectF letterRect(currentX, margin, letterW - margin, h - 2 * margin);
        p.drawText(letterRect, Qt::AlignCenter, QString(letter.toUpper()));
        currentX += letterW;

        // 2. Icon
        if (hasIcon) {
            const qreal sepX = currentX - 1.0 * scale;
            p.setPen(QPen(sepCol, 1.2 * scale));
            p.drawLine(QPointF(sepX, margin + 4.0 * scale), QPointF(sepX, h - margin - 4.0 * scale));
            currentX += 4.0 * scale;

            const qreal iconScaledDim = iconDim * scale;
            const QRectF iconRect(currentX, (h - iconScaledDim) / 2.0, iconScaledDim, iconScaledDim);
            icon.paint(&p, iconRect.toRect());
            currentX += iconScaledDim + 6.0 * scale;
        }

        // 3. Thumbnail
        if (hasThumb) {
            const qreal sepThumbX = currentX - 1.0 * scale;
            p.setPen(QPen(sepCol, 1.2 * scale));
            p.drawLine(QPointF(sepThumbX, margin + 4.0 * scale), QPointF(sepThumbX, h - margin - 4.0 * scale));
            currentX += 4.0 * scale;

            const qreal thumbScaledW = thumbW * scale;
            const qreal thumbScaledH = thumbH * scale;
            const QRectF thumbRect(currentX, (h - thumbScaledH) / 2.0, thumbScaledW, thumbScaledH);

            const qreal thumbRadius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, 3.0 * scale);
            QPainterPath thumbPath;
            thumbPath.addRoundedRect(thumbRect, thumbRadius, thumbRadius);
            p.save();
            p.setClipPath(thumbPath);
            p.drawImage(thumbRect, thumbnail);
            p.restore();

            p.setPen(QPen(isLightBg ? QColor(0, 0, 0, 70) : QColor(255, 255, 255, 90), 1.0 * scale));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(thumbRect, thumbRadius, thumbRadius);

            currentX += thumbScaledW + 6.0 * scale;
        }

        // 4. Label
        if (hasLabel) {
            const qreal sepLabelX = currentX - 1.0 * scale;
            p.setPen(QPen(sepCol, 1.2 * scale));
            p.drawLine(QPointF(sepLabelX, margin + 4.0 * scale), QPointF(sepLabelX, h - margin - 4.0 * scale));
            currentX += 6.0 * scale;

            const QRectF labelRect(currentX, margin, w - margin - currentX, h - 2 * margin);
            p.setFont(labelFont);
            const QColor effLabelColor = textColor.isValid()
                ? textColor
                : ((cfg.labelColorAuto || !cfg.labelUnmarkedColor.isValid()) ? autoTextColor : cfg.labelUnmarkedColor);
            p.setPen(effLabelColor);
            p.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, displayLabel);
        }
    }

    p.end();
    return img;
}

inline QImage renderBadgeImage(const QChar &letter, const QColor &bgColor, const QColor &textColor = QColor(),
                               bool isJumpMode = false, int baseSize = 28, qreal scale = 1.0,
                               const QString &customLabel = QString(),
                               bool showIcon = false, const QIcon &icon = QIcon(),
                               bool showThumbnail = false, const QImage &thumbnail = QImage())
{
    BadgeConfig cfg;
    cfg.windowBadgeSize = baseSize;
    cfg.jumpBadgeSize = qRound(baseSize * 1.25);
    cfg.showIcon = showIcon;
    cfg.showThumbnail = showThumbnail;
    const int effectiveSize = isJumpMode ? cfg.jumpBadgeSize : baseSize;
    return renderBadgeImage(cfg, letter, bgColor, textColor, isJumpMode, effectiveSize, scale, customLabel, icon, thumbnail);
}

inline QImage renderOffscreenTargetPill(const BadgeConfig &cfg,
                                        const QChar &letter,
                                        const QColor &bgColor,
                                        const QColor &textColor,
                                        const QString &locationText,
                                        const QString &caption,
                                        int badgeSize,
                                        qreal scale,
                                        const QString &customLabel = QString(),
                                        const QIcon &icon = QIcon(),
                                        const QImage &thumbnail = QImage(),
                                        bool isMarked = false)
{
    Q_UNUSED(textColor);
    const int effectivePillBaseH = (badgeSize > 0) ? badgeSize : cfg.hudBadgeSize;
    const int pillH = qMax(24, qRound(effectivePillBaseH * 1.35 * scale));
    const int badgeDim = qMax(16, pillH - qRound(8 * scale));
    const int padX = qRound(8 * scale);
    const int gapX = qRound(8 * scale);

    const bool hasIcon = cfg.showIcon && !icon.isNull();
    const int maxIconDim = qMax(12, pillH - qRound(8 * scale));
    const int iconDim = hasIcon
        ? ((cfg.iconSize > 0) ? qMin(qRound(cfg.iconSize * scale), maxIconDim) : qRound(badgeDim * 0.85))
        : 0;

    const bool hasThumb = cfg.showThumbnail && !thumbnail.isNull();
    const int maxThumbH = qMax(14, pillH - qRound(8 * scale));
    const int thumbH = hasThumb
        ? ((cfg.thumbnailHeight > 0) ? qMin(qRound(cfg.thumbnailHeight * scale), maxThumbH) : badgeDim)
        : 0;
    const qreal rawThumbAspect = (hasThumb && thumbnail.height() > 0)
        ? (qreal(thumbnail.width()) / qreal(thumbnail.height()))
        : 1.6;
    const qreal thumbAspect = qBound(1.0, rawThumbAspect, 2.0);
    const int thumbW = hasThumb
        ? ((cfg.thumbnailWidth > 0) ? qMin(qRound(cfg.thumbnailWidth * scale), qRound(thumbH * thumbAspect)) : qRound(thumbH * thumbAspect))
        : 0;

    const QString locFam = cfg.locationFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.locationFontFamily.trimmed();
    QFont locFont(locFam);
    const int locPx = (cfg.locationFontSize > 0) ? qRound(cfg.locationFontSize * scale) : qMax(10, qRound(11 * scale));
    locFont.setPixelSize(locPx);
    locFont.setBold(true);

    const QString capFam = cfg.captionFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.captionFontFamily.trimmed();
    QFont capFont(capFam);
    const int capPx = (cfg.captionFontSize > 0) ? qRound(cfg.captionFontSize * scale) : qMax(9, qRound(10 * scale));
    capFont.setPixelSize(capPx);
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

    if (hasCustomLabel) {
        const QString labelFam = cfg.labelFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.labelFontFamily.trimmed();
        line1Font = QFont(labelFam);
        const int labelPx = (cfg.labelFontSize > 0) ? qRound(cfg.labelFontSize * scale) : qMax(10, qRound(11.5 * scale));
        line1Font.setPixelSize(labelPx);
        line1Font.setBold(true);

        line2Font = capFont;

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

    int extraW = 0;
    if (hasIcon) {
        extraW += iconDim + gapX;
    }
    if (hasThumb) {
        extraW += thumbW + gapX;
    }

    const int pillW = padX + badgeDim + extraW + gapX + textWidth + padX;

    QImage img(pillW, pillH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal cornerRadius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, cfg.cornerRadius * 1.3 * scale);
    const QRectF pillRect(0.5, 0.5, pillW - 1.0, pillH - 1.0);
    QPainterPath bgPath;
    bgPath.addRoundedRect(pillRect, cornerRadius, cornerRadius);

    // Drop shadow (rendered outside the card so translucent cards remain vibrant)
    const QRectF shadowRect(1.0, 2.0 * scale, pillW - 2.0, pillH - 2.0);
    QPainterPath shadowPath;
    shadowPath.addRoundedRect(shadowRect, cornerRadius, cornerRadius);
    p.setPen(Qt::NoPen);
    p.fillPath(shadowPath.subtracted(bgPath), QColor(0, 0, 0, 140));

    QColor effectiveBg = bgColor.isValid() ? bgColor : cfg.defaultBgColor;
    effectiveBg.setAlphaF(qBound(0.1, effectiveBg.alphaF() * (cfg.opacityPercent / 100.0), 1.0));

    const bool effectivelyMarked = isMarked || (bgColor.isValid() && bgColor != cfg.defaultBgColor && bgColor != QColor(35, 38, 41, 235));

    QColor pillBg;
    QColor borderColor;
    QColor badgeBg;

    if (effectivelyMarked) {
        if (cfg.hudMarkColorMode == 0) { // Full mark color
            pillBg = effectiveBg;
            const bool isLight = (perceivedLuminance(pillBg) > 0.53);
            borderColor = isLight ? QColor(0, 0, 0, 160) : QColor(255, 255, 255, 180);
            badgeBg = isLight ? pillBg.darker(118) : pillBg.lighter(125);
        } else if (cfg.hudMarkColorMode == 1) { // Tinted mark color
            const QColor darkBase(22, 25, 29);
            pillBg = QColor::fromRgbF(
                darkBase.redF() * 0.45 + effectiveBg.redF() * 0.55,
                darkBase.greenF() * 0.45 + effectiveBg.greenF() * 0.55,
                darkBase.blueF() * 0.45 + effectiveBg.blueF() * 0.55
            );
            pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0), 1.0));
            borderColor = effectiveBg;
            borderColor.setAlpha(180);
            badgeBg = effectiveBg;
        } else { // 2 = Dark with badge (legacy / mixed)
            pillBg = QColor(22, 25, 29);
            pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0), 1.0));
            borderColor = effectiveBg;
            borderColor.setAlpha(160);
            badgeBg = effectiveBg;
        }
    } else {
        pillBg = QColor(22, 25, 29);
        pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0), 1.0));
        borderColor = effectiveBg;
        borderColor.setAlpha(140);
        badgeBg = effectiveBg;
    }

    p.fillPath(bgPath, pillBg);
    p.setPen(QPen(borderColor, 1.5 * scale));
    p.drawPath(bgPath);

    const QColor pillAutoText = contrastingTextColor(pillBg);
    const bool isLightPill = (pillAutoText == Qt::black) || (perceivedLuminance(pillBg) > 0.53);

    const QColor badgeAutoText = contrastingTextColor(badgeBg);
    const bool isLightBadge = (badgeAutoText == Qt::black) || (perceivedLuminance(badgeBg) > 0.53);

    const QRectF badgeRect(padX, (pillH - badgeDim) / 2.0, badgeDim, badgeDim);
    const qreal innerRadius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, cfg.cornerRadius * scale);
    QPainterPath bPath;
    bPath.addRoundedRect(badgeRect, innerRadius, innerRadius);
    p.fillPath(bPath, badgeBg);
    const QColor innerBorder = isLightBadge
        ? QColor(0, 0, 0, 160)
        : QColor(255, 255, 255, 200);
    p.setPen(QPen(innerBorder, 1.0 * scale));
    p.drawPath(bPath);

    const QString letterFam = cfg.letterFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.letterFontFamily.trimmed();
    QFont letterFont(letterFam);
    const int lPx = (cfg.letterFontSize > 0) ? qRound(cfg.letterFontSize * scale) : qRound(badgeDim * 0.62);
    letterFont.setPixelSize(lPx);
    letterFont.setBold(true);
    p.setFont(letterFont);
    const QColor effLetterCol = (cfg.letterColorAuto || !cfg.letterColor.isValid()) ? badgeAutoText : cfg.letterColor;
    p.setPen(effLetterCol);
    p.drawText(badgeRect, Qt::AlignCenter, QString(letter.toUpper()));

    int curX = padX + badgeDim;
    if (hasIcon) {
        curX += gapX;
        const QRect iconRect(curX, qRound((pillH - iconDim) / 2.0), iconDim, iconDim);
        icon.paint(&p, iconRect);
        curX += iconDim;
    }
    if (hasThumb) {
        curX += gapX;
        const QRectF thumbRect(curX, (pillH - thumbH) / 2.0, thumbW, thumbH);
        const qreal thumbRadius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, 3.0 * scale);
        QPainterPath tPath;
        tPath.addRoundedRect(thumbRect, thumbRadius, thumbRadius);
        p.save();
        p.setClipPath(tPath);
        p.drawImage(thumbRect, thumbnail);
        p.restore();
        p.setPen(QPen(isLightPill ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 80), 1.0 * scale));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(thumbRect, thumbRadius, thumbRadius);
        curX += thumbW;
    }

    QColor line1Color;
    QColor line2Color;

    if (hasCustomLabel) {
        line1Color = (cfg.labelColorAuto || !cfg.labelUnmarkedColor.isValid()) ? pillAutoText : cfg.labelUnmarkedColor;
        line2Color = (cfg.locationColorAuto || !cfg.locationColor.isValid()) ? (isLightPill ? QColor(40, 40, 40) : QColor(241, 196, 15)) : cfg.locationColor;
    } else {
        line1Color = (cfg.locationColorAuto || !cfg.locationColor.isValid()) ? (isLightPill ? QColor(20, 20, 20) : QColor(241, 196, 15)) : cfg.locationColor;
        line2Color = (cfg.captionColorAuto || !cfg.captionColor.isValid()) ? (isLightPill ? QColor(50, 50, 50) : QColor(220, 225, 230)) : cfg.captionColor;
    }

    const int textLeft = curX + gapX;
    if (line2Text.isEmpty()) {
        const QRectF locRect(textLeft, 0, textWidth, pillH);
        p.setFont(line1Font);
        p.setPen(line1Color);
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

inline QImage renderOffscreenTargetPill(const QChar &letter,
                                        const QColor &bgColor,
                                        const QColor &textColor,
                                        const QString &locationText,
                                        const QString &caption,
                                        int badgeSize,
                                        qreal scale,
                                        const QString &customLabel = QString(),
                                        bool showIcon = false, const QIcon &icon = QIcon(),
                                        bool showThumbnail = false, const QImage &thumbnail = QImage(),
                                        bool isMarked = false)
{
    BadgeConfig cfg;
    cfg.hudBadgeSize = badgeSize;
    cfg.showIcon = showIcon;
    cfg.showThumbnail = showThumbnail;
    return renderOffscreenTargetPill(cfg, letter, bgColor, textColor, locationText, caption, badgeSize, scale, customLabel, icon, thumbnail, isMarked);
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
