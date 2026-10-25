/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QVector4D>
#include <QtGlobal>

namespace LeapKShader {

struct ShaderParameter {
    QString id;           // GLSL uniform identifier (e.g. "speed", "waveFreq", "matrixColor")
    QString name;         // Human-readable label (e.g. "Drop Speed")
    QString type;         // "float", "int", "bool", "color"
    double minValue = 0.0;
    double maxValue = 1.0;
    double step = 0.05;
    double defaultValue = 0.5;
    double currentValue = 0.5;
    QString suffix;       // e.g. " Hz", "x", "%", " px"
    QString colorDefault = QStringLiteral("#00f0ff");
    QString colorCurrent = QStringLiteral("#00f0ff");

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("type")] = type;
        obj[QStringLiteral("min")] = minValue;
        obj[QStringLiteral("max")] = maxValue;
        obj[QStringLiteral("step")] = step;
        obj[QStringLiteral("default")] = defaultValue;
        obj[QStringLiteral("value")] = currentValue;
        obj[QStringLiteral("suffix")] = suffix;
        obj[QStringLiteral("colorDefault")] = colorDefault;
        obj[QStringLiteral("colorValue")] = colorCurrent;
        return obj;
    }

    static ShaderParameter fromJson(const QJsonObject &obj)
    {
        ShaderParameter p;
        p.id = obj.value(QStringLiteral("id")).toString();
        p.name = obj.value(QStringLiteral("name")).toString(p.id);
        p.type = obj.value(QStringLiteral("type")).toString(QStringLiteral("float"));
        p.minValue = obj.value(QStringLiteral("min")).toDouble(0.0);
        p.maxValue = obj.value(QStringLiteral("max")).toDouble(1.0);
        p.step = obj.value(QStringLiteral("step")).toDouble(0.05);
        p.defaultValue = obj.value(QStringLiteral("default")).toDouble(0.5);
        p.currentValue = obj.contains(QStringLiteral("value"))
            ? obj.value(QStringLiteral("value")).toDouble(p.defaultValue)
            : p.defaultValue;
        p.suffix = obj.value(QStringLiteral("suffix")).toString();
        p.colorDefault = obj.value(QStringLiteral("colorDefault")).toString(QStringLiteral("#00f0ff"));
        p.colorCurrent = obj.contains(QStringLiteral("colorValue"))
            ? obj.value(QStringLiteral("colorValue")).toString(p.colorDefault)
            : p.colorDefault;
        return p;
    }
};

struct CustomShaderEffect {
    QString id;
    QString name;
    QString description;
    QString author;
    QString version = QStringLiteral("1.0");
    bool isBuiltIn = false;
    bool enabled = false;
    QString target = QStringLiteral("glow"); // "glow", "outline", or "both"
    QList<ShaderParameter> parameters;
    QString glslCode;
    QString filePath;

    QJsonObject toJson() const
    {
        QJsonObject root;
        root[QStringLiteral("format")] = QStringLiteral("leapk-shader");
        root[QStringLiteral("version")] = version;
        root[QStringLiteral("id")] = id;
        root[QStringLiteral("name")] = name;
        root[QStringLiteral("description")] = description;
        root[QStringLiteral("author")] = author;
        root[QStringLiteral("isBuiltIn")] = isBuiltIn;
        root[QStringLiteral("enabled")] = enabled;
        root[QStringLiteral("target")] = target;

        QJsonArray paramArray;
        for (const auto &p : parameters) {
            paramArray.append(p.toJson());
        }
        root[QStringLiteral("parameters")] = paramArray;
        root[QStringLiteral("glsl")] = glslCode;
        return root;
    }

    static CustomShaderEffect fromJson(const QJsonObject &root)
    {
        CustomShaderEffect effect;
        effect.id = root.value(QStringLiteral("id")).toString();
        effect.name = root.value(QStringLiteral("name")).toString(effect.id);
        effect.description = root.value(QStringLiteral("description")).toString();
        effect.author = root.value(QStringLiteral("author")).toString();
        effect.version = root.value(QStringLiteral("version")).toString(QStringLiteral("1.0"));
        effect.isBuiltIn = root.value(QStringLiteral("isBuiltIn")).toBool(false);
        effect.enabled = root.value(QStringLiteral("enabled")).toBool(false);
        effect.target = root.value(QStringLiteral("target")).toString(QStringLiteral("glow"));
        effect.glslCode = root.value(QStringLiteral("glsl")).toString();

        const QJsonArray paramArray = root.value(QStringLiteral("parameters")).toArray();
        for (const auto &val : paramArray) {
            if (val.isObject()) {
                effect.parameters.append(ShaderParameter::fromJson(val.toObject()));
            }
        }
        return effect;
    }
};

inline QString defaultShadersDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const QString path = base + QStringLiteral("/kwin-active-window-outline/shaders");
    QDir().mkpath(path);
    return path;
}

inline QByteArray serializeShaderEffect(const CustomShaderEffect &effect)
{
    const QJsonObject json = effect.toJson();
    const QJsonDocument doc(json);
    return doc.toJson(QJsonDocument::Indented);
}

inline bool parseShaderEffect(const QByteArray &data, CustomShaderEffect &outEffect, QString *errorMessage = nullptr)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull() || !doc.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Invalid JSON: %1").arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("name")) && !root.contains(QStringLiteral("id"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Missing 'name' or 'id' field in shader effect JSON");
        }
        return false;
    }

    outEffect = CustomShaderEffect::fromJson(root);
    return true;
}

