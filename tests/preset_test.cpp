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
    assert(list.size() == 5);

    // Verify Default (v1.5)
    assert(list[0].name == QStringLiteral("Default (v1.5)"));
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

    // Verify Subtle Minimal
    assert(list[1].name == QStringLiteral("Subtle Minimal"));
    assert(list[1].settings[QStringLiteral("BorderWidth")].toInt() == 1);

    // Verify Cyberpunk Neon
    assert(list[2].name == QStringLiteral("Cyberpunk Neon"));
    assert(list[2].settings[QStringLiteral("GlowEnabled")].toBool() == true);
    assert(list[2].settings[QStringLiteral("GlitchEnabled")].toBool() == true);

    // Verify Vim Focus Navigator
    assert(list[3].name == QStringLiteral("Vim Focus Navigator"));
    assert(list[3].settings[QStringLiteral("BadgePosition")].toInt() == 1);
    assert(list[3].settings[QStringLiteral("BadgeDisplayMode")].toInt() == 1);

    // Verify Ambilight Flow
    assert(list[4].name == QStringLiteral("Ambilight Flow"));
    assert(list[4].settings[QStringLiteral("AmbilightEnabled")].toBool() == true);
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
    assert(loaded.size() == 5);

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
    assert(reloaded.size() == 6);

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
