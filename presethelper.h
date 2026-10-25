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

    return s;
}

inline QList<PresetInfo> getBuiltInPresets()
{
    QList<PresetInfo> list;

    // 1. Default (v1.5)
    {
        PresetInfo p;
        p.name = QStringLiteral("Default (v1.5)");
        p.description = QStringLiteral("Standard balanced appearance with top badges, subtle dashed marks and neutral timing.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        list.append(p);
    }

    // 2. Subtle Minimal
    {
        PresetInfo p;
        p.name = QStringLiteral("Subtle Minimal");
        p.description = QStringLiteral("Ultra-thin 1px border, compact 4px rounded corners, fast 100ms fade, no glow or animations.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("BorderWidth")] = 1;
        p.settings[QStringLiteral("BorderRadius")] = 4;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("FadeDuration")] = 100;
        p.settings[QStringLiteral("MenuHideDelay")] = 300;
        p.settings[QStringLiteral("MenuShowDelay")] = 300;
        p.settings[QStringLiteral("AccentOpacity")] = 85;
        p.settings[QStringLiteral("MarkInactiveWidthReduction")] = 0;
        p.settings[QStringLiteral("MarkInactiveDashGap")] = 4;
        p.settings[QStringLiteral("MarkInactiveDashSmooth")] = false;
        p.settings[QStringLiteral("BadgeSize")] = 24;
        list.append(p);
    }

    // 3. Cyberpunk Neon
    {
        PresetInfo p;
        p.name = QStringLiteral("Cyberpunk Neon");
        p.description = QStringLiteral("Electric cyan outline with glowing edge, glitch anomaly, and neon flickering effects.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("UseAccentColor")] = false;
        p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#00f0ff");
        p.settings[QStringLiteral("BorderWidth")] = 3;
        p.settings[QStringLiteral("BorderRadius")] = 2;
        p.settings[QStringLiteral("FadeCorners")] = false;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 12;
        p.settings[QStringLiteral("GlitchEnabled")] = true;
        p.settings[QStringLiteral("GlitchSpeed")] = 1.5;
        p.settings[QStringLiteral("GlitchIntensity")] = 45;
        p.settings[QStringLiteral("GlitchAnomalyEnabled")] = true;
        p.settings[QStringLiteral("GlitchFrequency")] = 2;
        p.settings[QStringLiteral("NeonFlickerEnabled")] = true;
        p.settings[QStringLiteral("NeonFlickerInterval")] = 2.5;
        p.settings[QStringLiteral("NeonFlickerDuration")] = 0.20;
        p.settings[QStringLiteral("NeonFlickerIntensity")] = 40;
        list.append(p);
    }

    // 4. Vim Focus Navigator
    {
        PresetInfo p;
        p.name = QStringLiteral("Vim Focus Navigator");
        p.description = QStringLiteral("High-visibility letter badges always shown at window tops for instant keyboard jumping.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("BorderWidth")] = 2;
        p.settings[QStringLiteral("BorderRadius")] = 4;
        p.settings[QStringLiteral("AccentOpacity")] = 75;
        p.settings[QStringLiteral("MarkShowLetterBadges")] = true;
        p.settings[QStringLiteral("BadgesAlwaysVisible")] = true;
        p.settings[QStringLiteral("BadgeDisplayMode")] = 1;
        p.settings[QStringLiteral("BadgePosition")] = 1;
        p.settings[QStringLiteral("BadgeMargin")] = 0;
        p.settings[QStringLiteral("BadgeSize")] = 28;
        p.settings[QStringLiteral("JumpTargets")] = 0;
        list.append(p);
    }

    // 5. Ambilight Flow
    {
        PresetInfo p;
        p.name = QStringLiteral("Ambilight Flow");
        p.description = QStringLiteral("Adaptive edge lighting dynamically sampling window content colors in real time.");
        p.isBuiltIn = true;
        p.settings = getDefaultSettings();
        p.settings[QStringLiteral("AmbilightEnabled")] = true;
        p.settings[QStringLiteral("AmbilightMode")] = 0;
        p.settings[QStringLiteral("AmbilightOpacity")] = 100;
        p.settings[QStringLiteral("AmbilightFrequency")] = 2.5;
        p.settings[QStringLiteral("AmbilightDuration")] = 0.3;
        p.settings[QStringLiteral("AmbilightSaturationLeveling")] = 70;
        p.settings[QStringLiteral("AmbilightFallbackStrength")] = 60;
        p.settings[QStringLiteral("AmbilightMinBrightness")] = 25;
        p.settings[QStringLiteral("AmbilightMaxBrightness")] = 85;
        p.settings[QStringLiteral("EnsurePerimeterContrast")] = true;
        p.settings[QStringLiteral("BorderWidth")] = 3;
        p.settings[QStringLiteral("BorderRadius")] = 8;
        p.settings[QStringLiteral("FadeCorners")] = true;
        p.settings[QStringLiteral("GlowEnabled")] = true;
        p.settings[QStringLiteral("GlowRadius")] = 8;
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
    root[QStringLiteral("version")] = QStringLiteral("1.5.0");
    root[QStringLiteral("settings")] = preset.settings;

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
                k != QLatin1String("version")) {
                s[k] = it.value();
            }
        }
        outPreset.settings = s;
    }

    if (outPreset.settings.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No valid outline settings found in preset file.");
        }
        return false;
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

    const QList<PresetInfo> builtIns = getBuiltInPresets();
    for (const auto &preset : builtIns) {
        QString safeName = preset.name;
        safeName.replace(QLatin1Char('/'), QLatin1Char('_'));
        const QString filePath = dir.filePath(safeName + QStringLiteral(".json"));
        if (!QFile::exists(filePath)) {
            savePreset(dirPath, preset);
        }
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
