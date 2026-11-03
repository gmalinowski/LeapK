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

constexpr const char *kNoMinSizeRuleId = "a9e701bc-6258-4903-a185-3b95a8bfb621";

struct TilingWindowRuleConfig
{
    QString id = QStringLiteral("a9e701bc-6258-4903-a185-3b95a8bfb621");
    QString description = QStringLiteral("LeapK Tiling: No minimum size limit");
    QString minsize = QStringLiteral("1,1");
    int minsizerule = 2; // Force
    bool ignoregeometry = true;
    int ignoregeometryrule = 2; // Force
    int types = 1; // Normal window
    QString wmclass;
    bool wmclasscomplete = false;
    int wmclassmatch = 0; // Unimportant match
};

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
    QJsonObject kdeJson;   // Custom KDE built-in tiling layout JSON representation
    bool isCustom = false; // User-defined or customized preset

    bool isFloatAction() const
    {
        return id == QStringLiteral("float") || tileSlots.isEmpty();
    }

    QJsonObject toKdeJson() const;

    bool operator==(const TilingPreset &other) const
    {
        return id == other.id;
    }
};

inline void parseKdeTileJsonInternal(const QJsonValue &val, const QRectF &availableArea, const QString &parentDir, QList<TileSlot> &outSlots)
{
    if (availableArea.isEmpty()) {
        return;
    }

    if (val.isObject()) {
        const QJsonObject obj = val.toObject();
        QRectF rect = availableArea;

        if (parentDir == QStringLiteral("horizontal")) {
            if (obj.contains(QStringLiteral("width")) && obj.value(QStringLiteral("width")).isDouble()) {
                rect.setWidth(std::min(obj.value(QStringLiteral("width")).toDouble(), availableArea.width()));
            }
            if (std::abs(rect.right() - availableArea.right()) < 0.02) {
                rect.setRight(availableArea.right());
            }
        } else if (parentDir == QStringLiteral("vertical")) {
            if (obj.contains(QStringLiteral("height")) && obj.value(QStringLiteral("height")).isDouble()) {
                rect.setHeight(std::min(obj.value(QStringLiteral("height")).toDouble(), availableArea.height()));
            }
            if (std::abs(rect.bottom() - availableArea.bottom()) < 0.02) {
                rect.setBottom(availableArea.bottom());
            }
        } else if (parentDir == QStringLiteral("floating")) {
            const qreal x = obj.value(QStringLiteral("x")).toDouble(availableArea.x());
            const qreal y = obj.value(QStringLiteral("y")).toDouble(availableArea.y());
            const qreal w = obj.value(QStringLiteral("width")).toDouble(availableArea.width());
            const qreal h = obj.value(QStringLiteral("height")).toDouble(availableArea.height());
            rect = QRectF(x, y, w, h);
        }

        if (obj.contains(QStringLiteral("tiles")) && obj.value(QStringLiteral("tiles")).isArray()) {
            const QJsonArray children = obj.value(QStringLiteral("tiles")).toArray();
            if (!children.isEmpty()) {
                const QString dir = obj.value(QStringLiteral("layoutDirection")).toString(QStringLiteral("floating"));
                parseKdeTileJsonInternal(children, rect, dir, outSlots);
                return;
            }
        }

        // Leaf tile
        if (rect.isValid() && rect.width() > 0.02 && rect.height() > 0.02) {
            outSlots.append(TileSlot{rect});
        }
    } else if (val.isArray()) {
        const QJsonArray arr = val.toArray();
        QRectF avail = availableArea;
        for (int i = 0; i < arr.size(); ++i) {
            const int countBefore = outSlots.size();
            QRectF childAvail = avail;
            if (i == arr.size() - 1 && parentDir != QStringLiteral("floating")) {
                if (parentDir == QStringLiteral("horizontal")) {
                    childAvail.setRight(availableArea.right());
                } else if (parentDir == QStringLiteral("vertical")) {
                    childAvail.setBottom(availableArea.bottom());
                }
            }
            parseKdeTileJsonInternal(arr.at(i), childAvail, parentDir, outSlots);

            // Advance available area for sibling layout
            if (outSlots.size() > countBefore) {
                if (parentDir == QStringLiteral("horizontal")) {
                    qreal maxRight = avail.left();
                    for (int k = countBefore; k < outSlots.size(); ++k) {
                        maxRight = std::max(maxRight, outSlots[k].relativeRect.right());
                    }
                    avail.setLeft(maxRight);
                } else if (parentDir == QStringLiteral("vertical")) {
                    qreal maxBottom = avail.top();
                    for (int k = countBefore; k < outSlots.size(); ++k) {
                        maxBottom = std::max(maxBottom, outSlots[k].relativeRect.bottom());
                    }
                    avail.setTop(maxBottom);
                }
            }
        }
    }
}

