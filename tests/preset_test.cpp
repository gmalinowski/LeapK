/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "presethelper.h"
#include "presetactivationdialog.h"
#include "customshaderhelper.h"
#include <cassert>
#include <iostream>
#include <QSet>
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

void testPresetActivationCategories()
{
    const auto categories = LeapK::PresetActivationDialog::getCategories();
    QSet<QString> dialogKeys;
    for (const auto &cat : categories) {
        for (const auto &item : cat.items) {
            assert(!dialogKeys.contains(item.key));
            dialogKeys.insert(item.key);
        }
    }

    const auto defaults = OutlinePreset::getDefaultSettings();
    for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
        if (!dialogKeys.contains(it.key())) {
            std::cerr << "Missing key in PresetActivationDialog: " << it.key().toStdString() << std::endl;
            assert(false);
        }
    }
    // dialogKeys contains all default settings keys plus virtual 'ShaderEffect'
    assert(dialogKeys.size() == defaults.size() + 1);

    // Test formatting helper
    assert(!LeapK::PresetActivationDialog::formatValue(QStringLiteral("BorderWidth"), 4, true).isEmpty());
    assert(LeapK::PresetActivationDialog::areValuesEqual(4, 4));
    assert(!LeapK::PresetActivationDialog::areValuesEqual(4, 5));
    assert(LeapK::PresetActivationDialog::areValuesEqual(true, true));
    assert(!LeapK::PresetActivationDialog::areValuesEqual(true, false));
}

void testPresetEditingRenamingAndPersistence()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());

    // 1. Create a custom preset
    OutlinePreset::PresetInfo p;
    p.name = QStringLiteral("Initial Gaming");
    p.description = QStringLiteral("First version");
    p.isBuiltIn = false;
    p.settings = OutlinePreset::getDefaultSettings();
    p.settings[QStringLiteral("BorderWidth")] = 6;
    p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#e74c3c");

    QString err;
    assert(OutlinePreset::savePreset(tempDir.path(), p, &err));

    auto list = OutlinePreset::loadPresets(tempDir.path());
    assert(list.size() == 10);
    assert(list.last().name == QStringLiteral("Initial Gaming"));
    assert(list.last().description == QStringLiteral("First version"));
    assert(list.last().settings[QStringLiteral("BorderWidth")].toInt() == 6);
    const QString filePath = list.last().filePath;
    assert(!filePath.isEmpty());
    assert(QFile::exists(filePath));

    // 2. Edit name and description (renaming)
    const QString oldFilePath = filePath;
    OutlinePreset::PresetInfo edited = list.last();
    edited.name = QStringLiteral("Pro Gaming Elite");
    edited.description = QStringLiteral("Updated description with HDR glow");
    edited.settings[QStringLiteral("BorderWidth")] = 8;
    assert(OutlinePreset::savePreset(tempDir.path(), edited, &err));

    // Remove old file upon rename if filename changed
    if (QFile::exists(oldFilePath)) {
        QFile::remove(oldFilePath);
    }

    auto reloaded = OutlinePreset::loadPresets(tempDir.path());
    assert(reloaded.size() == 10);
    assert(reloaded.last().name == QStringLiteral("Pro Gaming Elite"));
    assert(reloaded.last().description == QStringLiteral("Updated description with HDR glow"));
    assert(reloaded.last().settings[QStringLiteral("BorderWidth")].toInt() == 8);

    // 3. Delete preset file
    assert(QFile::remove(reloaded.last().filePath));
    auto afterDelete = OutlinePreset::loadPresets(tempDir.path());
    assert(afterDelete.size() == 9); // only built-ins remain
}