inline bool saveShaderEffectToFile(const CustomShaderEffect &effect, const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const QByteArray data = serializeShaderEffect(effect);
    file.write(data);
    file.close();
    return true;
}

inline bool loadShaderEffectFromFile(const QString &path, CustomShaderEffect &outEffect, QString *errorMessage = nullptr)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not open file: %1").arg(path);
        }
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    const bool ok = parseShaderEffect(data, outEffect, errorMessage);
    if (ok) {
        outEffect.filePath = path;
    }
    return ok;
}

inline QList<CustomShaderEffect> getBuiltInShaders()
{
    QList<CustomShaderEffect> list;

    // 1. Breathe / Pulsing
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("breathe_pulse");
        e.name = QStringLiteral("Breathe / Pulsing");
        e.description = QStringLiteral("Smooth rhythmic breathing luminance pulse.");
        e.author = QStringLiteral("LeapK Built-in");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("both");

        ShaderParameter p1;
        p1.id = QStringLiteral("speed");
        p1.name = QStringLiteral("Pulse Speed");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.01;
        p1.maxValue = 10.0;
        p1.step = 0.05;
        p1.defaultValue = 0.5;
        p1.currentValue = 0.5;
        p1.suffix = QStringLiteral(" Hz");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("intensity");
        p2.name = QStringLiteral("Pulse Intensity");
        p2.type = QStringLiteral("float");
        p2.minValue = 10.0;
        p2.maxValue = 100.0;
        p2.step = 5.0;
        p2.defaultValue = 40.0;
        p2.currentValue = 40.0;
        p2.suffix = QStringLiteral(" %");
        e.parameters.append(p2);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float depth = clamp(intensity, 10.0, 100.0) / 100.0;\n"
            "    float wave = 0.5 + 0.5 * sin(6.2831853 * speed * time);\n"
            "    float factor = 1.0 - depth * (1.0 - wave);\n"
            "    return vec4(baseColor.rgb * factor, baseColor.a * factor);\n"
            "}\n"
        );
        list.append(e);
    }

    // 2. Futuristic Glitch & Hologram
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("futuristic_glitch");
        e.name = QStringLiteral("Futuristic Glitch & Hologram");
        e.description = QStringLiteral("Cyberpunk sci-fi holographic scanlines, rolling sweep beam, and micro-jitter disturbances.");
        e.author = QStringLiteral("LeapK Built-in");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("both");

        ShaderParameter p1;
        p1.id = QStringLiteral("speed");
        p1.name = QStringLiteral("Animation Speed");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.1;
        p1.maxValue = 5.0;
        p1.step = 0.2;
        p1.defaultValue = 1.0;
        p1.currentValue = 1.0;
        p1.suffix = QStringLiteral("x");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("intensity");
        p2.name = QStringLiteral("Hologram Intensity");
        p2.type = QStringLiteral("float");
        p2.minValue = 10.0;
        p2.maxValue = 100.0;
        p2.step = 5.0;
        p2.defaultValue = 50.0;
        p2.currentValue = 50.0;
        p2.suffix = QStringLiteral(" %");
        e.parameters.append(p2);

        ShaderParameter p3;
        p3.id = QStringLiteral("anomalyEnabled");
        p3.name = QStringLiteral("Periodic Anomaly");
        p3.type = QStringLiteral("bool");
        p3.defaultValue = 0.0;
        p3.currentValue = 0.0;
        e.parameters.append(p3);

        ShaderParameter p4;
        p4.id = QStringLiteral("frequency");
        p4.name = QStringLiteral("Anomaly Frequency");
        p4.type = QStringLiteral("float");
        p4.minValue = 1.0;
        p4.maxValue = 10.0;
        p4.step = 1.0;
        p4.defaultValue = 3.0;
        p4.currentValue = 3.0;
        e.parameters.append(p4);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float intensityNorm = clamp(intensity, 10.0, 100.0) / 100.0;\n"
            "    float scanline1 = sin(pos.y * 0.35 - time * (3.0 * speed));\n"
            "    float scanline2 = sin(pos.y * 0.9 + time * (1.8 * speed));\n"
            "    float scanlines = 0.5 + 0.5 * (scanline1 * 0.7 + scanline2 * 0.3);\n"
            "    float beamCoord = fract(pos.y * 0.002 - time * (0.12 * speed));\n"
            "    float beam = smoothstep(0.08, 0.0, abs(beamCoord - 0.5));\n"
            "    float holoJitter = sin(pos.y * 0.5 + time * (3.5 * speed)) * cos(pos.y * 1.1 - time * (2.5 * speed));\n"
            "    float totalShift = holoJitter * (intensityNorm * 0.5);\n"
            "    float surge = 0.0;\n"
            "    if (anomalyEnabled > 0.5) {\n"
            "        float anomalyPhase = time * (0.35 + 0.2 * frequency);\n"
            "        float beat = sin(anomalyPhase) * sin(anomalyPhase * 0.618 + 1.5);\n"
            "        surge = smoothstep(0.5, 0.95, beat);\n"
            "        if (surge > 0.0) {\n"
            "            float slice = sin(pos.y * 1.2 + time * (16.0 * speed)) * cos(pos.y * 2.2 - time * (11.0 * speed));\n"
            "            totalShift += slice * surge * (intensityNorm * 1.5);\n"
            "        }\n"
            "    }\n"
            "    float energy = 1.0 + intensityNorm * (0.45 * (scanlines - 0.5) + 0.6 * beam + 0.8 * surge);\n"
            "    vec3 holoEmission = vec3(0.12, 0.28, 0.42) * (intensityNorm * (0.35 + 0.65 * beam + 0.8 * surge));\n"
            "    vec3 finalRgb = min(vec3(1.0), baseColor.rgb * energy + holoEmission);\n"
            "    return vec4(finalRgb, baseColor.a * (0.7 + 0.3 * energy));\n"
            "}\n"
        );
        list.append(e);
    }

    // 3. Neon Voltage Flicker
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("neon_flicker");
        e.name = QStringLiteral("Neon Voltage Flicker");
        e.description = QStringLiteral("Atmospheric neon tube voltage drops and erratic organic flickering.");
        e.author = QStringLiteral("LeapK Built-in");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("both");

        ShaderParameter p1;
        p1.id = QStringLiteral("interval");
        p1.name = QStringLiteral("Flicker Interval");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.5;
        p1.maxValue = 30.0;
        p1.step = 0.5;
        p1.defaultValue = 3.0;
        p1.currentValue = 3.0;
        p1.suffix = QStringLiteral(" s");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("duration");
        p2.name = QStringLiteral("Flicker Duration");
        p2.type = QStringLiteral("float");
        p2.minValue = 0.05;
        p2.maxValue = 2.0;
        p2.step = 0.05;
        p2.defaultValue = 0.25;
        p2.currentValue = 0.25;
        p2.suffix = QStringLiteral(" s");
        e.parameters.append(p2);

        ShaderParameter p3;
        p3.id = QStringLiteral("intensity");
        p3.name = QStringLiteral("Flicker Intensity");
        p3.type = QStringLiteral("float");
        p3.minValue = 10.0;
        p3.maxValue = 100.0;
        p3.step = 5.0;
        p3.defaultValue = 30.0;
        p3.currentValue = 30.0;
        p3.suffix = QStringLiteral(" %");
        e.parameters.append(p3);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float validInterval = max(0.5, interval);\n"
            "    float validDuration = clamp(duration, 0.02, validInterval * 0.95);\n"
            "    float phase = mod(time, validInterval);\n"
            "    float factor = 1.0;\n"
            "    if (phase < validDuration) {\n"
            "        float rawNoise = sin(time * 110.0) * 0.5 + cos(time * 67.0) * 0.3 + sin(time * 193.0) * 0.2;\n"
            "        float depth = clamp(intensity, 10.0, 100.0) / 100.0;\n"
            "        if (rawNoise < -0.2) {\n"
            "            factor = max(0.05, 1.0 - depth);\n"
            "        } else if (rawNoise < 0.25) {\n"
            "            factor = max(0.1, 1.0 - depth * 0.6);\n"
            "        } else {\n"
            "            factor = 1.0 - depth * 0.15 * abs(rawNoise);\n"
            "        }\n"
            "    }\n"
            "    return vec4(baseColor.rgb * factor, baseColor.a * factor);\n"
            "}\n"
        );
        list.append(e);
    }

    // 4. Flash / Strobe
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("flash_strobe");
        e.name = QStringLiteral("Flash / Strobe");
        e.description = QStringLiteral("Periodic exponential light flashes illuminating the outline.");
        e.author = QStringLiteral("LeapK Built-in");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("both");

        ShaderParameter p1;
        p1.id = QStringLiteral("interval");
        p1.name = QStringLiteral("Flash Interval");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.5;
        p1.maxValue = 20.0;
        p1.step = 0.5;
        p1.defaultValue = 4.0;
        p1.currentValue = 4.0;
        p1.suffix = QStringLiteral(" s");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("duration");
        p2.name = QStringLiteral("Flash Duration");
        p2.type = QStringLiteral("float");
        p2.minValue = 0.05;
        p2.maxValue = 1.0;
        p2.step = 0.05;
        p2.defaultValue = 0.15;
        p2.currentValue = 0.15;
        p2.suffix = QStringLiteral(" s");
        e.parameters.append(p2);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float validInterval = max(0.5, interval);\n"
            "    float validDuration = clamp(duration, 0.05, 1.0);\n"
            "    float phase = mod(time, validInterval);\n"
            "    float flash = 0.0;\n"
            "    if (phase < validDuration) {\n"
            "        float progress = phase / validDuration;\n"
            "        flash = 0.8 * exp(-4.0 * progress);\n"
            "    }\n"
            "    vec3 brightColor = mix(baseColor.rgb, vec3(1.0, 1.0, 1.0), flash * 0.7);\n"
            "    return vec4(brightColor * (1.0 + flash), baseColor.a * (1.0 + flash * 0.5));\n"
            "}\n"
        );
        list.append(e);
    }

    // 5. Rainbow Wave
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("rainbow_wave");
        e.name = QStringLiteral("Rainbow Wave");
        e.description = QStringLiteral("Smooth dynamic prismatic rainbow hue cycle flowing around the window perimeter.");
        e.author = QStringLiteral("LeapK Community");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("both");

        ShaderParameter p1;
        p1.id = QStringLiteral("speed");
        p1.name = QStringLiteral("Cycle Speed");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.1;
        p1.maxValue = 5.0;
        p1.step = 0.1;
        p1.defaultValue = 1.0;
        p1.currentValue = 1.0;
        p1.suffix = QStringLiteral("x");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("waveFrequency");
        p2.name = QStringLiteral("Wave Density");
        p2.type = QStringLiteral("float");
        p2.minValue = 0.0005;
        p2.maxValue = 0.02;
        p2.step = 0.0005;
        p2.defaultValue = 0.003;
        p2.currentValue = 0.003;
        e.parameters.append(p2);

        ShaderParameter p3;
        p3.id = QStringLiteral("saturation");
        p3.name = QStringLiteral("Color Saturation");
        p3.type = QStringLiteral("float");
        p3.minValue = 0.0;
        p3.maxValue = 1.0;
        p3.step = 0.05;
        p3.defaultValue = 0.9;
        p3.currentValue = 0.9;
        e.parameters.append(p3);

        ShaderParameter p4;
        p4.id = QStringLiteral("brightness");
        p4.name = QStringLiteral("Brightness Boost");
        p4.type = QStringLiteral("float");
        p4.minValue = 0.2;
        p4.maxValue = 2.0;
        p4.step = 0.1;
        p4.defaultValue = 1.2;
        p4.currentValue = 1.2;
        p4.suffix = QStringLiteral("x");
        e.parameters.append(p4);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float s = computePerimeterDistance(pos, box, 6.0);\n"
            "    float hue = fract(time * speed * 0.15 + s * waveFrequency);\n"
            "    vec3 rgb = hsv2rgb(vec3(hue, saturation, brightness));\n"
            "    return vec4(rgb * baseColor.rgb, baseColor.a);\n"
            "}\n"
        );
        list.append(e);
    }

    // 2. Matrix Digital Rain
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("matrix_rain");
        e.name = QStringLiteral("Matrix Digital Rain");
        e.description = QStringLiteral("Green cascading digital code streams falling down the aura with glowing drops.");
        e.author = QStringLiteral("LeapK Community");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("glow");

        ShaderParameter p1;
        p1.id = QStringLiteral("dropSpeed");
        p1.name = QStringLiteral("Rain Fall Speed");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.2;
        p1.maxValue = 5.0;
        p1.step = 0.1;
        p1.defaultValue = 1.5;
        p1.currentValue = 1.5;
        p1.suffix = QStringLiteral("x");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("streamDensity");
        p2.name = QStringLiteral("Stream Density");
        p2.type = QStringLiteral("float");
        p2.minValue = 5.0;
        p2.maxValue = 50.0;
        p2.step = 1.0;
        p2.defaultValue = 20.0;
        p2.currentValue = 20.0;
        e.parameters.append(p2);

        ShaderParameter p3;
        p3.id = QStringLiteral("tailLength");
        p3.name = QStringLiteral("Drop Trail Length");
        p3.type = QStringLiteral("float");
        p3.minValue = 0.2;
        p3.maxValue = 2.0;
        p3.step = 0.1;
        p3.defaultValue = 1.0;
        p3.currentValue = 1.0;
        e.parameters.append(p3);

        ShaderParameter p4;
        p4.id = QStringLiteral("matrixColor");
        p4.name = QStringLiteral("Code Color");
        p4.type = QStringLiteral("color");
        p4.colorDefault = QStringLiteral("#00ff66");
        p4.colorCurrent = QStringLiteral("#00ff66");
        e.parameters.append(p4);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float colWidth = 14.0;\n"
            "    float colIndex = floor((pos.x - box.x + box.z) / colWidth);\n"
            "    float seed = hash21(vec2(colIndex, 19.84));\n"
            "    float vSpeed = (0.7 + 0.6 * seed) * dropSpeed * 180.0;\n"
            "    float yRel = pos.y + time * vSpeed;\n"
            "    float stream = fract((yRel + seed * 500.0) / (streamDensity * 10.0));\n"
            "    float head = smoothstep(0.92, 1.0, stream);\n"
            "    float tail = pow(stream, tailLength * 4.0);\n"
            "    vec3 green = matrixColor.rgb;\n"
            "    vec3 headColor = mix(green, vec3(1.0, 1.0, 1.0), 0.7);\n"
            "    vec3 finalRgb = mix(green * tail, headColor, head);\n"
            "    float glow = max(tail * 0.9, head * 1.5);\n"
            "    return vec4(finalRgb, baseColor.a * (0.3 + 0.7 * glow));\n"
            "}\n"
        );
        list.append(e);
    }

    // 3. Electric Plasma
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("electric_plasma");
        e.name = QStringLiteral("Electric Plasma");
        e.description = QStringLiteral("High-voltage electric arcs and energy crackle pulsating along the outline.");
        e.author = QStringLiteral("LeapK Community");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("both");

        ShaderParameter p1;
        p1.id = QStringLiteral("voltage");
        p1.name = QStringLiteral("Voltage Speed");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.1;
        p1.maxValue = 5.0;
        p1.step = 0.1;
        p1.defaultValue = 1.5;
        p1.currentValue = 1.5;
        p1.suffix = QStringLiteral("x");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("sparkFrequency");
        p2.name = QStringLiteral("Spark Frequency");
        p2.type = QStringLiteral("float");
        p2.minValue = 1.0;
        p2.maxValue = 20.0;
        p2.step = 0.5;
        p2.defaultValue = 8.0;
        p2.currentValue = 8.0;
        e.parameters.append(p2);

        ShaderParameter p3;
        p3.id = QStringLiteral("glowBoost");
        p3.name = QStringLiteral("Energy Intensity");
        p3.type = QStringLiteral("float");
        p3.minValue = 0.5;
        p3.maxValue = 3.0;
        p3.step = 0.1;
        p3.defaultValue = 1.3;
        p3.currentValue = 1.3;
        p3.suffix = QStringLiteral("x");
        e.parameters.append(p3);

        ShaderParameter p4;
        p4.id = QStringLiteral("plasmaColor");
        p4.name = QStringLiteral("Plasma Arc Color");
        p4.type = QStringLiteral("color");
        p4.colorDefault = QStringLiteral("#00d4ff");
        p4.colorCurrent = QStringLiteral("#00d4ff");
        e.parameters.append(p4);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float arc1 = sin(pos.y * 0.15 + time * (4.0 * voltage)) * cos(pos.x * 0.12 - time * (3.0 * voltage));\n"
            "    float arc2 = sin(pos.x * 0.25 + pos.y * 0.2 + time * (7.0 * voltage));\n"
            "    float spark = pow(max(0.0, 0.5 + 0.5 * (arc1 * 0.6 + arc2 * 0.4)), sparkFrequency * 0.2);\n"
            "    vec3 col = mix(plasmaColor.rgb, vec3(1.0, 1.0, 1.0), spark * 0.6);\n"
            "    float energy = (1.0 + spark * 2.0) * glowBoost;\n"
            "    return vec4(col * energy, baseColor.a * (0.6 + 0.4 * spark));\n"
            "}\n"
        );
        list.append(e);
    }

    // 4. Fire Aura
    {
        CustomShaderEffect e;
        e.id = QStringLiteral("fire_aura");
        e.name = QStringLiteral("Fire Aura");
        e.description = QStringLiteral("Warm blazing flames and rising embers radiating from the window perimeter.");
        e.author = QStringLiteral("LeapK Community");
        e.version = QStringLiteral("1.0");
        e.isBuiltIn = true;
        e.target = QStringLiteral("glow");

        ShaderParameter p1;
        p1.id = QStringLiteral("flameSpeed");
        p1.name = QStringLiteral("Flame Speed");
        p1.type = QStringLiteral("float");
        p1.minValue = 0.2;
        p1.maxValue = 4.0;
        p1.step = 0.1;
        p1.defaultValue = 1.2;
        p1.currentValue = 1.2;
        p1.suffix = QStringLiteral("x");
        e.parameters.append(p1);

        ShaderParameter p2;
        p2.id = QStringLiteral("turbulence");
        p2.name = QStringLiteral("Flame Turbulence");
        p2.type = QStringLiteral("float");
        p2.minValue = 0.5;
        p2.maxValue = 5.0;
        p2.step = 0.1;
        p2.defaultValue = 2.5;
        p2.currentValue = 2.5;
        e.parameters.append(p2);

        ShaderParameter p3;
        p3.id = QStringLiteral("fireColor");
        p3.name = QStringLiteral("Outer Flame Color");
        p3.type = QStringLiteral("color");
        p3.colorDefault = QStringLiteral("#ff4500");
        p3.colorCurrent = QStringLiteral("#ff4500");
        e.parameters.append(p3);

        ShaderParameter p4;
        p4.id = QStringLiteral("coreColor");
        p4.name = QStringLiteral("Inner Core Color");
        p4.type = QStringLiteral("color");
        p4.colorDefault = QStringLiteral("#ffea00");
        p4.colorCurrent = QStringLiteral("#ffea00");
        e.parameters.append(p4);

        e.glslCode = QStringLiteral(
            "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n"
            "{\n"
            "    float t = time * flameSpeed * 2.0;\n"
            "    float n1 = sin(pos.x * 0.05 + t) * cos(pos.y * 0.05 - t * 0.8);\n"
            "    float n2 = sin(pos.x * 0.12 - t * 1.5 + n1 * turbulence);\n"
            "    float flame = 0.5 + 0.5 * (n1 * 0.5 + n2 * 0.5);\n"
            "    vec3 col = mix(fireColor.rgb, coreColor.rgb, pow(flame, 2.0));\n"
            "    return vec4(col * 1.3, baseColor.a * (0.5 + 0.5 * flame));\n"
            "}\n"
        );
        list.append(e);
    }

    return list;
}