inline QList<TileSlot> parseKdeTileJson(const QJsonObject &rootObj)
{
    QList<TileSlot> slotList;
    const QString dir = rootObj.value(QStringLiteral("layoutDirection")).toString(QStringLiteral("horizontal"));
    const QJsonValue tilesVal = rootObj.value(QStringLiteral("tiles"));
    parseKdeTileJsonInternal(tilesVal, QRectF(0.0, 0.0, 1.0, 1.0), dir, slotList);
    return slotList;
}

inline QList<int> computeSlotPriorityOrder(const QList<TileSlot> &tileSlotsList)
{
    QList<int> order;
    for (int i = 0; i < tileSlotsList.size(); ++i) {
        order.append(i);
    }

    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const QRectF &ra = tileSlotsList[a].relativeRect;
        const QRectF &rb = tileSlotsList[b].relativeRect;
        const qreal areaA = ra.width() * ra.height();
        const qreal areaB = rb.width() * rb.height();
        // 1. Significantly larger area takes precedence (Master window)
        if (std::abs(areaA - areaB) > 0.03) {
            return areaA > areaB;
        }
        // 2. Otherwise geometric order (top-to-bottom, left-to-right)
        if (std::abs(ra.top() - rb.top()) > 0.02) {
            return ra.top() < rb.top();
        }
        return ra.left() < rb.left();
    });

    return order;
}

inline TilingPreset createKdeCustomPreset(const QList<QRectF> &slotGeometries, const QJsonObject &rawKdeJson = QJsonObject())
{
    TilingPreset p;
    p.id = QStringLiteral("kde-custom");
    p.name = QStringLiteral("KDE Custom");
    p.triggerKey = QLatin1Char('K');
    p.iconDiagram = QStringLiteral("▨");
    p.kdeJson = rawKdeJson;

    const int count = slotGeometries.size();
    if (count == 2) {
        p.iconDiagram = QStringLiteral("▌▌");
    } else if (count == 3) {
        p.iconDiagram = QStringLiteral("▌▌▌");
    } else if (count == 4) {
        p.iconDiagram = QStringLiteral("⊞");
    } else if (count == 1) {
        p.iconDiagram = QStringLiteral("🗖");
    }
    p.description = QStringLiteral("%1 slots • KDE layout").arg(count);
    for (const auto &r : slotGeometries) {
        p.tileSlots.append(TileSlot{r});
    }
    return p;
}

inline TilingPreset createKdeCustomPreset(const QJsonObject &rawKdeJson)
{
    const QList<TileSlot> parsedSlots = parseKdeTileJson(rawKdeJson);
    QList<QRectF> rects;
    for (const auto &s : parsedSlots) {
        rects.append(s.relativeRect);
    }
    return createKdeCustomPreset(rects, rawKdeJson);
}

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

enum class ResizeDirection {
    Left,
    Right,
    Up,
    Down
};

enum class TileEdge {
    Auto,
    Left,
    Right,
    Top,
    Bottom
};

constexpr const char *kTileEditKeys = "ABCDEFGRTVWXYZ";

