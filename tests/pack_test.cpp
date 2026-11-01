/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "packhelper.h"
#include <cassert>
#include <iostream>
#include <QTemporaryDir>

static void testPackSerializationAndParsing()
{
    LeapKPack::PackBundle bundle;
    bundle.manifest.id = QStringLiteral("cyberpunk-experience");
    bundle.manifest.name = QStringLiteral("Cyberpunk 2077 Full Experience");
    bundle.manifest.version = QStringLiteral("1.2.0");
    bundle.manifest.author = QStringLiteral("Grzegorz Malinowski");
    bundle.manifest.description = QStringLiteral("High-octane neon synthwave experience with audio-reactive frame.");
    bundle.manifest.tags = {QStringLiteral("neon"), QStringLiteral("audio"), QStringLiteral("cyberpunk")};

    // 1. Theme
    bundle.theme = LeapKTheme::builtInThemes()[1]; // Cyberpunk
    bundle.hasTheme = true;

    // 2. Preset
    bundle.preset.name = QStringLiteral("Cyberpunk Twitch");
    bundle.preset.description = QStringLiteral("Snappy reaction preset");
    bundle.preset.settings[QStringLiteral("BorderWidth")] = 3;
    bundle.preset.settings[QStringLiteral("AudioShaderRuleEnabled")] = true;
    bundle.preset.settings[QStringLiteral("AudioShaderId")] = QStringLiteral("neon_audio_grid");
    bundle.hasPreset = true;

    // 3. Shaders
    LeapKShader::CustomShaderEffect sh;
    sh.id = QStringLiteral("neon_audio_grid");
    sh.name = QStringLiteral("Neon Audio Grid");
    sh.target = QStringLiteral("both");
    sh.glslCode = QStringLiteral("void main() { gl_FragColor = vec4(1.0, 0.0, 0.5, 1.0); }");
    bundle.shaders.append(sh);
    bundle.hasShaders = true;

    // 4. Shortcuts
    bundle.shortcuts[QStringLiteral("JumpToWindow")] = QStringLiteral("Meta+Space");
    bundle.shortcuts[QStringLiteral("MarkActiveWindowWithColor")] = QStringLiteral("Meta+Ctrl+Space");
    bundle.hasShortcuts = true;

    // Serialize
    const QByteArray json = LeapKPack::serializePack(bundle);
    assert(!json.isEmpty());

    // Parse back
    LeapKPack::PackBundle parsed;
    QString err;
    const bool ok = LeapKPack::parsePack(json, parsed, &err);
    assert(ok);
    assert(err.isEmpty());

    assert(parsed.manifest.id == QStringLiteral("cyberpunk-experience"));
    assert(parsed.manifest.name == QStringLiteral("Cyberpunk 2077 Full Experience"));
    assert(parsed.manifest.version == QStringLiteral("1.2.0"));
    assert(parsed.manifest.tags.size() == 3);
    assert(parsed.manifest.hasTheme == true);
    assert(parsed.manifest.hasPreset == true);
    assert(parsed.manifest.hasShaders == true);
    assert(parsed.manifest.hasShortcuts == true);

    assert(parsed.hasTheme);
    assert(parsed.theme.id == QStringLiteral("cyberpunk-neon"));
    assert(parsed.theme.hudContainer.shape == QStringLiteral("chamfered"));

    assert(parsed.hasPreset);
    assert(parsed.preset.name == QStringLiteral("Cyberpunk Twitch"));
    assert(parsed.preset.settings[QStringLiteral("AudioShaderRuleEnabled")].toBool() == true);

    assert(parsed.hasShaders);
    assert(parsed.shaders.size() == 1);
    assert(parsed.shaders[0].id == QStringLiteral("neon_audio_grid"));
    assert(parsed.shaders[0].name == QStringLiteral("Neon Audio Grid"));

    assert(parsed.hasShortcuts);
    assert(parsed.shortcuts.value(QStringLiteral("JumpToWindow")) == QStringLiteral("Meta+Space"));
}

static void testPackFileImportExport()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    const QString filePath = tempDir.filePath(QStringLiteral("test.leapkpack"));

    LeapKPack::PackBundle bundle;
    bundle.manifest.id = QStringLiteral("nordic-bundle");
    bundle.manifest.name = QStringLiteral("Nordic Frost Experience");
    bundle.theme = LeapKTheme::builtInThemes()[3]; // Nordic Frost
    bundle.hasTheme = true;

    QString err;
    assert(LeapKPack::exportPackToFile(filePath, bundle, &err));
    assert(QFile::exists(filePath));

    LeapKPack::PackBundle imported;
    assert(LeapKPack::importPackFromFile(filePath, imported, &err));
    assert(imported.manifest.id == QStringLiteral("nordic-bundle"));
    assert(imported.hasTheme);
    assert(imported.theme.id == QStringLiteral("nordic-frost"));
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testPackSerializationAndParsing();
    testPackFileImportExport();

    std::cout << "All pack tests passed successfully." << std::endl;
    return 0;
}