inline void ensureBuiltInShadersInstalled()
{
    const QString dirPath = defaultShadersDirectory();
    const auto builtIns = getBuiltInShaders();
    for (const auto &effect : builtIns) {
        const QString filePath = dirPath + QStringLiteral("/%1.json").arg(effect.id);
        if (!QFile::exists(filePath)) {
            saveShaderEffectToFile(effect, filePath);
        }
    }
}

inline QList<CustomShaderEffect> loadAllShaders()
{
    ensureBuiltInShadersInstalled();
    const QString dirPath = defaultShadersDirectory();
    QDir dir(dirPath);
    const QStringList filters = {QStringLiteral("*.json"), QStringLiteral("*.leapk-shader")};
    const QFileInfoList entries = dir.entryInfoList(filters, QDir::Files, QDir::Name);

    QList<CustomShaderEffect> result;
    for (const auto &entry : entries) {
        CustomShaderEffect effect;
        if (loadShaderEffectFromFile(entry.absoluteFilePath(), effect)) {
            result.append(effect);
        }
    }
    return result;
}

inline bool loadShaderById(const QString &id, CustomShaderEffect &outEffect)
{
    if (id.isEmpty()) {
        return false;
    }
    const QString dirPath = defaultShadersDirectory();
    const QString jsonPath = dirPath + QStringLiteral("/%1.json").arg(id);
    if (QFile::exists(jsonPath) && loadShaderEffectFromFile(jsonPath, outEffect)) {
        return true;
    }
    const QString leapkPath = dirPath + QStringLiteral("/%1.leapk-shader").arg(id);
    if (QFile::exists(leapkPath) && loadShaderEffectFromFile(leapkPath, outEffect)) {
        return true;
    }

    // Fallback: check built-ins
    const auto builtIns = getBuiltInShaders();
    for (const auto &b : builtIns) {
        if (b.id == id) {
            outEffect = b;
            return true;
        }
    }
    return false;
}