void testPresetCorruptedFileHandling()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());

    // Corrupted 1: Invalid syntax
    {
        QFile f(tempDir.path() + QStringLiteral("/broken1.json"));
        assert(f.open(QIODevice::WriteOnly));
        f.write("{ \"name\": \"Broken\", \"settings\": { ");
    }

    // Corrupted 2: Array root
    {
        QFile f(tempDir.path() + QStringLiteral("/broken2.json"));
        assert(f.open(QIODevice::WriteOnly));
        f.write("[1, 2, 3]");
    }

    // Corrupted 3: Empty file
    {
        QFile f(tempDir.path() + QStringLiteral("/broken3.json"));
        assert(f.open(QIODevice::WriteOnly));
    }

    // loadPresets must safely ignore all 3 corrupted files without crashing
    auto loaded = OutlinePreset::loadPresets(tempDir.path());
    assert(loaded.size() == 9);

    // Directly calling parsePresetJson must return false and write error message
    OutlinePreset::PresetInfo parsed;
    QString err;
    assert(!OutlinePreset::parsePresetJson(QByteArray("{ broken }"), parsed, &err));
    assert(!err.isEmpty());
    assert(!OutlinePreset::parsePresetJson(QByteArray(), parsed, &err));
    assert(!OutlinePreset::parsePresetJson(QByteArray("[\"array\"]"), parsed, &err));
}

void testPresetSelectiveFiltering()
{
    const QJsonObject defaults = OutlinePreset::getDefaultSettings();
    QJsonObject current = defaults;
    current[QStringLiteral("BorderWidth")] = 2;
    current[QStringLiteral("CustomColor")] = QStringLiteral("#111111");
    current[QStringLiteral("GlowEnabled")] = false;
    current[QStringLiteral("ColorCycleEnabled")] = false;

    OutlinePreset::PresetInfo preset;
    preset.name = QStringLiteral("Vibrant Preset");
    preset.settings = defaults;
    preset.settings[QStringLiteral("BorderWidth")] = 8;
    preset.settings[QStringLiteral("CustomColor")] = QStringLiteral("#00f0ff");
    preset.settings[QStringLiteral("GlowEnabled")] = true;
    preset.settings[QStringLiteral("ColorCycleEnabled")] = true;

    // Simulate selecting only "Borders & Geometry" category keys
    const auto categories = LeapK::PresetActivationDialog::getCategories();
    QSet<QString> geometryKeys;
    for (const auto &item : categories[0].items) {
        geometryKeys.insert(item.key);
    }
    assert(!geometryKeys.isEmpty());

    QJsonObject applied = current;
    for (const auto &key : geometryKeys) {
        if (preset.settings.contains(key)) {
            applied[key] = preset.settings[key];
        }
    }

    // Geometry and glow keys must be updated (category 0)
    assert(applied[QStringLiteral("BorderWidth")].toInt() == 8);
    assert(applied[QStringLiteral("GlowEnabled")].toBool() == true);

    // Other categories must remain untouched
    assert(applied[QStringLiteral("CustomColor")].toString() == QStringLiteral("#111111"));
    assert(applied[QStringLiteral("ColorCycleEnabled")].toBool() == false);
    assert(applied[QStringLiteral("AmbilightEnabled")].toBool() == false);
}

