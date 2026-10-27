/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QStandardPaths>
#include <QString>

namespace OutlinePreset {

struct PresetInfo {
    QString name;
    QString description;
    bool isBuiltIn = false;
    QString filePath;
    QJsonObject settings;
    QJsonObject shader;
};

inline QJsonObject getDefaultSettings()
{
    QJsonObject s;
    // General
    s[QStringLiteral("UseAccentColor")] = true;
    s[QStringLiteral("AccentOpacity")] = 66;
    s[QStringLiteral("CustomColor")] = QStringLiteral("#3daee9");
    s[QStringLiteral("EnsurePerimeterContrast")] = true;
    s[QStringLiteral("AmbilightEnabled")] = false;
    s[QStringLiteral("AmbilightMode")] = 0;
    s[QStringLiteral("AmbilightOpacity")] = 100;
    s[QStringLiteral("AmbilightFrequency")] = 1.5;
    s[QStringLiteral("AmbilightDuration")] = 0.5;
    s[QStringLiteral("AmbilightSaturationLeveling")] = 65;
    s[QStringLiteral("AmbilightFallbackStrength")] = 50;
    s[QStringLiteral("AmbilightMinBrightness")] = 25;
    s[QStringLiteral("AmbilightMaxBrightness")] = 85;
    s[QStringLiteral("AmbilightEnsureContrast")] = true;
    s[QStringLiteral("BorderWidth")] = 2;
    s[QStringLiteral("OutlineGap")] = 0;
    s[QStringLiteral("BorderRadius")] = 6;
    s[QStringLiteral("FadeCorners")] = true;
    s[QStringLiteral("DrawInside")] = true;
    s[QStringLiteral("SmoothEdges")] = true;
    s[QStringLiteral("GlowEnabled")] = false;
    s[QStringLiteral("GlowRadius")] = 6;
    s[QStringLiteral("HideOnFullScreen")] = true;
    s[QStringLiteral("TrayIconEnabled")] = true;

    // Behavior & Buffer Delays
    s[QStringLiteral("FadeDuration")] = 200;
    s[QStringLiteral("MenuHideDelay")] = 500;
    s[QStringLiteral("MenuShowDelay")] = 500;
    s[QStringLiteral("WindowHideDelay")] = 0;
    s[QStringLiteral("WindowShowDelay")] = 0;
    s[QStringLiteral("HighlightRecentWindow")] = false;
    s[QStringLiteral("RecentWindowsCount")] = 1;
    s[QStringLiteral("RecentWindowColorMode")] = 0;
    s[QStringLiteral("RecentWindowCustomColor")] = QStringLiteral("#7f8c8d");
    s[QStringLiteral("RecentWindowOpacity")] = 20;
    s[QStringLiteral("RecentWindowHueShift")] = 0;
    s[QStringLiteral("RecentWindowSaturation")] = 0;
    s[QStringLiteral("RecentWindowWidthReduction")] = 0;
    s[QStringLiteral("RecentWindowDashed")] = false;
    s[QStringLiteral("RecentWindowGapGrowth")] = 6;
    s[QStringLiteral("RecentWindowDashSmooth")] = false;

    // Mark Window
    s[QStringLiteral("MarkDefaultColor")] = QStringLiteral("#e74c3c");
    s[QStringLiteral("MarkSmartAdaptive")] = true;
    s[QStringLiteral("MarkRandomColor")] = false;
    s[QStringLiteral("StylePrecedence")] = QStringLiteral("Marked,Active,Recent");
    s[QStringLiteral("MarkInactiveWidthReduction")] = 1;
    s[QStringLiteral("MarkInactiveDashed")] = true;
    s[QStringLiteral("MarkInactiveDashGap")] = 6;
    s[QStringLiteral("MarkInactiveDashSmooth")] = true;
    s[QStringLiteral("MarkShowLetterBadges")] = true;
    s[QStringLiteral("BadgesAlwaysVisible")] = false;
    s[QStringLiteral("BadgeDisplayMode")] = 0;
    s[QStringLiteral("BadgePosition")] = 1;
    s[QStringLiteral("BadgeHudPosition")] = 0;
    s[QStringLiteral("BadgeSize")] = 28;
    s[QStringLiteral("BadgeMargin")] = 0;
    s[QStringLiteral("BadgeShowIcon")] = false;
    s[QStringLiteral("BadgeShowThumbnail")] = false;
    s[QStringLiteral("BadgeJumpSize")] = 34;
    s[QStringLiteral("BadgeHudSize")] = 36;
    s[QStringLiteral("BadgeIconSize")] = 20;
    s[QStringLiteral("BadgeThumbnailWidth")] = 56;
    s[QStringLiteral("BadgeThumbnailHeight")] = 32;
    s[QStringLiteral("BadgeCornerRadius")] = 6;
    s[QStringLiteral("BadgeOpacity")] = 92;
    s[QStringLiteral("BadgeHudMarkColorMode")] = 0;
    s[QStringLiteral("BadgeDefaultColor")] = QStringLiteral("#232629");
    s[QStringLiteral("BadgeLetterColorAuto")] = true;
    s[QStringLiteral("BadgeLetterColor")] = QStringLiteral("#ffffff");
    s[QStringLiteral("BadgeLabelColorAuto")] = true;
    s[QStringLiteral("BadgeLabelUnmarkedColor")] = QStringLiteral("#f1c40f");
    s[QStringLiteral("BadgeCaptionColorAuto")] = true;
    s[QStringLiteral("BadgeCaptionColor")] = QStringLiteral("#dce1e6");
    s[QStringLiteral("BadgeLocationColorAuto")] = true;
    s[QStringLiteral("BadgeLocationColor")] = QStringLiteral("#f1c40f");
    s[QStringLiteral("BadgeLetterFont")] = QString();
    s[QStringLiteral("BadgeLabelFont")] = QString();
    s[QStringLiteral("BadgeCaptionFont")] = QString();
    s[QStringLiteral("BadgeLocationFont")] = QString();
    s[QStringLiteral("BadgeLetterFontSize")] = 0;
    s[QStringLiteral("BadgeLabelFontSize")] = 0;
    s[QStringLiteral("BadgeCaptionFontSize")] = 0;
    s[QStringLiteral("BadgeLocationFontSize")] = 0;
    s[QStringLiteral("BadgeWindowShowIcon")] = false;
    s[QStringLiteral("BadgeWindowShowThumbnail")] = false;
    s[QStringLiteral("BadgeWindowIconSize")] = 20;
    s[QStringLiteral("BadgeWindowThumbnailWidth")] = 56;
    s[QStringLiteral("BadgeWindowThumbnailHeight")] = 32;
    s[QStringLiteral("BadgeWindowThumbnailSize")] = 56;
    s[QStringLiteral("BadgeWindowLetterFontSize")] = 0;
    s[QStringLiteral("BadgeWindowLabelFontSize")] = 0;
    s[QStringLiteral("BadgeHudShowIcon")] = true;
    s[QStringLiteral("BadgeHudShowThumbnail")] = true;
    s[QStringLiteral("BadgeHudIconSize")] = 24;
    s[QStringLiteral("BadgeHudThumbnailWidth")] = 80;
    s[QStringLiteral("BadgeHudThumbnailHeight")] = 48;
    s[QStringLiteral("BadgeHudThumbnailSize")] = 80;
    s[QStringLiteral("BadgeHudLetterFontSize")] = 0;
    s[QStringLiteral("BadgeHudCaptionFontSize")] = 0;
    s[QStringLiteral("BadgeHudLocationFontSize")] = 0;
    s[QStringLiteral("BadgeHudIncludeAll")] = false;
    s[QStringLiteral("BadgeTimeout")] = 5;
    s[QStringLiteral("JumpRecentKey")] = QStringLiteral("Space");
    s[QStringLiteral("BadgeLivePreviews")] = false;
    s[QStringLiteral("JumpTargets")] = 0;

    // Color Cycling
    s[QStringLiteral("ColorCycleEnabled")] = false;
    s[QStringLiteral("ColorTransitionDuration")] = 2.0;
    s[QStringLiteral("ColorCycleOrder")] = 0;
    s[QStringLiteral("ColorCycleEasing")] = 0;
    s[QStringLiteral("ColorList")] = QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0");

    // Shader Effects
    s[QStringLiteral("BreatheEnabled")] = false;
    s[QStringLiteral("BreatheSpeed")] = 0.5;
    s[QStringLiteral("BreatheIntensity")] = 40;
    s[QStringLiteral("GlitchEnabled")] = false;
    s[QStringLiteral("GlitchSpeed")] = 1.0;
    s[QStringLiteral("GlitchIntensity")] = 50;
    s[QStringLiteral("GlitchAnomalyEnabled")] = false;
    s[QStringLiteral("GlitchFrequency")] = 3;
    s[QStringLiteral("NeonFlickerEnabled")] = false;
    s[QStringLiteral("NeonFlickerInterval")] = 3.0;
    s[QStringLiteral("NeonFlickerDuration")] = 0.25;
    s[QStringLiteral("NeonFlickerIntensity")] = 30;
    s[QStringLiteral("FlashEnabled")] = false;
    s[QStringLiteral("FlashInterval")] = 4.0;
    s[QStringLiteral("FlashDuration")] = 0.15;
    s[QStringLiteral("CustomShaderEnabled")] = false;
    s[QStringLiteral("ActiveCustomShaderId")] = QString();

    return s;
}

inline QList<PresetInfo> getBuiltInPresets()
{
    QList<PresetInfo> list;

    // 1. LeapK Default (v1.6)
    {
        PresetInfo p;
        p.name = QStringLiteral("LeapK Default (v1.6)");
        p.description = QStringLiteral("Standard balanced daily driver with 2px KDE accent outline, top jump badges, bottom HUD preview cards, and Space jump.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("BorderWidth")] = 2;
        p.settings[QStringLiteral("BorderRadius")] = 6;
        p.settings[QStringLiteral("AccentOpacity")] = 80;
        p.settings[QStringLiteral("EnsurePerimeterContrast")] = true;
        p.settings[QStringLiteral("FadeCorners")] = true;
        p.settings[QStringLiteral("BadgeDisplayMode")] = 0; // Modal Jump Mode
        p.settings[QStringLiteral("BadgePosition")] = 1; // Top
        p.settings[QStringLiteral("BadgeHudPosition")] = 0; // Bottom
        p.settings[QStringLiteral("BadgeTimeout")] = 5;
        p.settings[QStringLiteral("JumpRecentKey")] = QStringLiteral("Space");
        p.settings[QStringLiteral("HighlightRecentWindow")] = true;
        p.settings[QStringLiteral("RecentWindowsCount")] = 1;
        p.settings[QStringLiteral("RecentWindowOpacity")] = 25;
        p.settings[QStringLiteral("RecentWindowDashed")] = true;
        p.settings[QStringLiteral("BadgeHudShowIcon")] = true;
        p.settings[QStringLiteral("BadgeHudShowThumbnail")] = true;
        list.append(p);
    }

    // 2. Vim Motion & HUD Center
    {
        PresetInfo p;
        p.name = QStringLiteral("Vim Motion & HUD Center");
        p.description = QStringLiteral("Command center for Vim/i3 tiling power users: background letter badges, all windows mapped in HUD, multi-level focus trail, zero latency.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#2ecc71");
        p.settings[QStringLiteral("BorderWidth")] = 2;
        p.settings[QStringLiteral("BorderRadius")] = 4;
        p.settings[QStringLiteral("DrawInside")] = true;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("FadeDuration")] = 50;
        p.settings[QStringLiteral("BadgeDisplayMode")] = 1; // Always visible on background windows
        p.settings[QStringLiteral("BadgeHudPosition")] = 0; // Bottom
        p.settings[QStringLiteral("BadgeHudIncludeAll")] = true; // All open windows in HUD
        p.settings[QStringLiteral("JumpTargets")] = 2; // All virtual desktops
        p.settings[QStringLiteral("BadgeTimeout")] = 0; // No timeout
        p.settings[QStringLiteral("BadgeWindowShowIcon")] = true;
        p.settings[QStringLiteral("BadgeWindowShowThumbnail")] = false;
        p.settings[QStringLiteral("HighlightRecentWindow")] = true;
        p.settings[QStringLiteral("RecentWindowsCount")] = 3;
        p.settings[QStringLiteral("RecentWindowColorMode")] = 0;
        p.settings[QStringLiteral("RecentWindowOpacity")] = 30;
        p.settings[QStringLiteral("RecentWindowDashed")] = true;
        p.settings[QStringLiteral("RecentWindowGapGrowth")] = 6;
        p.settings[QStringLiteral("RecentWindowDashSmooth")] = true;
        list.append(p);
    }

    // 3. Live Vision Dashboard
    {
        PresetInfo p;
        p.name = QStringLiteral("Live Vision Dashboard");
        p.description = QStringLiteral("Multitasking workstation with continuous real-time live window thumbnails (30-60 fps), full-size cards in HUD, and ambient glow.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#3498db");
        p.settings[QStringLiteral("BorderWidth")] = 3;
        p.settings[QStringLiteral("BorderRadius")] = 8;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 8;
        p.settings[QStringLiteral("BadgeLivePreviews")] = true; // Real-time video/app previews!
        p.settings[QStringLiteral("BadgeWindowShowIcon")] = true;
        p.settings[QStringLiteral("BadgeWindowShowThumbnail")] = true;
        p.settings[QStringLiteral("BadgeWindowThumbnailWidth")] = 96;
        p.settings[QStringLiteral("BadgeWindowThumbnailHeight")] = 54;
        p.settings[QStringLiteral("BadgeHudShowIcon")] = true;
        p.settings[QStringLiteral("BadgeHudShowThumbnail")] = true;
        p.settings[QStringLiteral("BadgeHudThumbnailWidth")] = 140;
        p.settings[QStringLiteral("BadgeHudThumbnailHeight")] = 80;
        p.settings[QStringLiteral("BadgeHudIncludeAll")] = true;
        p.settings[QStringLiteral("BadgeHudSize")] = 44;
        p.settings[QStringLiteral("BadgeSize")] = 34;
        p.settings[QStringLiteral("BadgePosition")] = 7; // Bottom
        p.settings[QStringLiteral("BadgeHudPosition")] = 0; // Bottom
        list.append(p);
    }

    // 4. Nordic Frost & Aura
    {
        PresetInfo p;
        p.name = QStringLiteral("Nordic Frost & Aura");
        p.description = QStringLiteral("Calming Scandinavian Nord aesthetic with 4px floating offset border, arctic ice cyan (#88c0d0), breathing pulse, and soft aura.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#88c0d0");
        p.settings[QStringLiteral("BorderWidth")] = 2;
        p.settings[QStringLiteral("OutlineGap")] = 4; // Floating gap!
        p.settings[QStringLiteral("BorderRadius")] = 8;
        p.settings[QStringLiteral("FadeCorners")] = true;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 10;
        p.settings[QStringLiteral("BreatheEnabled")] = true;
        p.settings[QStringLiteral("BreatheSpeed")] = 0.35;
        p.settings[QStringLiteral("BreatheIntensity")] = 28;
        p.settings[QStringLiteral("FadeDuration")] = 250;
        p.settings[QStringLiteral("BadgeCornerRadius")] = 8;
        p.settings[QStringLiteral("BadgePosition")] = 4; // Center
        p.settings[QStringLiteral("BadgeDefaultColor")] = QStringLiteral("#2e3440");
        list.append(p);
    }

    // 5. Cyberpunk Neon 2077
    {
        PresetInfo p;
        p.name = QStringLiteral("Cyberpunk Neon 2077");
        p.description = QStringLiteral("High-voltage cyberpunk outline: electric cyan (#00f0ff), massive 16px glow, digital glitch noise, anomalies, and neon flickering.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#00f0ff");
        p.settings[QStringLiteral("BorderWidth")] = 3;
        p.settings[QStringLiteral("BorderRadius")] = 2;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 16;
        p.settings[QStringLiteral("GlitchEnabled")] = true;
        p.settings[QStringLiteral("GlitchSpeed")] = 1.4;
        p.settings[QStringLiteral("GlitchIntensity")] = 45;
        p.settings[QStringLiteral("GlitchAnomalyEnabled")] = true;
        p.settings[QStringLiteral("GlitchFrequency")] = 2;
        p.settings[QStringLiteral("NeonFlickerEnabled")] = true;
        p.settings[QStringLiteral("NeonFlickerInterval")] = 2.5;
        p.settings[QStringLiteral("NeonFlickerDuration")] = 0.20;
        p.settings[QStringLiteral("NeonFlickerIntensity")] = 40;
        p.settings[QStringLiteral("MarkDefaultColor")] = QStringLiteral("#ff007f");
        p.settings[QStringLiteral("BadgeDefaultColor")] = QStringLiteral("#0d1117");
        p.settings[QStringLiteral("BadgeCornerRadius")] = 2;
        list.append(p);
    }

    // 6. Ambilight Screen Cinema
    {
        PresetInfo p;
        p.name = QStringLiteral("Ambilight Screen Cinema");
        p.description = QStringLiteral("Cinema reactive edge lighting: live color sampling from window contents, smooth transitions, and ambient matching perimeter glow.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("AmbilightEnabled")] = true;
        p.settings[QStringLiteral("AmbilightMode")] = 0;
        p.settings[QStringLiteral("AmbilightOpacity")] = 100;
        p.settings[QStringLiteral("AmbilightFrequency")] = 3.0;
        p.settings[QStringLiteral("AmbilightDuration")] = 0.25;
        p.settings[QStringLiteral("AmbilightSaturationLeveling")] = 75;
        p.settings[QStringLiteral("AmbilightFallbackStrength")] = 60;
        p.settings[QStringLiteral("AmbilightMinBrightness")] = 25;
        p.settings[QStringLiteral("AmbilightMaxBrightness")] = 85;
        p.settings[QStringLiteral("EnsurePerimeterContrast")] = true;
        p.settings[QStringLiteral("BorderWidth")] = 3;
        p.settings[QStringLiteral("BorderRadius")] = 8;
        p.settings[QStringLiteral("FadeCorners")] = true;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 12;
        p.settings[QStringLiteral("FadeDuration")] = 250;
        list.append(p);
    }

    // 7. macOS Sonoma Minimal
    {
        PresetInfo p;
        p.name = QStringLiteral("macOS Sonoma Minimal");
        p.description = QStringLiteral("Clean, understated modern minimalism: ultra-thin 1px crisp outline, generous 10px rounded corners, no glow or delay, compact pill badges.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("BorderWidth")] = 1;
        p.settings[QStringLiteral("BorderRadius")] = 10;
        p.settings[QStringLiteral("FadeCorners")] = true;
        p.settings[QStringLiteral("UseAccentColor")] = true;
        p.settings[QStringLiteral("AccentOpacity")] = 75;
        p.settings[QStringLiteral("FadeDuration")] = 80;
        p.settings[QStringLiteral("GlowEnabled")] = false;
        p.settings[QStringLiteral("BreatheEnabled")] = false;
        p.settings[QStringLiteral("BadgeSize")] = 24;
        p.settings[QStringLiteral("BadgeCornerRadius")] = 12; // Pill shape
        p.settings[QStringLiteral("BadgePosition")] = 2; // TopRight
        p.settings[QStringLiteral("BadgeWindowShowThumbnail")] = false;
        p.settings[QStringLiteral("BadgeHudShowThumbnail")] = false;
        p.settings[QStringLiteral("MenuHideDelay")] = 200;
        p.settings[QStringLiteral("MenuShowDelay")] = 200;
        list.append(p);
    }

    // 8. Tiling Matrix Trail
    {
        PresetInfo p;
        p.name = QStringLiteral("Tiling Matrix Trail");
        p.description = QStringLiteral("Strict tiling geometry: sharp emerald (#00ff66) with zero corner radius, 4-step focus breadcrumb trail, and left-docked vertical HUD.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#00ff66");
        p.settings[QStringLiteral("BorderWidth")] = 2;
        p.settings[QStringLiteral("BorderRadius")] = 0; // True sharp tiling corners!
        p.settings[QStringLiteral("DrawInside")] = true;
        p.settings[QStringLiteral("OutlineGap")] = 0;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("HighlightRecentWindow")] = true;
        p.settings[QStringLiteral("RecentWindowsCount")] = 4;
        p.settings[QStringLiteral("RecentWindowColorMode")] = 1;
        p.settings[QStringLiteral("RecentWindowCustomColor")] = QStringLiteral("#16a085");
        p.settings[QStringLiteral("RecentWindowOpacity")] = 40;
        p.settings[QStringLiteral("RecentWindowWidthReduction")] = 1;
        p.settings[QStringLiteral("RecentWindowDashed")] = true;
        p.settings[QStringLiteral("RecentWindowGapGrowth")] = 8;
        p.settings[QStringLiteral("RecentWindowDashSmooth")] = true;
        p.settings[QStringLiteral("BadgeHudPosition")] = 2; // Left vertical edge HUD
        p.settings[QStringLiteral("BadgePosition")] = 0; // TopLeft
        p.settings[QStringLiteral("BadgeCornerRadius")] = 0;
        list.append(p);
    }

    // 9. RGB Chroma Spectrum
    {
        PresetInfo p;
        p.name = QStringLiteral("RGB Chroma Spectrum");
        p.description = QStringLiteral("Dynamic animated rainbow wave for gamers: continuous smooth chromatic color cycling across spectrum hues with 10px glow.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("ColorCycleEnabled")] = true;
        p.settings[QStringLiteral("ColorTransitionDuration")] = 1.2;
        p.settings[QStringLiteral("ColorList")] = QStringLiteral("#ff0055:2.0;#ff9900:2.0;#00ffcc:2.0;#3388ff:2.0;#cc00ff:2.0");
        p.settings[QStringLiteral("BorderWidth")] = 3;
        p.settings[QStringLiteral("BorderRadius")] = 8;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 10;
        p.settings[QStringLiteral("AccentOpacity")] = 100;
        list.append(p);
    }

    // 10. High-Contrast Amber A11y
    {
        PresetInfo p;
        p.name = QStringLiteral("High-Contrast Amber A11y");
        p.description = QStringLiteral("Maximum visibility and cognitive clarity: bold 5px safety amber border (#f39c12), full 100% opacity, enlarged typography, and persistent badges.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#f39c12");
        p.settings[QStringLiteral("AccentOpacity")] = 100;
        p.settings[QStringLiteral("EnsurePerimeterContrast")] = true;
        p.settings[QStringLiteral("BorderWidth")] = 5;
        p.settings[QStringLiteral("BorderRadius")] = 4;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("BadgesAlwaysVisible")] = true;
        p.settings[QStringLiteral("BadgeDisplayMode")] = 1;
        p.settings[QStringLiteral("BadgePosition")] = 1; // Top
        p.settings[QStringLiteral("BadgeSize")] = 36;
        p.settings[QStringLiteral("BadgeJumpSize")] = 40;
        p.settings[QStringLiteral("BadgeLetterFontSize")] = 18;
        p.settings[QStringLiteral("JumpTargets")] = 1;
        p.settings[QStringLiteral("MarkInactiveWidthReduction")] = 0;
        p.settings[QStringLiteral("MarkInactiveDashed")] = true;
        p.settings[QStringLiteral("MarkInactiveDashGap")] = 4;
        p.settings[QStringLiteral("BadgeTimeout")] = 0; // No timeout
        list.append(p);
    }

    // 11. Retro CRT Amber Phosphor
    {
        PresetInfo p;
        p.name = QStringLiteral("Retro CRT Amber Phosphor");
        p.description = QStringLiteral("Vintage 1980s mainframe terminal: warm amber phosphor (#ffb000), subtle CRT flicker, gentle breathing, square corners, and top-docked HUD.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#ffb000");
        p.settings[QStringLiteral("AccentOpacity")] = 90;
        p.settings[QStringLiteral("BorderWidth")] = 2;
        p.settings[QStringLiteral("BorderRadius")] = 1;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("NeonFlickerEnabled")] = true;
        p.settings[QStringLiteral("NeonFlickerInterval")] = 4.0;
        p.settings[QStringLiteral("NeonFlickerDuration")] = 0.15;
        p.settings[QStringLiteral("NeonFlickerIntensity")] = 15;
        p.settings[QStringLiteral("BreatheEnabled")] = true;
        p.settings[QStringLiteral("BreatheSpeed")] = 0.25;
        p.settings[QStringLiteral("BreatheIntensity")] = 15;
        p.settings[QStringLiteral("BadgeHudPosition")] = 1; // Top screen edge HUD
        p.settings[QStringLiteral("BadgePosition")] = 1; // Top
        p.settings[QStringLiteral("BadgeCornerRadius")] = 1;
        p.settings[QStringLiteral("BadgeDefaultColor")] = QStringLiteral("#1a1200");
        p.settings[QStringLiteral("BadgeLetterColorAuto")] = false;
        p.settings[QStringLiteral("BadgeLetterColor")] = QStringLiteral("#ffb000");
        list.append(p);
    }

    // 12. Focus Zen Distraction-Free
    {
        PresetInfo p;
        p.name = QStringLiteral("Focus Zen Distraction-Free");
        p.description = QStringLiteral("Deep focus and writing: discreet 1px graphite outline with gentle 35% opacity, zero animations or glow, jump badges strictly on demand with fast auto-hide.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#7f8c8d");
        p.settings[QStringLiteral("AccentOpacity")] = 35;
        p.settings[QStringLiteral("BorderWidth")] = 1;
        p.settings[QStringLiteral("BorderRadius")] = 6;
        p.settings[QStringLiteral("FadeCorners")] = true;
        p.settings[QStringLiteral("GlowEnabled")] = false;
        p.settings[QStringLiteral("BreatheEnabled")] = false;
        p.settings[QStringLiteral("GlitchEnabled")] = false;
        p.settings[QStringLiteral("NeonFlickerEnabled")] = false;
        p.settings[QStringLiteral("FadeDuration")] = 400; // Soft gentle transition
        p.settings[QStringLiteral("BadgeDisplayMode")] = 0; // Only in Jump Mode
        p.settings[QStringLiteral("BadgeTimeout")] = 3; // 3-second auto-hide
        p.settings[QStringLiteral("BadgeWindowShowThumbnail")] = false;
        p.settings[QStringLiteral("BadgeHudShowThumbnail")] = false;
        p.settings[QStringLiteral("BadgeSize")] = 26;
        p.settings[QStringLiteral("BadgeHudSize")] = 30;
        list.append(p);
    }

    return list;
}

inline QString defaultPresetsDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString path = base + QStringLiteral("/kwin-active-window-outline/presets");
    QDir().mkpath(path);
    return path;
}

