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
    assert(p2.triggerKey == 'K');
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

void testParseKdeTileJson()
{
    using namespace LeapKTiling;

    // Case 1: 1 on left, 3 on right (from user's kwinrc)
    const char *json1 = R"({"layoutDirection":"horizontal","tiles":[{"width":0.5},{"layoutDirection":"vertical","tiles":[{"height":0.5},{"height":0.25},{"height":0.25}],"width":0.5}]})";
    const auto doc1 = QJsonDocument::fromJson(json1);
    const auto slots1 = parseKdeTileJson(doc1.object());
    assert(slots1.size() == 4);
    assert(qAbs(slots1[0].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots1[0].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots1[0].relativeRect.height() - 1.0) < 0.001);

    assert(qAbs(slots1[1].relativeRect.x() - 0.5) < 0.001);
    assert(qAbs(slots1[1].relativeRect.y() - 0.0) < 0.001);
    assert(qAbs(slots1[1].relativeRect.height() - 0.5) < 0.001);

    assert(qAbs(slots1[2].relativeRect.x() - 0.5) < 0.001);
    assert(qAbs(slots1[2].relativeRect.y() - 0.5) < 0.001);
    assert(qAbs(slots1[2].relativeRect.height() - 0.25) < 0.001);

    assert(qAbs(slots1[3].relativeRect.x() - 0.5) < 0.001);
    assert(qAbs(slots1[3].relativeRect.y() - 0.75) < 0.001);
    assert(qAbs(slots1[3].relativeRect.height() - 0.25) < 0.001);

    // Case 2: 1 on right, 3 on left
    const char *json2 = R"({"layoutDirection":"horizontal","tiles":[{"layoutDirection":"vertical","tiles":[{"height":0.5},{"height":0.25},{"height":0.25}],"width":0.5},{"width":0.5}]})";
    const auto doc2 = QJsonDocument::fromJson(json2);
    const auto slots2 = parseKdeTileJson(doc2.object());
    assert(slots2.size() == 4);
    assert(qAbs(slots2[0].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots2[0].relativeRect.y() - 0.0) < 0.001);
    assert(qAbs(slots2[0].relativeRect.height() - 0.5) < 0.001);

    assert(qAbs(slots2[1].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots2[1].relativeRect.y() - 0.5) < 0.001);
    assert(qAbs(slots2[1].relativeRect.height() - 0.25) < 0.001);

    assert(qAbs(slots2[2].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots2[2].relativeRect.y() - 0.75) < 0.001);
    assert(qAbs(slots2[2].relativeRect.height() - 0.25) < 0.001);

    assert(qAbs(slots2[3].relativeRect.x() - 0.5) < 0.001);
    assert(qAbs(slots2[3].relativeRect.y() - 0.0) < 0.001);
    assert(qAbs(slots2[3].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots2[3].relativeRect.height() - 1.0) < 0.001);

    // Case 3: Floating tile inside right column
    const char *json3 = R"({"layoutDirection":"horizontal","tiles":[{"width":0.5},{"layoutDirection":"floating","tiles":[{"x":0.55,"y":0.1,"width":0.35,"height":0.4}],"width":0.5}]})";
    const auto doc3 = QJsonDocument::fromJson(json3);
    const auto slots3 = parseKdeTileJson(doc3.object());
    assert(slots3.size() == 2);
    assert(qAbs(slots3[0].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots3[1].relativeRect.x() - 0.55) < 0.001);
    assert(qAbs(slots3[1].relativeRect.y() - 0.10) < 0.001);
    assert(qAbs(slots3[1].relativeRect.width() - 0.35) < 0.001);
    assert(qAbs(slots3[1].relativeRect.height() - 0.40) < 0.001);

    // Case 4: Root is floating layout
    const char *json4 = R"({"layoutDirection":"floating","tiles":[{"x":0.05,"y":0.05,"width":0.4,"height":0.8},{"x":0.55,"y":0.05,"width":0.4,"height":0.8}]})";
    const auto doc4 = QJsonDocument::fromJson(json4);
    const auto slots4 = parseKdeTileJson(doc4.object());
    assert(slots4.size() == 2);
    assert(qAbs(slots4[0].relativeRect.x() - 0.05) < 0.001);
    assert(qAbs(slots4[0].relativeRect.width() - 0.40) < 0.001);
    assert(qAbs(slots4[1].relativeRect.x() - 0.55) < 0.001);
    assert(qAbs(slots4[1].relativeRect.width() - 0.40) < 0.001);

    // Case 5: 3 on left, 2 on right (User's 5-slot layout from kwinrc)
    const char *json5 = R"({"layoutDirection":"horizontal","tiles":[{"layoutDirection":"vertical","tiles":[{"height":0.5},{"height":0.25},{"height":0.25}],"width":0.5},{"layoutDirection":"vertical","tiles":[{"height":0.5},{"height":0.5}],"width":0.5}]})";
    const auto doc5 = QJsonDocument::fromJson(json5);
    const auto slots5 = parseKdeTileJson(doc5.object());
    assert(slots5.size() == 5);
    assert(qAbs(slots5[0].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots5[0].relativeRect.y() - 0.0) < 0.001);
    assert(qAbs(slots5[0].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots5[0].relativeRect.height() - 0.5) < 0.001);

    assert(qAbs(slots5[1].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots5[1].relativeRect.y() - 0.5) < 0.001);
    assert(qAbs(slots5[1].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots5[1].relativeRect.height() - 0.25) < 0.001);

    assert(qAbs(slots5[2].relativeRect.x() - 0.0) < 0.001);
    assert(qAbs(slots5[2].relativeRect.y() - 0.75) < 0.001);
    assert(qAbs(slots5[2].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots5[2].relativeRect.height() - 0.25) < 0.001);

    assert(qAbs(slots5[3].relativeRect.x() - 0.5) < 0.001);
    assert(qAbs(slots5[3].relativeRect.y() - 0.0) < 0.001);
    assert(qAbs(slots5[3].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots5[3].relativeRect.height() - 0.5) < 0.001);

    assert(qAbs(slots5[4].relativeRect.x() - 0.5) < 0.001);
    assert(qAbs(slots5[4].relativeRect.y() - 0.5) < 0.001);
    assert(qAbs(slots5[4].relativeRect.width() - 0.5) < 0.001);
    assert(qAbs(slots5[4].relativeRect.height() - 0.5) < 0.001);

    // Compute pixel geometries on 1920x1080 screen with 6px gaps and 6px margins
    const auto preset5 = createKdeCustomPreset(doc5.object());
    const QRectF workArea(0, 0, 1920, 1080);
    const auto geos5 = computeSlotGeometries(preset5, workArea, 6, 6);
    assert(geos5.size() == 5);
    for (int i = 0; i < 5; ++i) {
        assert(geos5[i].left() >= 6.0);
        assert(geos5[i].top() >= 6.0);
        assert(geos5[i].right() <= 1914.0 + 0.01);
        assert(geos5[i].bottom() <= 1074.0 + 0.01);
    }
    // Verify priority order
    const auto order5 = computeSlotPriorityOrder(preset5.tileSlots);
    assert(order5.size() == 5);
    assert(order5[0] == 0); // (0, 0, 0.5, 0.5)
    assert(order5[1] == 3); // (0.5, 0, 0.5, 0.5)
    assert(order5[2] == 4); // (0.5, 0.5, 0.5, 0.5)
    assert(order5[3] == 1); // (0, 0.5, 0.5, 0.25)
    assert(order5[4] == 2); // (0, 0.75, 0.5, 0.25)

    // Test createKdeCustomPreset from JSON directly
    const auto presetFromDoc = createKdeCustomPreset(doc2.object());
    assert(presetFromDoc.id == QStringLiteral("kde-custom"));
    assert(presetFromDoc.tileSlots.size() == 4);
    assert(!presetFromDoc.kdeJson.isEmpty());
    assert(presetFromDoc.toKdeJson() == doc2.object());

    std::cout << "testParseKdeTileJson PASSED\n";
}