void testPresetFormatValueAndAreValuesEqual()
{
    using LeapK::PresetActivationDialog;

    // Test areValuesEqual
    assert(PresetActivationDialog::areValuesEqual(10, 10));
    assert(!PresetActivationDialog::areValuesEqual(10, 12));
    assert(PresetActivationDialog::areValuesEqual(2.5, 2.5));
    assert(!PresetActivationDialog::areValuesEqual(2.5, 2.8));
    assert(PresetActivationDialog::areValuesEqual(true, true));
    assert(!PresetActivationDialog::areValuesEqual(true, false));

    // Colors: case-insensitive equality
    assert(PresetActivationDialog::areValuesEqual(QStringLiteral("#3daee9"), QStringLiteral("#3DAEE9")));
    assert(!PresetActivationDialog::areValuesEqual(QStringLiteral("#3daee9"), QStringLiteral("#ff0000")));

    // Strings
    assert(PresetActivationDialog::areValuesEqual(QStringLiteral("Alpha"), QStringLiteral("Alpha")));
    assert(!PresetActivationDialog::areValuesEqual(QStringLiteral("Alpha"), QStringLiteral("Beta")));

    // Test formatValue (Polish & English)
    assert(PresetActivationDialog::formatValue(QStringLiteral("BorderWidth"), 3, true) == QStringLiteral("3 px"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("BorderWidth"), 3, false) == QStringLiteral("3 px"));

    assert(PresetActivationDialog::formatValue(QStringLiteral("FadeDuration"), 250, true) == QStringLiteral("250 ms"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("FadeDuration"), 250, false) == QStringLiteral("250 ms"));

    assert(PresetActivationDialog::formatValue(QStringLiteral("BreatheSpeed"), 0.5, true) == QStringLiteral("0.50 Hz"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("GlitchSpeed"), 1.5, true) == QStringLiteral("1.50x"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("NeonFlickerInterval"), 4.0, true) == QStringLiteral("4.00 s"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("NeonFlickerIntensity"), 30, true) == QStringLiteral("30%"));

    // Enums
    assert(!PresetActivationDialog::formatValue(QStringLiteral("BadgeDisplayMode"), 0, true).isEmpty());
    assert(!PresetActivationDialog::formatValue(QStringLiteral("BadgeDisplayMode"), 1, true).isEmpty());
    assert(!PresetActivationDialog::formatValue(QStringLiteral("BadgeDisplayMode"), 2, true).isEmpty());
    assert(!PresetActivationDialog::formatValue(QStringLiteral("BadgePosition"), 1, true).isEmpty());
    assert(!PresetActivationDialog::formatValue(QStringLiteral("JumpTargets"), 0, true).isEmpty());
    assert(!PresetActivationDialog::formatValue(QStringLiteral("JumpTargets"), 1, false).isEmpty());

    // Booleans
    assert(PresetActivationDialog::formatValue(QStringLiteral("GlowEnabled"), true, true) == QStringLiteral("Włączone"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("GlowEnabled"), false, true) == QStringLiteral("Wyłączone"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("GlowEnabled"), true, false) == QStringLiteral("Enabled"));
    assert(PresetActivationDialog::formatValue(QStringLiteral("GlowEnabled"), false, false) == QStringLiteral("Disabled"));
}

void testAllBuiltInPresetsConsistency()
{
    const auto list = OutlinePreset::getBuiltInPresets();
    const auto categories = LeapK::PresetActivationDialog::getCategories();
    QSet<QString> validKeys;
    for (const auto &cat : categories) {
        for (const auto &item : cat.items) {
            validKeys.insert(item.key);
        }
    }

    for (const auto &p : list) {
        assert(!p.name.isEmpty());
        assert(!p.description.isEmpty());
        assert(p.isBuiltIn == true);
        assert(!p.settings.isEmpty());

        for (auto it = p.settings.constBegin(); it != p.settings.constEnd(); ++it) {
            if (!validKeys.contains(it.key())) {
                std::cerr << "Preset '" << p.name.toStdString() << "' has unregistered key: " << it.key().toStdString() << std::endl;
                assert(false);
            }
        }
    }
}

