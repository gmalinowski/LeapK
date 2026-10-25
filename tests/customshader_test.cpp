/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "customshaderhelper.h"
#include <cassert>
#include <iostream>
#include <QTemporaryDir>

void testBuiltInShaders()
{
    const auto list = LeapKShader::getBuiltInShaders();
    assert(list.size() == 4);

    // 1. Rainbow Wave
    assert(list[0].id == QStringLiteral("rainbow_wave"));
    assert(list[0].name == QStringLiteral("Rainbow Wave"));
    assert(list[0].target == QStringLiteral("glow"));
    assert(list[0].isBuiltIn == true);
    assert(list[0].parameters.size() == 4);
    assert(list[0].parameters[0].id == QStringLiteral("speed"));
    assert(list[0].glslCode.contains(QStringLiteral("evaluateCustomEffect")));

    // 2. Matrix Digital Rain
    assert(list[1].id == QStringLiteral("matrix_rain"));
    assert(list[1].name == QStringLiteral("Matrix Digital Rain"));
    assert(list[1].target == QStringLiteral("glow"));
    assert(list[1].parameters.size() == 4);
    assert(list[1].parameters[3].id == QStringLiteral("matrixColor"));
    assert(list[1].parameters[3].type == QStringLiteral("color"));

    // 3. Electric Plasma
    assert(list[2].id == QStringLiteral("electric_plasma"));
    assert(list[2].name == QStringLiteral("Electric Plasma"));
    assert(list[2].target == QStringLiteral("both"));
    assert(list[2].parameters.size() == 4);
    assert(list[2].parameters[0].id == QStringLiteral("voltage"));

    // 4. Fire Aura
    assert(list[3].id == QStringLiteral("fire_aura"));
    assert(list[3].name == QStringLiteral("Fire Aura"));
    assert(list[3].target == QStringLiteral("glow"));
    assert(list[3].parameters.size() == 4);
}

void testSerializationAndDeserialization()
{
    LeapKShader::CustomShaderEffect e;
    e.id = QStringLiteral("cyber_scan");
    e.name = QStringLiteral("Cyberpunk Grid Scanner");
    e.description = QStringLiteral("High tech holographic scan lines");
    e.author = QStringLiteral("Neo");
    e.version = QStringLiteral("2.1");
    e.target = QStringLiteral("both");

    LeapKShader::ShaderParameter p1;
    p1.id = QStringLiteral("scanSpeed");
    p1.name = QStringLiteral("Scan Speed");
    p1.type = QStringLiteral("float");
    p1.minValue = 0.1;
    p1.maxValue = 10.0;
    p1.step = 0.2;
    p1.defaultValue = 2.0;
    p1.currentValue = 3.5;
    p1.suffix = QStringLiteral("x");
    e.parameters.append(p1);

    LeapKShader::ShaderParameter p2;
    p2.id = QStringLiteral("scanColor");
    p2.name = QStringLiteral("Beam Color");
    p2.type = QStringLiteral("color");
    p2.colorDefault = QStringLiteral("#00e5ff");
    p2.colorCurrent = QStringLiteral("#ff0077");
    e.parameters.append(p2);

    e.glslCode = QStringLiteral(
        "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time) {\n"
        "    return vec4(scanColor.rgb, baseColor.a);\n"
        "}\n"
    );

    const QByteArray json = LeapKShader::serializeShaderEffect(e);
    assert(!json.isEmpty());

    LeapKShader::CustomShaderEffect parsed;
    QString err;
    const bool ok = LeapKShader::parseShaderEffect(json, parsed, &err);
    assert(ok);
    assert(parsed.id == e.id);
    assert(parsed.name == e.name);
    assert(parsed.description == e.description);
    assert(parsed.author == e.author);
    assert(parsed.version == e.version);
    assert(parsed.target == e.target);
    assert(parsed.parameters.size() == 2);
    assert(parsed.parameters[0].id == QStringLiteral("scanSpeed"));
    assert(parsed.parameters[0].currentValue == 3.5);
    assert(parsed.parameters[1].id == QStringLiteral("scanColor"));
    assert(parsed.parameters[1].colorCurrent == QStringLiteral("#ff0077"));
    assert(parsed.glslCode == e.glslCode);
}