inline bool resizeSlotGeometries(QList<QRectF> &geometries, int activeIdx, ResizeDirection dir, qreal delta, qreal minSize = 100.0, bool localOnly = false, TileEdge targetEdge = TileEdge::Auto)
{
    if (activeIdx < 0 || activeIdx >= geometries.size() || geometries.size() < 2 || delta <= 0.0) {
        return false;
    }

    const QRectF cur = geometries[activeIdx];
    const qreal tol = 5.0;

    auto verticalOverlap = [](const QRectF &a, const QRectF &b) -> bool {
        return qMax(a.top(), b.top()) < qMin(a.bottom(), b.bottom()) - 2.0;
    };

    auto horizontalOverlap = [](const QRectF &a, const QRectF &b) -> bool {
        return qMax(a.left(), b.left()) < qMin(a.right(), b.right()) - 2.0;
    };

    if (dir == ResizeDirection::Left || dir == ResizeDirection::Right) {
        // Find candidate vertical dividers (divL is right edge of left group, divR is left edge of right group)

        // 1. Divider to the right of cur (cur is in left group)
        bool hasRightDivider = false;
        qreal rightDivL = cur.right();
        qreal rightDivR = 0.0;
        qreal bestRightGap = 999999.0;
        for (int i = 0; i < geometries.size(); ++i) {
            if (i == activeIdx) continue;
            const QRectF &other = geometries[i];
            if (verticalOverlap(cur, other) && other.left() >= cur.right() - tol) {
                qreal dist = other.left() - cur.right();
                if (dist >= -tol && dist < bestRightGap) {
                    bestRightGap = dist;
                    hasRightDivider = true;
                }
            }
        }
        if (hasRightDivider) {
            rightDivR = cur.right() + bestRightGap;
        }

        // 2. Divider to the left of cur (cur is in right group)
        bool hasLeftDivider = false;
        qreal leftDivR = cur.left();
        qreal leftDivL = 0.0;
        qreal bestLeftGap = 999999.0;
        for (int i = 0; i < geometries.size(); ++i) {
            if (i == activeIdx) continue;
            const QRectF &other = geometries[i];
            if (verticalOverlap(cur, other) && other.right() <= cur.left() + tol) {
                qreal dist = cur.left() - other.right();
                if (dist >= -tol && dist < bestLeftGap) {
                    bestLeftGap = dist;
                    hasLeftDivider = true;
                }
            }
        }
        if (hasLeftDivider) {
            leftDivL = cur.left() - bestLeftGap;
        }

        auto shiftHorizontalDivider = [&](qreal divL, qreal divR, qreal dShift) -> bool {
            if (qAbs(dShift) < 0.5) {
                return false;
            }
            QList<int> groupL;
            QList<int> groupR;
            for (int i = 0; i < geometries.size(); ++i) {
                if (qAbs(geometries[i].right() - divL) <= tol) {
                    groupL.append(i);
                }
                if (qAbs(geometries[i].left() - divR) <= tol) {
                    groupR.append(i);
                }
            }

            QList<int> filteredL;
            for (int l : groupL) {
                if (localOnly && !verticalOverlap(geometries[l], cur) && l != activeIdx) {
                    bool sharesCol = false;
                    for (int otherL : groupL) {
                        if (otherL == activeIdx || verticalOverlap(geometries[otherL], cur)) {
                            if (qAbs(geometries[l].left() - geometries[otherL].left()) <= tol) {
                                sharesCol = true;
                                break;
                            }
                        }
                    }
                    if (!sharesCol) continue;
                }
                for (int r : groupR) {
                    if (verticalOverlap(geometries[l], geometries[r])) {
                        filteredL.append(l);
                        break;
                    }
                }
            }
            QList<int> filteredR;
            for (int r : groupR) {
                if (localOnly && !verticalOverlap(geometries[r], cur) && r != activeIdx) {
                    bool sharesCol = false;
                    for (int otherR : groupR) {
                        if (otherR == activeIdx || verticalOverlap(geometries[otherR], cur)) {
                            if (qAbs(geometries[r].right() - geometries[otherR].right()) <= tol) {
                                sharesCol = true;
                                break;
                            }
                        }
                    }
                    if (!sharesCol) continue;
                }
                for (int l : groupL) {
                    if (verticalOverlap(geometries[l], geometries[r])) {
                        filteredR.append(r);
                        break;
                    }
                }
            }

            if (filteredL.isEmpty() || filteredR.isEmpty()) {
                return false;
            }

            if (dShift > 0.0) {
                // Divider moves right (+X): Group L expands, Group R shrinks
                qreal eff = dShift;
                for (int r : filteredR) {
                    eff = qMin(eff, geometries[r].width() - minSize);
                }
                if (eff > 0.5) {
                    for (int l : filteredL) {
                        geometries[l].setRight(geometries[l].right() + eff);
                    }
                    for (int r : filteredR) {
                        geometries[r].setLeft(geometries[r].left() + eff);
                    }
                    return true;
                }
            } else {
                // Divider moves left (-X): Group L shrinks, Group R expands
                qreal eff = -dShift;
                for (int l : filteredL) {
                    eff = qMin(eff, geometries[l].width() - minSize);
                }
                if (eff > 0.5) {
                    for (int l : filteredL) {
                        geometries[l].setRight(geometries[l].right() - eff);
                    }
                    for (int r : filteredR) {
                        geometries[r].setLeft(geometries[r].left() - eff);
                    }
                    return true;
                }
            }
            return false;
        };

        if (targetEdge == TileEdge::Right) {
            if (hasRightDivider) {
                const qreal shift = (dir == ResizeDirection::Right) ? delta : -delta;
                return shiftHorizontalDivider(rightDivL, rightDivR, shift);
            }
        } else if (targetEdge == TileEdge::Left) {
            if (hasLeftDivider) {
                const qreal shift = (dir == ResizeDirection::Right) ? delta : -delta;
                return shiftHorizontalDivider(leftDivL, leftDivR, shift);
            }
        } else {
            // targetEdge == TileEdge::Auto (standard keyboard resize navigation)
            if (hasLeftDivider && hasRightDivider) {
                // Middle slot: both borders move symmetrically so window grows/shrinks cleanly
                if (dir == ResizeDirection::Right) {
                    // Expand width (+delta): Left divider moves left (-delta/2), Right divider moves right (+delta/2)
                    const qreal half = delta / 2.0;
                    bool rOk = shiftHorizontalDivider(rightDivL, rightDivR, half);
                    bool lOk = shiftHorizontalDivider(leftDivL, leftDivR, -half);
                    return rOk || lOk;
                } else {
                    // Shrink width (-delta): Left divider moves right (+delta/2), Right divider moves left (-delta/2)
                    const qreal maxShrink = cur.width() - minSize;
                    const qreal d = qMin(delta, maxShrink);
                    if (d < 1.0) return false;
                    const qreal half = d / 2.0;
                    bool rOk = shiftHorizontalDivider(rightDivL, rightDivR, -half);
                    bool lOk = shiftHorizontalDivider(leftDivL, leftDivR, half);
                    return rOk || lOk;
                }
            } else if (hasRightDivider || hasLeftDivider) {
                // Boundary slot with one divider:
                // Right: divider moves right (+delta)
                // Left:  divider moves left (-delta)
                const qreal dL = hasRightDivider ? rightDivL : leftDivL;
                const qreal dR = hasRightDivider ? rightDivR : leftDivR;
                const qreal shift = (dir == ResizeDirection::Right) ? delta : -delta;
                return shiftHorizontalDivider(dL, dR, shift);
            }
        }
        return false;
    } else { // Up or Down
        // Find candidate horizontal dividers (divT is bottom edge of top group, divB is top edge of bottom group)

        // 1. Divider below cur (cur is in top group)
        bool hasBottomDivider = false;
        qreal bottomDivT = cur.bottom();
        qreal bottomDivB = 0.0;
        qreal bestBottomGap = 999999.0;
        for (int i = 0; i < geometries.size(); ++i) {
            if (i == activeIdx) continue;
            const QRectF &other = geometries[i];
            if (horizontalOverlap(cur, other) && other.top() >= cur.bottom() - tol) {
                qreal dist = other.top() - cur.bottom();
                if (dist >= -tol && dist < bestBottomGap) {
                    bestBottomGap = dist;
                    hasBottomDivider = true;
                }
            }
        }
        if (hasBottomDivider) {
            bottomDivB = cur.bottom() + bestBottomGap;
        }

        // 2. Divider above cur (cur is in bottom group)
        bool hasTopDivider = false;
        qreal topDivB = cur.top();
        qreal topDivT = 0.0;
        qreal bestTopGap = 999999.0;
        for (int i = 0; i < geometries.size(); ++i) {
            if (i == activeIdx) continue;
            const QRectF &other = geometries[i];
            if (horizontalOverlap(cur, other) && other.bottom() <= cur.top() + tol) {
                qreal dist = cur.top() - other.bottom();
                if (dist >= -tol && dist < bestTopGap) {
                    bestTopGap = dist;
                    hasTopDivider = true;
                }
            }
        }
        if (hasTopDivider) {
            topDivT = cur.top() - bestTopGap;
        }

        auto shiftVerticalDivider = [&](qreal divT, qreal divB, qreal dShift) -> bool {
            if (qAbs(dShift) < 0.5) {
                return false;
            }
            QList<int> groupT;
            QList<int> groupB;
            for (int i = 0; i < geometries.size(); ++i) {
                if (qAbs(geometries[i].bottom() - divT) <= tol) {
                    groupT.append(i);
                }
                if (qAbs(geometries[i].top() - divB) <= tol) {
                    groupB.append(i);
                }
            }

            QList<int> filteredT;
            for (int t : groupT) {
                if (localOnly && !horizontalOverlap(geometries[t], cur) && t != activeIdx) {
                    continue;
                }
                for (int b : groupB) {
                    if (horizontalOverlap(geometries[t], geometries[b])) {
                        filteredT.append(t);
                        break;
                    }
                }
            }
            QList<int> filteredB;
            for (int b : groupB) {
                if (localOnly && !horizontalOverlap(geometries[b], cur) && b != activeIdx) {
                    continue;
                }
                for (int t : groupT) {
                    if (horizontalOverlap(geometries[t], geometries[b])) {
                        filteredB.append(b);
                        break;
                    }
                }
            }

            if (filteredT.isEmpty() || filteredB.isEmpty()) {
                return false;
            }

            if (dShift > 0.0) {
                // Divider moves down (+Y): Group T expands, Group B shrinks
                qreal eff = dShift;
                for (int b : filteredB) {
                    eff = qMin(eff, geometries[b].height() - minSize);
                }
                if (eff > 0.5) {
                    for (int t : filteredT) {
                        geometries[t].setBottom(geometries[t].bottom() + eff);
                    }
                    for (int b : filteredB) {
                        geometries[b].setTop(geometries[b].top() + eff);
                    }
                    return true;
                }
            } else {
                // Divider moves up (-Y): Group T shrinks, Group B expands
                qreal eff = -dShift;
                for (int t : filteredT) {
                    eff = qMin(eff, geometries[t].height() - minSize);
                }
                if (eff > 0.5) {
                    for (int t : filteredT) {
                        geometries[t].setBottom(geometries[t].bottom() - eff);
                    }
                    for (int b : filteredB) {
                        geometries[b].setTop(geometries[b].top() - eff);
                    }
                    return true;
                }
            }
            return false;
        };

        if (targetEdge == TileEdge::Bottom) {
            if (hasBottomDivider) {
                const qreal shift = (dir == ResizeDirection::Down) ? delta : -delta;
                return shiftVerticalDivider(bottomDivT, bottomDivB, shift);
            }
        } else if (targetEdge == TileEdge::Top) {
            if (hasTopDivider) {
                const qreal shift = (dir == ResizeDirection::Down) ? delta : -delta;
                return shiftVerticalDivider(topDivT, topDivB, shift);
            }
        } else {
            // targetEdge == TileEdge::Auto (fallback for keyboard navigation)
            if (hasTopDivider && hasBottomDivider) {
                // Vertical middle slot: symmetric expansion/contraction
                if (dir == ResizeDirection::Down) {
                    // Expand height (+delta): Top moves up (-delta/2), Bottom moves down (+delta/2)
                    const qreal half = delta / 2.0;
                    bool bOk = shiftVerticalDivider(bottomDivT, bottomDivB, half);
                    bool tOk = shiftVerticalDivider(topDivT, topDivB, -half);
                    return bOk || tOk;
                } else {
                    // Shrink height (-delta): Top moves down (+delta/2), Bottom moves up (-delta/2)
                    const qreal maxShrink = cur.height() - minSize;
                    const qreal d = qMin(delta, maxShrink);
                    if (d < 1.0) return false;
                    const qreal half = d / 2.0;
                    bool bOk = shiftVerticalDivider(bottomDivT, bottomDivB, -half);
                    bool tOk = shiftVerticalDivider(topDivT, topDivB, half);
                    return bOk || tOk;
                }
            } else if (hasBottomDivider || hasTopDivider) {
                // Boundary slot with one vertical divider:
                // Down: divider moves down (+delta)
                // Up:   divider moves up (-delta)
                const qreal dT = hasBottomDivider ? bottomDivT : topDivT;
                const qreal dB = hasBottomDivider ? bottomDivB : topDivB;
                const qreal shift = (dir == ResizeDirection::Down) ? delta : -delta;
                return shiftVerticalDivider(dT, dB, shift);
            }
        }
        return false;
    }
}

