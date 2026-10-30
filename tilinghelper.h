// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QChar>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

namespace LeapKTiling
{

struct TileSlot
{
    QRectF relativeRect; // Normalized coordinates [0.0, 1.0]

    bool operator==(const TileSlot &other) const
    {
        return relativeRect == other.relativeRect;
    }
};

struct TilingPreset
{
    QString id;
    QString name;
    QChar triggerKey;      // e.g. 'A', 'S', 'D', 'F', 'G', 'X'
    QString iconDiagram;   // e.g. "▌▌", "▌▌▌", "▌☰", "⊞", "🗖", "✕"
    QString description;   // e.g. "2 slots • 50/50"
    QList<TileSlot> tileSlots;

    bool isFloatAction() const
    {
        return id == QStringLiteral("float") || tileSlots.isEmpty();
    }

    bool operator==(const TilingPreset &other) const
    {
        return id == other.id;
    }
};

inline QList<TilingPreset> defaultPresets()
{
    QList<TilingPreset> presets;

    // 1. Two Columns (50 / 50)
    {
        TilingPreset p;
        p.id = QStringLiteral("columns-2");
        p.name = QStringLiteral("Two Columns");
        p.triggerKey = QLatin1Char('A');
        p.iconDiagram = QStringLiteral("▌▌");
        p.description = QStringLiteral("2 slots • 50% / 50%");
        p.tileSlots = {
            TileSlot{QRectF(0.0, 0.0, 0.5, 1.0)},
            TileSlot{QRectF(0.5, 0.0, 0.5, 1.0)}
        };
        presets.append(p);
    }

    // 2. Three Columns Ultrawide (25 / 50 / 25)
    {
        TilingPreset p;
        p.id = QStringLiteral("columns-3");
        p.name = QStringLiteral("Three Columns");
        p.triggerKey = QLatin1Char('S');
        p.iconDiagram = QStringLiteral("▌▌▌");
        p.description = QStringLiteral("3 slots • 25% / 50% / 25%");
        p.tileSlots = {
            TileSlot{QRectF(0.0, 0.0, 0.25, 1.0)},
            TileSlot{QRectF(0.25, 0.0, 0.50, 1.0)},
            TileSlot{QRectF(0.75, 0.0, 0.25, 1.0)}
        };
        presets.append(p);
    }

    // 3. Master & Stack (60% Main left, 2x50% vertical Stack right)
    {
        TilingPreset p;
        p.id = QStringLiteral("master-stack");
        p.name = QStringLiteral("Master & Stack");
        p.triggerKey = QLatin1Char('D');
        p.iconDiagram = QStringLiteral("▌☰");
        p.description = QStringLiteral("3 slots • 60% / 40%");
        p.tileSlots = {
            TileSlot{QRectF(0.0, 0.0, 0.60, 1.0)},
            TileSlot{QRectF(0.60, 0.0, 0.40, 0.50)},
            TileSlot{QRectF(0.60, 0.50, 0.40, 0.50)}
        };
        presets.append(p);
    }

    // 4. 2x2 Quad Grid
    {
        TilingPreset p;
        p.id = QStringLiteral("grid-4");
        p.name = QStringLiteral("2x2 Quad Grid");
        p.triggerKey = QLatin1Char('F');
        p.iconDiagram = QStringLiteral("⊞");
        p.description = QStringLiteral("4 slots • 2x2 quarters");
        p.tileSlots = {
            TileSlot{QRectF(0.0, 0.0, 0.5, 0.5)},
            TileSlot{QRectF(0.5, 0.0, 0.5, 0.5)},
            TileSlot{QRectF(0.0, 0.5, 0.5, 0.5)},
            TileSlot{QRectF(0.5, 0.5, 0.5, 0.5)}
        };
        presets.append(p);
    }

    // 5. Full Focus / Monocle
    {
        TilingPreset p;
        p.id = QStringLiteral("monocle");
        p.name = QStringLiteral("Full Focus");
        p.triggerKey = QLatin1Char('G');
        p.iconDiagram = QStringLiteral("🗖");
        p.description = QStringLiteral("1 slot • 100%");
        p.tileSlots = {
            TileSlot{QRectF(0.0, 0.0, 1.0, 1.0)}
        };
        presets.append(p);
    }

    // 6. Float / Disable Tiling
    {
        TilingPreset p;
        p.id = QStringLiteral("float");
        p.name = QStringLiteral("Float / Turn Off");
        p.triggerKey = QLatin1Char('X');
        p.iconDiagram = QStringLiteral("✕");
        p.description = QStringLiteral("Return windows to floating");
        p.tileSlots = {};
        presets.append(p);
    }

    return presets;
}

inline QList<QRectF> computeSlotGeometries(const TilingPreset &preset, const QRectF &workArea, int gaps = 6, int margins = 8)
{
    QList<QRectF> geometries;
    if (preset.tileSlots.isEmpty() || workArea.width() <= 100 || workArea.height() <= 100) {
        return geometries;
    }

    const qreal m = static_cast<qreal>(margins);
    const qreal g = static_cast<qreal>(gaps);
    const QRectF innerArea = workArea.adjusted(m, m, -m, -m);
    if (innerArea.width() <= 0 || innerArea.height() <= 0) {
        return geometries;
    }

    for (const auto &slot : preset.tileSlots) {
        const QRectF &rel = slot.relativeRect;
        qreal x = innerArea.x() + rel.x() * innerArea.width();
        qreal y = innerArea.y() + rel.y() * innerArea.height();
        qreal w = rel.width() * innerArea.width();
        qreal h = rel.height() * innerArea.height();

        // Apply gaps: if not touching outer boundary, indent by half gap
        if (rel.left() > 0.001) {
            x += g / 2.0;
            w -= g / 2.0;
        }
        if (rel.right() < 0.999) {
            w -= g / 2.0;
        }
        if (rel.top() > 0.001) {
            y += g / 2.0;
            h -= g / 2.0;
        }
        if (rel.bottom() < 0.999) {
            h -= g / 2.0;
        }

        w = qMax<qreal>(50.0, w);
        h = qMax<qreal>(50.0, h);

        geometries.append(QRectF(x, y, w, h));
    }

    return geometries;
}

inline QString defaultPresetsFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/leapk/tiling_presets.json");
}

