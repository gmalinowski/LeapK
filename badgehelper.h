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
#include <QLocale>
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
    DarkWithBadge = 0,
    FullMarkColor = 1,
    TintedMarkColor = 2
};

struct BadgeConfig {
    // Window Badges Sizing & Features
    int windowBadgeSize = 28;
    int jumpBadgeSize = 34;
    int windowIconSize = 20;
    int windowThumbnailSize = 56;
    int windowThumbnailWidth = 56;
    int windowThumbnailHeight = 32;
    int margin = 0;
    bool windowBadgesEnabled = true;
    bool windowShowIcon = false;
    bool windowShowThumbnail = false;
    int windowLetterFontSize = 0; // 0 = auto
    int windowLabelFontSize = 0; // 0 = auto

    // HUD Badges Sizing & Features
    int hudBadgeSize = 36;
    int hudIconSize = 24;
    int hudThumbnailSize = 80;
    int hudThumbnailWidth = 80;
    int hudThumbnailHeight = 48;
    bool hudShowIcon = true;
    bool hudShowThumbnail = true;
    int hudLetterFontSize = 0; // 0 = auto
    int hudCaptionFontSize = 0; // 0 = auto
    int hudLocationFontSize = 0; // 0 = auto
    bool hudIncludeAll = false;
    bool hudGroupWindows = true;
    qreal hudGroupBorderWidth = 2.0;
    int hudGroupFontSize = 0; // 0 = auto
    int hudGroupCornerRadius = 8;
    bool hudGroupShaderEnabled = true;
    int badgeTimeout = 5;
    QString jumpRecentKey = QStringLiteral("Space");
    bool livePreviews = false;

    // Appearance
    QColor defaultBgColor = QColor(35, 38, 41, 235);
    int cornerRadius = 6;
    int opacityPercent = 92;
    int hudPosition = 0; // 0 = Bottom, 1 = Top, 2 = Left, 3 = Right
    int hudMarkColorMode = 0; // 0 = Dark card with mark border & badge, 1 = Full mark color, 2 = Tinted mark color

    // Features (Legacy / Fallback)
    bool showIcon = false;
    bool showThumbnail = false;
    int thumbnailSize = 56;

    // Typography (Global / Fallback)
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