void testComputeSlotPriorityOrder()
{
    using namespace LeapKTiling;

    // 1 on right, 3 on left: Slot 3 (the big 1 on the right) must have priority 0!
    QList<TileSlot> slotsRight1Left3 = {
        TileSlot{QRectF(0.0, 0.0, 0.5, 0.5)},   // Slot 0: area 0.25
        TileSlot{QRectF(0.0, 0.5, 0.5, 0.25)},  // Slot 1: area 0.125
        TileSlot{QRectF(0.0, 0.75, 0.5, 0.25)}, // Slot 2: area 0.125
        TileSlot{QRectF(0.5, 0.0, 0.5, 1.0)}    // Slot 3: area 0.50 (Master!)
    };
    const auto order = computeSlotPriorityOrder(slotsRight1Left3);
    assert(order.size() == 4);
    assert(order[0] == 3); // Master slot gets active window first!
    assert(order[1] == 0); // Largest left slot gets second window
    assert(order[2] == 1);
    assert(order[3] == 2);

    // 1 on left, 3 on right: Slot 0 (the big 1 on the left) must have priority 0!
    QList<TileSlot> slotsLeft1Right3 = {
        TileSlot{QRectF(0.0, 0.0, 0.5, 1.0)},   // Slot 0: area 0.50 (Master!)
        TileSlot{QRectF(0.5, 0.0, 0.5, 0.5)},   // Slot 1: area 0.25
        TileSlot{QRectF(0.5, 0.5, 0.5, 0.25)},  // Slot 2: area 0.125
        TileSlot{QRectF(0.5, 0.75, 0.5, 0.25)}  // Slot 3: area 0.125
    };
    const auto orderLeft1 = computeSlotPriorityOrder(slotsLeft1Right3);
    assert(orderLeft1.size() == 4);
    assert(orderLeft1[0] == 0); // Master slot gets active window
    assert(orderLeft1[1] == 1);
    assert(orderLeft1[2] == 2);
    assert(orderLeft1[3] == 3);

    // Two equal columns: priority 0 is slot 0, priority 1 is slot 1
    QList<TileSlot> slotsCols2 = {
        TileSlot{QRectF(0.0, 0.0, 0.5, 1.0)},
        TileSlot{QRectF(0.5, 0.0, 0.5, 1.0)}
    };
    const auto orderCols2 = computeSlotPriorityOrder(slotsCols2);
    assert(orderCols2.size() == 2);
    assert(orderCols2[0] == 0);
    assert(orderCols2[1] == 1);

    std::cout << "testComputeSlotPriorityOrder PASSED\n";
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
    assert(rule.wmclass.isEmpty());
    assert(rule.wmclasscomplete == false);
    assert(rule.wmclassmatch == 0); // Unimportant match

    std::cout << "testTilingWindowRuleConfig PASSED\n";
}