inline CustomShaderEffect findShaderById(const QString &id, const QList<CustomShaderEffect> &shaders = {})
{
    for (const auto &s : shaders) {
        if (s.id == id) {
            return s;
        }
    }
    CustomShaderEffect effect;
    if (loadShaderById(id, effect)) {
        return effect;
    }
    return effect;
}

inline QJsonObject createShaderBundle(const QString &shaderId, bool enabled, const QList<CustomShaderEffect> &shaders = {})
{
    QJsonObject obj;
    obj[QStringLiteral("enabled")] = enabled;
    obj[QStringLiteral("id")] = shaderId;
    if (!shaderId.isEmpty()) {
        const auto effect = findShaderById(shaderId, shaders);
        if (!effect.id.isEmpty()) {
            obj[QStringLiteral("effect")] = effect.toJson();
        }
    }
    return obj;
}

inline bool applyShaderBundle(const QJsonObject &bundle, QList<CustomShaderEffect> *inOutShaders = nullptr)
{
    if (bundle.isEmpty()) {
        return false;
    }
    QJsonObject effectObj;
    if (bundle.contains(QStringLiteral("effect")) && bundle.value(QStringLiteral("effect")).isObject()) {
        effectObj = bundle.value(QStringLiteral("effect")).toObject();
    } else if (bundle.contains(QStringLiteral("format")) && bundle.value(QStringLiteral("format")).toString() == QStringLiteral("leapk-shader")) {
        effectObj = bundle;
    }
    if (effectObj.isEmpty()) {
        return false;
    }

    const CustomShaderEffect incoming = CustomShaderEffect::fromJson(effectObj);
    if (incoming.id.isEmpty()) {
        return false;
    }

    if (inOutShaders) {
        int idx = -1;
        for (int i = 0; i < inOutShaders->size(); ++i) {
            if ((*inOutShaders)[i].id == incoming.id) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            for (const auto &p : incoming.parameters) {
                for (auto &existingP : (*inOutShaders)[idx].parameters) {
                    if (existingP.id == p.id) {
                        existingP.currentValue = p.currentValue;
                        existingP.colorCurrent = p.colorCurrent;
                        break;
                    }
                }
            }
        } else {
            inOutShaders->append(incoming);
        }
    }

    const QString destPath = defaultShadersDirectory() + QStringLiteral("/%1.json").arg(incoming.id);
    return saveShaderEffectToFile(incoming, destPath);
}

