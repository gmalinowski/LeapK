/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "presethelper.h"
#include <cassert>
#include <iostream>
#include <QTemporaryDir>

void testBuiltInPresets()
{
    const auto list = OutlinePreset::getBuiltInPresets();
    assert(list.size() == 9);

    // 1. Verify LeapK Default (v1.5)
    assert(list[0].name == QStringLiteral("LeapK Default (v1.5)"));
    assert(list[0].isBuiltIn == true);
    assert(list[0].settings[QStringLiteral("GlowEnabled")].toBool() == false);
    assert(list[0].settings[QStringLiteral("MenuHideDelay")].toInt() == 500);
    assert(list[0].settings[QStringLiteral("MenuShowDelay")].toInt() == 500);
    assert(list[0].settings[QStringLiteral("MarkSmartAdaptive")].toBool() == true);
    assert(list[0].settings[QStringLiteral("MarkRandomColor")].toBool() == false);
    assert(list[0].settings[QStringLiteral("BadgePosition")].toInt() == 1);
    assert(list[0].settings[QStringLiteral("BadgeMargin")].toInt() == 0);
    assert(list[0].settings[QStringLiteral("MarkInactiveWidthReduction")].toInt() == 1);
    assert(list[0].settings[QStringLiteral("MarkInactiveDashed")].toBool() == true);
    assert(list[0].settings[QStringLiteral("MarkInactiveDashSmooth")].toBool() == true);

    // 2. Verify Vim HUD Navigator
    assert(list[1].name == QStringLiteral("Vim HUD Navigator"));
    assert(list[1].settings[QStringLiteral("BadgeDisplayMode")].toInt() == 1);
    assert(list[1].settings[QStringLiteral("BadgeHudPosition")].toInt() == 0);
    assert(list[1].settings[QStringLiteral("JumpTargets")].toInt() == 2);
    assert(list[1].settings[QStringLiteral("HighlightRecentWindow")].toBool() == true);

    // 3. Verify Subtle Minimalist
    assert(list[2].name == QStringLiteral("Subtle Minimalist"));
    assert(list[2].settings[QStringLiteral("BorderWidth")].toInt() == 1);
    assert(list[2].settings[QStringLiteral("FadeDuration")].toInt() == 100);

    // 4. Verify Nordic Frost & Glow
    assert(list[3].name == QStringLiteral("Nordic Frost & Glow"));
    assert(list[3].settings[QStringLiteral("CustomColor")].toString() == QStringLiteral("#88c0d0"));
    assert(list[3].settings[QStringLiteral("GlowEnabled")].toBool() == true);
    assert(list[3].settings[QStringLiteral("BreatheEnabled")].toBool() == true);

    // 5. Verify Cyberpunk Neon 2077
    assert(list[4].name == QStringLiteral("Cyberpunk Neon 2077"));
    assert(list[4].settings[QStringLiteral("CustomColor")].toString() == QStringLiteral("#00f0ff"));
    assert(list[4].settings[QStringLiteral("GlowEnabled")].toBool() == true);
    assert(list[4].settings[QStringLiteral("GlitchEnabled")].toBool() == true);
    assert(list[4].settings[QStringLiteral("NeonFlickerEnabled")].toBool() == true);

    // 6. Verify Ambilight Dynamic Flow
    assert(list[5].name == QStringLiteral("Ambilight Dynamic Flow"));
    assert(list[5].settings[QStringLiteral("AmbilightEnabled")].toBool() == true);
    assert(list[5].settings[QStringLiteral("GlowEnabled")].toBool() == true);

    // 7. Verify Tiling Matrix Focus Trail
    assert(list[6].name == QStringLiteral("Tiling Matrix Focus Trail"));
    assert(list[6].settings[QStringLiteral("CustomColor")].toString() == QStringLiteral("#2ecc71"));
    assert(list[6].settings[QStringLiteral("HighlightRecentWindow")].toBool() == true);
    assert(list[6].settings[QStringLiteral("RecentWindowsCount")].toInt() == 3);

    // 8. Verify RGB Chroma Spectrum
    assert(list[7].name == QStringLiteral("RGB Chroma Spectrum"));
    assert(list[7].settings[QStringLiteral("ColorCycleEnabled")].toBool() == true);

    // 9. Verify High-Contrast Accessibility
    assert(list[8].name == QStringLiteral("High-Contrast Accessibility"));
    assert(list[8].settings[QStringLiteral("BorderWidth")].toInt() == 4);
    assert(list[8].settings[QStringLiteral("BadgesAlwaysVisible")].toBool() == true);
    assert(list[8].settings[QStringLiteral("BadgeSize")].toInt() == 34);
}