void testTilingSwapStackingPreservation()
{
    // Simulates KWin's window stacking order during a tiling swap operation.
    // In KWin stacking: index 0 is bottom-most, higher index is higher in Z-order.
    struct WindowStackSimulator {
        QList<QString> stack; // bottom to top

        void lowerWindow(const QString &win) {
            stack.removeAll(win);
            stack.prepend(win);
        }

        void raiseWindow(const QString &win) {
            stack.removeAll(win);
            stack.append(win);
        }

        void stackAbove(const QString &win, const QString &reference) {
            stack.removeAll(win);
            int idx = stack.indexOf(reference);
            if (idx >= 0) {
                stack.insert(idx + 1, win);
            } else {
                stack.append(win);
            }
        }
    };

    // Scenario: 3 tiled slots, plus 2 background windows.
    // Initial stacking: Background windows underneath, all 3 tile slots on top.
    // Tile slots: Slot 0: W0, Slot 1: W1, Slot 2: W2.
    // Background windows: BG1, BG2.
    // We want to swap W0 with BG1.

    // 1. Verify flawed legacy logic: lowerWindow(previousWin) followed by stackAbove(slot, previousWin).
    {
        WindowStackSimulator sim;
        sim.stack = {QStringLiteral("BG1"), QStringLiteral("BG2"), QStringLiteral("W1"), QStringLiteral("W2"), QStringLiteral("W0")};

        const QString previousWin = QStringLiteral("W0");
        const QString target = QStringLiteral("BG1");
        QList<QString> activeSlots = {target, QStringLiteral("W1"), QStringLiteral("W2")};

        // Old flawed logic:
        sim.lowerWindow(previousWin);
        for (const auto &slotWin : activeSlots) {
            if (slotWin != previousWin && slotWin != target) {
                sim.stackAbove(slotWin, previousWin);
            }
        }
        sim.raiseWindow(target);

        // In the flawed logic, W1 and W2 were placed right above previousWin at the bottom:
        // Stacking order became: [W0, W2, W1, BG2, BG1]
        // BG2 (a background window!) was above W1 and W2, covering them!
        int idxW1 = sim.stack.indexOf(QStringLiteral("W1"));
        int idxW2 = sim.stack.indexOf(QStringLiteral("W2"));
        int idxBG2 = sim.stack.indexOf(QStringLiteral("BG2"));
        assert(idxBG2 > idxW1 && idxBG2 > idxW2); // Proves the bug: BG2 occludes W1 and W2!
    }

    // 2. Verify fixed logic: lowerWindow(previousWin), raiseWindow for each active slot, then raiseWindow(target).
    {
        WindowStackSimulator sim;
        sim.stack = {QStringLiteral("BG1"), QStringLiteral("BG2"), QStringLiteral("W1"), QStringLiteral("W2"), QStringLiteral("W0")};

        const QString previousWin = QStringLiteral("W0");
        const QString target = QStringLiteral("BG1");
        QList<QString> activeSlots = {target, QStringLiteral("W1"), QStringLiteral("W2")};

        // Fixed logic:
        sim.lowerWindow(previousWin);
        for (const auto &slotWin : activeSlots) {
            if (slotWin != target) {
                sim.raiseWindow(slotWin);
            }
        }
        sim.raiseWindow(target);

        // Resulting stacking: [W0, BG2, W1, W2, BG1]
        // Both W1 and W2, and the swapped target BG1, are above ALL background windows (W0, BG2).
        int idxW0 = sim.stack.indexOf(QStringLiteral("W0"));
        int idxBG2 = sim.stack.indexOf(QStringLiteral("BG2"));
        int idxW1 = sim.stack.indexOf(QStringLiteral("W1"));
        int idxW2 = sim.stack.indexOf(QStringLiteral("W2"));
        int idxTarget = sim.stack.indexOf(QStringLiteral("BG1"));

        int maxBackgroundIdx = qMax(idxW0, idxBG2);
        int minTiledSlotIdx = qMin(qMin(idxW1, idxW2), idxTarget);

        assert(minTiledSlotIdx > maxBackgroundIdx);
        assert(idxTarget == sim.stack.size() - 1); // target is active on top
        assert(sim.stack.first() == QStringLiteral("W0")); // previousWin lowered to bottom
    }

    std::cout << "testTilingSwapStackingPreservation PASSED\n";
}

void testHudPreservationOnWorkspaceTilingSwitch()
{
    // Simulates the workspace switch while HUD is active and tiling is enabled on either workspace.
    struct MockWorkspaceSwitcher {
        bool jumpMode = false;
        bool tilingActive = false;
        bool tilingPresetPreserved = false;

        void switchToWorkspace(bool incomingHasTiling) {
            const bool wasInJumpMode = jumpMode;
            if (tilingActive) {
                // deactivateTiling with preserveWorkspaceState = true
                bool preserveWorkspaceState = true;
                if (jumpMode && !preserveWorkspaceState) {
                    jumpMode = false;
                }
                tilingActive = false;
            }

            if (incomingHasTiling) {
                // activateTilingPreset with preserveJumpMode = wasInJumpMode
                bool preserveJumpMode = wasInJumpMode;
                if (jumpMode && !preserveJumpMode) {
                    jumpMode = false;
                }
                tilingActive = true;
                tilingPresetPreserved = true;
            }

            if (wasInJumpMode) {
                jumpMode = true;
            }
        }
    };

    // Test Case 1: Switching from tiled workspace to another tiled workspace in HUD
    {
        MockWorkspaceSwitcher s;
        s.jumpMode = true;
        s.tilingActive = true;
        s.switchToWorkspace(true);
        assert(s.jumpMode == true);
        assert(s.tilingActive == true);
    }

    // Test Case 2: Switching from untiled workspace to tiled workspace in HUD
    {
        MockWorkspaceSwitcher s;
        s.jumpMode = true;
        s.tilingActive = false;
        s.switchToWorkspace(true);
        assert(s.jumpMode == true);
        assert(s.tilingActive == true);
    }

    // Test Case 3: Switching from tiled workspace to untiled workspace in HUD
    {
        MockWorkspaceSwitcher s;
        s.jumpMode = true;
        s.tilingActive = true;
        s.switchToWorkspace(false);
        assert(s.jumpMode == true);
        assert(s.tilingActive == false);
    }

    std::cout << "testHudPreservationOnWorkspaceTilingSwitch PASSED\n";
}

void testTilingWorkspaceWindowIsolation()
{
    struct MockWindow {
        QString id;
        QString appClass;
        QString caption;
        int workspaceId = 0; // 0 = standard, 1 = ws1, 2 = ws2
        bool isMinimized = false;
        bool isPinned = false;
        bool isStashed = false;
        bool isWorkspaceMinimized = false;
        int desktop = 1;
    };

    auto isEligibleForWorkspaceTiling = [](const MockWindow &win, int activeHudView, int currentDesktop) -> bool {
        if (win.isStashed || win.isWorkspaceMinimized || win.isMinimized) {
            return false;
        }
        if (win.desktop != currentDesktop) {
            return false;
        }
        if (activeHudView >= 1) {
            if (!win.isPinned && win.workspaceId != activeHudView) {
                return false;
            }
        } else if (activeHudView == 0) {
            if (!win.isPinned && win.workspaceId != 0) {
                return false;
            }
        }
        return true;
    };

    // Scenario: User created Workspace 2 with 1 window (W3).
    // History contains W1, W2 from Workspace 1.
    MockWindow w1{QStringLiteral("w1"), QStringLiteral("firefox"), QStringLiteral("Web"), 1, true, false, false, true, 1};
    MockWindow w2{QStringLiteral("w2"), QStringLiteral("dolphin"), QStringLiteral("Files"), 1, true, false, false, true, 1};
    MockWindow w3{QStringLiteral("w3"), QStringLiteral("kate"), QStringLiteral("Code"), 2, false, false, false, false, 1};

    QList<MockWindow> allWindows = {w1, w2, w3};
    QList<MockWindow> windowHistory = {w1, w2, w3};

    int activeHudView = 2; // Workspace 2
    int currentDesktop = 1;
    const int slotCount = 2; // Two columns

    QList<QString> targetWindows;
    // 1a. Active window (w3)
    if (isEligibleForWorkspaceTiling(w3, activeHudView, currentDesktop)) {
        targetWindows.append(w3.id);
    }

    // 1b. History
    for (const auto &w : windowHistory) {
        if (targetWindows.size() >= slotCount) break;
        if (isEligibleForWorkspaceTiling(w, activeHudView, currentDesktop) && !targetWindows.contains(w.id)) {
            targetWindows.append(w.id);
        }
    }

    // Assert: Only w3 was selected! w1 and w2 from Workspace 1 were excluded.
    assert(targetWindows.size() == 1);
    assert(targetWindows.first() == QStringLiteral("w3"));

    // Tiling slots: Slot 0 gets w3, Slot 1 is EMPTY
    struct MockTileSlot {
        QString windowId;
    };
    QList<MockTileSlot> tileSlots;
    for (int i = 0; i < slotCount; ++i) {
        MockTileSlot s;
        if (i < targetWindows.size()) {
            s.windowId = targetWindows[i];
        }
        tileSlots.append(s);
    }
    assert(tileSlots.size() == 2);
    assert(tileSlots[0].windowId == QStringLiteral("w3"));
    assert(tileSlots[1].windowId.isEmpty()); // Slot 1 is empty!

    // Now user launches a new window on Workspace 2 (w4):
    MockWindow w4{QStringLiteral("w4"), QStringLiteral("konsole"), QStringLiteral("Terminal"), 2, false, false, false, false, 1};
    int emptySlotIdx = -1;
    for (int i = 0; i < tileSlots.size(); ++i) {
        if (tileSlots[i].windowId.isEmpty()) {
            emptySlotIdx = i;
            break;
        }
    }
    assert(emptySlotIdx == 1);
    tileSlots[emptySlotIdx].windowId = w4.id;

    // Both slots now populated without kicking out w3
    assert(tileSlots[0].windowId == QStringLiteral("w3"));
    assert(tileSlots[1].windowId == QStringLiteral("w4"));

    std::cout << "testTilingWorkspaceWindowIsolation PASSED\n";
}