inline bool validateShaderCode(const QString &glsl, const QList<ShaderParameter> &parameters, QString *errorMessage = nullptr)
{
    if (glsl.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GLSL code cannot be empty.");
        }
        return false;
    }

    // Check for balanced braces
    int braceCount = 0;
    int parenCount = 0;
    for (int i = 0; i < glsl.size(); ++i) {
        const QChar c = glsl.at(i);
        if (c == QLatin1Char('{')) ++braceCount;
        else if (c == QLatin1Char('}')) --braceCount;
        else if (c == QLatin1Char('(')) ++parenCount;
        else if (c == QLatin1Char(')')) --parenCount;

        if (braceCount < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Unmatched closing brace '}' at position %1.").arg(i);
            return false;
        }
        if (parenCount < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Unmatched closing parenthesis ')' at position %1.").arg(i);
            return false;
        }
    }
    if (braceCount != 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing closing brace '}' (%1 unclosed).").arg(braceCount);
        return false;
    }
    if (parenCount != 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing closing parenthesis ')' (%1 unclosed).").arg(parenCount);
        return false;
    }

    // Check parameter IDs validity
    const QRegularExpression validIdRegex(QStringLiteral("^[a-zA-Z_][a-zA-Z0-9_]*$"));
    for (const auto &p : parameters) {
        if (!validIdRegex.match(p.id).hasMatch()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Parameter uniform ID '%1' is not a valid GLSL identifier.").arg(p.id);
            }
            return false;
        }
    }

    // Check for return statement
    if (!glsl.contains(QStringLiteral("return"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GLSL code must contain a 'return' statement producing a vec4 color.");
        }
        return false;
    }

    return true;
}