inline QByteArray serializePreset(const PresetInfo &preset)
{
    QJsonObject root;
    root[QStringLiteral("name")] = preset.name;
    root[QStringLiteral("description")] = preset.description;
    root[QStringLiteral("isBuiltIn")] = preset.isBuiltIn;
    root[QStringLiteral("version")] = QStringLiteral("1.6.0");
    root[QStringLiteral("settings")] = preset.settings;

    if (!preset.shader.isEmpty()) {
        root[QStringLiteral("shader")] = preset.shader;
    } else if (preset.settings.contains(QStringLiteral("ShaderEffect"))) {
        root[QStringLiteral("shader")] = preset.settings.value(QStringLiteral("ShaderEffect")).toObject();
    }

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented);
}

inline bool parsePresetJson(const QByteArray &data, PresetInfo &outPreset, QString *errorMessage = nullptr)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid JSON document: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();

    if (root.contains(QStringLiteral("name"))) {
        outPreset.name = root.value(QStringLiteral("name")).toString();
    }
    if (root.contains(QStringLiteral("description"))) {
        outPreset.description = root.value(QStringLiteral("description")).toString();
    }
    if (root.contains(QStringLiteral("isBuiltIn"))) {
        outPreset.isBuiltIn = root.value(QStringLiteral("isBuiltIn")).toBool();
    }

    // Settings can be nested under "settings" or directly at root level
    if (root.contains(QStringLiteral("settings")) && root.value(QStringLiteral("settings")).isObject()) {
        outPreset.settings = root.value(QStringLiteral("settings")).toObject();
    } else {
        // Collect all keys except metadata
        QJsonObject s;
        for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
            const QString k = it.key();
            if (k != QLatin1String("name") &&
                k != QLatin1String("description") &&
                k != QLatin1String("isBuiltIn") &&
                k != QLatin1String("version") &&
                k != QLatin1String("shader")) {
                s[k] = it.value();
            }
        }
        outPreset.settings = s;
    }

    // Shader bundle support (root level "shader" or nested in settings)
    if (root.contains(QStringLiteral("shader")) && root.value(QStringLiteral("shader")).isObject()) {
        outPreset.shader = root.value(QStringLiteral("shader")).toObject();
        outPreset.settings[QStringLiteral("ShaderEffect")] = outPreset.shader;
    } else if (outPreset.settings.contains(QStringLiteral("ShaderEffect")) && outPreset.settings.value(QStringLiteral("ShaderEffect")).isObject()) {
        outPreset.shader = outPreset.settings.value(QStringLiteral("ShaderEffect")).toObject();
    }

    if (outPreset.settings.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No valid outline settings found in preset file.");
        }
        return false;
    }

    // Ensure all modern settings have defined defaults if missing from older preset files
    const QJsonObject defaults = getDefaultSettings();
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        if (!outPreset.settings.contains(it.key())) {
            outPreset.settings.insert(it.key(), it.value());
        }
    }

    return true;
}