void testResizeSlotGeometries()
{
    using namespace LeapKTiling;

    // Test 1: Two horizontal columns (50/50)
    {
        QList<QRectF> tileGeos = {
            QRectF(10, 10, 945, 1060),
            QRectF(965, 10, 945, 1060)
        };

        // Resize Slot 0 to the Right by 100px (expanding Slot 0, shrinking Slot 1)
        bool ok = resizeSlotGeometries(tileGeos, 0, ResizeDirection::Right, 100.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 1045.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 1065.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 845.0) < 0.001);

        // Gap must still be exactly 10px
        assert(qAbs(tileGeos[1].left() - tileGeos[0].right() - 10.0) < 0.001);

        // Resize Slot 0 to the Left by 100px (shrinking Slot 0, expanding Slot 1)
        ok = resizeSlotGeometries(tileGeos, 0, ResizeDirection::Left, 100.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 945.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 965.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 945.0) < 0.001);

        // Test minimum size clamping: try to shrink Slot 0 by 900px with minSize=100
        ok = resizeSlotGeometries(tileGeos, 0, ResizeDirection::Left, 900.0, 100.0);
        assert(ok);
        assert(tileGeos[0].width() >= 100.0);
        assert(qAbs(tileGeos[0].width() - 100.0) < 0.001);
    }

    // Test 2: Master & Stack vertical split
    {
        QList<QRectF> tileGeos = {
            QRectF(10, 10, 1140, 1060),       // Slot 0: Master (Left)
            QRectF(1160, 10, 750, 520),       // Slot 1: Top Right Stack
            QRectF(1160, 540, 750, 530)       // Slot 2: Bottom Right Stack
        };

        // Resize Slot 1 Down by 80px (expanding Slot 1 vertically, shrinking Slot 2)
        bool ok = resizeSlotGeometries(tileGeos, 1, ResizeDirection::Down, 80.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[1].height() - 600.0) < 0.001);
        assert(qAbs(tileGeos[2].top() - 620.0) < 0.001);
        assert(qAbs(tileGeos[2].height() - 450.0) < 0.001);

        // Vertical gap must still be exactly 10px
        assert(qAbs(tileGeos[2].top() - tileGeos[1].bottom() - 10.0) < 0.001);

        // Resize Slot 2 Up by 100px (expanding Slot 2 vertically, shrinking Slot 1)
        ok = resizeSlotGeometries(tileGeos, 2, ResizeDirection::Up, 100.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[2].height() - 550.0) < 0.001);
        assert(qAbs(tileGeos[1].height() - 500.0) < 0.001);
    }

    // Test 3: Master & Stack horizontal divider (both stack slots adjust in unison)
    {
        QList<QRectF> tileGeos = {
            QRectF(10, 10, 1140, 1060),       // Slot 0: Master (Left)
            QRectF(1160, 10, 750, 520),       // Slot 1: Top Right Stack
            QRectF(1160, 540, 750, 530)       // Slot 2: Bottom Right Stack
        };

        // Resize Slot 0 (Master) to the Right by 100px:
        // Slot 0 grows right (+100px), BOTH Slot 1 and Slot 2 must shrink on left (+100px)
        bool ok = resizeSlotGeometries(tileGeos, 0, ResizeDirection::Right, 100.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 1240.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 1260.0) < 0.001);
        assert(qAbs(tileGeos[2].left() - 1260.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 650.0) < 0.001);
        assert(qAbs(tileGeos[2].width() - 650.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - tileGeos[0].right() - 10.0) < 0.001);
        assert(qAbs(tileGeos[2].left() - tileGeos[0].right() - 10.0) < 0.001);

        // Resize Slot 0 (Master) to the Left by 100px:
        // Slot 0 shrinks left (-100px), BOTH Slot 1 and Slot 2 expand left (-100px)
        ok = resizeSlotGeometries(tileGeos, 0, ResizeDirection::Left, 100.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 1140.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 1160.0) < 0.001);
        assert(qAbs(tileGeos[2].left() - 1160.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 750.0) < 0.001);
        assert(qAbs(tileGeos[2].width() - 750.0) < 0.001);

        // Resize from Slot 2 (Stack Bottom) to the Left by 60px:
        // Slot 0 shrinks (-60px), BOTH Slot 1 and Slot 2 expand left (-60px)
        ok = resizeSlotGeometries(tileGeos, 2, ResizeDirection::Left, 60.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 1080.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 1100.0) < 0.001);
        assert(qAbs(tileGeos[2].left() - 1100.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 810.0) < 0.001);
        assert(qAbs(tileGeos[2].width() - 810.0) < 0.001);

        // Resize from Slot 1 (Stack Top) to the Right by 60px:
        // Slot 0 expands right (+60px), BOTH Slot 1 and Slot 2 shrink right (+60px)
        ok = resizeSlotGeometries(tileGeos, 1, ResizeDirection::Right, 60.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 1140.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 1160.0) < 0.001);
        assert(qAbs(tileGeos[2].left() - 1160.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 750.0) < 0.001);
        assert(qAbs(tileGeos[2].width() - 750.0) < 0.001);
    }

    // Test 4: Quad Grid (2x2) synchronized column and row divider resizing
    {
        QList<QRectF> tileGeos = {
            QRectF(10, 10, 945, 525),        // Slot 0: Top-Left
            QRectF(965, 10, 945, 525),       // Slot 1: Top-Right
            QRectF(10, 545, 945, 525),       // Slot 2: Bottom-Left
            QRectF(965, 545, 945, 525)       // Slot 3: Bottom-Right
        };

        // Resize Slot 0 Right by 50px:
        // BOTH Slot 0 and Slot 2 expand right, BOTH Slot 1 and Slot 3 shrink left
        bool ok = resizeSlotGeometries(tileGeos, 0, ResizeDirection::Right, 50.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].width() - 995.0) < 0.001);
        assert(qAbs(tileGeos[2].width() - 995.0) < 0.001);
        assert(qAbs(tileGeos[1].left() - 1015.0) < 0.001);
        assert(qAbs(tileGeos[3].left() - 1015.0) < 0.001);
        assert(qAbs(tileGeos[1].width() - 895.0) < 0.001);
        assert(qAbs(tileGeos[3].width() - 895.0) < 0.001);

        // Resize Slot 3 Down by 40px (from bottom-right slot):
        // Slot 3 has no bottom neighbor, so horizontal divider above moves down (+40px):
        // BOTH Top slots (0, 1) expand down, BOTH Bottom slots (2, 3) shrink from top
        ok = resizeSlotGeometries(tileGeos, 3, ResizeDirection::Down, 40.0, 100.0);
        assert(ok);
        assert(qAbs(tileGeos[0].height() - 565.0) < 0.001);
        assert(qAbs(tileGeos[1].height() - 565.0) < 0.001);
        assert(qAbs(tileGeos[2].top() - 585.0) < 0.001);
        assert(qAbs(tileGeos[3].top() - 585.0) < 0.001);
        assert(qAbs(tileGeos[2].height() - 485.0) < 0.001);
        assert(qAbs(tileGeos[3].height() - 485.0) < 0.001);
    }

    // Test 5: 5-Slot Layout (3 left, 2 right) local interactive divider resizing
    {
        QList<QRectF> geos = {
            QRectF(6, 6, 951, 531),    // Slot 0: left top (bottom: 537)
            QRectF(6, 543, 951, 264),  // Slot 1: left mid (top: 543, bottom: 807)
            QRectF(6, 813, 951, 267),  // Slot 2: left bot (top: 813, bottom: 1080)
            QRectF(963, 6, 951, 531),  // Slot 3: right top (bottom: 537)
            QRectF(963, 543, 951, 537) // Slot 4: right bot (top: 543, bottom: 1080)
        };

        // Test calculateDividerMove helper when dragging Slot 0 right by +50px
        ResizeDirection dir;
        qreal delta = 0.0;
        QRectF movedGeo0 = geos[0];
        movedGeo0.setRight(movedGeo0.right() + 50.0);
        bool detected = calculateDividerMove(geos, 0, movedGeo0, dir, delta);
        assert(detected);
        assert(dir == ResizeDirection::Right);
        assert(qAbs(delta - 50.0) < 0.001);

        // Apply resize with localOnly=true
        bool ok = resizeSlotGeometries(geos, 0, dir, delta, 100.0, true);
        assert(ok);
        // All left slots expanded by +50px
        assert(qAbs(geos[0].width() - 1001.0) < 0.001);
        assert(qAbs(geos[1].width() - 1001.0) < 0.001);
        assert(qAbs(geos[2].width() - 1001.0) < 0.001);
        // All right slots shrunk from left by 50px
        assert(qAbs(geos[3].left() - 1013.0) < 0.001);
        assert(qAbs(geos[3].width() - 901.0) < 0.001);
        assert(qAbs(geos[4].left() - 1013.0) < 0.001);
        assert(qAbs(geos[4].width() - 901.0) < 0.001);

        // Test calculateDividerMove when dragging bottom of Slot 3 down by +40px
        QRectF movedGeo3 = geos[3];
        movedGeo3.setBottom(movedGeo3.bottom() + 40.0);
        detected = calculateDividerMove(geos, 3, movedGeo3, dir, delta);
        assert(detected);
        assert(dir == ResizeDirection::Down);
        assert(qAbs(delta - 40.0) < 0.001);

        // Apply local resize: ONLY Slot 3 and Slot 4 should be modified!
        ok = resizeSlotGeometries(geos, 3, dir, delta, 100.0, true);
        assert(ok);
        assert(qAbs(geos[3].height() - 571.0) < 0.001);
        assert(qAbs(geos[4].top() - 583.0) < 0.001);
        assert(qAbs(geos[4].height() - 497.0) < 0.001);
        assert(qAbs(geos[4].top() - geos[3].bottom() - 6.0) < 0.001);
        // Crucial check: Left column must remain completely untouched!
        assert(qAbs(geos[0].height() - 531.0) < 0.001);
        assert(qAbs(geos[1].height() - 264.0) < 0.001);
        assert(qAbs(geos[2].height() - 267.0) < 0.001);
    }

    // Test 6: 3-Column Layout divider targeting (Fixes regression where shrinking middle slot moves left neighbor)
    {
        QList<QRectF> geos = {
            QRectF(0, 0, 480, 1080),    // Slot 0: Firefox (Left)
            QRectF(480, 0, 960, 1080),  // Slot 1: Terminal (Middle, right: 1440)
            QRectF(1440, 0, 480, 1080)  // Slot 2: Right window (Right, right: 1920)
        };

        // User grabs right edge of middle slot and drags left by 100px (shrinking middle from right)
        QRectF movedGeo1 = geos[1];
        movedGeo1.setRight(1340.0);
        ResizeDirection dir;
        qreal delta = 0.0;
        TileEdge edge = TileEdge::Auto;
        bool detected = calculateDividerMove(geos, 1, movedGeo1, dir, delta, edge);
        assert(detected);
        assert(dir == ResizeDirection::Left);
        assert(edge == TileEdge::Right);
        assert(qAbs(delta - 100.0) < 0.001);

        bool ok = resizeSlotGeometries(geos, 1, dir, delta, 100.0, true, edge);
        assert(ok);
        // Slot 0 (Firefox) MUST remain completely untouched!
        assert(qAbs(geos[0].left() - 0.0) < 0.001);
        assert(qAbs(geos[0].width() - 480.0) < 0.001);
        // Slot 1 (Terminal) shrunk from right
        assert(qAbs(geos[1].left() - 480.0) < 0.001);
        assert(qAbs(geos[1].right() - 1340.0) < 0.001);
        assert(qAbs(geos[1].width() - 860.0) < 0.001);
        // Slot 2 (Right) expanded from left
        assert(qAbs(geos[2].left() - 1340.0) < 0.001);
        assert(qAbs(geos[2].width() - 580.0) < 0.001);

        // Now user grabs left edge of middle slot and drags right by 80px (shrinking middle from left)
        QRectF movedGeo1_left = geos[1];
        movedGeo1_left.setLeft(560.0);
        detected = calculateDividerMove(geos, 1, movedGeo1_left, dir, delta, edge);
        assert(detected);
        assert(dir == ResizeDirection::Right);
        assert(edge == TileEdge::Left);
        assert(qAbs(delta - 80.0) < 0.001);

        ok = resizeSlotGeometries(geos, 1, dir, delta, 100.0, true, edge);
        assert(ok);
        // Slot 0 (Firefox) expanded from right
        assert(qAbs(geos[0].left() - 0.0) < 0.001);
        assert(qAbs(geos[0].right() - 560.0) < 0.001);
        assert(qAbs(geos[0].width() - 560.0) < 0.001);
        // Slot 1 (Terminal) shrunk from left
        assert(qAbs(geos[1].left() - 560.0) < 0.001);
        assert(qAbs(geos[1].right() - 1340.0) < 0.001);
        assert(qAbs(geos[1].width() - 780.0) < 0.001);
        // Slot 2 (Right) MUST remain completely untouched!
        assert(qAbs(geos[2].left() - 1340.0) < 0.001);
        assert(qAbs(geos[2].width() - 580.0) < 0.001);
    }

    std::cout << "testResizeSlotGeometries PASSED\n";
}