void testPresetWithEmbeddedShader()
{
    // 1. Create a shader effect (tested identically for built-ins and custom shaders)
    LeapKShader::CustomShaderEffect effect;
    effect.id = QStringLiteral("synthwave_sun");
    effect.name = QStringLiteral("Synthwave 80s Sun");
    effect.description = QStringLiteral("Retro 80s horizontal scanlines and glowing sunset.");
    effect.author = QStringLiteral("CyberArtist");
    effect.target = QStringLiteral("glow");
    effect.glslCode = QStringLiteral("void main() { gl_FragColor = vec4(1.0, 0.2, 0.5, 1.0); }");

    LeapKShader::ShaderParameter p1;
    p1.id = QStringLiteral("speed");
    p1.name = QStringLiteral("Sun Speed");
    p1.type = QStringLiteral("float");
    p1.currentValue = 0.85;
    effect.parameters.append(p1);

    LeapKShader::ShaderParameter p2;
    p2.id = QStringLiteral("sunColor");
    p2.name = QStringLiteral("Sun Color");
    p2.type = QStringLiteral("color");
    p2.colorCurrent = QStringLiteral("#ff007f");
    effect.parameters.append(p2);

    // 2. Package into a preset
    OutlinePreset::PresetInfo p;
    p.name = QStringLiteral("Outrun Synthwave");
    p.description = QStringLiteral("Complete 80s aesthetic with glowing grid and custom shader.");
    p.isBuiltIn = false;
    p.settings[QStringLiteral("BorderWidth")] = 3;
    p.settings[QStringLiteral("CustomColor")] = QStringLiteral("#ff007f");
    p.settings[QStringLiteral("CustomShaderEnabled")] = true;
    p.settings[QStringLiteral("ActiveCustomShaderId")] = effect.id;

    QJsonObject bundle = LeapKShader::createShaderBundle(effect.id, true, {effect});
    assert(!bundle.isEmpty());
    assert(bundle[QStringLiteral("id")].toString() == effect.id);
    assert(bundle[QStringLiteral("enabled")].toBool() == true);
    p.shader = bundle;
    p.settings[QStringLiteral("ShaderEffect")] = bundle;

    // 3. Serialize preset
    const QByteArray json = OutlinePreset::serializePreset(p);
    assert(!json.isEmpty());
    assert(json.contains("synthwave_sun"));
    assert(json.contains("Synthwave 80s Sun"));
    assert(json.contains("#ff007f"));

    // 4. Parse preset back
    OutlinePreset::PresetInfo parsed;
    QString err;
    assert(OutlinePreset::parsePresetJson(json, parsed, &err));
    assert(parsed.name == p.name);
    assert(!parsed.shader.isEmpty());
    assert(parsed.shader[QStringLiteral("id")].toString() == effect.id);

    // 5. Apply shader bundle to local list
    QList<LeapKShader::CustomShaderEffect> localShaders;
    const bool applied = LeapKShader::applyShaderBundle(parsed.shader, &localShaders);
    assert(applied);
    assert(localShaders.size() == 1);
    assert(localShaders[0].id == effect.id);
    assert(localShaders[0].parameters.size() == 2);
    assert(qFuzzyCompare(localShaders[0].parameters[0].currentValue, 0.85));
    assert(localShaders[0].parameters[1].colorCurrent == QStringLiteral("#ff007f"));

    // 6. Test updating an existing shader's parameters
    QJsonObject updatedBundle = parsed.shader;
    QJsonObject effectObj = updatedBundle[QStringLiteral("effect")].toObject();
    QJsonArray paramsArr = effectObj[QStringLiteral("parameters")].toArray();
    QJsonObject param0 = paramsArr[0].toObject();
    param0[QStringLiteral("value")] = 1.45;
    paramsArr[0] = param0;
    effectObj[QStringLiteral("parameters")] = paramsArr;
    updatedBundle[QStringLiteral("effect")] = effectObj;

    const bool reApplied = LeapKShader::applyShaderBundle(updatedBundle, &localShaders);
    assert(reApplied);
    assert(localShaders.size() == 1);
    assert(qFuzzyCompare(localShaders[0].parameters[0].currentValue, 1.45));

    // 7. Verify PresetActivationDialog formatting and equality
    assert(!LeapK::PresetActivationDialog::formatValue(QStringLiteral("ShaderEffect"), parsed.shader, true).isEmpty());
    assert(LeapK::PresetActivationDialog::areValuesEqual(parsed.shader, parsed.shader));
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testBuiltInPresets();
    testSerializationAndParsing();
    testFlatJsonParsing();
    testDirectoryLoadingAndSaving();
    testPresetActivationCategories();
    testPresetEditingRenamingAndPersistence();
    testPresetCorruptedFileHandling();
    testPresetSelectiveFiltering();
    testPresetFormatValueAndAreValuesEqual();
    testAllBuiltInPresetsConsistency();
    testPresetWithEmbeddedShader();

    std::cout << "All preset tests passed successfully." << std::endl;
    return 0;
}
