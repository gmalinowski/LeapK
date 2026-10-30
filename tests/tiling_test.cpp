/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "tilinghelper.h"
#include <QTemporaryFile>
#undef NDEBUG
#include <cassert>
#include <iostream>

void testDefaultPresets()
{
    using namespace LeapKTiling;

    const auto presets = defaultPresets();
    assert(presets.size() == 6);

    // 1. Two Columns
    assert(presets[0].id == QStringLiteral("columns-2"));
    assert(presets[0].triggerKey == 'A');
    assert(presets[0].tileSlots.size() == 2);
    assert(presets[0].tileSlots[0].relativeRect == QRectF(0.0, 0.0, 0.5, 1.0));
    assert(presets[0].tileSlots[1].relativeRect == QRectF(0.5, 0.0, 0.5, 1.0));
    assert(!presets[0].isFloatAction());

    // 2. Three Columns
    assert(presets[1].id == QStringLiteral("columns-3"));
    assert(presets[1].triggerKey == 'S');
    assert(presets[1].tileSlots.size() == 3);

    // 3. Master & Stack
    assert(presets[2].id == QStringLiteral("master-stack"));
    assert(presets[2].triggerKey == 'D');
    assert(presets[2].tileSlots.size() == 3);

    // 4. Quad Grid
    assert(presets[3].id == QStringLiteral("grid-4"));
    assert(presets[3].triggerKey == 'F');
    assert(presets[3].tileSlots.size() == 4);

    // 5. Monocle
    assert(presets[4].id == QStringLiteral("monocle"));
    assert(presets[4].triggerKey == 'G');
    assert(presets[4].tileSlots.size() == 1);

    // 6. Float
    assert(presets[5].id == QStringLiteral("float"));
    assert(presets[5].triggerKey == 'X');
    assert(presets[5].isFloatAction());
    assert(presets[5].tileSlots.isEmpty());

    std::cout << "testDefaultPresets PASSED\n";
}

void testComputeSlotGeometries()
{
    using namespace LeapKTiling;

    const auto presets = defaultPresets();
    const QRectF workArea(0, 0, 1920, 1080);
    const int gaps = 10;
    const int margins = 20;

    // Test Two Columns (50/50)
    {
        const auto rects = computeSlotGeometries(presets[0], workArea, gaps, margins);
        assert(rects.size() == 2);

        // Inner width = 1920 - 40 = 1880
        // Inner height = 1080 - 40 = 1040
        // Slot 0: left at margin 20, width = 1880/2 - gap/2 = 940 - 5 = 935
        assert(qAbs(rects[0].x() - 20.0) < 0.001);
        assert(qAbs(rects[0].y() - 20.0) < 0.001);
        assert(qAbs(rects[0].width() - 935.0) < 0.001);
        assert(qAbs(rects[0].height() - 1040.0) < 0.001);

        // Slot 1: left at 20 + 940 + gap/2 = 965, width = 940 - 5 = 935
        assert(qAbs(rects[1].x() - 965.0) < 0.001);
        assert(qAbs(rects[1].y() - 20.0) < 0.001);
        assert(qAbs(rects[1].width() - 935.0) < 0.001);
        assert(qAbs(rects[1].height() - 1040.0) < 0.001);

        // Check gap between slot 0 and slot 1
        const qreal actualGap = rects[1].left() - rects[0].right();
        assert(qAbs(actualGap - 10.0) < 0.001);
    }

    // Test Float Action returns empty geometries
    {
        const auto rects = computeSlotGeometries(presets[5], workArea, gaps, margins);
        assert(rects.isEmpty());
    }

    std::cout << "testComputeSlotGeometries PASSED\n";
}

void testJsonPersistence()
{
    using namespace LeapKTiling;

    QTemporaryFile tempFile;
    assert(tempFile.open());
    const QString tempPath = tempFile.fileName();
    tempFile.close();

    const auto defaults = defaultPresets();
    assert(savePresets(defaults, tempPath));

    const auto loaded = loadPresets(tempPath);
    assert(loaded.size() == defaults.size());
    for (int i = 0; i < defaults.size(); ++i) {
        assert(loaded[i].id == defaults[i].id);
        assert(loaded[i].name == defaults[i].name);
        assert(loaded[i].triggerKey == defaults[i].triggerKey);
        assert(loaded[i].iconDiagram == defaults[i].iconDiagram);
        assert(loaded[i].tileSlots.size() == defaults[i].tileSlots.size());
        for (int s = 0; s < defaults[i].tileSlots.size(); ++s) {
            assert(loaded[i].tileSlots[s] == defaults[i].tileSlots[s]);
        }
    }

    std::cout << "testJsonPersistence PASSED\n";
}