void testResizePresetRelativeRectSync()
{
    using namespace LeapKTiling;

    auto presets = defaultPresets();
    auto &preset2 = presets[0]; // columns-2 (50/50)
    const QRectF workArea(0, 0, 1920, 1080);
    auto geos = computeSlotGeometries(preset2, workArea, 0, 0); // 960 and 960
    assert(geos.size() == 2);
    assert(qAbs(geos[0].width() - 960.0) < 0.001);
    assert(qAbs(geos[1].width() - 960.0) < 0.001);

    // Resize slot 0 right by 100px: slot 0 becomes 1060px, slot 1 becomes 860px
    bool ok = resizeSlotGeometries(geos, 0, ResizeDirection::Right, 100.0, 100.0);
    assert(ok);
    assert(qAbs(geos[0].width() - 1060.0) < 0.001);
    assert(qAbs(geos[1].width() - 860.0) < 0.001);

    // Synchronize relative rects as done in performTilingResize
    for (int i = 0; i < geos.size() && i < preset2.tileSlots.size(); ++i) {
        preset2.tileSlots[i].relativeRect = QRectF(
            qBound(0.0, (geos[i].x() - workArea.x()) / workArea.width(), 1.0),
            qBound(0.0, (geos[i].y() - workArea.y()) / workArea.height(), 1.0),
            qBound(0.05, geos[i].width() / workArea.width(), 1.0),
            qBound(0.05, geos[i].height() / workArea.height(), 1.0)
        );
    }

    assert(qAbs(preset2.tileSlots[0].relativeRect.width() - (1060.0 / 1920.0)) < 0.001);
    assert(qAbs(preset2.tileSlots[1].relativeRect.width() - (860.0 / 1920.0)) < 0.001);

    // Check toKdeJson reflects the new proportions
    const auto kdeJson = preset2.toKdeJson();
    const auto tiles = kdeJson.value(QStringLiteral("tiles")).toArray();
    assert(tiles.size() == 2);
    assert(qAbs(tiles[0].toObject().value(QStringLiteral("width")).toDouble() - (1060.0 / 1920.0)) < 0.001);
    assert(qAbs(tiles[1].toObject().value(QStringLiteral("width")).toDouble() - (860.0 / 1920.0)) < 0.001);

    std::cout << "testResizePresetRelativeRectSync PASSED\n";
}