void testGlslValidation()
{
    QList<LeapKShader::ShaderParameter> params;
    LeapKShader::ShaderParameter p;
    p.id = QStringLiteral("freq");
    p.type = QStringLiteral("float");
    params.append(p);

    QString err;

    // 1. Valid code
    const QString validCode = QStringLiteral("vec4 evaluateCustomEffect(...) { return baseColor; }");
    assert(LeapKShader::validateShaderCode(validCode, params, &err));

    // 2. Empty code
    assert(!LeapKShader::validateShaderCode(QString(), params, &err));

    // 3. Unmatched braces
    const QString badBraces = QStringLiteral("vec4 evaluateCustomEffect(...) { return baseColor;");
    assert(!LeapKShader::validateShaderCode(badBraces, params, &err));

    // 4. Missing return
    const QString noReturn = QStringLiteral("vec4 evaluateCustomEffect(...) { float x = 1.0; }");
    assert(!LeapKShader::validateShaderCode(noReturn, params, &err));

    // 5. Invalid parameter identifier
    QList<LeapKShader::ShaderParameter> badParams;
    LeapKShader::ShaderParameter badP;
    badP.id = QStringLiteral("123bad-name");
    badParams.append(badP);
    assert(!LeapKShader::validateShaderCode(validCode, badParams, &err));
}

void testShaderGeneration()
{
    const auto list = LeapKShader::getBuiltInShaders();
    const auto &rainbow = list[0];

    // Glow fragment shader (modern desktop GL)
    const QByteArray glowSource = LeapKShader::buildGlowFragmentSource(rainbow, true, false);
    assert(glowSource.contains("#version 140"));
    assert(glowSource.contains("in vec2 position0;"));
    assert(glowSource.contains("out vec4 fragColor;"));
    assert(glowSource.contains("uniform float speed;"));
    assert(glowSource.contains("uniform float waveFrequency;"));
    assert(glowSource.contains("uniform float saturation;"));
    assert(glowSource.contains("uniform float brightness;"));
    assert(glowSource.contains("hsv2rgb"));
    assert(glowSource.contains("roundedBoxDistance"));
    assert(glowSource.contains("evaluateCustomEffect(baseColor, position0, outerBox, distanceToOutline, time)"));

    // Outline fragment shader (modern OpenGL ES)
    const QByteArray outlineSource = LeapKShader::buildOutlineFragmentSource(rainbow, true, true);
    assert(outlineSource.contains("#version 300 es"));
    assert(outlineSource.contains("precision highp float;"));
    assert(outlineSource.contains("uniform float speed;"));
    assert(outlineSource.contains("evaluateCustomEffect(baseColor, position0, outerBox, 0.0, time)"));
}

void testFilePersistence()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());

    const auto list = LeapKShader::getBuiltInShaders();
    const auto &fire = list[3];

    const QString filePath = tempDir.path() + QStringLiteral("/fire_test.json");
    assert(LeapKShader::saveShaderEffectToFile(fire, filePath));

    LeapKShader::CustomShaderEffect reloaded;
    QString err;
    assert(LeapKShader::loadShaderEffectFromFile(filePath, reloaded, &err));
    assert(reloaded.id == fire.id);
    assert(reloaded.name == fire.name);
    assert(reloaded.parameters.size() == fire.parameters.size());
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testBuiltInShaders();
    testSerializationAndDeserialization();
    testGlslValidation();
    testShaderGeneration();
    testFilePersistence();

    std::cout << "All custom shader effect tests passed successfully." << std::endl;
    return 0;
}
