/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "presethelper.h"

#include <QDialog>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QLocale>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace LeapK
{

struct PresetSettingItem {
    QString key;
    QString label;
};

struct PresetCategory {
    QString title;
    QString iconTheme;
    QList<PresetSettingItem> items;
};

class PresetActivationDialog
{
public:
    static QList<PresetCategory> getCategories()
    {
        return QList<PresetCategory>{
            // 1. Geometry & Border
            {
                QStringLiteral("📐 Geometry & Border Style"),
                QStringLiteral("draw-rectangle"),
                {
                    {QStringLiteral("BorderWidth"), QStringLiteral("Border Width")},
                    {QStringLiteral("OutlineGap"), QStringLiteral("Outline Gap")},
                    {QStringLiteral("BorderRadius"), QStringLiteral("Corner Radius")},
                    {QStringLiteral("FadeCorners"), QStringLiteral("Fade Corners")},
                    {QStringLiteral("DrawInside"), QStringLiteral("Draw Inside Window")},
                    {QStringLiteral("SmoothEdges"), QStringLiteral("Smooth Edges")},
                    {QStringLiteral("GlowEnabled"), QStringLiteral("Outer Glow")},
                    {QStringLiteral("GlowRadius"), QStringLiteral("Glow Radius")}
                }
            },
            // 2. Colors & Opacity
            {
                QStringLiteral("🎨 Colors & Opacity"),
                QStringLiteral("color-management"),
                {
                    {QStringLiteral("UseAccentColor"), QStringLiteral("Use KDE Accent Color")},
                    {QStringLiteral("CustomColor"), QStringLiteral("Custom Outline Color")},
                    {QStringLiteral("AccentOpacity"), QStringLiteral("Outline Opacity")},
                    {QStringLiteral("EnsurePerimeterContrast"), QStringLiteral("Perimeter Contrast Safeguard")}
                }
            },
            // 3. Dynamic Ambilight
            {
                QStringLiteral("🌈 Dynamic Ambilight (Color Sampling)"),
                QStringLiteral("video-display"),
                {
                    {QStringLiteral("AmbilightEnabled"), QStringLiteral("Enable Ambilight")},
                    {QStringLiteral("AmbilightMode"), QStringLiteral("Color Sampling Mode")},
                    {QStringLiteral("AmbilightOpacity"), QStringLiteral("Ambilight Opacity")},
                    {QStringLiteral("AmbilightFrequency"), QStringLiteral("Sampling Frequency")},
                    {QStringLiteral("AmbilightDuration"), QStringLiteral("Transition Duration")},
                    {QStringLiteral("AmbilightSaturationLeveling"), QStringLiteral("Saturation Leveling")},
                    {QStringLiteral("AmbilightFallbackStrength"), QStringLiteral("Fallback Strength")},
                    {QStringLiteral("AmbilightMinBrightness"), QStringLiteral("Minimum Brightness")},
                    {QStringLiteral("AmbilightMaxBrightness"), QStringLiteral("Maximum Brightness")},
                    {QStringLiteral("AmbilightEnsureContrast"), QStringLiteral("Ambilight Contrast Safeguard")}
                }
            },
            // 4. Shader Effects & Animations
            {
                QStringLiteral("⚡ Shader Effects & Animations"),
                QStringLiteral("preferences-desktop-effects"),
                {
                    {QStringLiteral("BreatheEnabled"), QStringLiteral("Breathing Pulse")},
                    {QStringLiteral("BreatheSpeed"), QStringLiteral("Breathing Speed")},
                    {QStringLiteral("BreatheIntensity"), QStringLiteral("Breathing Intensity")},
                    {QStringLiteral("GlitchEnabled"), QStringLiteral("Glitch Effect")},
                    {QStringLiteral("GlitchSpeed"), QStringLiteral("Glitch Speed")},
                    {QStringLiteral("GlitchIntensity"), QStringLiteral("Glitch Intensity")},
                    {QStringLiteral("GlitchAnomalyEnabled"), QStringLiteral("Glitch Anomaly")},
                    {QStringLiteral("GlitchFrequency"), QStringLiteral("Anomaly Frequency")},
                    {QStringLiteral("NeonFlickerEnabled"), QStringLiteral("Neon Flicker")},
                    {QStringLiteral("NeonFlickerInterval"), QStringLiteral("Neon Flicker Interval")},
                    {QStringLiteral("NeonFlickerDuration"), QStringLiteral("Neon Flicker Duration")},
                    {QStringLiteral("NeonFlickerIntensity"), QStringLiteral("Neon Flicker Intensity")},
                    {QStringLiteral("FlashEnabled"), QStringLiteral("Flash Pulse Effect")},
                    {QStringLiteral("FlashInterval"), QStringLiteral("Flash Interval")},
                    {QStringLiteral("FlashDuration"), QStringLiteral("Flash Duration")},
                    {QStringLiteral("ColorCycleEnabled"), QStringLiteral("Color Cycling")},
                    {QStringLiteral("ColorTransitionDuration"), QStringLiteral("Cycle Duration")},
                    {QStringLiteral("ColorCycleOrder"), QStringLiteral("Cycle Order")},
                    {QStringLiteral("ColorCycleEasing"), QStringLiteral("Cycle Easing")},
                    {QStringLiteral("ColorList"), QStringLiteral("Color Palette")},
                    {QStringLiteral("CustomShaderEnabled"), QStringLiteral("Custom GLSL Shader")},
                    {QStringLiteral("ActiveCustomShaderId"), QStringLiteral("Active Shader ID")},
                    {QStringLiteral("ShaderEffect"), QStringLiteral("Bundled Shader & Parameters")}
                }
            },
            // 5. Leap Mode & Badges
            {
                QStringLiteral("🚀 Leap Mode & Badges"),
                QStringLiteral("input-keyboard"),
                {
                    {QStringLiteral("BadgeDisplayMode"), QStringLiteral("Badge Display Mode")},
                    {QStringLiteral("JumpTargets"), QStringLiteral("Jump Targets Scope")},
                    {QStringLiteral("BadgePosition"), QStringLiteral("Window Badge Position")},
                    {QStringLiteral("BadgeHudPosition"), QStringLiteral("Screen HUD Position")},
                    {QStringLiteral("BadgeSize"), QStringLiteral("Badge Size")},
                    {QStringLiteral("BadgeMargin"), QStringLiteral("Badge Margin")},
                    {QStringLiteral("MarkShowLetterBadges"), QStringLiteral("Show Letter Badges")},
                    {QStringLiteral("BadgesAlwaysVisible"), QStringLiteral("Badges Always Visible")},
                    {QStringLiteral("BadgeShowIcon"), QStringLiteral("Application Icon in Badge")},
                    {QStringLiteral("BadgeShowThumbnail"), QStringLiteral("Window Thumbnail Preview in Badge")},
                    {QStringLiteral("BadgeJumpSize"), QStringLiteral("Jump Mode Badge Size")},
                    {QStringLiteral("BadgeHudSize"), QStringLiteral("HUD Cards Height")},
                    {QStringLiteral("BadgeIconSize"), QStringLiteral("Badge Icon Size")},
                    {QStringLiteral("BadgeThumbnailWidth"), QStringLiteral("Thumbnail Width")},
                    {QStringLiteral("BadgeThumbnailHeight"), QStringLiteral("Thumbnail Height")},
                    {QStringLiteral("BadgeCornerRadius"), QStringLiteral("Badge Corner Radius")},
                    {QStringLiteral("BadgeOpacity"), QStringLiteral("Badge Opacity")},
                    {QStringLiteral("BadgeHudMarkColorMode"), QStringLiteral("HUD Card Mark Color Mode")},
                    {QStringLiteral("BadgeDefaultColor"), QStringLiteral("Badge Default Background Color")},
                    {QStringLiteral("BadgeLetterColorAuto"), QStringLiteral("Auto Contrast Jump Letter")},
                    {QStringLiteral("BadgeLetterColor"), QStringLiteral("Jump Letter Color")},
                    {QStringLiteral("BadgeLabelColorAuto"), QStringLiteral("Auto Contrast Window Label")},
                    {QStringLiteral("BadgeLabelUnmarkedColor"), QStringLiteral("Custom Label Color (Unmarked)")},
                    {QStringLiteral("BadgeCaptionColorAuto"), QStringLiteral("Auto Contrast HUD Window Caption")},
                    {QStringLiteral("BadgeCaptionColor"), QStringLiteral("HUD Window Caption Color")},
                    {QStringLiteral("BadgeLocationColorAuto"), QStringLiteral("Auto Contrast HUD Location")},
                    {QStringLiteral("BadgeLocationColor"), QStringLiteral("HUD Location Color")},
                    {QStringLiteral("BadgeLetterFont"), QStringLiteral("Jump Letter Font")},
                    {QStringLiteral("BadgeLabelFont"), QStringLiteral("Window Label Font")},
                    {QStringLiteral("BadgeCaptionFont"), QStringLiteral("HUD Caption Font")},
                    {QStringLiteral("BadgeLocationFont"), QStringLiteral("HUD Location Font")},
                    {QStringLiteral("BadgeLetterFontSize"), QStringLiteral("Jump Letter Font Size")},
                    {QStringLiteral("BadgeLabelFontSize"), QStringLiteral("Window Label Font Size")},
                    {QStringLiteral("BadgeCaptionFontSize"), QStringLiteral("HUD Caption Font Size")},
                    {QStringLiteral("BadgeLocationFontSize"), QStringLiteral("HUD Location Font Size")},
                    {QStringLiteral("BadgeLivePreviews"), QStringLiteral("Real-Time Live Previews")},
                    {QStringLiteral("BadgeHudIncludeAll"), QStringLiteral("Include All Windows in HUD")},
                    {QStringLiteral("BadgeTimeout"), QStringLiteral("Badges Display Timeout")},
                    {QStringLiteral("JumpRecentKey"), QStringLiteral("Jump to Recent Window Key")},
                    {QStringLiteral("BadgeWindowBadgesEnabled"), QStringLiteral("Show Badges on Windows")},
                    {QStringLiteral("BadgeWindowShowIcon"), QStringLiteral("Application Icon in Window Badge")},
                    {QStringLiteral("BadgeWindowShowThumbnail"), QStringLiteral("Thumbnail in Window Badge")},
                    {QStringLiteral("BadgeWindowIconSize"), QStringLiteral("Window Badge Icon Size")},
                    {QStringLiteral("BadgeWindowThumbnailWidth"), QStringLiteral("Window Thumbnail Width")},
                    {QStringLiteral("BadgeWindowThumbnailHeight"), QStringLiteral("Window Thumbnail Height")},
                    {QStringLiteral("BadgeWindowThumbnailSize"), QStringLiteral("Window Thumbnail Base Size")},
                    {QStringLiteral("BadgeWindowLetterFontSize"), QStringLiteral("Window Badge Letter Font Size")},
                    {QStringLiteral("BadgeWindowLabelFontSize"), QStringLiteral("Window Badge Label Font Size")},
                    {QStringLiteral("BadgeHudShowIcon"), QStringLiteral("Application Icon in HUD Card")},
                    {QStringLiteral("BadgeHudShowThumbnail"), QStringLiteral("Thumbnail in HUD Card")},
                    {QStringLiteral("BadgeHudIconSize"), QStringLiteral("HUD Card Icon Size")},
                    {QStringLiteral("BadgeHudThumbnailWidth"), QStringLiteral("HUD Thumbnail Width")},
                    {QStringLiteral("BadgeHudThumbnailHeight"), QStringLiteral("HUD Thumbnail Height")},
                    {QStringLiteral("BadgeHudThumbnailSize"), QStringLiteral("HUD Thumbnail Base Size")},
                    {QStringLiteral("BadgeHudLetterFontSize"), QStringLiteral("HUD Card Letter Font Size")},
                    {QStringLiteral("BadgeHudCaptionFontSize"), QStringLiteral("HUD Card Caption Font Size")},
                    {QStringLiteral("BadgeHudLocationFontSize"), QStringLiteral("HUD Card Location Font Size")}
                }
            },
            // 6. Window Marks
            {
                QStringLiteral("🏷️ Window Marks"),
                QStringLiteral("bookmark-new"),
                {
                    {QStringLiteral("MarkDefaultColor"), QStringLiteral("Default Mark Color")},
                    {QStringLiteral("MarkSmartAdaptive"), QStringLiteral("Smart Adaptive Color")},
                    {QStringLiteral("MarkRandomColor"), QStringLiteral("Random Mark Color")},
                    {QStringLiteral("StylePrecedence"), QStringLiteral("Style Precedence Order")},
                    {QStringLiteral("MarkInactiveWidthReduction"), QStringLiteral("Inactive Width Reduction")},
                    {QStringLiteral("MarkInactiveDashed"), QStringLiteral("Dashed Inactive Outline")},
                    {QStringLiteral("MarkInactiveDashGap"), QStringLiteral("Inactive Dash Gap")},
                    {QStringLiteral("MarkInactiveDashSmooth"), QStringLiteral("Smooth Inactive Dashes")}
                }
            },
            // 7. Focus History & Behavior
            {
                QStringLiteral("🕒 Focus History & Behavior"),
                QStringLiteral("preferences-system-windows"),
                {
                    {QStringLiteral("HighlightRecentWindow"), QStringLiteral("Highlight Recent Windows")},
                    {QStringLiteral("RecentWindowsCount"), QStringLiteral("Recent Windows Count")},
                    {QStringLiteral("RecentWindowColorMode"), QStringLiteral("Recent Windows Color Mode")},
                    {QStringLiteral("RecentWindowCustomColor"), QStringLiteral("Custom Recent Color")},
                    {QStringLiteral("RecentWindowOpacity"), QStringLiteral("Recent Window Opacity")},
                    {QStringLiteral("RecentWindowHueShift"), QStringLiteral("Recent Hue Shift")},
                    {QStringLiteral("RecentWindowSaturation"), QStringLiteral("Recent Saturation")},
                    {QStringLiteral("RecentWindowWidthReduction"), QStringLiteral("Recent Width Reduction")},
                    {QStringLiteral("RecentWindowDashed"), QStringLiteral("Dashed Recent Outline")},
                    {QStringLiteral("RecentWindowGapGrowth"), QStringLiteral("Recent Gap Growth")},
                    {QStringLiteral("RecentWindowDashSmooth"), QStringLiteral("Smooth Recent Dashes")},
                    {QStringLiteral("FadeDuration"), QStringLiteral("Outline Fade Duration")},
                    {QStringLiteral("MenuHideDelay"), QStringLiteral("Menu Hide Delay")},
                    {QStringLiteral("MenuShowDelay"), QStringLiteral("Menu Show Delay")},
                    {QStringLiteral("WindowHideDelay"), QStringLiteral("Window Hide Delay")},
                    {QStringLiteral("WindowShowDelay"), QStringLiteral("Window Show Delay")},
                    {QStringLiteral("HideOnFullScreen"), QStringLiteral("Hide Outline on Full Screen")},
                    {QStringLiteral("TrayIconEnabled"), QStringLiteral("System Tray Icon")}
                }
            }
        };
    }

    static QString formatValue(const QString &key, const QJsonValue &val)
    {
        if (val.isBool()) {
            return val.toBool() ? QStringLiteral("Enabled") : QStringLiteral("Disabled");
        }
        if (key == QStringLiteral("BadgeDisplayMode")) {
            const int m = val.toInt();
            if (m == 0) return QStringLiteral("Only in Jump Mode");
            if (m == 1) return QStringLiteral("Always visible on background");
            return QStringLiteral("Disabled");
        }
        if (key == QStringLiteral("JumpTargets")) {
            const int t = val.toInt();
            if (t == 0) return QStringLiteral("Current desktop (visible) + marked");
            if (t == 1) return QStringLiteral("Current desktop (visible & min) + marked");
            if (t == 2) return QStringLiteral("All desktops (visible & min)");
            return QStringLiteral("Marked windows only");
        }
        if (key == QStringLiteral("BadgeHudPosition")) {
            const int p = val.toInt();
            if (p == 0) return QStringLiteral("Bottom");
            if (p == 1) return QStringLiteral("Top");
            if (p == 2) return QStringLiteral("Left");
            return QStringLiteral("Right");
        }
        if (key == QStringLiteral("BadgeHudMarkColorMode")) {
            const int mode = val.toInt();
            switch (mode) {
            case 0: return QStringLiteral("Dark card with mark border & badge");
            case 1: return QStringLiteral("Full mark color");
            case 2: return QStringLiteral("Tinted mark color");
            default: return QString::number(mode);
            }
        }
        if (key == QStringLiteral("BadgeTimeout")) {
            const int t = val.toInt();
            if (t == 0) return QStringLiteral("Unlimited (0 s)");
            return QStringLiteral("%1 s").arg(t);
        }
        if (key == QStringLiteral("BadgePosition")) {
            const int p = val.toInt();
            const char *posEn[] = {"Top-Left", "Top", "Top-Right", "Left", "Center", "Right", "Bottom-Left", "Bottom", "Bottom-Right"};
            if (p >= 0 && p < 9) {
                return QString::fromUtf8(posEn[p]);
            }
        }
        if (key == QStringLiteral("RecentWindowColorMode")) {
            const int m = val.toInt();
            if (m == 0) return QStringLiteral("Dimmed active color");
            if (m == 1) return QStringLiteral("Custom color");
            return QStringLiteral("Grayscale");
        }
        if (key == QStringLiteral("AmbilightMode")) {
            const int m = val.toInt();
            if (m == 0) return QStringLiteral("Dominant average");
            return QStringLiteral("Perimeter segments");
        }
        if (key.endsWith(QStringLiteral("FontSize"))) {
            const int sz = val.toInt();
            if (sz == 0) return QStringLiteral("Default (auto)");
            return QStringLiteral("%1 px").arg(sz);
        }
        if (key.endsWith(QStringLiteral("Font"))) {
            const QString f = val.toString().trimmed();
            if (f.isEmpty()) return QStringLiteral("System default");
            return f;
        }
        if (key.endsWith(QStringLiteral("Width")) || key.endsWith(QStringLiteral("Height")) ||
            key.endsWith(QStringLiteral("Radius")) || key.endsWith(QStringLiteral("Size")) ||
            key == QStringLiteral("BadgeMargin") || key == QStringLiteral("OutlineGap") ||
            key == QStringLiteral("MarkInactiveDashGap") || key == QStringLiteral("RecentWindowGapGrowth")) {
            return QStringLiteral("%1 px").arg(val.toInt());
        }
        if (key.endsWith(QStringLiteral("Delay")) || key == QStringLiteral("FadeDuration")) {
            return QStringLiteral("%1 ms").arg(val.toInt());
        }
        if (key.endsWith(QStringLiteral("Opacity")) || key.endsWith(QStringLiteral("Intensity")) ||
            key == QStringLiteral("AmbilightSaturationLeveling") || key == QStringLiteral("AmbilightFallbackStrength") ||
            key == QStringLiteral("AmbilightMinBrightness") || key == QStringLiteral("AmbilightMaxBrightness")) {
            return QStringLiteral("%1%").arg(val.toInt());
        }
        if (val.isDouble()) {
            if (key == QStringLiteral("BreatheSpeed")) {
                return QStringLiteral("%1 Hz").arg(val.toDouble(), 0, 'f', 2);
            }
            if (key == QStringLiteral("GlitchSpeed")) {
                return QStringLiteral("%1x").arg(val.toDouble(), 0, 'f', 2);
            }
            if (key.endsWith(QStringLiteral("Duration")) || key.endsWith(QStringLiteral("Interval")) || key == QStringLiteral("ColorTransitionDuration")) {
                return QStringLiteral("%1 s").arg(val.toDouble(), 0, 'f', 2);
            }
            if (key.endsWith(QStringLiteral("Frequency"))) {
                return QStringLiteral("%1 Hz").arg(val.toDouble(), 0, 'f', 1);
            }
            return QString::number(val.toDouble(), 'g', 3);
        }
        if (key == QStringLiteral("ShaderEffect")) {
            if (val.isObject()) {
                const QJsonObject o = val.toObject();
                QString name;
                if (o.contains(QStringLiteral("effect")) && o.value(QStringLiteral("effect")).isObject()) {
                    name = o.value(QStringLiteral("effect")).toObject().value(QStringLiteral("name")).toString();
                } else if (o.contains(QStringLiteral("name"))) {
                    name = o.value(QStringLiteral("name")).toString();
                }
                if (name.isEmpty()) name = o.value(QStringLiteral("id")).toString();
                return QStringLiteral("Bundle: %1").arg(name);
            }
            return QStringLiteral("None");
        }
        if (val.isString()) {
            const QString s = val.toString();
            if (s.length() > 26) {
                return s.left(23) + QStringLiteral("…");
            }
            return s;
        }
        return QString::number(val.toInt());
    }

    static bool areValuesEqual(const QJsonValue &v1, const QJsonValue &v2)
    {
        if (v1.type() != v2.type()) {
            if ((v1.isDouble() || v1.isDouble()) && (v2.isDouble() || v2.isDouble())) {
                return qFuzzyCompare(v1.toDouble() + 1.0, v2.toDouble() + 1.0);
            }
            return false;
        }
        if (v1.isBool()) return v1.toBool() == v2.toBool();
        if (v1.isDouble()) return qFuzzyCompare(v1.toDouble() + 1.0, v2.toDouble() + 1.0);
        if (v1.isString()) return v1.toString().compare(v2.toString(), Qt::CaseInsensitive) == 0;
        if (v1.isObject() && v2.isObject()) {
            return QJsonDocument(v1.toObject()).toJson(QJsonDocument::Compact) == QJsonDocument(v2.toObject()).toJson(QJsonDocument::Compact);
        }
        return v1.toInt() == v2.toInt();
    }

    static bool selectSettings(QWidget *parent,
                               const OutlinePreset::PresetInfo &preset,
                               const QJsonObject &currentSettings,
                               QJsonObject &outSettingsToApply)
    {
        QDialog dlg(parent);
        dlg.setWindowTitle(QStringLiteral("Select Elements to Apply from Preset: %1").arg(preset.name));
        dlg.setWindowIcon(QIcon::fromTheme(QStringLiteral("leapk"), QIcon(QStringLiteral(":/icons/leapk.svg"))));
        dlg.resize(760, 600);

        auto *mainLayout = new QVBoxLayout(&dlg);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(10);

        // Header
        auto *headerLabel = new QLabel(&dlg);
        headerLabel->setTextFormat(Qt::RichText);
        headerLabel->setWordWrap(true);
        const QString desc = preset.description.isEmpty()
            ? QStringLiteral("No description.")
            : preset.description;
        headerLabel->setText(QStringLiteral("<h3>Preset: <b>%1</b></h3>"
                                            "<p style='color: #888;'>%2</p>"
                                            "<p>Select the categories or individual settings to apply. "
                                            "Unchecked options will keep your current configuration intact.</p>")
                                .arg(preset.name, desc));
        mainLayout->addWidget(headerLabel);

        // Quick toolbar & search filter
        auto *toolsLayout = new QHBoxLayout();
        auto *btnSelectAll = new QPushButton(QStringLiteral("☑ Select All"), &dlg);
        auto *btnDeselectAll = new QPushButton(QStringLiteral("☐ Deselect All"), &dlg);
        auto *btnOnlyDiff = new QPushButton(&dlg);

        toolsLayout->addWidget(btnSelectAll);
        toolsLayout->addWidget(btnDeselectAll);
        toolsLayout->addWidget(btnOnlyDiff);
        toolsLayout->addSpacing(16);

        auto *searchBox = new QLineEdit(&dlg);
        searchBox->setPlaceholderText(QStringLiteral("🔍 Filter options…"));
        searchBox->setClearButtonEnabled(true);
        toolsLayout->addWidget(searchBox);

        mainLayout->addLayout(toolsLayout);

        // Options Tree
        auto *tree = new QTreeWidget(&dlg);
        tree->setHeaderLabels(QStringList{QStringLiteral("Setting / Category"), QStringLiteral("Preset Value"), QStringLiteral("Current Value")});
        tree->setAlternatingRowColors(true);
        tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        mainLayout->addWidget(tree);

        const auto categories = getCategories();
        int totalDiffCount = 0;

        for (const auto &cat : categories) {
            auto *catItem = new QTreeWidgetItem(tree);
            catItem->setText(0, cat.title);
            catItem->setIcon(0, QIcon::fromTheme(cat.iconTheme));
            catItem->setFlags(catItem->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
            catItem->setCheckState(0, Qt::Checked);

            QFont f = catItem->font(0);
            f.setBold(true);
            catItem->setFont(0, f);

            for (const auto &it : cat.items) {
                auto *child = new QTreeWidgetItem(catItem);
                child->setText(0, it.label);
                child->setData(0, Qt::UserRole, it.key);
                child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
                child->setCheckState(0, Qt::Checked);

                const QJsonValue presetVal = preset.settings.value(it.key);
                const QJsonValue currentVal = currentSettings.value(it.key);
                const bool differs = !areValuesEqual(presetVal, currentVal);

                if (differs) {
                    totalDiffCount++;
                }
                child->setData(0, Qt::UserRole + 1, differs);

                const QString presetStr = formatValue(it.key, presetVal);
                const QString currentStr = formatValue(it.key, currentVal);

                child->setText(1, presetStr);
                child->setText(2, currentStr);

                if (differs) {
                    child->setForeground(1, QBrush(QColor(QStringLiteral("#2ecc71"))));
                    child->setToolTip(1, QStringLiteral("Value differs from your current configuration"));
                } else {
                    child->setForeground(1, QBrush(QColor(QStringLiteral("#888888"))));
                }
            }
            catItem->setExpanded(true);
        }

        btnOnlyDiff->setText(QStringLiteral("⚡ Differences Only (%1)").arg(totalDiffCount));

        // Bottom action layout
        auto *bottomLayout = new QHBoxLayout();
        auto *lblCount = new QLabel(&dlg);
        bottomLayout->addWidget(lblCount);
        bottomLayout->addStretch();

        auto *btnApplySelected = new QPushButton(QStringLiteral("✓ Apply Selected"), &dlg);
        btnApplySelected->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));

        auto *btnApplyAll = new QPushButton(QStringLiteral("⚡ Apply All"), &dlg);
        btnApplyAll->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok")));

        auto *btnCancel = new QPushButton(QStringLiteral("Cancel"), &dlg);
        btnCancel->setIcon(QIcon::fromTheme(QStringLiteral("dialog-cancel")));

        bottomLayout->addWidget(btnApplySelected);
        bottomLayout->addWidget(btnApplyAll);
        bottomLayout->addWidget(btnCancel);
        mainLayout->addLayout(bottomLayout);

        bool updatingChecks = false;

        auto updateCountLabel = [&]() {
            int checked = 0;
            int total = 0;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    total++;
                    if (catItem->child(j)->checkState(0) == Qt::Checked) {
                        checked++;
                    }
                }
            }
            lblCount->setText(QStringLiteral("Selected: <b>%1</b> of %2 options").arg(checked).arg(total));
            btnApplySelected->setEnabled(checked > 0);
            btnApplySelected->setText(QStringLiteral("✓ Apply Selected (%1)").arg(checked));
        };
        updateCountLabel();

        // Handle item check changes
        QObject::connect(tree, &QTreeWidget::itemChanged, [&](QTreeWidgetItem *item, int col) {
            if (updatingChecks || col != 0) return;
            updatingChecks = true;

            if (item->parent() == nullptr) {
                // Category parent clicked
                const Qt::CheckState state = item->checkState(0);
                if (state != Qt::PartiallyChecked) {
                    for (int i = 0; i < item->childCount(); ++i) {
                        item->child(i)->setCheckState(0, state);
                    }
                }
            } else {
                // Child clicked: update parent tristate
                auto *parentItem = item->parent();
                int checked = 0;
                for (int i = 0; i < parentItem->childCount(); ++i) {
                    if (parentItem->child(i)->checkState(0) == Qt::Checked) {
                        checked++;
                    }
                }
                if (checked == 0) {
                    parentItem->setCheckState(0, Qt::Unchecked);
                } else if (checked == parentItem->childCount()) {
                    parentItem->setCheckState(0, Qt::Checked);
                } else {
                    parentItem->setCheckState(0, Qt::PartiallyChecked);
                }
            }

            updatingChecks = false;
            updateCountLabel();
        });

        // Quick toolbar connects
        QObject::connect(btnSelectAll, &QPushButton::clicked, [&]() {
            updatingChecks = true;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                catItem->setCheckState(0, Qt::Checked);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    catItem->child(j)->setCheckState(0, Qt::Checked);
                }
            }
            updatingChecks = false;
            updateCountLabel();
        });

        QObject::connect(btnDeselectAll, &QPushButton::clicked, [&]() {
            updatingChecks = true;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                catItem->setCheckState(0, Qt::Unchecked);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    catItem->child(j)->setCheckState(0, Qt::Unchecked);
                }
            }
            updatingChecks = false;
            updateCountLabel();
        });

        QObject::connect(btnOnlyDiff, &QPushButton::clicked, [&]() {
            updatingChecks = true;
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                int checked = 0;
                for (int j = 0; j < catItem->childCount(); ++j) {
                    auto *child = catItem->child(j);
                    const bool differs = child->data(0, Qt::UserRole + 1).toBool();
                    child->setCheckState(0, differs ? Qt::Checked : Qt::Unchecked);
                    if (differs) checked++;
                }
                if (checked == 0) {
                    catItem->setCheckState(0, Qt::Unchecked);
                } else if (checked == catItem->childCount()) {
                    catItem->setCheckState(0, Qt::Checked);
                } else {
                    catItem->setCheckState(0, Qt::PartiallyChecked);
                }
            }
            updatingChecks = false;
            updateCountLabel();
        });

        // Search filter connect
        QObject::connect(searchBox, &QLineEdit::textChanged, [&](const QString &text) {
            const QString query = text.trimmed();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                bool anyChildVisible = false;
                for (int j = 0; j < catItem->childCount(); ++j) {
                    auto *child = catItem->child(j);
                    const bool match = query.isEmpty() ||
                                       child->text(0).contains(query, Qt::CaseInsensitive) ||
                                       child->data(0, Qt::UserRole).toString().contains(query, Qt::CaseInsensitive);
                    child->setHidden(!match);
                    if (match) anyChildVisible = true;
                }
                const bool catMatch = catItem->text(0).contains(query, Qt::CaseInsensitive);
                catItem->setHidden(!anyChildVisible && !catMatch);
                if (!query.isEmpty() && (anyChildVisible || catMatch)) {
                    catItem->setExpanded(true);
                }
            }
        });

        bool confirmed = false;

        QObject::connect(btnApplyAll, &QPushButton::clicked, [&]() {
            outSettingsToApply = preset.settings;
            confirmed = true;
            dlg.accept();
        });

        QObject::connect(btnApplySelected, &QPushButton::clicked, [&]() {
            outSettingsToApply = QJsonObject();
            for (int i = 0; i < tree->topLevelItemCount(); ++i) {
                auto *catItem = tree->topLevelItem(i);
                for (int j = 0; j < catItem->childCount(); ++j) {
                    auto *child = catItem->child(j);
                    if (child->checkState(0) == Qt::Checked) {
                        const QString key = child->data(0, Qt::UserRole).toString();
                        if (preset.settings.contains(key)) {
                            outSettingsToApply[key] = preset.settings[key];
                        }
                    }
                }
            }
            if (outSettingsToApply.contains(QStringLiteral("ActiveCustomShaderId")) ||
                outSettingsToApply.contains(QStringLiteral("CustomShaderEnabled")) ||
                outSettingsToApply.contains(QStringLiteral("ShaderEffect"))) {
                if (preset.settings.contains(QStringLiteral("ShaderEffect"))) {
                    outSettingsToApply[QStringLiteral("ShaderEffect")] = preset.settings[QStringLiteral("ShaderEffect")];
                }
            }
            confirmed = true;
            dlg.accept();
        });

        QObject::connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);

        dlg.exec();
        return confirmed;
    }
};

} // namespace LeapK