inline bool savePreset(const QString &dirPath, const PresetInfo &preset, QString *errorMessage = nullptr)
{
    QDir dir(dirPath);
    if (!dir.exists() && !dir.mkpath(dirPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create presets directory: %1").arg(dirPath);
        }
        return false;
    }

    QString safeName = preset.name;
    safeName.replace(QLatin1Char('/'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('\\'), QLatin1Char('_'));
    safeName.replace(QLatin1Char(':'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('*'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('?'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('"'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('<'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('>'), QLatin1Char('_'));
    safeName.replace(QLatin1Char('|'), QLatin1Char('_'));

    const QString filePath = dir.filePath(safeName + QStringLiteral(".json"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to open file for writing: %1 (%2)").arg(filePath, file.errorString());
        }
        return false;
    }

    file.write(serializePreset(preset));
    file.close();
    return true;
}

inline void ensureBuiltInPresets(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(dirPath);
    }

    // Clean up stale built-in preset filenames from older development versions
    const QStringList legacyBuiltInFiles = {
        QStringLiteral("Default (v1.5).json"),
        QStringLiteral("KLeap Default (v1.5).json"),
        QStringLiteral("Vim Focus Navigator.json"),
        QStringLiteral("Subtle Minimal.json"),
        QStringLiteral("Cyberpunk Neon.json"),
        QStringLiteral("Ambilight Flow.json")
    };
    for (const QString &oldFile : legacyBuiltInFiles) {
        const QString p = dir.filePath(oldFile);
        if (QFile::exists(p)) {
            QFile::remove(p);
        }
    }

    const QList<PresetInfo> builtIns = getBuiltInPresets();
    for (const auto &preset : builtIns) {
        savePreset(dirPath, preset);
    }
}

inline QList<PresetInfo> loadPresets(const QString &dirPath)
{
    ensureBuiltInPresets(dirPath);

    QList<PresetInfo> result;
    QDir dir(dirPath);
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);

    // Keep track of names to avoid duplicates
    QSet<QString> seenNames;

    for (const QString &file : files) {
        const QString fullPath = dir.filePath(file);
        QFile f(fullPath);
        if (f.open(QIODevice::ReadOnly)) {
            PresetInfo p;
            p.filePath = fullPath;
            if (parsePresetJson(f.readAll(), p)) {
                if (p.name.isEmpty()) {
                    p.name = QFileInfo(file).baseName();
                }
                p.filePath = fullPath;
                if (!seenNames.contains(p.name)) {
                    seenNames.insert(p.name);
                    result.append(p);
                }
            }
        }
    }

    // Sort built-ins first, then custom alphabetically
    std::sort(result.begin(), result.end(), [](const PresetInfo &a, const PresetInfo &b) {
        if (a.isBuiltIn != b.isBuiltIn) {
            return a.isBuiltIn > b.isBuiltIn;
        }
        return a.name.localeAwareCompare(b.name) < 0;
    });

    return result;
}

} // namespace OutlinePreset