    // Legacy Sizing Aliases
    int iconSize = 20;
    int thumbnailWidth = 56;
    int thumbnailHeight = 32;
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

inline qreal contrastRatio(const QColor &c1, const QColor &c2)
{
    const qreal l1 = perceivedLuminance(c1);
    const qreal l2 = perceivedLuminance(c2);
    const qreal lighter = qMax(l1, l2);
    const qreal darker = qMin(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
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

inline bool isColorTooClose(const QColor &c1, const QColor &c2, qreal minHueDelta = 35.0, qreal minRgbDistance = 65.0)
{
    if (!c1.isValid() || !c2.isValid()) {
        return false;
    }
    const qreal dr = c1.red() - c2.red();
    const qreal dg = c1.green() - c2.green();
    const qreal db = c1.blue() - c2.blue();
    const qreal rgbDist = std::sqrt(dr * dr + dg * dg + db * db);
    if (rgbDist < minRgbDistance) {
        return true;
    }
    if (c1.saturationF() > 0.15 && c2.saturationF() > 0.15) {
        int h1 = c1.hue();
        int h2 = c2.hue();
        if (h1 >= 0 && h2 >= 0) {
            int diff = std::abs(h1 - h2);
            if (diff > 180) diff = 360 - diff;
            if (diff < minHueDelta) {
                return true;
            }
        }
    }
    return false;
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
        bool collision = false;
        for (const QColor &used : usedColors) {
            if (isColorTooClose(col, used)) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            available.append(col);
        }
    }

    if (!available.isEmpty()) {
        const int idx = QRandomGenerator::global()->bounded(available.size());
        return available.at(idx);
    }

    for (int attempt = 0; attempt < 16; ++attempt) {
        const int hue = QRandomGenerator::global()->bounded(360);
        const int sat = 210 + QRandomGenerator::global()->bounded(45);
        const int val = 220 + QRandomGenerator::global()->bounded(35);
        const QColor cand = QColor::fromHsv(hue, sat, val);
        bool collision = false;
        for (const QColor &used : usedColors) {
            if (isColorTooClose(cand, used)) {
                collision = true;
                break;
            }
        }
        if (!collision) {
            return cand;
        }
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
                               int thumbHeight = 0,
                               const QString &recentKey = QString(),
                               int letterFontSize = 0,
                               const QString &letterFontFamily = QString())
{
    const qreal baseW = qRound(baseHeight * 1.35);
    qreal totalW = baseW;

    if (!recentKey.trimmed().isEmpty()) {
        const QString rKey = recentKey.trimmed();
        const QString fam = letterFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : letterFontFamily.trimmed();
        QFont rFont(fam);
        rFont.setBold(true);
        const int lPx = (letterFontSize > 0) ? letterFontSize : qMax(8, qRound(baseHeight * 0.52));
        rFont.setPixelSize(qMax(8, qRound(lPx * 0.82)));
        QFontMetrics fm(rFont);
        totalW += fm.horizontalAdvance(rKey) + 12.0;
    }

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

inline qreal computeBadgeWidth(const BadgeConfig &cfg, qreal baseHeight, const QString &customLabel = QString(), const QString &recentKey = QString())
{
    const bool showI = cfg.windowShowIcon || cfg.showIcon;
    const bool showT = cfg.windowShowThumbnail || cfg.showThumbnail;
    const int iconSz = (cfg.windowIconSize > 0) ? cfg.windowIconSize : cfg.iconSize;
    const int thumbW = (cfg.windowThumbnailWidth > 0) ? cfg.windowThumbnailWidth : cfg.thumbnailWidth;
    const int thumbH = (cfg.windowThumbnailHeight > 0) ? cfg.windowThumbnailHeight : cfg.thumbnailHeight;
    const int labelSz = (cfg.windowLabelFontSize > 0) ? cfg.windowLabelFontSize : cfg.labelFontSize;
    const int letterSz = (cfg.windowLetterFontSize > 0) ? cfg.windowLetterFontSize : cfg.letterFontSize;
    return computeBadgeWidth(baseHeight, customLabel, showI, showT,
                             1.6, iconSz, thumbW, labelSz, cfg.labelFontFamily,
                             thumbH, recentKey, letterSz, cfg.letterFontFamily);
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
                               const QImage &thumbnail = QImage(),
                               const QString &recentKey = QString(),
                               bool isRecent = false,
                               bool isStashed = false)
{
    Q_UNUSED(isRecent);
    const int effectiveBaseHeight = (baseSize > 0)
        ? baseSize
        : (isJumpMode ? cfg.jumpBadgeSize : cfg.windowBadgeSize);

    const QString cleanRecentKey = recentKey.trimmed();
    const bool hasRecentKey = !cleanRecentKey.isEmpty();

    const QString cleanLabel = customLabel.trimmed();
    const bool hasLabel = !cleanLabel.isEmpty();

    const bool hasIcon = (cfg.windowShowIcon || cfg.showIcon) && !icon.isNull();
    const int effectiveIconSize = (cfg.windowIconSize > 0) ? cfg.windowIconSize : cfg.iconSize;
    const int maxIconDim = qMax(12, effectiveBaseHeight - 6);
    const int iconDim = hasIcon
        ? ((effectiveIconSize > 0) ? qMin(effectiveIconSize, maxIconDim) : qRound(effectiveBaseHeight * 0.70))
        : 0;
    const int iconSectionW = hasIcon ? (iconDim + 10) : 0;

    const bool hasThumb = (cfg.windowShowThumbnail || cfg.showThumbnail) && !thumbnail.isNull();
    const int effWinThumbSize = (cfg.windowThumbnailSize > 0) ? cfg.windowThumbnailSize : cfg.thumbnailSize;
    const int maxThumbW = (cfg.windowThumbnailWidth > 0) ? cfg.windowThumbnailWidth : ((effWinThumbSize > 0) ? effWinThumbSize : cfg.thumbnailWidth);
    const int maxThumbH = (cfg.windowThumbnailHeight > 0) ? cfg.windowThumbnailHeight : ((effWinThumbSize > 0) ? effWinThumbSize : cfg.thumbnailHeight);
    const int maxPillThumbH = qMax(12, effectiveBaseHeight - 6);
    int thumbW = 0;
    int thumbH = 0;
    if (hasThumb) {
        if (thumbnail.width() > 0 && thumbnail.height() > 0) {
            const int targetMaxH = (maxThumbH > 0) ? qMin(maxThumbH, maxPillThumbH) : maxPillThumbH;
            const QSize maxBox(maxThumbW, targetMaxH);
            const QSize fitSize = thumbnail.size().scaled(maxBox, Qt::KeepAspectRatio);
            thumbW = qMax(10, fitSize.width());
            thumbH = qMax(10, fitSize.height());
        } else {
            thumbH = (maxThumbH > 0) ? qMin(maxThumbH, maxPillThumbH) : maxPillThumbH;
            thumbW = (maxThumbW > 0) ? maxThumbW : qRound(thumbH * 1.6);
        }
    }
    const int thumbSectionW = hasThumb ? (thumbW + 10) : 0;

    const QString labelFam = cfg.labelFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.labelFontFamily.trimmed();
    QFont labelFont(labelFam);
    const int effectiveLabelFontSize = (cfg.windowLabelFontSize > 0) ? cfg.windowLabelFontSize : cfg.labelFontSize;
    const int labelPx = (effectiveLabelFontSize > 0) ? qRound(effectiveLabelFontSize * scale) : qMax(8, qRound(effectiveBaseHeight * 0.44 * scale));
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

    const QString letterFam = cfg.letterFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.letterFontFamily.trimmed();
    QFont font(letterFam);
    font.setBold(true);
    const int effectiveLetterFontSize = (cfg.windowLetterFontSize > 0) ? cfg.windowLetterFontSize : cfg.letterFontSize;
    const int letterPx = (effectiveLetterFontSize > 0) ? qRound(effectiveLetterFontSize * scale) : qMax(8, qRound(effectiveBaseHeight * 0.52 * scale));
    font.setPixelSize(letterPx);

    QFont rFont(letterFam);
    int rTextW = 0;
    int recentSectionW = 0;
    if (hasRecentKey) {
        rFont.setBold(true);
        rFont.setPixelSize(qMax(8, qRound(letterPx * 0.82)));
        QFontMetrics fmR(rFont);
        rTextW = fmR.horizontalAdvance(cleanRecentKey);
        recentSectionW = qRound((rTextW + 12.0 * scale) / scale);
    }

    const int effectiveLetterWidth = qRound(effectiveBaseHeight * 1.35);
    const int effectiveBaseWidth = effectiveLetterWidth + recentSectionW + iconSectionW + thumbSectionW + labelSectionW;

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
    if (isStashed) {
        int h, s, v, a;
        effectiveBg.getHsv(&h, &s, &v, &a);
        effectiveBg = QColor::fromHsv(h, qRound(s * 0.35), qRound(v * 0.85), a);
        effectiveBg.setAlphaF(qBound(0.1, effectiveBg.alphaF() * (cfg.opacityPercent / 100.0) * 0.82, 1.0));
    } else {
        effectiveBg.setAlphaF(qBound(0.1, effectiveBg.alphaF() * (cfg.opacityPercent / 100.0), 1.0));
    }

    // Contrasting text color & border
    const QColor autoTextColor = contrastingTextColor(effectiveBg);
    const QColor effectiveTextColor = textColor.isValid() ? textColor : autoTextColor;
    const bool isLightBg = (effectiveTextColor == Qt::black) || (perceivedLuminance(effectiveBg) > 0.53);

    // Background pill
    p.setBrush(effectiveBg);
    QColor borderColor;
    if (isLightBg) {
        borderColor = isJumpMode ? QColor(0, 0, 0, 180) : QColor(0, 0, 0, 120);
    } else {
        if (!cfg.letterColorAuto && cfg.letterColor.isValid() && contrastRatio(effectiveBg, cfg.letterColor) >= 2.5) {
            borderColor = QColor(cfg.letterColor.red(), cfg.letterColor.green(), cfg.letterColor.blue(), isJumpMode ? 220 : 160);
        } else {
            borderColor = isJumpMode ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 170);
        }
    }
    QPen borderPen(borderColor, isJumpMode ? (2.0 * scale) : (1.2 * scale));
    if (isStashed) {
        borderPen.setStyle(Qt::CustomDashLine);
        borderPen.setDashPattern({3, 2});
    }
    p.setPen(borderPen);
    p.drawRoundedRect(rect, radius, radius);

    const QColor sepCol = isLightBg ? QColor(0, 0, 0, 70) : QColor(255, 255, 255, 90);

    p.setFont(font);
    QColor effLetterColor = autoTextColor;
    if (!cfg.letterColorAuto && cfg.letterColor.isValid()) {
        if (contrastRatio(effectiveBg, cfg.letterColor) >= 2.5) {
            effLetterColor = cfg.letterColor;
        } else {
            // Background color does not provide sufficient contrast for the configured letter color
            // (e.g. amber letters on an amber recent/marked badge, or white on light badge).
            // Fall back to auto-contrast text color to prevent unreadable badges.
            effLetterColor = autoTextColor;
        }
    }
    p.setPen(effLetterColor);

    const bool hasExtra = hasIcon || hasThumb || hasLabel || hasRecentKey;
    if (!hasExtra) {
        p.drawText(rect, Qt::AlignCenter, QString(letter.toUpper()));
    } else {
        qreal currentX = margin;

        // 1. Letter & optional recent key
        const qreal letterW = qRound(effectiveLetterWidth * scale);
        const QRectF letterRect(currentX, margin, letterW - margin, h - 2 * margin);
        p.drawText(letterRect, Qt::AlignCenter, QString(letter.toUpper()));
        currentX += letterW;

        if (hasRecentKey) {
            const qreal sepRecentX = currentX - 1.0 * scale;
            p.setPen(QPen(sepCol, 1.2 * scale));
            p.drawLine(QPointF(sepRecentX, margin + 4.0 * scale), QPointF(sepRecentX, h - margin - 4.0 * scale));

            const qreal recentAreaW = recentSectionW * scale;
            const QRectF recentRect(sepRecentX, margin, recentAreaW, h - 2 * margin);
            p.setFont(rFont);
            p.setPen(effLetterColor);
            p.drawText(recentRect, Qt::AlignCenter, cleanRecentKey);

            p.setFont(font);
            currentX += recentAreaW;
        }

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
            if (isStashed) {
                p.setOpacity(0.65);
            }
            p.drawImage(thumbRect, thumbnail);
            p.restore();

            QPen tPen(isLightBg ? QColor(0, 0, 0, 70) : QColor(255, 255, 255, 90), 1.0 * scale);
            if (isStashed) {
                tPen.setStyle(Qt::CustomDashLine);
                tPen.setDashPattern({3, 2});
            }
            p.setPen(tPen);
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
            QColor effLabelColor = autoTextColor;
            if (!cfg.labelColorAuto && cfg.labelUnmarkedColor.isValid()) {
                if (contrastRatio(effectiveBg, cfg.labelUnmarkedColor) >= 2.5) {
                    effLabelColor = cfg.labelUnmarkedColor;
                }
            } else if (textColor.isValid()) {
                effLabelColor = textColor;
            }
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
                               bool showThumbnail = false, const QImage &thumbnail = QImage(),
                               bool isStashed = false)
{
    BadgeConfig cfg;
    cfg.windowBadgeSize = baseSize;
    cfg.jumpBadgeSize = qRound(baseSize * 1.25);
    cfg.showIcon = showIcon;
    cfg.showThumbnail = showThumbnail;
    const int effectiveSize = isJumpMode ? cfg.jumpBadgeSize : baseSize;
    return renderBadgeImage(cfg, letter, bgColor, textColor, isJumpMode, effectiveSize, scale, customLabel, icon, thumbnail, QString(), false, isStashed);
}

inline QSizeF computeHudPillSize(const BadgeConfig &cfg,
                                 const QString &locationText,
                                 const QString &caption,
                                 int badgeSize = 0,
                                 qreal scale = 1.0,
                                 const QString &customLabel = QString(),
                                 bool hasIcon = false,
                                 bool hasThumb = false,
                                 const QSize &thumbSize = QSize(),
                                 const QString &recentKey = QString(),
                                 bool isRecent = false,
                                 int groupCount = 0,
                                 bool isStashed = false)
{
    Q_UNUSED(isStashed);
    const int effectivePillBaseH = (badgeSize > 0) ? badgeSize : cfg.hudBadgeSize;

    const int effectiveIconSize = (cfg.hudIconSize > 0) ? cfg.hudIconSize : 24;
    const int effectiveThumbSize = (cfg.hudThumbnailSize > 0) ? cfg.hudThumbnailSize : 80;
    const int effectiveThumbW = (cfg.hudThumbnailWidth > 0) ? cfg.hudThumbnailWidth : effectiveThumbSize;
    const int effectiveThumbH = (cfg.hudThumbnailHeight > 0) ? cfg.hudThumbnailHeight : effectiveThumbSize;
    const bool isTopOrBottom = (cfg.hudPosition == 0 || cfg.hudPosition == 1);

    const int padX = qRound(8 * scale);
    const int padY = qRound(6 * scale);
    const int gapX = qRound(8 * scale);
    const int gapY = qRound(6 * scale);

    int pillW = 0;
    int pillH = 0;
    int badgeDim = 0;
    int badgeW = 0;
    int iconDim = 0;
    int thumbW = 0;
    int thumbH = 0;
    int headerH = 0;

    const QString cleanRecentKey = recentKey.trimmed();
    const bool hasRecentKey = !cleanRecentKey.isEmpty();

    const QString locFam = cfg.locationFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.locationFontFamily.trimmed();
    QFont locFont(locFam);
    const int effectiveLocFontSize = (cfg.hudLocationFontSize > 0) ? cfg.hudLocationFontSize : cfg.locationFontSize;
    const int locPx = (effectiveLocFontSize > 0) ? qRound(effectiveLocFontSize * scale) : qMax(10, qRound(11 * scale));
    locFont.setPixelSize(locPx);
    locFont.setBold(true);

    const QString capFam = cfg.captionFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.captionFontFamily.trimmed();
    QFont capFont(capFam);
    const int effectiveCapFontSize = (cfg.hudCaptionFontSize > 0) ? cfg.hudCaptionFontSize : cfg.captionFontSize;
    const int capPx = (effectiveCapFontSize > 0) ? qRound(effectiveCapFontSize * scale) : qMax(9, qRound(10 * scale));
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
        if (groupCount > 1) {
            line1Text += QStringLiteral(" (%1)").arg(groupCount);
        }
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
        if (isRecent) {
            line1Text += QStringLiteral(" · Recent");
        }
        line2Text = displayCap;
        if (groupCount > 1 && !line2Text.isEmpty()) {
            line2Text += QStringLiteral(" (%1)").arg(groupCount);
        } else if (groupCount > 1) {
            line1Text += QStringLiteral(" (%1)").arg(groupCount);
        }

        const int locWidth = fmLoc.horizontalAdvance(line1Text);
        const int capWidth = line2Text.isEmpty() ? 0 : fmCap.horizontalAdvance(line2Text);
        textWidth = qMax(locWidth, capWidth);
    }

    const QString letterFam = cfg.letterFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.letterFontFamily.trimmed();
    QFont letterFont(letterFam);
    const int effectiveLetterFontSize = (cfg.hudLetterFontSize > 0) ? cfg.hudLetterFontSize : cfg.letterFontSize;
    const int lPx = (effectiveLetterFontSize > 0) ? qRound(effectiveLetterFontSize * scale) : qMax(8, qRound(effectivePillBaseH * 0.62 * scale));
    letterFont.setPixelSize(lPx);
    letterFont.setBold(true);

    QFont rFont(letterFam);
    rFont.setBold(true);
    rFont.setPixelSize(qMax(8, qRound(lPx * 0.78)));
    QFontMetrics fmR(rFont);
    const int rKeyWidth = hasRecentKey ? (fmR.horizontalAdvance(cleanRecentKey) + qRound(10 * scale)) : 0;

    if (isTopOrBottom && hasThumb) {
        headerH = qMax(22, qRound(effectivePillBaseH * 1.05 * scale));
        badgeDim = qMax(16, headerH - qRound(4 * scale));
        badgeW = badgeDim + rKeyWidth;
        iconDim = hasIcon
            ? ((effectiveIconSize > 0) ? qMin(qRound(effectiveIconSize * scale), badgeDim) : qRound(badgeDim * 0.85))
            : 0;

        if (thumbSize.width() > 0 && thumbSize.height() > 0) {
            const int targetMaxW = (effectiveThumbW > 0) ? qRound(effectiveThumbW * scale) : qRound(80 * scale);
            const int targetMaxH = (effectiveThumbH > 0) ? qRound(effectiveThumbH * scale) : qMax(32, qRound(48 * scale));
            const QSize maxBox(targetMaxW, targetMaxH);
            const QSize fitSize = thumbSize.scaled(maxBox, Qt::KeepAspectRatio);
            thumbW = qMax(qRound(16 * scale), fitSize.width());
            thumbH = qMax(qRound(12 * scale), fitSize.height());
        } else {
            thumbH = (effectiveThumbH > 0) ? qRound(effectiveThumbH * scale) : qMax(32, qRound(48 * scale));
            thumbW = (effectiveThumbW > 0) ? qRound(effectiveThumbW * scale) : qRound(thumbH * 1.6);
        }

        const int headerContentW = badgeW + (hasIcon ? (gapX + iconDim) : 0) + gapX + textWidth;
        const int contentW = qMax(headerContentW, thumbW);

        pillW = padX + contentW + padX;
        pillH = padY + headerH + gapY + thumbH + padY;
    } else {
        pillH = qMax(24, qRound(effectivePillBaseH * 1.35 * scale));
        badgeDim = qMax(16, pillH - qRound(8 * scale));
        badgeW = badgeDim + rKeyWidth;
        const int maxIconDim = qMax(12, pillH - qRound(8 * scale));
        iconDim = hasIcon
            ? ((effectiveIconSize > 0) ? qMin(qRound(effectiveIconSize * scale), maxIconDim) : qRound(badgeDim * 0.85))
            : 0;

        const int maxThumbH = qMax(14, pillH - qRound(8 * scale));
        if (hasThumb) {
            const int targetMaxW = (effectiveThumbW > 0) ? qRound(effectiveThumbW * scale) : qRound(80 * scale);
            const int targetMaxH = (effectiveThumbH > 0) ? qMin(qRound(effectiveThumbH * scale), maxThumbH) : maxThumbH;
            if (thumbSize.width() > 0 && thumbSize.height() > 0) {
                const QSize maxBox(targetMaxW, targetMaxH);
                const QSize fitSize = thumbSize.scaled(maxBox, Qt::KeepAspectRatio);
                thumbW = qMax(qRound(14 * scale), fitSize.width());
                thumbH = qMax(qRound(14 * scale), fitSize.height());
            } else {
                thumbH = targetMaxH;
                thumbW = (effectiveThumbW > 0) ? qMin(qRound(effectiveThumbW * scale), qRound(thumbH * 1.6)) : qRound(thumbH * 1.6);
            }
        } else {
            thumbW = 0;
            thumbH = 0;
        }

        int extraW = 0;
        if (hasIcon) {
            extraW += iconDim + gapX;
        }
        if (hasThumb) {
            extraW += thumbW + gapX;
        }

        pillW = padX + badgeW + extraW + gapX + textWidth + padX;
    }

    return QSizeF(static_cast<qreal>(pillW) / scale, static_cast<qreal>(pillH) / scale);
}

inline QSizeF computeHudPillSize(const QString &locationText,
                                 const QString &caption,
                                 int badgeSize = 28,
                                 qreal scale = 1.0,
                                 const QString &customLabel = QString(),
                                 bool hasIcon = false,
                                 bool hasThumb = false,
                                 const QSize &thumbSize = QSize(),
                                 const QString &recentKey = QString(),
                                 bool isRecent = false,
                                 int groupCount = 0,
                                 bool isStashed = false)
{
    BadgeConfig cfg;
    cfg.hudBadgeSize = badgeSize;
    cfg.hudShowIcon = hasIcon;
    cfg.hudShowThumbnail = hasThumb;
    return computeHudPillSize(cfg, locationText, caption, badgeSize, scale, customLabel, hasIcon, hasThumb, thumbSize, recentKey, isRecent, groupCount, isStashed);
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
                                        bool isMarked = false,
                                        const QString &recentKey = QString(),
                                        bool isRecent = false,
                                        int groupCount = 0,
                                        bool isStashed = false)
{
    Q_UNUSED(textColor);
    const int effectivePillBaseH = (badgeSize > 0) ? badgeSize : cfg.hudBadgeSize;

    const bool hasIcon = cfg.hudShowIcon && !icon.isNull();
    const bool hasThumb = cfg.hudShowThumbnail && !thumbnail.isNull();
    const int effectiveIconSize = (cfg.hudIconSize > 0) ? cfg.hudIconSize : 24;
    const int effectiveThumbSize = (cfg.hudThumbnailSize > 0) ? cfg.hudThumbnailSize : 80;
    const int effectiveThumbW = (cfg.hudThumbnailWidth > 0) ? cfg.hudThumbnailWidth : effectiveThumbSize;
    const int effectiveThumbH = (cfg.hudThumbnailHeight > 0) ? cfg.hudThumbnailHeight : effectiveThumbSize;
    const bool isTopOrBottom = (cfg.hudPosition == 0 || cfg.hudPosition == 1);

    const int padX = qRound(8 * scale);
    const int padY = qRound(6 * scale);
    const int gapX = qRound(8 * scale);
    const int gapY = qRound(6 * scale);

    int pillW = 0;
    int pillH = 0;
    int badgeDim = 0;
    int badgeW = 0;
    int iconDim = 0;
    int thumbW = 0;
    int thumbH = 0;
    int headerH = 0;

    const QString cleanRecentKey = recentKey.trimmed();
    const bool hasRecentKey = !cleanRecentKey.isEmpty();

    const QString locFam = cfg.locationFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.locationFontFamily.trimmed();
    QFont locFont(locFam);
    const int effectiveLocFontSize = (cfg.hudLocationFontSize > 0) ? cfg.hudLocationFontSize : cfg.locationFontSize;
    const int locPx = (effectiveLocFontSize > 0) ? qRound(effectiveLocFontSize * scale) : qMax(10, qRound(11 * scale));
    locFont.setPixelSize(locPx);
    locFont.setBold(true);

    const QString capFam = cfg.captionFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.captionFontFamily.trimmed();
    QFont capFont(capFam);
    const int effectiveCapFontSize = (cfg.hudCaptionFontSize > 0) ? cfg.hudCaptionFontSize : cfg.captionFontSize;
    const int capPx = (effectiveCapFontSize > 0) ? qRound(effectiveCapFontSize * scale) : qMax(9, qRound(10 * scale));
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
        if (groupCount > 1) {
            line1Text += QStringLiteral(" (%1)").arg(groupCount);
        }
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
        if (isRecent) {
            line1Text += QStringLiteral(" · Recent");
        }
        line2Text = displayCap;
        if (groupCount > 1 && !line2Text.isEmpty()) {
            line2Text += QStringLiteral(" (%1)").arg(groupCount);
        } else if (groupCount > 1) {
            line1Text += QStringLiteral(" (%1)").arg(groupCount);
        }

        const int locWidth = fmLoc.horizontalAdvance(line1Text);
        const int capWidth = line2Text.isEmpty() ? 0 : fmCap.horizontalAdvance(line2Text);
        textWidth = qMax(locWidth, capWidth);
    }

    const QString letterFam = cfg.letterFontFamily.trimmed().isEmpty() ? QStringLiteral("sans-serif") : cfg.letterFontFamily.trimmed();
    QFont letterFont(letterFam);
    const int effectiveLetterFontSize = (cfg.hudLetterFontSize > 0) ? cfg.hudLetterFontSize : cfg.letterFontSize;
    const int lPx = (effectiveLetterFontSize > 0) ? qRound(effectiveLetterFontSize * scale) : qMax(8, qRound(effectivePillBaseH * 0.62 * scale));
    letterFont.setPixelSize(lPx);
    letterFont.setBold(true);

    QFont rFont(letterFam);
    rFont.setBold(true);
    rFont.setPixelSize(qMax(8, qRound(lPx * 0.78)));
    QFontMetrics fmR(rFont);
    const int rKeyWidth = hasRecentKey ? (fmR.horizontalAdvance(cleanRecentKey) + qRound(10 * scale)) : 0;

    if (isTopOrBottom && hasThumb) {
        // Top/Bottom with thumbnail: 2-row card with [badge] [icon] [text] on row 1, and thumbnail UNDERNEATH
        headerH = qMax(22, qRound(effectivePillBaseH * 1.05 * scale));
        badgeDim = qMax(16, headerH - qRound(4 * scale));
        badgeW = badgeDim + rKeyWidth;
        iconDim = hasIcon
            ? ((effectiveIconSize > 0) ? qMin(qRound(effectiveIconSize * scale), badgeDim) : qRound(badgeDim * 0.85))
            : 0;

        if (thumbnail.width() > 0 && thumbnail.height() > 0) {
            const int targetMaxW = (effectiveThumbW > 0) ? qRound(effectiveThumbW * scale) : qRound(80 * scale);
            const int targetMaxH = (effectiveThumbH > 0) ? qRound(effectiveThumbH * scale) : qMax(32, qRound(48 * scale));
            const QSize maxBox(targetMaxW, targetMaxH);
            const QSize fitSize = thumbnail.size().scaled(maxBox, Qt::KeepAspectRatio);
            thumbW = qMax(qRound(16 * scale), fitSize.width());
            thumbH = qMax(qRound(12 * scale), fitSize.height());
        } else {
            thumbH = (effectiveThumbH > 0) ? qRound(effectiveThumbH * scale) : qMax(32, qRound(48 * scale));
            thumbW = (effectiveThumbW > 0) ? qRound(effectiveThumbW * scale) : qRound(thumbH * 1.6);
        }

        const int headerContentW = badgeW + (hasIcon ? (gapX + iconDim) : 0) + gapX + textWidth;
        const int contentW = qMax(headerContentW, thumbW);

        pillW = padX + contentW + padX;
        pillH = padY + headerH + gapY + thumbH + padY;
    } else {
        // Horizontal single-row pill (Left/Right, or Top/Bottom without thumbnail)
        pillH = qMax(24, qRound(effectivePillBaseH * 1.35 * scale));
        badgeDim = qMax(16, pillH - qRound(8 * scale));
        badgeW = badgeDim + rKeyWidth;
        const int maxIconDim = qMax(12, pillH - qRound(8 * scale));
        iconDim = hasIcon
            ? ((effectiveIconSize > 0) ? qMin(qRound(effectiveIconSize * scale), maxIconDim) : qRound(badgeDim * 0.85))
            : 0;

        const int maxThumbH = qMax(14, pillH - qRound(8 * scale));
        if (hasThumb) {
            const int targetMaxW = (effectiveThumbW > 0) ? qRound(effectiveThumbW * scale) : qRound(80 * scale);
            const int targetMaxH = (effectiveThumbH > 0) ? qMin(qRound(effectiveThumbH * scale), maxThumbH) : maxThumbH;
            if (thumbnail.width() > 0 && thumbnail.height() > 0) {
                const QSize maxBox(targetMaxW, targetMaxH);
                const QSize fitSize = thumbnail.size().scaled(maxBox, Qt::KeepAspectRatio);
                thumbW = qMax(qRound(14 * scale), fitSize.width());
                thumbH = qMax(qRound(14 * scale), fitSize.height());
            } else {
                thumbH = targetMaxH;
                thumbW = (effectiveThumbW > 0) ? qMin(qRound(effectiveThumbW * scale), qRound(thumbH * 1.6)) : qRound(thumbH * 1.6);
            }
        } else {
            thumbW = 0;
            thumbH = 0;
        }

        int extraW = 0;
        if (hasIcon) {
            extraW += iconDim + gapX;
        }
        if (hasThumb) {
            extraW += thumbW + gapX;
        }

        pillW = padX + badgeW + extraW + gapX + textWidth + padX;
    }

    QImage img(pillW, pillH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
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

    const bool effectivelyMarked = isMarked || isRecent || (bgColor.isValid() && bgColor != cfg.defaultBgColor && bgColor != QColor(35, 38, 41, 235));
    const QColor rawMarkColor = bgColor.isValid() ? bgColor : cfg.defaultBgColor;
    QColor pureMarkColor = rawMarkColor;
    pureMarkColor.setAlpha(255); // Solid, vibrant, undimmed 100% mark color

    if (isStashed) {
        int h, s, v, a;
        pureMarkColor.getHsv(&h, &s, &v, &a);
        // Decrease saturation significantly to 35% and value to 82% for an archived/shelved feel
        pureMarkColor = QColor::fromHsv(h, qRound(s * 0.35), qRound(v * 0.82), a);
    }

    QColor pillBg(22, 25, 29);
    pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0) * (isStashed ? 0.78 : 1.0), 1.0));

