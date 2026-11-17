/*
 * SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QMap>
#include <QProcess>
#include "themehelper.h"
#include "presethelper.h"
#include "customshaderhelper.h"

#ifndef ACTIVE_WINDOW_OUTLINE_VERSION
#define ACTIVE_WINDOW_OUTLINE_VERSION "1.6.0"
#endif

namespace LeapKPack
{

struct PackManifest
{
    QString id;
    QString name;
    QString version = QStringLiteral("1.0.0");
    QString author;
    QString description;
    QString website;
    QString previewImageBase64;
    QString minLeapKVersion = QStringLiteral(ACTIVE_WINDOW_OUTLINE_VERSION);
    QStringList tags;
    bool hasTheme = false;
    bool hasPreset = false;
    bool hasShaders = false;
    bool hasShortcuts = false;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QStringLiteral("id")] = id;
        o[QStringLiteral("name")] = name;
        o[QStringLiteral("version")] = version;
        o[QStringLiteral("author")] = author;
        o[QStringLiteral("description")] = description;
        o[QStringLiteral("website")] = website;
        o[QStringLiteral("previewImage")] = previewImageBase64;
        o[QStringLiteral("minLeapKVersion")] = minLeapKVersion;
        o[QStringLiteral("tags")] = QJsonArray::fromStringList(tags);
        o[QStringLiteral("hasTheme")] = hasTheme;
        o[QStringLiteral("hasPreset")] = hasPreset;
        o[QStringLiteral("hasShaders")] = hasShaders;
        o[QStringLiteral("hasShortcuts")] = hasShortcuts;
        return o;
    }

    static PackManifest fromJson(const QJsonObject &o)
    {
        PackManifest m;
        m.id = o.value(QStringLiteral("id")).toString();
        m.name = o.value(QStringLiteral("name")).toString();
        m.version = o.value(QStringLiteral("version")).toString(m.version);
        m.author = o.value(QStringLiteral("author")).toString();
        m.description = o.value(QStringLiteral("description")).toString();
        m.website = o.value(QStringLiteral("website")).toString();
        m.previewImageBase64 = o.value(QStringLiteral("previewImage")).toString();
        m.minLeapKVersion = o.value(QStringLiteral("minLeapKVersion")).toString(m.minLeapKVersion);
        const QJsonArray tagsArr = o.value(QStringLiteral("tags")).toArray();
        for (const auto &t : tagsArr) {
            m.tags.append(t.toString());
        }
        m.hasTheme = o.value(QStringLiteral("hasTheme")).toBool(false);
        m.hasPreset = o.value(QStringLiteral("hasPreset")).toBool(false);
        m.hasShaders = o.value(QStringLiteral("hasShaders")).toBool(false);
        m.hasShortcuts = o.value(QStringLiteral("hasShortcuts")).toBool(false);
        return m;
    }
};

struct PackBundle
{
    PackManifest manifest;
    LeapKTheme::Theme theme;
    OutlinePreset::PresetInfo preset;
    QList<LeapKShader::CustomShaderEffect> shaders;
    QMap<QString, QString> shortcuts; // actionName -> keySequence (e.g. "JumpToWindow" -> "Meta+Space")
    bool hasTheme = false;
    bool hasPreset = false;
    bool hasShaders = false;
    bool hasShortcuts = false;
};

inline QByteArray serializePack(const PackBundle &bundle)
{
    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("leapk-pack");
    root[QStringLiteral("packVersion")] = QStringLiteral("1.0");
    root[QStringLiteral("createdAt")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    PackManifest m = bundle.manifest;
    m.hasTheme = bundle.hasTheme;
    m.hasPreset = bundle.hasPreset;
    m.hasShaders = bundle.hasShaders && !bundle.shaders.isEmpty();
    m.hasShortcuts = bundle.hasShortcuts && !bundle.shortcuts.isEmpty();
    root[QStringLiteral("manifest")] = m.toJson();

    if (bundle.hasTheme) {
        root[QStringLiteral("theme")] = bundle.theme.toJson();
    }

    if (bundle.hasPreset) {
        QJsonObject presetObj;
        presetObj[QStringLiteral("name")] = bundle.preset.name;
        presetObj[QStringLiteral("description")] = bundle.preset.description;
        presetObj[QStringLiteral("settings")] = bundle.preset.settings;
        if (!bundle.preset.shader.isEmpty()) {
            presetObj[QStringLiteral("shader")] = bundle.preset.shader;
        }
        root[QStringLiteral("preset")] = presetObj;
    }

    if (bundle.hasShaders && !bundle.shaders.isEmpty()) {
        QJsonArray shadersArr;
        for (const auto &s : bundle.shaders) {
            shadersArr.append(s.toJson());
        }
        root[QStringLiteral("shaders")] = shadersArr;
    }

    if (bundle.hasShortcuts && !bundle.shortcuts.isEmpty()) {
        QJsonObject scObj;
        for (auto it = bundle.shortcuts.constBegin(); it != bundle.shortcuts.constEnd(); ++it) {
            scObj[it.key()] = it.value();
        }
        root[QStringLiteral("shortcuts")] = scObj;
    }

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

inline bool parsePack(const QByteArray &data, PackBundle &outBundle, QString *errorMsg = nullptr)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMsg) *errorMsg = QStringLiteral("JSON parse error: %1").arg(err.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const QString format = root.value(QStringLiteral("format")).toString();
    if (format != QStringLiteral("leapk-pack")) {
        if (errorMsg) *errorMsg = QStringLiteral("Invalid pack format (expected 'leapk-pack')");
        return false;
    }

    outBundle = PackBundle();
    outBundle.manifest = PackManifest::fromJson(root.value(QStringLiteral("manifest")).toObject());

    if (root.contains(QStringLiteral("theme"))) {
        outBundle.theme = LeapKTheme::Theme::fromJson(root.value(QStringLiteral("theme")).toObject());
        outBundle.hasTheme = !outBundle.theme.id.isEmpty();
    }

    if (root.contains(QStringLiteral("preset"))) {
        const QJsonObject pObj = root.value(QStringLiteral("preset")).toObject();
        outBundle.preset.name = pObj.value(QStringLiteral("name")).toString();
        outBundle.preset.description = pObj.value(QStringLiteral("description")).toString();
        outBundle.preset.settings = pObj.value(QStringLiteral("settings")).toObject();
        outBundle.preset.shader = pObj.value(QStringLiteral("shader")).toObject();
        outBundle.preset.isBuiltIn = false;
        outBundle.hasPreset = !outBundle.preset.name.isEmpty();
    }

    if (root.contains(QStringLiteral("shaders"))) {
        const QJsonArray arr = root.value(QStringLiteral("shaders")).toArray();
        for (const auto &v : arr) {
            const auto s = LeapKShader::CustomShaderEffect::fromJson(v.toObject());
            if (!s.id.isEmpty()) {
                outBundle.shaders.append(s);
            }
        }
        outBundle.hasShaders = !outBundle.shaders.isEmpty();
    }

    if (root.contains(QStringLiteral("shortcuts"))) {
        const QJsonObject scObj = root.value(QStringLiteral("shortcuts")).toObject();
        for (auto it = scObj.constBegin(); it != scObj.constEnd(); ++it) {
            outBundle.shortcuts[it.key()] = it.value().toString();
        }
        outBundle.hasShortcuts = !outBundle.shortcuts.isEmpty();
    }

    return true;
}

inline bool exportPackToFile(const QString &filePath, const PackBundle &bundle, QString *errorMsg = nullptr)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMsg) *errorMsg = QStringLiteral("Failed to open file for writing: %1").arg(file.errorString());
        return false;
    }
    const QByteArray bytes = serializePack(bundle);
    if (file.write(bytes) != bytes.size()) {
        if (errorMsg) *errorMsg = QStringLiteral("Incomplete write operation");
        file.close();
        return false;
    }
    file.close();
    return true;
}

inline bool importPackFromFile(const QString &filePath, PackBundle &outBundle, QString *errorMsg = nullptr)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMsg) *errorMsg = QStringLiteral("Failed to open file for reading: %1").arg(file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return parsePack(data, outBundle, errorMsg);
}

struct InstallOptions
{
    bool installTheme = true;
    bool installPreset = true;
    bool installShaders = true;
    bool installShortcuts = false; // By default off to preserve user shortcuts
    bool activateTheme = true;
    bool activatePreset = false;
};

inline bool installPack(const PackBundle &bundle, const InstallOptions &options, QString *errorMsg = nullptr)
{
    QStringList installedSummary;

    // 1. Shaders
    if (options.installShaders && bundle.hasShaders) {
        const QString shadersDir = LeapKShader::defaultShadersDirectory();
        QDir().mkpath(shadersDir);
        for (const auto &s : bundle.shaders) {
            const QString shaderPath = shadersDir + QLatin1Char('/') + s.id + QStringLiteral(".json");
            if (!LeapKShader::saveShaderEffectToFile(s, shaderPath)) {
                if (errorMsg) *errorMsg = QStringLiteral("Failed to save shader: %1").arg(s.name);
                return false;
            }
            installedSummary.append(QStringLiteral("Shader: %1").arg(s.name));
        }
    }

    // 2. Theme
    if (options.installTheme && bundle.hasTheme) {
        const QString themeDir = LeapKTheme::defaultThemesDirectory();
        QDir().mkpath(themeDir);
        const QString themePath = themeDir + QLatin1Char('/') + bundle.theme.id + QStringLiteral(".json");
        if (!LeapKTheme::saveThemeToFile(bundle.theme, themePath)) {
            if (errorMsg) *errorMsg = QStringLiteral("Failed to save theme: %1").arg(bundle.theme.name);
            return false;
        }
        installedSummary.append(QStringLiteral("Theme: %1").arg(bundle.theme.name));
    }

    // 3. Preset
    if (options.installPreset && bundle.hasPreset) {
        QString presetErr;
        if (!OutlinePreset::savePreset(OutlinePreset::defaultPresetsDirectory(), bundle.preset, &presetErr)) {
            if (errorMsg) *errorMsg = QStringLiteral("Failed to save preset: %1").arg(presetErr);
            return false;
        }
        installedSummary.append(QStringLiteral("Preset: %1").arg(bundle.preset.name));
    }

    // 4. Global Shortcuts
    if (options.installShortcuts && bundle.hasShortcuts) {
        for (auto it = bundle.shortcuts.constBegin(); it != bundle.shortcuts.constEnd(); ++it) {
            const QString action = it.key();
            const QString seq = it.value();
            if (!action.isEmpty() && !seq.isEmpty()) {
                QProcess::execute(QStringLiteral("kwriteconfig6"), {
                    QStringLiteral("--file"), QStringLiteral("kglobalshortcutsrc"),
                    QStringLiteral("--group"), QStringLiteral("kwin"),
                    QStringLiteral("--key"), action,
                    QStringLiteral("%1,%1,%2").arg(seq, action)
                });
            }
        }
        QProcess::execute(QStringLiteral("qdbus6"), {
            QStringLiteral("org.kde.KGlobalAccel"),
            QStringLiteral("/KGlobalAccel"),
            QStringLiteral("org.kde.KGlobalAccel.reloadConfig")
        });
        installedSummary.append(QStringLiteral("Shortcuts updated"));
    }

    return true;
}

} // namespace LeapKPack