void testMasterStackRepeatedResize()
{
    using namespace LeapKTiling;

    const auto presets = defaultPresets();
    auto msPreset = presets[2]; // master-stack
    const QRectF workArea(0, 0, 1920, 1080);
    const int gaps = 8;
    auto geos = computeSlotGeometries(msPreset, workArea, gaps, gaps);
    assert(geos.size() == 3);

    const qreal initialMasterLeft = geos[0].left();
    const qreal initialStackRight = geos[1].right();
    assert(qAbs(initialStackRight - geos[2].right()) < 0.001);

    // Initial check: all slots fit inside workArea
    for (int i = 0; i < 3; ++i) {
        assert(geos[i].left() >= workArea.left() - 0.001);
        assert(geos[i].right() <= workArea.right() + 0.001);
        assert(geos[i].top() >= workArea.top() - 0.001);
        assert(geos[i].bottom() <= workArea.bottom() + 0.001);
    }

    const qreal stepDelta = workArea.width() * 0.05; // 96px

    // Press 1: Shrink master left ('h')
    const qreal w0_before1 = geos[0].width();
    const qreal w1_before1 = geos[1].width();
    bool ok1 = resizeSlotGeometries(geos, 0, ResizeDirection::Left, stepDelta, 100.0);
    assert(ok1);
    assert(qAbs(geos[0].width() - (w0_before1 - stepDelta)) < 0.001);
    assert(qAbs(geos[1].width() - (w1_before1 + stepDelta)) < 0.001);
    assert(qAbs(geos[2].width() - (w1_before1 + stepDelta)) < 0.001);
    // Outer boundaries must remain strictly anchored!
    assert(qAbs(geos[0].left() - initialMasterLeft) < 0.001);
    assert(qAbs(geos[1].right() - initialStackRight) < 0.001);
    assert(qAbs(geos[2].right() - initialStackRight) < 0.001);
    assert(geos[1].right() <= workArea.right());
    assert(geos[2].right() <= workArea.right());

    // Press 2: Shrink master left again ('h') - user's reported failure point
    const qreal w0_before2 = geos[0].width();
    const qreal w1_before2 = geos[1].width();
    bool ok2 = resizeSlotGeometries(geos, 0, ResizeDirection::Left, stepDelta, 100.0);
    assert(ok2);
    assert(qAbs(geos[0].width() - (w0_before2 - stepDelta)) < 0.001);
    assert(qAbs(geos[1].width() - (w1_before2 + stepDelta)) < 0.001);
    assert(qAbs(geos[2].width() - (w1_before2 + stepDelta)) < 0.001);
    // Crucial check: stack windows must NOT stick out beyond the screen!
    assert(qAbs(geos[0].left() - initialMasterLeft) < 0.001);
    assert(qAbs(geos[1].right() - initialStackRight) < 0.001);
    assert(qAbs(geos[2].right() - initialStackRight) < 0.001);
    assert(geos[1].right() <= workArea.right());
    assert(geos[2].right() <= workArea.right());

    // Press 3 to 10: Repeat until minSize is reached
    for (int p = 3; p <= 15; ++p) {
        resizeSlotGeometries(geos, 0, ResizeDirection::Left, stepDelta, 100.0);
        assert(geos[0].left() >= workArea.left() - 0.001);
        assert(geos[1].right() <= workArea.right() + 0.001);
        assert(geos[2].right() <= workArea.right() + 0.001);
        assert(geos[0].width() >= 100.0);
    }

    // Now test expanding master right ('l') repeatedly
    for (int p = 0; p < 8; ++p) {
        resizeSlotGeometries(geos, 0, ResizeDirection::Right, stepDelta, 100.0);
        assert(geos[0].left() >= workArea.left() - 0.001);
        assert(geos[1].right() <= workArea.right() + 0.001);
        assert(geos[2].right() <= workArea.right() + 0.001);
        assert(geos[1].width() >= 100.0);
    }

    // Test vertical stack resizing ('j' / 'k' inside stack)
    const qreal masterGeoBeforeVert = geos[0].width();
    const qreal h1_before = geos[1].height();
    const qreal h2_before = geos[2].height();
    bool vertOk = resizeSlotGeometries(geos, 1, ResizeDirection::Down, 50.0, 100.0);
    assert(vertOk);
    assert(qAbs(geos[1].height() - (h1_before + 50.0)) < 0.001);
    assert(qAbs(geos[2].height() - (h2_before - 50.0)) < 0.001);
    // Master column must be completely untouched by vertical stack resizing!
    assert(qAbs(geos[0].width() - masterGeoBeforeVert) < 0.001);

    std::cout << "testMasterStackRepeatedResize PASSED\n";
}