    QColor borderColor;
    QColor badgeBg;

    if (effectivelyMarked) {
        if (cfg.hudMarkColorMode == 1) { // 1 = Full mark color
            pillBg = pureMarkColor;
            pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0) * (isStashed ? 0.78 : 1.0), 1.0));
            const bool isLight = (perceivedLuminance(pillBg) > 0.53);
            borderColor = isLight ? QColor(0, 0, 0, 160) : QColor(255, 255, 255, 180);
            badgeBg = pureMarkColor;
        } else if (cfg.hudMarkColorMode == 2) { // 2 = Tinted mark color
            const QColor darkBase(22, 25, 29);
            pillBg = QColor::fromRgbF(
                darkBase.redF() * 0.45 + pureMarkColor.redF() * 0.55,
                darkBase.greenF() * 0.45 + pureMarkColor.greenF() * 0.55,
                darkBase.blueF() * 0.45 + pureMarkColor.blueF() * 0.55
            );
            pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0) * (isStashed ? 0.78 : 1.0), 1.0));
            borderColor = pureMarkColor;
            borderColor.setAlpha(isStashed ? 180 : 220);
            badgeBg = pureMarkColor;
        } else { // 0 = Dark card with mark border & badge (Recommended default)
            pillBg = QColor(22, 25, 29);
            pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0) * (isStashed ? 0.78 : 1.0), 1.0));
            borderColor = pureMarkColor;
            borderColor.setAlpha(isStashed ? 200 : 245);
            badgeBg = pureMarkColor;
        }
    } else {
        pillBg = QColor(22, 25, 29);
        pillBg.setAlphaF(qBound(0.1, (235.0 / 255.0) * (cfg.opacityPercent / 100.0) * (isStashed ? 0.78 : 1.0), 1.0));
        if (isStashed) {
            badgeBg = QColor(48, 54, 62);
            borderColor = QColor(140, 150, 165, 130);
        } else {
            borderColor = QColor(255, 255, 255, 45);
            badgeBg = cfg.defaultBgColor;
            badgeBg.setAlpha(255);
        }
    }

    p.fillPath(bgPath, pillBg);
    QPen borderPen(borderColor, 1.5 * scale);
    if (isStashed) {
        borderPen.setStyle(Qt::CustomDashLine);
        borderPen.setDashPattern({4, 3});
    }
    p.setPen(borderPen);
    p.drawPath(bgPath);

    const QColor pillAutoText = contrastingTextColor(pillBg);
    const bool isLightPill = (pillAutoText == Qt::black) || (perceivedLuminance(pillBg) > 0.53);

    const QColor badgeAutoText = contrastingTextColor(badgeBg);
    const bool isLightBadge = (badgeAutoText == Qt::black) || (perceivedLuminance(badgeBg) > 0.53);

    const QRectF badgeRect = (isTopOrBottom && hasThumb)
        ? QRectF(padX, padY + (headerH - badgeDim) / 2.0, badgeW, badgeDim)
        : QRectF(padX, (pillH - badgeDim) / 2.0, badgeW, badgeDim);
    const qreal innerRadius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, cfg.cornerRadius * scale);
    QPainterPath bPath;
    bPath.addRoundedRect(badgeRect, innerRadius, innerRadius);
    p.fillPath(bPath, badgeBg);

    if (effectivelyMarked) {
        // Pure undimmed badge: no dark stroke dimming the bright mark color.
        // For dark badges sitting on a dark card, draw a subtle light stroke to preserve edge definition.
        if (!isLightBadge) {
            QPen bPen(isStashed ? QColor(255, 255, 255, 90) : QColor(255, 255, 255, 75), 1.0 * scale);
            if (isStashed) {
                bPen.setStyle(Qt::CustomDashLine);
                bPen.setDashPattern({3, 2});
            }
            p.setPen(bPen);
            p.drawPath(bPath);
        }
    } else {
        QPen bPen(isStashed ? QColor(255, 255, 255, 80) : QColor(255, 255, 255, 45), 1.0 * scale);
        if (isStashed) {
            bPen.setStyle(Qt::CustomDashLine);
            bPen.setDashPattern({3, 2});
        }
        p.setPen(bPen);
        p.drawPath(bPath);
    }

    QColor effLetterCol = badgeAutoText;
    if (!cfg.letterColorAuto && cfg.letterColor.isValid()) {
        if (contrastRatio(badgeBg, cfg.letterColor) >= 2.5) {
            effLetterCol = cfg.letterColor;
        } else {
            effLetterCol = badgeAutoText;
        }
    }
    if (!hasRecentKey) {
        p.setFont(letterFont);
        p.setPen(effLetterCol);
        p.drawText(badgeRect, Qt::AlignCenter, QString(letter.toUpper()));
    } else {
        const QRectF lRect(badgeRect.left(), badgeRect.top(), badgeDim, badgeDim);
        p.setFont(letterFont);
        p.setPen(effLetterCol);
        p.drawText(lRect, Qt::AlignCenter, QString(letter.toUpper()));

        const qreal sepX = badgeRect.left() + badgeDim;
        p.setPen(QPen(isLightBadge ? QColor(0, 0, 0, 75) : QColor(255, 255, 255, 90), 1.0 * scale));
        p.drawLine(QPointF(sepX, badgeRect.top() + 4.0 * scale), QPointF(sepX, badgeRect.bottom() - 4.0 * scale));

        const QRectF rRect(sepX, badgeRect.top(), badgeW - badgeDim, badgeDim);
        p.setFont(rFont);
        p.setPen(effLetterCol);
        p.drawText(rRect, Qt::AlignCenter, cleanRecentKey);
    }

    int textLeft = 0;
    qreal textCenterY = 0;
    qreal textLineHeight = 0;

    if (isTopOrBottom && hasThumb) {
        // Row 1: Header [Badge] [Icon] [Text]
        int curX = padX + badgeW;
        if (hasIcon) {
            curX += gapX;
            const QRect iconRect(curX, qRound(padY + (headerH - iconDim) / 2.0), iconDim, iconDim);
            icon.paint(&p, iconRect);
            curX += iconDim;
        }
        textLeft = curX + gapX;
        textCenterY = padY + headerH / 2.0;
        textLineHeight = headerH;

        // Row 2: Window preview underneath header ("pod badges")
        const int headerContentW = badgeW + (hasIcon ? (gapX + iconDim) : 0) + gapX + textWidth;
        const int contentW = qMax(headerContentW, thumbW);
        const qreal thumbX = padX + (contentW - thumbW) / 2.0;
        const qreal thumbY = padY + headerH + gapY;
        const QRectF thumbRect(thumbX, thumbY, thumbW, thumbH);
        const qreal thumbRadius = (cfg.cornerRadius == 0) ? 0.0 : qMax(1.0, 3.0 * scale);
        QPainterPath tPath;
        tPath.addRoundedRect(thumbRect, thumbRadius, thumbRadius);
        p.save();
        p.setClipPath(tPath);
        if (isStashed) {
            p.setOpacity(0.65);
        }
        p.drawImage(thumbRect, thumbnail);
        p.restore();
        QPen tPen(isLightPill ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 80), 1.0 * scale);
        if (isStashed) {
            tPen.setStyle(Qt::CustomDashLine);
            tPen.setDashPattern({3, 2});
        }
        p.setPen(tPen);
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(thumbRect, thumbRadius, thumbRadius);
    } else {
        // Horizontal single-row pill (Left/Right, or Top/Bottom without thumbnail)
        // Order: [Badge] -> [Icon] -> [Preview] -> [Text]
        int curX = padX + badgeW;
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
            if (isStashed) {
                p.setOpacity(0.65);
            }
            p.drawImage(thumbRect, thumbnail);
            p.restore();
            QPen tPen(isLightPill ? QColor(0, 0, 0, 80) : QColor(255, 255, 255, 80), 1.0 * scale);
            if (isStashed) {
                tPen.setStyle(Qt::CustomDashLine);
                tPen.setDashPattern({3, 2});
            }
            p.setPen(tPen);
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(thumbRect, thumbRadius, thumbRadius);
            curX += thumbW;
        }
        textLeft = curX + gapX;
        textCenterY = pillH / 2.0;
        textLineHeight = pillH;
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

    if (line2Text.isEmpty()) {
        const QRectF locRect(textLeft, (isTopOrBottom && hasThumb) ? padY : 0, textWidth, textLineHeight);
        p.setFont(line1Font);
        p.setPen(line1Color);
        p.drawText(locRect, Qt::AlignVCenter | Qt::AlignLeft, line1Text);
    } else {
        QFontMetrics fm1(line1Font);
        QFontMetrics fm2(line2Font);
        const QRectF r1(textLeft, qRound(textCenterY - fm1.height() * 0.95), textWidth, fm1.height());
        const QRectF r2(textLeft, qRound(textCenterY + fm2.height() * 0.05), textWidth, fm2.height());

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
                                        bool isMarked = false,
                                        const QString &recentKey = QString(),
                                        bool isRecent = false,
                                        int groupCount = 0,
                                        bool isStashed = false)
{
    BadgeConfig cfg;
    cfg.hudBadgeSize = badgeSize;
    cfg.showIcon = showIcon;
    cfg.showThumbnail = showThumbnail;
    return renderOffscreenTargetPill(cfg, letter, bgColor, textColor, locationText, caption, badgeSize, scale, customLabel, icon, thumbnail, isMarked, recentKey, isRecent, groupCount, isStashed);
}

