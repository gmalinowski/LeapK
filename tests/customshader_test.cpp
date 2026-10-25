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
    assert(list.size() == 8);

    // 1. Breathe / Pulsing
    assert(list[0].id == QStringLiteral("breathe_pulse"));
    assert(list[0].name == QStringLiteral("Breathe / Pulsing"));
    assert(list[0].target == QStringLiteral("both"));
    assert(list[0].isBuiltIn == true);
    assert(list[0].parameters.size() == 2);
    assert(list[0].parameters[0].id == QStringLiteral("speed"));
    assert(list[0].glslCode.contains(QStringLiteral("evaluateCustomEffect")));

    // 2. Futuristic Glitch & Hologram
    assert(list[1].id == QStringLiteral("futuristic_glitch"));
    assert(list[1].name == QStringLiteral("Futuristic Glitch & Hologram"));
    assert(list[1].target == QStringLiteral("both"));
    assert(list[1].parameters.size() == 4);
    assert(list[1].parameters[0].id == QStringLiteral("speed"));

    // 3. Neon Voltage Flicker
    assert(list[2].id == QStringLiteral("neon_flicker"));
    assert(list[2].name == QStringLiteral("Neon Voltage Flicker"));
    assert(list[2].target == QStringLiteral("both"));
    assert(list[2].parameters.size() == 3);
    assert(list[2].parameters[0].id == QStringLiteral("interval"));

    // 4. Flash / Strobe
    assert(list[3].id == QStringLiteral("flash_strobe"));
    assert(list[3].name == QStringLiteral("Flash / Strobe"));
    assert(list[3].target == QStringLiteral("both"));
    assert(list[3].parameters.size() == 2);
    assert(list[3].parameters[0].id == QStringLiteral("interval"));

    // 5. Rainbow Wave
    assert(list[4].id == QStringLiteral("rainbow_wave"));
    assert(list[4].name == QStringLiteral("Rainbow Wave"));
    assert(list[4].target == QStringLiteral("both"));
    assert(list[4].isBuiltIn == true);
    assert(list[4].parameters.size() == 4);
    assert(list[4].parameters[0].id == QStringLiteral("speed"));
    assert(list[4].glslCode.contains(QStringLiteral("evaluateCustomEffect")));

    // 6. Matrix Digital Rain
    assert(list[5].id == QStringLiteral("matrix_rain"));
    assert(list[5].name == QStringLiteral("Matrix Digital Rain"));
    assert(list[5].target == QStringLiteral("glow"));
    assert(list[5].parameters.size() == 4);
    assert(list[5].parameters[3].id == QStringLiteral("matrixColor"));
    assert(list[5].parameters[3].type == QStringLiteral("color"));

    // 7. Electric Plasma
    assert(list[6].id == QStringLiteral("electric_plasma"));
    assert(list[6].name == QStringLiteral("Electric Plasma"));
    assert(list[6].target == QStringLiteral("both"));
    assert(list[6].parameters.size() == 4);
    assert(list[6].parameters[0].id == QStringLiteral("voltage"));

    // 8. Fire Aura
    assert(list[7].id == QStringLiteral("fire_aura"));
    assert(list[7].name == QStringLiteral("Fire Aura"));
    assert(list[7].target == QStringLiteral("glow"));
    assert(list[7].parameters.size() == 4);
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
    const auto &breathe = list[0]; // breathe_pulse
    const auto &rainbow = list[4]; // rainbow_wave

    // Breathe pulse glow fragment shader
    const QByteArray breatheSource = LeapKShader::buildGlowFragmentSource(breathe, true, false);
    assert(breatheSource.contains("#version 140"));
    assert(breatheSource.contains("uniform float speed;"));
    assert(breatheSource.contains("uniform float intensity;"));
    assert(breatheSource.contains("evaluateCustomEffect(baseColor, position0, outerBox, distanceToOutline, time)"));

    // Rainbow wave glow fragment shader (modern desktop GL)
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