inline bool calculateDividerMove(const QList<QRectF> &geometries, int slotIdx, const QRectF &newGeo, ResizeDirection &outDir, qreal &outDelta, TileEdge &outEdge)
{
    if (slotIdx < 0 || slotIdx >= geometries.size()) {
        return false;
    }
    const QRectF cur = geometries[slotIdx];
    const qreal tol = 2.0;

    qreal dRight = newGeo.right() - cur.right();
    qreal dLeft = newGeo.left() - cur.left();
    qreal dBottom = newGeo.bottom() - cur.bottom();
    qreal dTop = newGeo.top() - cur.top();

    qreal maxDisp = 0.0;
    ResizeDirection bestDir = ResizeDirection::Right;
    TileEdge bestEdge = TileEdge::Auto;

    if (qAbs(dRight) > tol && qAbs(dRight) > maxDisp) {
        maxDisp = qAbs(dRight);
        bestDir = (dRight > 0.0) ? ResizeDirection::Right : ResizeDirection::Left;
        bestEdge = TileEdge::Right;
    }
    if (qAbs(dLeft) > tol && qAbs(dLeft) > maxDisp) {
        maxDisp = qAbs(dLeft);
        bestDir = (dLeft > 0.0) ? ResizeDirection::Right : ResizeDirection::Left;
        bestEdge = TileEdge::Left;
    }
    if (qAbs(dBottom) > tol && qAbs(dBottom) > maxDisp) {
        maxDisp = qAbs(dBottom);
        bestDir = (dBottom > 0.0) ? ResizeDirection::Down : ResizeDirection::Up;
        bestEdge = TileEdge::Bottom;
    }
    if (qAbs(dTop) > tol && qAbs(dTop) > maxDisp) {
        maxDisp = qAbs(dTop);
        bestDir = (dTop > 0.0) ? ResizeDirection::Down : ResizeDirection::Up;
        bestEdge = TileEdge::Top;
    }

    if (maxDisp > tol) {
        outDir = bestDir;
        outDelta = maxDisp;
        outEdge = bestEdge;
        return true;
    }
    return false;
}