inline QList<QRectF> computeHudPillPositions(const QList<QSizeF> &pillSizes,
                                             const QRectF &screenRect,
                                             int hudPosition = 0,
                                             qreal margin = 24.0,
                                             qreal gap = 12.0,
                                             qreal rowGap = -1.0)
{
    if (pillSizes.isEmpty()) {
        return {};
    }

    QList<QRectF> result;
    result.resize(pillSizes.size());

    const int clampedPos = qBound(0, hudPosition, 3);
    const qreal vGap = (rowGap >= 0.0) ? rowGap : gap;

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
        clusterHeight += (rows.size() - 1) * vGap;

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
                const qreal itemY = rowY; // Top-aligned so letter badges align in a straight horizontal row
                result[idx] = QRectF(currentX, itemY, sz.width(), sz.height());
                currentX += sz.width() + gap;
            }
            rowY += r.maxHeight + vGap;
        }
    } else {
        // Vertical layout with column wrapping: 2 = Left, 3 = Right
        const qreal availHeight = qMax(60.0, screenRect.height() - 2.0 * margin);
        const qreal hGap = (rowGap >= 0.0) ? rowGap : gap;

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
        clusterWidth += (cols.size() - 1) * hGap;

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
            colX += c.maxWidth + hGap;
        }
    }

    return result;
}