void testAllBuiltInShadersGLSLGeneration()
{
    const auto list = LeapKShader::getBuiltInShaders();
    assert(list.size() == 8);

    for (const auto &effect : list) {
        // Basic metadata
        assert(!effect.id.isEmpty());
        assert(!effect.name.isEmpty());
        assert(!effect.author.isEmpty());
        assert(!effect.version.isEmpty());
        assert(!effect.description.isEmpty());
        assert(effect.target == QStringLiteral("glow") ||
               effect.target == QStringLiteral("outline") ||
               effect.target == QStringLiteral("both"));

        // Validate GLSL code
        QString valErr;
        const bool codeValid = LeapKShader::validateShaderCode(effect.glslCode, effect.parameters, &valErr);
        if (!codeValid) {
            std::cerr << "Validation failed for shader " << effect.id.toStdString() << ": " << valErr.toStdString() << std::endl;
        }
        assert(codeValid);

        // Parameters validity
        for (const auto &p : effect.parameters) {
            assert(!p.id.isEmpty());
            assert(!p.name.isEmpty());
            if (p.type == QStringLiteral("float") || p.type == QStringLiteral("int")) {
                assert(p.minValue <= p.maxValue);
                assert(p.defaultValue >= p.minValue && p.defaultValue <= p.maxValue);
                assert(p.currentValue >= p.minValue && p.currentValue <= p.maxValue);
                assert(p.step > 0.0);
            } else if (p.type == QStringLiteral("color")) {
                assert(!p.colorDefault.isEmpty());
                assert(!p.colorCurrent.isEmpty());
            }
        }

        // Test modern desktop GL Glow shader
        const QByteArray glowDesktop = LeapKShader::buildGlowFragmentSource(effect, true, false);
        assert(glowDesktop.contains("#version 140"));
        assert(glowDesktop.contains("in vec2 position0;"));
        assert(glowDesktop.contains("out vec4 fragColor;"));
        assert(glowDesktop.contains("evaluateCustomEffect(baseColor, position0, outerBox, distanceToOutline, time)"));

        // Test modern GLES Glow shader
        const QByteArray glowGLES = LeapKShader::buildGlowFragmentSource(effect, true, true);
        assert(glowGLES.contains("#version 300 es"));
        assert(glowGLES.contains("precision highp float;"));

        // Test legacy desktop GL Glow shader
        const QByteArray glowLegacy = LeapKShader::buildGlowFragmentSource(effect, false, false);
        assert(glowLegacy.contains("varying vec2 position0;"));
        assert(glowLegacy.contains("#define fragColor gl_FragColor"));

        // Test modern desktop GL Outline shader
        const QByteArray outlineDesktop = LeapKShader::buildOutlineFragmentSource(effect, true, false);
        assert(outlineDesktop.contains("#version 140"));
        assert(outlineDesktop.contains("evaluateCustomEffect(baseColor, position0, outerBox, 0.0, time)"));

        // Test modern GLES Outline shader
        const QByteArray outlineGLES = LeapKShader::buildOutlineFragmentSource(effect, true, true);
        assert(outlineGLES.contains("#version 300 es"));
        assert(outlineGLES.contains("precision highp float;"));

        // Test legacy desktop GL Outline shader
        const QByteArray outlineLegacy = LeapKShader::buildOutlineFragmentSource(effect, false, false);
        assert(outlineLegacy.contains("varying vec2 position0;"));

        // Ensure all parameter uniforms are present in both shaders
        for (const auto &p : effect.parameters) {
            const QByteArray uniformName = p.id.toUtf8();
            assert(glowDesktop.contains(uniformName));
            assert(glowGLES.contains(uniformName));
            assert(glowLegacy.contains(uniformName));
            assert(outlineDesktop.contains(uniformName));
            assert(outlineGLES.contains(uniformName));
            assert(outlineLegacy.contains(uniformName));
        }
    }
}

void testUniformDeclarationsGeneration()
{
    QList<LeapKShader::ShaderParameter> params;

    LeapKShader::ShaderParameter pFloat;
    pFloat.id = QStringLiteral("myFloat");
    pFloat.type = QStringLiteral("float");
    params.append(pFloat);

    LeapKShader::ShaderParameter pInt;
    pInt.id = QStringLiteral("myInt");
    pInt.type = QStringLiteral("int");
    params.append(pInt);

    LeapKShader::ShaderParameter pBool;
    pBool.id = QStringLiteral("myBool");
    pBool.type = QStringLiteral("bool");
    params.append(pBool);

    LeapKShader::ShaderParameter pColor;
    pColor.id = QStringLiteral("myColor");
    pColor.type = QStringLiteral("color");
    params.append(pColor);

    const QByteArray decls = LeapKShader::generateUniformDeclarations(params);
    assert(decls.contains("uniform float myFloat;\n"));
    assert(decls.contains("uniform int myInt;\n"));
    assert(decls.contains("uniform float myBool;\n"));
    assert(decls.contains("uniform vec4 myColor;\n"));

    // Empty list produces empty declarations
    assert(LeapKShader::generateUniformDeclarations({}).isEmpty());
}