inline QByteArray generateUniformDeclarations(const QList<ShaderParameter> &params)
{
    QByteArray decls;
    for (const auto &p : params) {
        if (p.type == QStringLiteral("float")) {
            decls += "uniform float " + p.id.toUtf8() + ";\n";
        } else if (p.type == QStringLiteral("int")) {
            decls += "uniform int " + p.id.toUtf8() + ";\n";
        } else if (p.type == QStringLiteral("bool")) {
            decls += "uniform float " + p.id.toUtf8() + ";\n";
        } else if (p.type == QStringLiteral("color")) {
            decls += "uniform vec4 " + p.id.toUtf8() + ";\n";
        }
    }
    return decls;
}

inline QByteArray buildGlowFragmentSource(const CustomShaderEffect &effect, bool modern, bool isOpenGLES)
{
    QByteArray source;
    if (isOpenGLES) {
        if (modern) {
            source += "#version 300 es\n";
        }
        source += "precision highp float;\n";
    } else if (modern) {
        source += "#version 140\n";
    }
    source += modern
        ? "in vec2 position0;\nout vec4 fragColor;\n"
        : "varying vec2 position0;\n#define fragColor gl_FragColor\n";

    source += R"GLSL(
uniform vec4 geometryColor;
uniform vec4 outerBox;
uniform vec4 innerBox;
uniform float outerRadius;
uniform float innerRadius;
uniform float glowRadius;
uniform float time;
uniform float glitchActive;
uniform float glitchSpeed;
uniform float glitchIntensity;
uniform float glitchAnomalyActive;
uniform float glitchFrequency;
uniform float fadeCorners;
uniform float cornerCutoff;
uniform float fadeDistance;
uniform float dashActive;
uniform float dashLength;
uniform float dashPeriod;
uniform float dashSmooth;

)GLSL";

    // Inject custom uniform declarations
    source += generateUniformDeclarations(effect.parameters);

    source += R"GLSL(