void testToKdeJson()
{
    using namespace LeapKTiling;

    const auto presets = defaultPresets();

    // 1. Two columns
    const auto kde2 = presets[0].toKdeJson();
    assert(kde2.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("horizontal"));
    const auto tiles2 = kde2.value(QStringLiteral("tiles")).toArray();
    assert(tiles2.size() == 2);
    assert(qAbs(tiles2[0].toObject().value(QStringLiteral("width")).toDouble() - 0.5) < 0.001);
    assert(qAbs(tiles2[1].toObject().value(QStringLiteral("width")).toDouble() - 0.5) < 0.001);

    // 2. Three columns
    const auto kde3 = presets[1].toKdeJson();
    assert(kde3.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("horizontal"));
    const auto tiles3 = kde3.value(QStringLiteral("tiles")).toArray();
    assert(tiles3.size() == 3);
    assert(qAbs(tiles3[0].toObject().value(QStringLiteral("width")).toDouble() - 0.25) < 0.001);
    assert(qAbs(tiles3[1].toObject().value(QStringLiteral("width")).toDouble() - 0.50) < 0.001);
    assert(qAbs(tiles3[2].toObject().value(QStringLiteral("width")).toDouble() - 0.25) < 0.001);

    // 3. Master & Stack
    const auto kdeMs = presets[2].toKdeJson();
    assert(kdeMs.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("horizontal"));
    const auto tilesMs = kdeMs.value(QStringLiteral("tiles")).toArray();
    assert(tilesMs.size() == 2);
    assert(qAbs(tilesMs[0].toObject().value(QStringLiteral("width")).toDouble() - 0.60) < 0.001);
    const auto stackObj = tilesMs[1].toObject();
    assert(stackObj.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("vertical"));
    assert(qAbs(stackObj.value(QStringLiteral("width")).toDouble() - 0.40) < 0.001);
    const auto stackTiles = stackObj.value(QStringLiteral("tiles")).toArray();
    assert(stackTiles.size() == 2);
    assert(qAbs(stackTiles[0].toObject().value(QStringLiteral("height")).toDouble() - 0.50) < 0.001);
    assert(qAbs(stackTiles[1].toObject().value(QStringLiteral("height")).toDouble() - 0.50) < 0.001);

    // 4. Grid-4
    const auto kdeG4 = presets[3].toKdeJson();
    assert(kdeG4.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("horizontal"));
    const auto colTiles = kdeG4.value(QStringLiteral("tiles")).toArray();
    assert(colTiles.size() == 2);
    assert(colTiles[0].toObject().value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("vertical"));
    assert(colTiles[1].toObject().value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("vertical"));

    // 5. Monocle
    const auto kdeMono = presets[4].toKdeJson();
    assert(kdeMono.value(QStringLiteral("tiles")).toArray().size() == 1);

    // 6. Float
    const auto kdeFloat = presets[5].toKdeJson();
    assert(kdeFloat.value(QStringLiteral("layoutDirection")).toString() == QStringLiteral("floating"));
    assert(kdeFloat.value(QStringLiteral("tiles")).toArray().isEmpty());

    std::cout << "testToKdeJson PASSED\n";
}

void testKdeCustomPreset()
{
    using namespace LeapKTiling;

    // Test with 2 slots
    QList<QRectF> slots2 = {
        QRectF(0.0, 0.0, 0.48, 1.0),
        QRectF(0.48, 0.0, 0.52, 1.0)
    };
    const auto p2 = createKdeCustomPreset(slots2);
    assert(p2.id == QStringLiteral("kde-custom"));
    assert(p2.triggerKey == 'C');
    assert(p2.iconDiagram == QStringLiteral("▌▌"));
    assert(p2.tileSlots.size() == 2);
    assert(!p2.isFloatAction());
    assert(qAbs(p2.tileSlots[0].relativeRect.width() - 0.48) < 0.001);
    assert(qAbs(p2.tileSlots[1].relativeRect.width() - 0.52) < 0.001);

    // Test slot geometries computation for 2 custom slots
    const QRectF workArea(0, 0, 1920, 1080);
    const auto rects = computeSlotGeometries(p2, workArea, 10, 0);
    assert(rects.size() == 2);
    assert(rects[0].left() == 0);
    assert(rects[1].right() == 1920);

    // Test with 3 slots
    QList<QRectF> slots3 = {
        QRectF(0.0, 0.0, 0.33, 1.0),
        QRectF(0.33, 0.0, 0.34, 1.0),
        QRectF(0.67, 0.0, 0.33, 1.0)
    };
    const auto p3 = createKdeCustomPreset(slots3);
    assert(p3.iconDiagram == QStringLiteral("▌▌▌"));
    assert(p3.tileSlots.size() == 3);

    // Test with 4 slots
    QList<QRectF> slots4 = {
        QRectF(0.0, 0.0, 0.5, 0.5),
        QRectF(0.5, 0.0, 0.5, 0.5),
        QRectF(0.0, 0.5, 0.5, 0.5),
        QRectF(0.5, 0.5, 0.5, 0.5)
    };
    const auto p4 = createKdeCustomPreset(slots4);
    assert(p4.iconDiagram == QStringLiteral("⊞"));
    assert(p4.tileSlots.size() == 4);

    std::cout << "testKdeCustomPreset PASSED\n";
}

void testTilingWindowRuleConfig()
{
    using namespace LeapKTiling;

    TilingWindowRuleConfig rule;
    assert(rule.id == QString::fromLatin1(kNoMinSizeRuleId));
    assert(rule.minsize == QStringLiteral("1,1"));
    assert(rule.minsizerule == 2); // Force
    assert(rule.ignoregeometry == true);
    assert(rule.ignoregeometryrule == 2); // Force
    assert(rule.types == 1); // Normal window
    assert(rule.wmclassmatch == 0); // Unimportant match

    std::cout << "testTilingWindowRuleConfig PASSED\n";
}

int main()
{
    testDefaultPresets();
    testComputeSlotGeometries();
    testJsonPersistence();
    testToKdeJson();
    testKdeCustomPreset();
    testTilingWindowRuleConfig();

    std::cout << "All LeapKTiling tests passed successfully!\n";
    return 0;
}