void testAllBuiltInsSerializationRoundTrip()
{
    const auto list = LeapKShader::getBuiltInShaders();
    for (const auto &orig : list) {
        const QByteArray json = LeapKShader::serializeShaderEffect(orig);
        assert(!json.isEmpty());

        LeapKShader::CustomShaderEffect parsed;
        QString err;
        const bool ok = LeapKShader::parseShaderEffect(json, parsed, &err);
        assert(ok);
        assert(err.isEmpty());

        assert(parsed.id == orig.id);
        assert(parsed.name == orig.name);
        assert(parsed.description == orig.description);
        assert(parsed.author == orig.author);
        assert(parsed.version == orig.version);
        assert(parsed.target == orig.target);
        assert(parsed.isBuiltIn == orig.isBuiltIn);
        assert(parsed.glslCode == orig.glslCode);
        assert(parsed.parameters.size() == orig.parameters.size());

        for (int i = 0; i < orig.parameters.size(); ++i) {
            const auto &pOrig = orig.parameters[i];
            const auto &pParsed = parsed.parameters[i];
            assert(pParsed.id == pOrig.id);
            assert(pParsed.name == pOrig.name);
            assert(pParsed.type == pOrig.type);
            assert(pParsed.minValue == pOrig.minValue);
            assert(pParsed.maxValue == pOrig.maxValue);
            assert(pParsed.step == pOrig.step);
            assert(pParsed.defaultValue == pOrig.defaultValue);
            assert(pParsed.currentValue == pOrig.currentValue);
            assert(pParsed.suffix == pOrig.suffix);
            assert(pParsed.colorDefault == pOrig.colorDefault);
            assert(pParsed.colorCurrent == pOrig.colorCurrent);
        }
    }
}

void testGlslValidationEdgeCases()
{
    QList<LeapKShader::ShaderParameter> params;
    LeapKShader::ShaderParameter p;
    p.id = QStringLiteral("alpha");
    p.type = QStringLiteral("float");
    params.append(p);

    QString err;
    // 1. Empty and whitespace
    assert(!LeapKShader::validateShaderCode(QString(), params, &err));
    assert(!LeapKShader::validateShaderCode(QStringLiteral("   \n\t  "), params, &err));

    // 2. Unbalanced braces
    assert(!LeapKShader::validateShaderCode(QStringLiteral("vec4 f() { return baseColor;"), params, &err));
    assert(!LeapKShader::validateShaderCode(QStringLiteral("vec4 f() { return baseColor; } }"), params, &err));

    // 3. Unbalanced parentheses
    assert(!LeapKShader::validateShaderCode(QStringLiteral("vec4 f( { return baseColor; }"), params, &err));
    assert(!LeapKShader::validateShaderCode(QStringLiteral("vec4 f) { return baseColor; }"), params, &err));

    // 4. Missing return
    assert(!LeapKShader::validateShaderCode(QStringLiteral("vec4 f() { float y = 2.0; }"), params, &err));

    // 5. Valid and invalid parameter names
    auto checkParamName = [&](const QString &id, bool expectedValid) {
        QList<LeapKShader::ShaderParameter> testParams;
        LeapKShader::ShaderParameter tp;
        tp.id = id;
        tp.type = QStringLiteral("float");
        testParams.append(tp);
        const bool res = LeapKShader::validateShaderCode(QStringLiteral("vec4 f() { return baseColor; }"), testParams, &err);
        assert(res == expectedValid);
    };

    checkParamName(QStringLiteral("validParam"), true);
    checkParamName(QStringLiteral("_valid_123"), true);
    checkParamName(QStringLiteral("param1"), true);
    checkParamName(QStringLiteral("123invalid"), false);
    checkParamName(QStringLiteral("has space"), false);
    checkParamName(QStringLiteral("has-hyphen"), false);
    checkParamName(QStringLiteral("has$dollar"), false);
}