void testSerializationAndParsing()
{
    OutlinePreset::PresetInfo p;
    p.name = QStringLiteral("Custom Gaming");
    p.description = QStringLiteral("Vibrant gaming preset");
    p.isBuiltIn = false;
    p.settings[QStringLiteral("BorderWidth")] = 4;
    p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#ff0055");
    p.settings[QStringLiteral("GlowEnabled")] = true;

    const QByteArray json = OutlinePreset::serializePreset(p);
    assert(!json.isEmpty());

    OutlinePreset::PresetInfo parsed;
    QString err;
    const bool ok = OutlinePreset::parsePresetJson(json, parsed, &err);
    assert(ok);
    assert(parsed.name == p.name);
    assert(parsed.description == p.description);
    assert(parsed.isBuiltIn == false);
    assert(parsed.settings[QStringLiteral("BorderWidth")].toInt() == 4);
    assert(parsed.settings[QStringLiteral("CustomColor")].toString() == QStringLiteral("#ff0055"));
    assert(parsed.settings[QStringLiteral("GlowEnabled")].toBool() == true);
}

void testFlatJsonParsing()
{
    // Test parsing a flat JSON file (where settings are directly at root level)
    const QByteArray flatJson = R"json({
        "name": "Flat Preset",
        "description": "Direct root keys",
        "BorderWidth": 5,
        "GlowEnabled": false
    })json";

    OutlinePreset::PresetInfo parsed;
    const bool ok = OutlinePreset::parsePresetJson(flatJson, parsed);
    assert(ok);
    assert(parsed.name == QStringLiteral("Flat Preset"));
    assert(parsed.settings[QStringLiteral("BorderWidth")].toInt() == 5);
    assert(parsed.settings[QStringLiteral("GlowEnabled")].toBool() == false);
}

void testDirectoryLoadingAndSaving()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());

    // 1. Initial load should populate built-ins
    auto loaded = OutlinePreset::loadPresets(tempDir.path());
    assert(loaded.size() == 9);

    // 2. Add custom preset
    OutlinePreset::PresetInfo custom;
    custom.name = QStringLiteral("My Awesome Setup");
    custom.description = QStringLiteral("User custom");
    custom.isBuiltIn = false;
    custom.settings = OutlinePreset::getDefaultSettings();
    custom.settings[QStringLiteral("BorderWidth")] = 10;

    QString err;
    const bool saveOk = OutlinePreset::savePreset(tempDir.path(), custom, &err);
    assert(saveOk);

    // 3. Reload
    auto reloaded = OutlinePreset::loadPresets(tempDir.path());
    assert(reloaded.size() == 10);

    // Built-ins come first, then custom
    assert(reloaded.last().name == QStringLiteral("My Awesome Setup"));
    assert(reloaded.last().settings[QStringLiteral("BorderWidth")].toInt() == 10);
    assert(reloaded.last().isBuiltIn == false);
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testBuiltInPresets();
    testSerializationAndParsing();
    testFlatJsonParsing();
    testDirectoryLoadingAndSaving();

    std::cout << "All preset tests passed successfully." << std::endl;
    return 0;
}