float roundedBoxDistance(vec2 point, vec4 box, float radius)
{
    vec2 q = abs(point - box.xy) - box.zw + vec2(radius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

float computePerimeterDistance(vec2 pt, vec4 box, float r)
{
    vec2 q = pt - box.xy;
    float w0 = max(0.0, box.z - r);
    float h0 = max(0.0, box.w - r);
    float Lx = 2.0 * w0;
    float Ly = 2.0 * h0;
    float Lc = 1.57079632679 * r;

    if (q.x > w0 && q.y < -h0) {
        float angle = atan(q.x - w0, -(q.y + h0));
        return Lx + r * angle;
    }
    if (q.x > w0 && q.y > h0) {
        float angle = atan(q.y - h0, q.x - w0);
        return Lx + Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y > h0) {
        float angle = atan(-(q.x + w0), q.y - h0);
        return 2.0 * Lx + 2.0 * Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y < -h0) {
        float angle = atan(-(q.y + h0), -(q.x + w0));
        return 2.0 * Lx + 3.0 * Lc + 2.0 * Ly + r * angle;
    }

    if (q.y < -h0) {
        return clamp(q.x + w0, 0.0, Lx);
    }
    if (q.x > w0) {
        return Lx + Lc + clamp(q.y + h0, 0.0, Ly);
    }
    if (q.y > h0) {
        return Lx + Lc + Ly + Lc + clamp(w0 - q.x, 0.0, Lx);
    }
    return 2.0 * Lx + 3.0 * Lc + Ly + clamp(h0 - q.y, 0.0, Ly);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

)GLSL";

    // Inject custom shader code
    const QString code = effect.glslCode.trimmed();
    if (code.contains(QStringLiteral("evaluateCustomEffect"))) {
        source += code.toUtf8() + "\n";
    } else if (code.contains(QStringLiteral("evaluateEffect"))) {
        source += "#define evaluateCustomEffect evaluateEffect\n";
        source += code.toUtf8() + "\n";
    } else {
        source += "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n{\n";
        source += code.toUtf8() + "\n}\n";
    }

    source += R"GLSL(
void main()
{
    float outside = roundedBoxDistance(position0, outerBox, outerRadius);
    float inside = -roundedBoxDistance(position0, innerBox, innerRadius);
    float distanceToOutline = max(outside, inside);
    float sigma = max(glowRadius * 0.42, 0.5);

    float cornerFade = 1.0;
    if (fadeCorners > 0.5) {
        vec2 p = abs(position0 - outerBox.xy);
        vec2 d = outerBox.zw - p;
        float edgeDist = max(d.x, d.y);
        cornerFade = smoothstep(cornerCutoff, cornerCutoff + fadeDistance, edgeDist);
    }

    float normalized = max(distanceToOutline, 0.0) / sigma;
    float falloff = exp(-0.5 * normalized * normalized);
    float outsideMask = smoothstep(-1.0, 0.0, distanceToOutline);
    vec4 baseColor = vec4(geometryColor.rgb,
                          geometryColor.a * 0.38 * falloff * outsideMask * cornerFade);

    fragColor = evaluateCustomEffect(baseColor, position0, outerBox, distanceToOutline, time);

    if (dashActive > 0.5 && dashPeriod > 0.0) {
        float s = computePerimeterDistance(position0, outerBox, outerRadius);
        float d = mod(s, dashPeriod);
        float edgeDist;
        if (d < dashLength) {
            edgeDist = min(d, dashLength - d);
        } else {
            float gapLength = dashPeriod - dashLength;
            float distFromDash = d - dashLength;
            edgeDist = -min(distFromDash, gapLength - distFromDash);
        }

        float dashAlpha;
        if (dashSmooth > 0.5) {
            float fadeWidth = max(1.0, min(dashLength * 0.5, (dashPeriod - dashLength) * 0.5));
            dashAlpha = smoothstep(-fadeWidth, fadeWidth, edgeDist);
        } else {
            dashAlpha = clamp(edgeDist + 0.5, 0.0, 1.0);
        }

        if (dashAlpha <= 0.001) {
            discard;
        }
        fragColor.a *= dashAlpha;
    }
}
)GLSL";
    return source;
}