void testFilePersistenceAndCorruptedJson()
{
    QTemporaryDir tempDir;
    assert(tempDir.isValid());

    const auto list = LeapKShader::getBuiltInShaders();
    for (const auto &effect : list) {
        const QString filePath = tempDir.path() + QStringLiteral("/%1.json").arg(effect.id);
        assert(LeapKShader::saveShaderEffectToFile(effect, filePath));
        assert(QFile::exists(filePath));

        LeapKShader::CustomShaderEffect reloaded;
        QString err;
        assert(LeapKShader::loadShaderEffectFromFile(filePath, reloaded, &err));
        assert(reloaded.id == effect.id);
        assert(reloaded.name == effect.name);
        assert(reloaded.parameters.size() == effect.parameters.size());
    }

    // Corrupted file 1: truncated JSON
    const QString truncPath = tempDir.path() + QStringLiteral("/truncated.json");
    {
        QFile f(truncPath);
        assert(f.open(QIODevice::WriteOnly));
        f.write("{ \"id\": \"truncated\", \"name\": \"Trunc");
    }
    LeapKShader::CustomShaderEffect effTrunc;
    QString errTrunc;
    assert(!LeapKShader::loadShaderEffectFromFile(truncPath, effTrunc, &errTrunc));
    assert(!errTrunc.isEmpty());

    // Corrupted file 2: non-object JSON array
    const QString arrayPath = tempDir.path() + QStringLiteral("/array.json");
    {
        QFile f(arrayPath);
        assert(f.open(QIODevice::WriteOnly));
        f.write("[1, 2, 3, 4]");
    }
    LeapKShader::CustomShaderEffect effArray;
    QString errArray;
    assert(!LeapKShader::loadShaderEffectFromFile(arrayPath, effArray, &errArray));

    // Corrupted file 3: empty file
    const QString emptyPath = tempDir.path() + QStringLiteral("/empty.json");
    {
        QFile f(emptyPath);
        assert(f.open(QIODevice::WriteOnly));
    }
    LeapKShader::CustomShaderEffect effEmpty;
    QString errEmpty;
    assert(!LeapKShader::loadShaderEffectFromFile(emptyPath, effEmpty, &errEmpty));

    // Non-existent file
    LeapKShader::CustomShaderEffect effMissing;
    assert(!LeapKShader::loadShaderEffectFromFile(tempDir.path() + QStringLiteral("/does_not_exist.json"), effMissing));
}

void testLoadShaderByIdFallback()
{
    const QStringList builtInIds = {
        QStringLiteral("breathe_pulse"),
        QStringLiteral("futuristic_glitch"),
        QStringLiteral("neon_flicker"),
        QStringLiteral("flash_strobe"),
        QStringLiteral("rainbow_wave"),
        QStringLiteral("matrix_rain"),
        QStringLiteral("electric_plasma"),
        QStringLiteral("fire_aura")
    };

    for (const auto &id : builtInIds) {
        LeapKShader::CustomShaderEffect effect;
        const bool found = LeapKShader::loadShaderById(id, effect);
        assert(found);
        assert(effect.id == id);
        assert(!effect.name.isEmpty());
    }

    // Invalid IDs
    LeapKShader::CustomShaderEffect out;
    assert(!LeapKShader::loadShaderById(QString(), out));
    assert(!LeapKShader::loadShaderById(QStringLiteral("non_existent_id_999"), out));
}

void testGlslMacroWrapperAndAutoWrapping()
{
    // 1. Using evaluateEffect macro definition
    LeapKShader::CustomShaderEffect macroEffect;
    macroEffect.id = QStringLiteral("macro_test");
    macroEffect.name = QStringLiteral("Macro Test");
    macroEffect.glslCode = QStringLiteral(
        "vec4 evaluateEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time) {\n"
        "    return vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n"
    );

    const QByteArray glowSource = LeapKShader::buildGlowFragmentSource(macroEffect, true, false);
    assert(glowSource.contains("#define evaluateCustomEffect evaluateEffect"));

    const QByteArray outlineSource = LeapKShader::buildOutlineFragmentSource(macroEffect, true, false);
    assert(outlineSource.contains("#define evaluateCustomEffect evaluateEffect"));

    // 2. Raw code without function signature
    LeapKShader::CustomShaderEffect rawEffect;
    rawEffect.id = QStringLiteral("raw_test");
    rawEffect.name = QStringLiteral("Raw Test");
    rawEffect.glslCode = QStringLiteral("return vec4(baseColor.rgb * 0.5, baseColor.a);\n");

    const QByteArray rawGlow = LeapKShader::buildGlowFragmentSource(rawEffect, true, false);
    assert(rawGlow.contains("vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)"));
    assert(rawGlow.contains("return vec4(baseColor.rgb * 0.5, baseColor.a);"));

    const QByteArray rawOutline = LeapKShader::buildOutlineFragmentSource(rawEffect, true, false);
    assert(rawOutline.contains("vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)"));
    assert(rawOutline.contains("return vec4(baseColor.rgb * 0.5, baseColor.a);"));
}

int main(int argc, char *argv[])
{
    Q_UNUSED(argc);
    Q_UNUSED(argv);

    testBuiltInShaders();
    testSerializationAndDeserialization();
    testAllBuiltInShadersGLSLGeneration();
    testUniformDeclarationsGeneration();
    testAllBuiltInsSerializationRoundTrip();
    testGlslValidationEdgeCases();
    testFilePersistenceAndCorruptedJson();
    testLoadShaderByIdFallback();
    testGlslMacroWrapperAndAutoWrapping();

    std::cout << "All custom shader effect tests passed successfully." << std::endl;
    return 0;
}