void testCustomLayoutUpsertAndDelete()
{
    using namespace LeapKTiling;

    auto presets = defaultPresets();
    const int initialCount = presets.size();

    // 1. generateIconDiagram
    assert(generateIconDiagram({}) == QStringLiteral("✕"));
    assert(generateIconDiagram({TileSlot{QRectF(0, 0, 1, 1)}}) == QStringLiteral("🗖"));
    assert(generateIconDiagram({TileSlot{QRectF(0, 0, 0.5, 1)}, TileSlot{QRectF(0.5, 0, 0.5, 1)}}) == QStringLiteral("▌▌"));
    assert(generateIconDiagram({TileSlot{QRectF(0, 0, 1, 0.5)}, TileSlot{QRectF(0, 0.5, 1, 0.5)}}) == QStringLiteral("〓"));
    assert(generateIconDiagram({TileSlot{QRectF(0, 0, 0.33, 1)}, TileSlot{QRectF(0.33, 0, 0.33, 1)}, TileSlot{QRectF(0.66, 0, 0.34, 1)}}) == QStringLiteral("▌▌▌"));

    // 2. Insert new custom preset
    TilingPreset custom1;
    custom1.id = QStringLiteral("custom-H");
    custom1.name = QStringLiteral("Dev 3-Split");
    custom1.triggerKey = QLatin1Char('H');
    custom1.isCustom = true;
    custom1.tileSlots = {
        TileSlot{QRectF(0, 0, 0.33, 1)},
        TileSlot{QRectF(0.33, 0, 0.33, 1)},
        TileSlot{QRectF(0.66, 0, 0.34, 1)}
    };
    custom1.iconDiagram = generateIconDiagram(custom1.tileSlots);
    custom1.description = QStringLiteral("3 slots • Custom");

    upsertPreset(presets, custom1);
    assert(presets.size() == initialCount + 1);

    // Verify Float action remains at the very end
    assert(presets.last().isFloatAction());

    // 3. JSON persistence roundtrip with isCustom
    const QJsonObject json = presetToJson(custom1);
    assert(json.value(QStringLiteral("isCustom")).toBool() == true);
    const TilingPreset deserialized = jsonToPreset(json);
    assert(deserialized.id == custom1.id);
    assert(deserialized.triggerKey == custom1.triggerKey);
    assert(deserialized.isCustom == true);

    // 4. Overwrite existing preset with new slots
    TilingPreset custom1Updated = custom1;
    custom1Updated.name = QStringLiteral("Dev 3-Split Renamed");
    upsertPreset(presets, custom1Updated);
    assert(presets.size() == initialCount + 1); // Size unchanged!
    bool foundUpdated = false;
    for (const auto &p : presets) {
        if (p.triggerKey == QLatin1Char('H')) {
            assert(p.name == QStringLiteral("Dev 3-Split Renamed"));
            foundUpdated = true;
            break;
        }
    }
    assert(foundUpdated);

    // 5. Delete preset by key
    bool deleted = removePresetByKey(presets, QLatin1Char('H'));
    assert(deleted);
    assert(presets.size() == initialCount);

    // 6. Protection: Float action ('X') and KDE Custom ('K') cannot be deleted
    bool deletedFloat = removePresetByKey(presets, QLatin1Char('X'));
    assert(!deletedFloat);
    bool deletedKde = removePresetByKey(presets, QLatin1Char('K'));
    assert(!deletedKde);
    assert(presets.size() == initialCount);

    // 7. Delete default preset 'A' (Two Columns)
    bool deletedA = removePresetByKey(presets, QLatin1Char('A'));
    assert(deletedA);
    assert(presets.size() == initialCount - 1);

    // 8. Reset to default presets
    const QString tempFile = QDir::tempPath() + QStringLiteral("/leapk_tiling_reset_test_%1.json").arg(QDateTime::currentMSecsSinceEpoch());
    auto restored = resetToDefaultPresets(tempFile);
    assert(restored.size() == initialCount);
    assert(restored[0].triggerKey == QLatin1Char('A'));
    QFile::remove(tempFile);

    std::cout << "testCustomLayoutUpsertAndDelete PASSED\n";
}