inline QImage renderGroupContainerFrame(const QSizeF &size,
                                        const QChar &groupLetter,
                                        const QColor &groupColor,
                                        const QString &groupTitle,
                                        int windowCount,
                                        qreal scale,
                                        int cornerRadius = 8,
                                        qreal borderWidth = 2.0,
                                        int fontSize = 0,
                                        bool tabAtBottom = false)
{
    const int w = qRound(size.width() * scale);
    const int h = qRound(size.height() * scale);
    if (w <= 0 || h <= 0) {
        return QImage();
    }

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal r = cornerRadius * scale;
    const qreal bw = qMax(1.0, borderWidth * scale);

    const int effFontSize = (fontSize > 0) ? qRound(fontSize * scale) : qMax(9, qRound(11 * scale));
    const int badgeH = qMax(qRound(22 * scale), effFontSize + qRound(10 * scale));
    const int tabHalfH = badgeH / 2;

    const QRectF rect(bw / 2.0, tabAtBottom ? (bw / 2.0) : (tabHalfH + bw / 2.0),
                     w - bw, h - tabHalfH - bw);

    // Subtle container background tint
    QColor fillCol = groupColor.isValid() ? groupColor : QColor(52, 152, 219);
    fillCol.setAlpha(26);
    p.setBrush(fillCol);

    // Border in group color
    QColor borderCol = groupColor.isValid() ? groupColor : QColor(52, 152, 219);
    borderCol.setAlpha(175);
    QPen pen(borderCol, bw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.drawRoundedRect(rect, r, r);

    // Group badge tab
    if (!groupLetter.isNull() && groupLetter.toLatin1() != 0) {
        const int badgePad = qRound(7 * scale);

        QFont font(QStringLiteral("sans-serif"));
        font.setPixelSize(effFontSize);
        font.setBold(true);
        p.setFont(font);
        QFontMetrics fm(font);

        QString countStr = (windowCount > 1) ? QStringLiteral(" (%1)").arg(windowCount) : QString();
        QString titleStr = groupTitle.trimmed();
        if (titleStr.isEmpty()) {
            titleStr = QStringLiteral("Grupa");
        }
        QString tabText = titleStr + countStr;

        const int letterW = qMax(badgeH, fm.horizontalAdvance(QString(groupLetter)) + qRound(8 * scale));
        const int textW = fm.horizontalAdvance(tabText);
        const int totalTabW = letterW + (textW > 0 ? (badgePad + textW + badgePad) : 0);

        const qreal tabY = tabAtBottom ? (rect.bottom() - tabHalfH) : (rect.top() - tabHalfH);
        const QRectF tabRect(rect.left() + qRound(10 * scale), tabY, totalTabW, badgeH);

        // Tab background
        p.setPen(Qt::NoPen);
        QColor tabBg = groupColor.isValid() ? groupColor : QColor(52, 152, 219);
        tabBg.setAlpha(235);
        p.setBrush(tabBg);
        p.drawRoundedRect(tabRect, badgeH / 2.0, badgeH / 2.0);

        // Draw letter
        const QColor textCol = contrastingTextColor(tabBg);
        p.setPen(textCol);
        const QRectF letRect(tabRect.left(), tabRect.top(), letterW, badgeH);
        p.drawText(letRect, Qt::AlignCenter, QString(groupLetter));

        // Draw tab text
        if (textW > 0) {
            const QRectF txtRect(tabRect.left() + letterW, tabRect.top(), textW + badgePad, badgeH);
            QFont subFont = font;
            subFont.setBold(false);
            p.setFont(subFont);
            p.drawText(txtRect, Qt::AlignVCenter | Qt::AlignLeft, tabText);
        }
    }

    p.end();
    return img;
}

inline QImage renderHudActionHintBar(bool isPolish, qreal scale, bool isSubLeap = false, int activeView = 0, const QString &customViewName = QString(), const QColor &customViewColor = QColor(), bool isStashMode = false, bool isSuperJump = false)
{
    Q_UNUSED(isPolish);
    Q_UNUSED(customViewName);
    Q_UNUSED(customViewColor);

    struct Segment {
        QString key;
        QString label;
        QColor keyColor;
    };

    QList<Segment> segments;
    if (isSuperJump) {
        segments = {
            {QStringLiteral("Key"), QStringLiteral("Pull to workspace"), QColor(0, 195, 255)},
            {QStringLiteral("Meta+A / Esc"), QStringLiteral("Back"), QColor(149, 165, 166)}
        };
    } else if (isStashMode) {
        segments = {
            {QStringLiteral("Key"), QStringLiteral("Add to workspace"), QColor(46, 204, 113)},
            {QStringLiteral("Ctrl+Key"), QStringLiteral("Add to stash"), QColor(46, 204, 113)},
            {QStringLiteral(", / ."), QStringLiteral("Switch workspace"), QColor(52, 152, 219)},
            {QStringLiteral("Shift+Key"), QStringLiteral("Close (:bd)"), QColor(231, 76, 60)},
            {QStringLiteral("Esc"), QStringLiteral("Back"), QColor(149, 165, 166)}
        };
    } else if (isSubLeap) {
        segments = {
            {QStringLiteral("Key"), QStringLiteral("Select"), QColor(52, 152, 219)},
            {QStringLiteral("Shift+Key"), QStringLiteral("Close"), QColor(231, 76, 60)},
            {QStringLiteral("Alt+Key"), QStringLiteral("Pull here"), QColor(241, 196, 15)},
            {QStringLiteral("Ctrl+Key"), QStringLiteral("Stash"), QColor(46, 204, 113)},
            {QStringLiteral("Backspace / Esc"), QStringLiteral("Back"), QColor(149, 165, 166)}
        };
    } else {
        segments = {
            {QStringLiteral("Key"), QStringLiteral("Jump"), QColor(52, 152, 219)},
            {QStringLiteral("Meta+A"), QStringLiteral("Super Jump"), QColor(0, 195, 255)},
            {QStringLiteral("Shift+Key"), QStringLiteral("Close (:bd)"), QColor(231, 76, 60)},
            {QStringLiteral("Alt+Key"), QStringLiteral("Pull here"), QColor(241, 196, 15)},
            {QStringLiteral("Ctrl+Key"), QStringLiteral("Stash (:hide)"), QColor(46, 204, 113)},
            {QStringLiteral("Tab"), QStringLiteral("Group"), QColor(155, 89, 182)},
            {QStringLiteral("Meta+S"), QStringLiteral("Stash"), QColor(46, 204, 113)},
            {QStringLiteral("Meta+M"), QStringLiteral("Markers"), QColor(155, 89, 182)},
            {QStringLiteral("Shift++"), QStringLiteral("New workspace"), QColor(52, 152, 219)},
            {QStringLiteral(", / ."), QStringLiteral("Switch"), QColor(52, 152, 219)}
        };
        if (activeView >= 1) {
            segments.append({QStringLiteral("_"), QStringLiteral("Delete workspace"), QColor(231, 76, 60)});
        }
    }

    QFont keyFont(QStringLiteral("sans-serif"));
    keyFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
    keyFont.setBold(true);
    QFontMetrics fmKey(keyFont);

    QFont lblFont(QStringLiteral("sans-serif"));
    lblFont.setPixelSize(qMax(9, qRound(11.0 * scale)));
    lblFont.setBold(false);
    QFontMetrics fmLbl(lblFont);

    const int padX = qRound(14 * scale);
    const int padY = qRound(5 * scale);
    const int itemGap = qRound(10 * scale);

    int totalContentW = 0;
    for (int i = 0; i < segments.size(); ++i) {
        const auto &s = segments.at(i);
        int segW = fmKey.horizontalAdvance(s.key) + qRound(4 * scale) + fmLbl.horizontalAdvance(s.label);
        totalContentW += segW;
        if (i < segments.size() - 1) {
            totalContentW += itemGap + fmLbl.horizontalAdvance(QStringLiteral("•")) + itemGap;
        }
    }

    const int w = totalContentW + 2 * padX;
    const int h = qMax(qRound(24 * scale), fmKey.height() + 2 * padY);

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF rect(0.5 * scale, 0.5 * scale, w - 1.0 * scale, h - 1.0 * scale);
    const qreal radius = h / 2.0;

    // Background capsule with subtle border
    p.setPen(QPen(QColor(255, 255, 255, 42), 1.0 * scale));
    p.setBrush(QColor(16, 20, 25, 235));
    p.drawRoundedRect(rect, radius, radius);

    qreal curX = padX;
    const qreal baselineY = padY + fmKey.ascent();

    for (int i = 0; i < segments.size(); ++i) {
        const auto &s = segments.at(i);

        // Draw key
        p.setFont(keyFont);
        p.setPen(s.keyColor);
        p.drawText(QPointF(curX, baselineY), s.key);
        curX += fmKey.horizontalAdvance(s.key) + qRound(4 * scale);

        // Draw label
        p.setFont(lblFont);
        p.setPen(QColor(215, 220, 225));
        p.drawText(QPointF(curX, baselineY), s.label);
        curX += fmLbl.horizontalAdvance(s.label);

        // Draw bullet separator
        if (i < segments.size() - 1) {
            curX += itemGap;
            p.setPen(QColor(255, 255, 255, 60));
            p.drawText(QPointF(curX, baselineY), QStringLiteral("•"));
            curX += fmLbl.horizontalAdvance(QStringLiteral("•")) + itemGap;
        }
    }

    p.end();
    return img;
}

struct HudModeHeaderView {
    QString name;
    QColor color;
    int windowCount = 0;
    bool isActive = false;
};

inline QImage renderHudModeHeader(bool isPolish,
                                  qreal scale,
                                  bool isSubLeap,
                                  const QString &subLeapTitle,
                                  const QColor &subLeapColor,
                                  int subLeapCount,
                                  const QList<HudModeHeaderView> &views,
                                  int stashCount = -1,
                                  bool isStashMode = false,
                                  const QString &targetWorkspaceName = QString(),
                                  bool isSuperJump = false)
{
    Q_UNUSED(isPolish);
    if (isSuperJump) {
        const QString leftText = QStringLiteral("⚡ SUPER JUMP: Pick any window to pull into [%1]").arg(!targetWorkspaceName.isEmpty() ? targetWorkspaceName : QStringLiteral("Workspace"));
        const QString rightText = QStringLiteral("Meta+A / Esc : Back");

        QFont titleFont(QStringLiteral("sans-serif"));
        titleFont.setPixelSize(qMax(10, qRound(13.0 * scale)));
        titleFont.setBold(true);
        QFontMetrics fmTitle(titleFont);

        QFont keyFont(QStringLiteral("sans-serif"));
        keyFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
        keyFont.setBold(true);
        QFontMetrics fmKey(keyFont);

        const int padX = qRound(16 * scale);
        const int padY = qRound(6 * scale);
        const int gap = qRound(16 * scale);

        const int w = padX + fmTitle.horizontalAdvance(leftText) + gap + fmKey.horizontalAdvance(rightText) + padX;
        const int h = qMax(qRound(34 * scale), fmTitle.height() + 2 * padY);

        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);

        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF rect(0.5 * scale, 0.5 * scale, w - 1.0 * scale, h - 1.0 * scale);
        const qreal radius = h / 2.0;

        const QColor accent(0, 195, 255); // Electric Cyan
        p.setPen(QPen(accent, 1.8 * scale));
        p.setBrush(QColor(14, 22, 28, 245));
        p.drawRoundedRect(rect, radius, radius);

        const qreal baselineY = padY + fmTitle.ascent();
        p.setFont(titleFont);
        p.setPen(QColor(255, 255, 255));
        p.drawText(QPointF(padX, baselineY), leftText);

        p.setFont(keyFont);
        p.setPen(accent);
        const qreal keyX = w - padX - fmKey.horizontalAdvance(rightText);
        p.drawText(QPointF(keyX, baselineY), rightText);

        p.end();
        return img;
    }

    if (isSubLeap) {
        const QString leftText = QStringLiteral("🗂 GROUP DISAMBIGUATION: %1 (%2 windows)  •  Press target letter").arg(subLeapTitle).arg(subLeapCount);
        const QString rightText = QStringLiteral("Backspace / Esc : Back");

        QFont titleFont(QStringLiteral("sans-serif"));
        titleFont.setPixelSize(qMax(10, qRound(13.0 * scale)));
        titleFont.setBold(true);
        QFontMetrics fmTitle(titleFont);

        QFont keyFont(QStringLiteral("sans-serif"));
        keyFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
        keyFont.setBold(true);
        QFontMetrics fmKey(keyFont);

        const int padX = qRound(16 * scale);
        const int padY = qRound(6 * scale);
        const int gap = qRound(16 * scale);

        const int w = padX + fmTitle.horizontalAdvance(leftText) + gap + fmKey.horizontalAdvance(rightText) + padX;
        const int h = qMax(qRound(34 * scale), fmTitle.height() + 2 * padY);

        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);

        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF rect(0.5 * scale, 0.5 * scale, w - 1.0 * scale, h - 1.0 * scale);
        const qreal radius = h / 2.0;

        const QColor accent = subLeapColor.isValid() ? subLeapColor : QColor(155, 89, 182);
        p.setPen(QPen(accent, 1.8 * scale));
        p.setBrush(QColor(16, 20, 26, 245));
        p.drawRoundedRect(rect, radius, radius);

        const qreal baselineY = padY + fmTitle.ascent();
        p.setFont(titleFont);
        p.setPen(QColor(255, 255, 255));
        p.drawText(QPointF(padX, baselineY), leftText);

        p.setFont(keyFont);
        p.setPen(QColor(231, 76, 60));
        const qreal keyX = w - padX - fmKey.horizontalAdvance(rightText);
        p.drawText(QPointF(keyX, baselineY), rightText);

        p.end();
        return img;
    }

    if (isStashMode) {
        const QString leftText = QStringLiteral("📦 STASH: Pick a window to add to [%1]").arg(!targetWorkspaceName.isEmpty() ? targetWorkspaceName : QStringLiteral("Workspace"));
        const QString rightText = QStringLiteral(", / . : Switch workspace  •  Esc : Back");

        QFont titleFont(QStringLiteral("sans-serif"));
        titleFont.setPixelSize(qMax(10, qRound(13.0 * scale)));
        titleFont.setBold(true);
        QFontMetrics fmTitle(titleFont);

        QFont keyFont(QStringLiteral("sans-serif"));
        keyFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
        keyFont.setBold(true);
        QFontMetrics fmKey(keyFont);

        const int padX = qRound(16 * scale);
        const int padY = qRound(6 * scale);
        const int gap = qRound(16 * scale);

        const int w = padX + fmTitle.horizontalAdvance(leftText) + gap + fmKey.horizontalAdvance(rightText) + padX;
        const int h = qMax(qRound(34 * scale), fmTitle.height() + 2 * padY);

        QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);

        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);

        const QRectF rect(0.5 * scale, 0.5 * scale, w - 1.0 * scale, h - 1.0 * scale);
        const qreal radius = h / 2.0;

        const QColor accent(46, 204, 113); // Emerald Green
        p.setPen(QPen(accent, 1.8 * scale));
        p.setBrush(QColor(14, 24, 18, 245));
        p.drawRoundedRect(rect, radius, radius);

        const qreal baselineY = padY + fmTitle.ascent();
        p.setFont(titleFont);
        p.setPen(QColor(255, 255, 255));
        p.drawText(QPointF(padX, baselineY), leftText);

        p.setFont(keyFont);
        p.setPen(QColor(46, 204, 113));
        const qreal keyX = w - padX - fmKey.horizontalAdvance(rightText);
        p.drawText(QPointF(keyX, baselineY), rightText);

        p.end();
        return img;
    }

    if (views.isEmpty()) {
        return QImage();
    }

    const QString navLeft = QStringLiteral("◂ ,");
    const QString navRight = QStringLiteral(". ▸");

    QFont navFont(QStringLiteral("sans-serif"));
    navFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
    navFont.setBold(true);
    QFontMetrics fmNav(navFont);

    QFont activeFont(QStringLiteral("sans-serif"));
    activeFont.setPixelSize(qMax(10, qRound(12.5 * scale)));
    activeFont.setBold(true);
    QFontMetrics fmActive(activeFont);

    QFont inactiveFont(QStringLiteral("sans-serif"));
    inactiveFont.setPixelSize(qMax(9, qRound(12.0 * scale)));
    inactiveFont.setBold(false);
    QFontMetrics fmInactive(inactiveFont);

    const int padX = qRound(12 * scale);
    const int padY = qRound(5 * scale);
    const int tabPadX = qRound(10 * scale);
    const int tabGap = qRound(6 * scale);
    const int navGap = qRound(10 * scale);

    const int headerH = qRound(34 * scale);
    const int tabH = headerH - 2 * padY;

    struct TabRenderInfo {
        QString text;
        int width;
        bool isActive;
        QColor color;
    };

    QList<TabRenderInfo> tabInfos;
    tabInfos.reserve(views.size());

    int allTabsW = 0;
    for (const auto &v : views) {
        const QString text = QStringLiteral("%1 (%2)").arg(v.name).arg(v.windowCount);
        const int textW = v.isActive ? fmActive.horizontalAdvance(text) : fmInactive.horizontalAdvance(text);
        const int tabW = textW + 2 * tabPadX;
        tabInfos.append({text, tabW, v.isActive, v.color});
        allTabsW += tabW;
    }
    allTabsW += (views.size() - 1) * tabGap;

    const int navLeftW = fmNav.horizontalAdvance(navLeft);
    const int navRightW = fmNav.horizontalAdvance(navRight);

    // Stash Drawer pill on the right (separated from workspaces)
    const QString stashText = QStringLiteral("📦 Stash (%1) • Meta+S").arg(qMax(0, stashCount));
    QFont stashFont(QStringLiteral("sans-serif"));
    stashFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
    stashFont.setBold(true);
    QFontMetrics fmStash(stashFont);

    const int stashTextW = fmStash.horizontalAdvance(stashText);
    const int stashPillW = stashTextW + 2 * tabPadX;

    // New Workspace pill [+ New • Shift++]
    const QString newWsText = QStringLiteral("+ New • Shift++");
    QFont newWsFont(QStringLiteral("sans-serif"));
    newWsFont.setPixelSize(qMax(9, qRound(11.5 * scale)));
    newWsFont.setBold(true);
    QFontMetrics fmNewWs(newWsFont);

    const int newWsTextW = fmNewWs.horizontalAdvance(newWsText);
    const int newWsPillW = newWsTextW + 2 * tabPadX;
    const int newWsSectionW = navGap + newWsPillW;

    const int stashSectionW = (stashCount >= 0) ? (navGap + qRound(1 * scale) + navGap + stashPillW) : 0;

    const int totalW = padX + navLeftW + navGap + allTabsW + navGap + navRightW + newWsSectionW + stashSectionW + padX;

    QImage img(totalW, headerH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Outer background capsule
    const QRectF outerRect(0.5 * scale, 0.5 * scale, totalW - 1.0 * scale, headerH - 1.0 * scale);
    const qreal outerRadius = headerH / 2.0;

    p.setPen(QPen(QColor(255, 255, 255, 38), 1.0 * scale));
    p.setBrush(QColor(14, 18, 24, 242));
    p.drawRoundedRect(outerRect, outerRadius, outerRadius);

    // Navigation Left ◂ [,]
    p.setFont(navFont);
    p.setPen(QColor(230, 126, 34));
    const qreal navLeftY = (headerH - fmNav.height()) / 2.0 + fmNav.ascent();
    p.drawText(QPointF(padX, navLeftY), navLeft);

    // Draw tabs
    qreal curX = padX + navLeftW + navGap;
    const qreal tabY = padY;

    for (const auto &tab : tabInfos) {
        const QRectF tabRect(curX, tabY, tab.width, tabH);
        const qreal tabRadius = tabH / 2.0;

        if (tab.isActive) {
            const QColor col = tab.color.isValid() ? tab.color : QColor(52, 152, 219);
            p.setPen(QPen(col, 1.8 * scale));
            p.setBrush(QColor(col.red(), col.green(), col.blue(), 55));
            p.drawRoundedRect(tabRect, tabRadius, tabRadius);

            p.setFont(activeFont);
            p.setPen(QColor(255, 255, 255));
            p.drawText(tabRect, Qt::AlignCenter, tab.text);
        } else {
            p.setPen(QPen(QColor(255, 255, 255, 24), 1.0 * scale));
            p.setBrush(QColor(255, 255, 255, 12));
            p.drawRoundedRect(tabRect, tabRadius, tabRadius);

            p.setFont(inactiveFont);
            p.setPen(QColor(190, 195, 205, 175));
            p.drawText(tabRect, Qt::AlignCenter, tab.text);
        }

        curX += tab.width + tabGap;
    }

    // Navigation Right [.] ▸
    p.setFont(navFont);
    p.setPen(QColor(230, 126, 34));
    const qreal navRightX = curX - tabGap + navGap;
    p.drawText(QPointF(navRightX, navLeftY), navRight);

    // New Workspace pill [+ Nowy • Ctrl+N]
    const qreal newWsX = navRightX + navRightW + navGap;
    const QRectF newWsRect(newWsX, tabY, newWsPillW, tabH);
    const qreal newWsRadius = tabH / 2.0;

    p.setPen(QPen(QColor(52, 152, 219, 180), 1.2 * scale));
    p.setBrush(QColor(52, 152, 219, 35));
    p.drawRoundedRect(newWsRect, newWsRadius, newWsRadius);

    p.setFont(newWsFont);
    p.setPen(QColor(52, 152, 219));
    p.drawText(newWsRect, Qt::AlignCenter, newWsText);

    // Stash drawer pill separated on the right
    if (stashCount >= 0) {
        const qreal sepX = newWsX + newWsPillW + navGap;
        p.setPen(QPen(QColor(255, 255, 255, 38), 1.0 * scale));
        p.drawLine(QPointF(sepX, padY + 3.0 * scale), QPointF(sepX, headerH - padY - 3.0 * scale));

        const qreal stashX = sepX + navGap;
        const QRectF stashRect(stashX, tabY, stashPillW, tabH);
        const qreal stashRadius = tabH / 2.0;

        p.setPen(QPen(QColor(46, 204, 113, 150), 1.2 * scale));
        p.setBrush(QColor(46, 204, 113, 30));
        p.drawRoundedRect(stashRect, stashRadius, stashRadius);

        p.setFont(stashFont);
        p.setPen(QColor(46, 204, 113));
        p.drawText(stashRect, Qt::AlignCenter, stashText);
    }

    p.end();
    return img;
}

} // namespace OutlineBadge