inline QJsonObject presetToJson(const TilingPreset &preset)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = preset.id;
    obj[QStringLiteral("name")] = preset.name;
    obj[QStringLiteral("triggerKey")] = QString(preset.triggerKey);
    obj[QStringLiteral("iconDiagram")] = preset.iconDiagram;
    obj[QStringLiteral("description")] = preset.description;

    QJsonArray slotsArr;
    for (const auto &s : preset.tileSlots) {
        QJsonObject sObj;
        sObj[QStringLiteral("x")] = s.relativeRect.x();
        sObj[QStringLiteral("y")] = s.relativeRect.y();
        sObj[QStringLiteral("w")] = s.relativeRect.width();
        sObj[QStringLiteral("h")] = s.relativeRect.height();
        slotsArr.append(sObj);
    }
    obj[QStringLiteral("slots")] = slotsArr;
    return obj;
}

inline TilingPreset jsonToPreset(const QJsonObject &obj)
{
    TilingPreset p;
    p.id = obj.value(QStringLiteral("id")).toString();
    p.name = obj.value(QStringLiteral("name")).toString();
    const QString keyStr = obj.value(QStringLiteral("triggerKey")).toString();
    p.triggerKey = keyStr.isEmpty() ? QChar() : keyStr.at(0).toUpper();
    p.iconDiagram = obj.value(QStringLiteral("iconDiagram")).toString();
    p.description = obj.value(QStringLiteral("description")).toString();

    const QJsonArray slotsArr = obj.value(QStringLiteral("slots")).toArray();
    for (const auto &val : slotsArr) {
        const QJsonObject sObj = val.toObject();
        const qreal x = sObj.value(QStringLiteral("x")).toDouble();
        const qreal y = sObj.value(QStringLiteral("y")).toDouble();
        const qreal w = sObj.value(QStringLiteral("w")).toDouble();
        const qreal h = sObj.value(QStringLiteral("h")).toDouble();
        p.tileSlots.append(TileSlot{QRectF(x, y, w, h)});
    }
    return p;
}

inline QList<TilingPreset> loadPresets(const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultPresetsFilePath() : filePath;
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return defaultPresets();
    }

    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return defaultPresets();
    }

    QList<TilingPreset> presets;
    const QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        if (val.isObject()) {
            presets.append(jsonToPreset(val.toObject()));
        }
    }

    if (presets.isEmpty()) {
        return defaultPresets();
    }
    return presets;
}

inline bool savePresets(const QList<TilingPreset> &presets, const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultPresetsFilePath() : filePath;
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonArray arr;
    for (const auto &p : presets) {
        arr.append(presetToJson(p));
    }

    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

} // namespace LeapKTiling