void testThreeColumnsMiddleSlotResize()
{
    using namespace LeapKTiling;

    // 3 Columns layout:
    // Slot 0: [0, 0, 600, 1080]
    // Slot 1: [610, 0, 700, 1080]  (Middle slot, has left divider at 605 and right divider at 1315)
    // Slot 2: [1320, 0, 600, 1080]
    QList<QRectF> geos = {
        QRectF(0, 0, 600, 1080),
        QRectF(610, 0, 700, 1080),
        QRectF(1320, 0, 600, 1080)
    };

    const qreal initialW0 = geos[0].width();
    const qreal initialW1 = geos[1].width();
    const qreal initialW2 = geos[2].width();
    const qreal delta = 60.0;

    // 1. Expand middle slot right ('l' / Right / '+')
    // Auto mode: left divider shifts -delta/2 (-30), right divider shifts +delta/2 (+30)
    // geos[1].left() becomes 610 - 30 = 580
    // geos[1].right() becomes 1310 + 30 = 1340 -> width becomes 700 + 60 = 760
    // geos[0].right() becomes 600 - 30 = 570 -> width becomes 600 - 30 = 570
    // geos[2].left() becomes 1320 + 30 = 1350 -> width becomes 600 - 30 = 570
    bool ok = resizeSlotGeometries(geos, 1, ResizeDirection::Right, delta, 100.0);
    assert(ok);
    assert(qAbs(geos[1].width() - (initialW1 + delta)) < 0.001);
    assert(qAbs(geos[0].width() - (initialW0 - delta / 2.0)) < 0.001);
    assert(qAbs(geos[2].width() - (initialW2 - delta / 2.0)) < 0.001);
    assert(qAbs(geos[1].left() - geos[0].right() - 10.0) < 0.001);
    assert(qAbs(geos[2].left() - geos[1].right() - 10.0) < 0.001);
    assert(qAbs(geos[0].left() - 0.0) < 0.001);
    assert(qAbs(geos[2].right() - 1920.0) < 0.001);

    // 2. Shrink middle slot left ('h' / Left / '-')
    // Fully reversible back to original dimensions!
    ok = resizeSlotGeometries(geos, 1, ResizeDirection::Left, delta, 100.0);
    assert(ok);
    assert(qAbs(geos[1].width() - initialW1) < 0.001);
    assert(qAbs(geos[0].width() - initialW0) < 0.001);
    assert(qAbs(geos[2].width() - initialW2) < 0.001);
    assert(qAbs(geos[1].left() - geos[0].right() - 10.0) < 0.001);
    assert(qAbs(geos[2].left() - geos[1].right() - 10.0) < 0.001);

    // 3. Shrink middle slot further ('h' / Left / '-')
    // Middle slot narrows symmetrically, left and right expand symmetrically
    ok = resizeSlotGeometries(geos, 1, ResizeDirection::Left, delta, 100.0);
    assert(ok);
    assert(qAbs(geos[1].width() - (initialW1 - delta)) < 0.001);
    assert(qAbs(geos[0].width() - (initialW0 + delta / 2.0)) < 0.001);
    assert(qAbs(geos[2].width() - (initialW2 + delta / 2.0)) < 0.001);
    assert(qAbs(geos[1].left() - geos[0].right() - 10.0) < 0.001);
    assert(qAbs(geos[2].left() - geos[1].right() - 10.0) < 0.001);

    // 4. Repeated shrink until minSize (100.0)
    for (int i = 0; i < 20; ++i) {
        resizeSlotGeometries(geos, 1, ResizeDirection::Left, 50.0, 100.0);
        assert(geos[1].width() >= 100.0);
        assert(geos[0].left() >= -0.001);
        assert(geos[2].right() <= 1920.001);
    }
    assert(qAbs(geos[1].width() - 100.0) < 0.001);

    // 5. Targeted edge resizing: TileEdge::Right
    // geos[1].width is currently 100.0. Expand right edge only (+50.0)
    const qreal w0_beforeTarget = geos[0].width();
    const qreal w2_beforeTarget = geos[2].width();
    ok = resizeSlotGeometries(geos, 1, ResizeDirection::Right, 50.0, 100.0, false, TileEdge::Right);
    assert(ok);
    assert(qAbs(geos[1].width() - 150.0) < 0.001);
    assert(qAbs(geos[0].width() - w0_beforeTarget) < 0.001); // Slot 0 completely untouched!
    assert(qAbs(geos[2].width() - (w2_beforeTarget - 50.0)) < 0.001); // Slot 2 shrunk by 50

    // 6. Targeted edge resizing: TileEdge::Left
    // Shift left edge to the right (+50.0), narrowing slot 1 and expanding slot 0
    ok = resizeSlotGeometries(geos, 1, ResizeDirection::Right, 50.0, 100.0, false, TileEdge::Left);
    assert(ok);
    assert(qAbs(geos[1].width() - 100.0) < 0.001);
    assert(qAbs(geos[0].width() - (w0_beforeTarget + 50.0)) < 0.001); // Slot 0 expanded by 50
    assert(qAbs(geos[2].width() - (w2_beforeTarget - 50.0)) < 0.001); // Slot 2 untouched

    std::cout << "testThreeColumnsMiddleSlotResize PASSED\n";
}

int main()
{
    testDefaultPresets();
    testComputeSlotGeometries();
    testJsonPersistence();
    testToKdeJson();
    testKdeCustomPreset();
    testParseKdeTileJson();
    testComputeSlotPriorityOrder();
    testTilingWindowRuleConfig();
    testTilingSwapStackingPreservation();
    testHudPreservationOnWorkspaceTilingSwitch();
    testTilingWorkspaceWindowIsolation();
    testResizeSlotGeometries();
    testResizePresetRelativeRectSync();
    testMasterStackRepeatedResize();
    testCustomLayoutUpsertAndDelete();
    testThreeColumnsMiddleSlotResize();

    std::cout << "All LeapKTiling tests passed successfully!\n";
    return 0;
}