inline QByteArray buildOutlineFragmentSource(const CustomShaderEffect &effect, bool modern, bool isOpenGLES)
{
    QByteArray source;
    if (isOpenGLES) {
        if (modern) {
            source += "#version 300 es\n";
        }
        source += "precision highp float;\n";
    } else if (modern) {
        source += "#version 140\n";
    }
    source += modern
        ? "in vec2 position0;\nout vec4 fragColor;\n"
        : "varying vec2 position0;\n#define fragColor gl_FragColor\n";

    source += R"GLSL(
uniform vec4 geometryColor;
uniform vec4 outerBox;
uniform float outerRadius;
uniform float fadeCorners;
uniform float cornerCutoff;
uniform float fadeDistance;
uniform float dashActive;
uniform float dashLength;
uniform float dashPeriod;
uniform float dashSmooth;
uniform float time;

)GLSL";

    source += generateUniformDeclarations(effect.parameters);

    source += R"GLSL(
float computePerimeterDistance(vec2 pt, vec4 box, float r)
{
    vec2 q = pt - box.xy;
    float w0 = max(0.0, box.z - r);
    float h0 = max(0.0, box.w - r);
    float Lx = 2.0 * w0;
    float Ly = 2.0 * h0;
    float Lc = 1.57079632679 * r;

    if (q.x > w0 && q.y < -h0) {
        float angle = atan(q.x - w0, -(q.y + h0));
        return Lx + r * angle;
    }
    if (q.x > w0 && q.y > h0) {
        float angle = atan(q.y - h0, q.x - w0);
        return Lx + Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y > h0) {
        float angle = atan(-(q.x + w0), q.y - h0);
        return 2.0 * Lx + 2.0 * Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y < -h0) {
        float angle = atan(-(q.y + h0), -(q.x + w0));
        return 2.0 * Lx + 3.0 * Lc + 2.0 * Ly + r * angle;
    }

    if (q.y < -h0) {
        return clamp(q.x + w0, 0.0, Lx);
    }
    if (q.x > w0) {
        return Lx + Lc + clamp(q.y + h0, 0.0, Ly);
    }
    if (q.y > h0) {
        return Lx + Lc + Ly + Lc + clamp(w0 - q.x, 0.0, Lx);
    }
    return 2.0 * Lx + 3.0 * Lc + Ly + clamp(h0 - q.y, 0.0, Ly);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

)GLSL";

    const QString code = effect.glslCode.trimmed();
    if (code.contains(QStringLiteral("evaluateCustomEffect"))) {
        source += code.toUtf8() + "\n";
    } else if (code.contains(QStringLiteral("evaluateEffect"))) {
        source += "#define evaluateCustomEffect evaluateEffect\n";
        source += code.toUtf8() + "\n";
    } else {
        source += "vec4 evaluateCustomEffect(vec4 baseColor, vec2 pos, vec4 box, float dist, float time)\n{\n";
        source += code.toUtf8() + "\n}\n";
    }

    source += R"GLSL(
void main()
{
    float cornerFade = 1.0;
    if (fadeCorners > 0.5) {
        vec2 p = abs(position0 - outerBox.xy);
        vec2 d = outerBox.zw - p;
        float edgeDist = max(d.x, d.y);
        cornerFade = smoothstep(cornerCutoff, cornerCutoff + fadeDistance, edgeDist);
    }

    vec4 baseColor = vec4(geometryColor.rgb, geometryColor.a * cornerFade);
    fragColor = evaluateCustomEffect(baseColor, position0, outerBox, 0.0, time);

    if (dashActive > 0.5 && dashPeriod > 0.0) {
        float s = computePerimeterDistance(position0, outerBox, outerRadius);
        float d = mod(s, dashPeriod);
        float edgeDist;
        if (d < dashLength) {
            edgeDist = min(d, dashLength - d);
        } else {
            float gapLength = dashPeriod - dashLength;
            float distFromDash = d - dashLength;
            edgeDist = -min(distFromDash, gapLength - distFromDash);
        }

        float dashAlpha;
        if (dashSmooth > 0.5) {
            float fadeWidth = max(1.0, min(dashLength * 0.5, (dashPeriod - dashLength) * 0.5));
            dashAlpha = smoothstep(-fadeWidth, fadeWidth, edgeDist);
        } else {
            dashAlpha = clamp(edgeDist + 0.5, 0.0, 1.0);
        }

        if (dashAlpha <= 0.001) {
            discard;
        }
        fragColor.a *= dashAlpha;
    }
}
)GLSL";
    return source;
}

} // namespace LeapKShader