inline bool calculateDividerMove(const QList<QRectF> &geometries, int slotIdx, const QRectF &newGeo, ResizeDirection &outDir, qreal &outDelta)
{
    TileEdge edge = TileEdge::Auto;
    return calculateDividerMove(geometries, slotIdx, newGeo, outDir, outDelta, edge);
}

inline QJsonObject toKdeJson(const TilingPreset &preset)
{
    if (!preset.kdeJson.isEmpty()) {
        return preset.kdeJson;
    }

    QJsonObject root;
    if (preset.isFloatAction() || preset.tileSlots.isEmpty()) {
        root[QStringLiteral("layoutDirection")] = QStringLiteral("floating");
        root[QStringLiteral("tiles")] = QJsonArray();
        return root;
    }

    if (preset.id == QStringLiteral("columns-2")) {
        root[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
        QJsonArray tiles;
        QJsonObject t1, t2;
        const double w1 = (preset.tileSlots.size() >= 1 && preset.tileSlots[0].relativeRect.width() > 0.01)
            ? preset.tileSlots[0].relativeRect.width() : 0.5;
        const double w2 = (preset.tileSlots.size() >= 2 && preset.tileSlots[1].relativeRect.width() > 0.01)
            ? preset.tileSlots[1].relativeRect.width() : (1.0 - w1);
        t1[QStringLiteral("width")] = w1;
        t2[QStringLiteral("width")] = w2;
        tiles.append(t1);
        tiles.append(t2);
        root[QStringLiteral("tiles")] = tiles;
        return root;
    }

    if (preset.id == QStringLiteral("columns-3")) {
        root[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
        QJsonArray tiles;
        QJsonObject t1, t2, t3;
        const double w1 = (preset.tileSlots.size() >= 1 && preset.tileSlots[0].relativeRect.width() > 0.01)
            ? preset.tileSlots[0].relativeRect.width() : 0.25;
        const double w2 = (preset.tileSlots.size() >= 2 && preset.tileSlots[1].relativeRect.width() > 0.01)
            ? preset.tileSlots[1].relativeRect.width() : 0.50;
        const double w3 = (preset.tileSlots.size() >= 3 && preset.tileSlots[2].relativeRect.width() > 0.01)
            ? preset.tileSlots[2].relativeRect.width() : (1.0 - w1 - w2);
        t1[QStringLiteral("width")] = w1;
        t2[QStringLiteral("width")] = w2;
        t3[QStringLiteral("width")] = w3;
        tiles.append(t1);
        tiles.append(t2);
        tiles.append(t3);
        root[QStringLiteral("tiles")] = tiles;
        return root;
    }

    if (preset.id == QStringLiteral("master-stack")) {
        root[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
        QJsonArray tiles;
        QJsonObject master;
        const double w1 = (preset.tileSlots.size() >= 1 && preset.tileSlots[0].relativeRect.width() > 0.01)
            ? preset.tileSlots[0].relativeRect.width() : 0.60;
        const double w2 = (preset.tileSlots.size() >= 2 && preset.tileSlots[1].relativeRect.width() > 0.01)
            ? preset.tileSlots[1].relativeRect.width() : (1.0 - w1);
        master[QStringLiteral("width")] = w1;
        tiles.append(master);

        QJsonObject stack;
        stack[QStringLiteral("layoutDirection")] = QStringLiteral("vertical");
        stack[QStringLiteral("width")] = w2;
        QJsonArray stackTiles;
        QJsonObject s1, s2;
        const double totalH = (preset.tileSlots.size() >= 3)
            ? (preset.tileSlots[1].relativeRect.height() + preset.tileSlots[2].relativeRect.height())
            : 1.0;
        const double h1 = (preset.tileSlots.size() >= 2 && totalH > 0.01)
            ? (preset.tileSlots[1].relativeRect.height() / totalH) : 0.50;
        const double h2 = 1.0 - h1;
        s1[QStringLiteral("height")] = h1;
        s2[QStringLiteral("height")] = h2;
        stackTiles.append(s1);
        stackTiles.append(s2);
        stack[QStringLiteral("tiles")] = stackTiles;
        tiles.append(stack);

        root[QStringLiteral("tiles")] = tiles;
        return root;
    }

    if (preset.id == QStringLiteral("grid-4")) {
        root[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
        QJsonArray tiles;

        const double wCol1 = (preset.tileSlots.size() >= 1 && preset.tileSlots[0].relativeRect.width() > 0.01)
            ? preset.tileSlots[0].relativeRect.width() : 0.50;
        const double wCol2 = (preset.tileSlots.size() >= 2 && preset.tileSlots[1].relativeRect.width() > 0.01)
            ? preset.tileSlots[1].relativeRect.width() : (1.0 - wCol1);

        const double totalCol1H = (preset.tileSlots.size() >= 3)
            ? (preset.tileSlots[0].relativeRect.height() + preset.tileSlots[2].relativeRect.height())
            : 1.0;
        const double hCol1Top = (preset.tileSlots.size() >= 1 && totalCol1H > 0.01)
            ? (preset.tileSlots[0].relativeRect.height() / totalCol1H) : 0.50;
        const double hCol1Bot = 1.0 - hCol1Top;

        const double totalCol2H = (preset.tileSlots.size() >= 4)
            ? (preset.tileSlots[1].relativeRect.height() + preset.tileSlots[3].relativeRect.height())
            : 1.0;
        const double hCol2Top = (preset.tileSlots.size() >= 2 && totalCol2H > 0.01)
            ? (preset.tileSlots[1].relativeRect.height() / totalCol2H) : 0.50;
        const double hCol2Bot = 1.0 - hCol2Top;

        QJsonObject col1;
        col1[QStringLiteral("layoutDirection")] = QStringLiteral("vertical");
        col1[QStringLiteral("width")] = wCol1;
        QJsonArray col1Tiles;
        QJsonObject c1t1, c1t2;
        c1t1[QStringLiteral("height")] = hCol1Top;
        c1t2[QStringLiteral("height")] = hCol1Bot;
        col1Tiles.append(c1t1);
        col1Tiles.append(c1t2);
        col1[QStringLiteral("tiles")] = col1Tiles;
        tiles.append(col1);

        QJsonObject col2;
        col2[QStringLiteral("layoutDirection")] = QStringLiteral("vertical");
        col2[QStringLiteral("width")] = wCol2;
        QJsonArray col2Tiles;
        QJsonObject c2t1, c2t2;
        c2t1[QStringLiteral("height")] = hCol2Top;
        c2t2[QStringLiteral("height")] = hCol2Bot;
        col2Tiles.append(c2t1);
        col2Tiles.append(c2t2);
        col2[QStringLiteral("tiles")] = col2Tiles;
        tiles.append(col2);

        root[QStringLiteral("tiles")] = tiles;
        return root;
    }

    if (preset.id == QStringLiteral("monocle") || preset.tileSlots.size() == 1) {
        root[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
        QJsonArray tiles;
        QJsonObject t1;
        t1[QStringLiteral("width")] = 1.0;
        tiles.append(t1);
        root[QStringLiteral("tiles")] = tiles;
        return root;
    }

    bool hasNonHorizontal = false;
    for (const auto &slot : preset.tileSlots) {
        if (slot.relativeRect.top() > 0.01 || slot.relativeRect.height() < 0.98) {
            hasNonHorizontal = true;
            break;
        }
    }

    if (hasNonHorizontal) {
        // Use floating layout in KWin: preserves exact x, y, width, height for arbitrary / vertical splits
        root[QStringLiteral("layoutDirection")] = QStringLiteral("floating");
        QJsonArray tiles;
        for (const auto &slot : preset.tileSlots) {
            QJsonObject t;
            t[QStringLiteral("x")] = slot.relativeRect.x();
            t[QStringLiteral("y")] = slot.relativeRect.y();
            t[QStringLiteral("width")] = slot.relativeRect.width();
            t[QStringLiteral("height")] = slot.relativeRect.height();
            tiles.append(t);
        }
        root[QStringLiteral("tiles")] = tiles;
        return root;
    }

    root[QStringLiteral("layoutDirection")] = QStringLiteral("horizontal");
    QJsonArray tiles;
    const int count = preset.tileSlots.size();
    const double w = count > 0 ? (1.0 / count) : 1.0;
    for (int i = 0; i < count; ++i) {
        QJsonObject t;
        t[QStringLiteral("width")] = preset.tileSlots[i].relativeRect.width() > 0 ? preset.tileSlots[i].relativeRect.width() : w;
        tiles.append(t);
    }
    root[QStringLiteral("tiles")] = tiles;
    return root;
}

inline QJsonObject TilingPreset::toKdeJson() const
{
    return LeapKTiling::toKdeJson(*this);
}

inline QString defaultPresetsFilePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    return base + QStringLiteral("/leapk/tiling_presets.json");
}

inline QString generateIconDiagram(const QList<TileSlot> &tileSlots)
{
    const int count = tileSlots.size();
    if (count == 0) return QStringLiteral("✕");
    if (count == 1) return QStringLiteral("🗖");
    if (count == 2) {
        if (tileSlots.size() >= 2 && std::abs(tileSlots[0].relativeRect.top() - tileSlots[1].relativeRect.top()) < 0.05) {
            return QStringLiteral("▌▌");
        } else {
            return QStringLiteral("〓");
        }
    }
    if (count == 3) {
        bool allCols = true;
        for (int i = 1; i < tileSlots.size(); ++i) {
            if (std::abs(tileSlots[i].relativeRect.top() - tileSlots[0].relativeRect.top()) > 0.05) {
                allCols = false;
                break;
            }
        }
        if (allCols) return QStringLiteral("▌▌▌");
        return QStringLiteral("▌☰");
    }
    if (count == 4) return QStringLiteral("⊞");
    return QStringLiteral("%1▤").arg(count);
}

inline QJsonObject presetToJson(const TilingPreset &preset)
{
    QJsonObject obj;
    obj[QStringLiteral("id")] = preset.id;
    obj[QStringLiteral("name")] = preset.name;
    obj[QStringLiteral("triggerKey")] = QString(preset.triggerKey);
    obj[QStringLiteral("iconDiagram")] = preset.iconDiagram;
    obj[QStringLiteral("description")] = preset.description;
    obj[QStringLiteral("isCustom")] = preset.isCustom;

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
    if (!preset.kdeJson.isEmpty()) {
        obj[QStringLiteral("kdeJson")] = preset.kdeJson;
    }
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
    p.isCustom = obj.value(QStringLiteral("isCustom")).toBool(false);
    if (obj.contains(QStringLiteral("kdeJson"))) {
        p.kdeJson = obj.value(QStringLiteral("kdeJson")).toObject();
    }

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
        if (p.id == QStringLiteral("kde-custom")) {
            continue; // Transient dynamic layout; do not persist to JSON
        }
        arr.append(presetToJson(p));
    }

    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

inline QList<TilingPreset> loadPresets(const QString &filePath = QString())
{
    const QString path = filePath.isEmpty() ? defaultPresetsFilePath() : filePath;
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        const auto defs = defaultPresets();
        savePresets(defs, path);
        return defs;
    }

    const QByteArray data = file.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        const auto defs = defaultPresets();
        savePresets(defs, path);
        return defs;
    }

    QList<TilingPreset> presets;
    const QJsonArray arr = doc.array();
    for (const auto &val : arr) {
        if (val.isObject()) {
            const auto p = jsonToPreset(val.toObject());
            if (p.id != QStringLiteral("kde-custom")) {
                presets.append(p);
            }
        }
    }

    if (presets.isEmpty()) {
        const auto defs = defaultPresets();
        savePresets(defs, path);
        return defs;
    }

    // Ensure Float action is always present at the end
    bool hasFloat = false;
    for (const auto &p : presets) {
        if (p.isFloatAction()) {
            hasFloat = true;
            break;
        }
    }
    if (!hasFloat) {
        TilingPreset fl;
        fl.id = QStringLiteral("float");
        fl.name = QStringLiteral("Float / Turn Off");
        fl.triggerKey = QLatin1Char('X');
        fl.iconDiagram = QStringLiteral("✕");
        fl.description = QStringLiteral("Return windows to floating");
        presets.append(fl);
    }

    return presets;
}

inline bool removePresetByKey(QList<TilingPreset> &presets, const QChar &key)
{
    if (key.isNull()) {
        return false;
    }
    const QChar targetKey = key.toUpper();
    if (targetKey == QLatin1Char('X') || targetKey == QLatin1Char('K')) {
        return false; // Protect Float action ('X') and KDE Custom bridge ('K')
    }
    for (int i = 0; i < presets.size(); ++i) {
        if (presets[i].triggerKey.toUpper() == targetKey) {
            if (presets[i].isFloatAction() || presets[i].id == QStringLiteral("kde-custom")) {
                return false; // Protect Float action and KDE Custom
            }
            presets.removeAt(i);
            return true;
        }
    }
    return false;
}

inline void upsertPreset(QList<TilingPreset> &presets, const TilingPreset &newPreset)
{
    const QChar targetKey = newPreset.triggerKey.toUpper();
    for (int i = 0; i < presets.size(); ++i) {
        if (presets[i].triggerKey.toUpper() == targetKey) {
            presets[i] = newPreset;
            return;
        }
    }

    // Insert before float action if present, else append
    int insertIdx = presets.size();
    for (int i = 0; i < presets.size(); ++i) {
        if (presets[i].isFloatAction()) {
            insertIdx = i;
            break;
        }
    }
    presets.insert(insertIdx, newPreset);
}

inline QList<TilingPreset> resetToDefaultPresets(const QString &filePath = QString())
{
    const auto defs = defaultPresets();
    savePresets(defs, filePath);
    return defs;
}

} // namespace LeapKTiling
