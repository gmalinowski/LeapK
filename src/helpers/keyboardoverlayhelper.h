/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QChar>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QLineF>
#include <QList>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <cmath>

namespace LeapKKeyboardOverlay {

// 26 distinct, vibrant, high-contrast colors ensuring every window arrow has a unique color
inline const QList<QColor> &distinctArrowColors()
{
    static const QList<QColor> palette = {
        QColor(QStringLiteral("#FF3366")), // Hot Pink / Neon Crimson
        QColor(QStringLiteral("#00E5FF")), // Electric Cyan
        QColor(QStringLiteral("#FFD600")), // Bright Amber Yellow
        QColor(QStringLiteral("#00E676")), // Neon Mint Green
        QColor(QStringLiteral("#FF9100")), // Vivid Orange
        QColor(QStringLiteral("#D500F9")), // Electric Magenta
        QColor(QStringLiteral("#76FF03")), // Bright Lime
        QColor(QStringLiteral("#2979FF")), // Royal Azure Blue
        QColor(QStringLiteral("#FF1744")), // Vivid Red
        QColor(QStringLiteral("#651FFF")), // Deep Electric Indigo
        QColor(QStringLiteral("#1DE9B6")), // Turquoise Aqua
        QColor(QStringLiteral("#FFC400")), // Golden Yellow
        QColor(QStringLiteral("#F50057")), // Deep Rose
        QColor(QStringLiteral("#00C853")), // Emerald Green
        QColor(QStringLiteral("#FF6D00")), // Tangerine
        QColor(QStringLiteral("#3D5AFE")), // Cobalt Blue
        QColor(QStringLiteral("#AEEA00")), // Chartreuse
        QColor(QStringLiteral("#C51162")), // Ruby Crimson
        QColor(QStringLiteral("#00B0FF")), // Light Neon Blue
        QColor(QStringLiteral("#AA00FF")), // Bright Violet
        QColor(QStringLiteral("#00BFA5")), // Sea Green
        QColor(QStringLiteral("#FFAB00")), // Dark Amber
        QColor(QStringLiteral("#DD2C00")), // Deep Orange Red
        QColor(QStringLiteral("#304FFE")), // Ultramarine Blue
        QColor(QStringLiteral("#64DD17")), // Grass Green
        QColor(QStringLiteral("#E040FB"))  // Orchid Purple
    };
    return palette;
}

// Deterministic unique color mapping for every letter key A-Z
inline QColor colorForLetter(QChar key)
{
    const auto &palette = distinctArrowColors();
    const QChar u = key.toUpper();
    if (u >= QLatin1Char('A') && u <= QLatin1Char('Z')) {
        const int idx = (u.unicode() - 'A') % palette.size();
        return palette[idx];
    }
    return QColor(0, 229, 255);
}

inline bool isKeyboardOverlayActive(bool enabled, bool tilingOnly, bool tilingActive,
                                    bool limitByWindowCount = false, int maxWindows = 5, int currentWindowCount = -1)
{
    if (!enabled) {
        return false;
    }
    if (tilingOnly && !tilingActive) {
        return false;
    }
    if (limitByWindowCount && maxWindows > 0 && currentWindowCount > maxWindows) {
        return false;
    }
    return true;
}

struct OverlayDisplayConfig {
    bool enabled = true;
    bool tilingOnly = false;
    bool limitByWindowCount = true;
    int maxWindows = 5;
    bool extendToHud = true;
    qreal keySize = 64.0;
    int keyFontSize = 14;
    qreal lineWidth = 3.4;
    qreal badgeRadius = 18.0;
    int badgeFontSize = 13;
    int targetLabelFontSize = 10;
    QString fontFamily;
    QString targetBadgeShape = QStringLiteral("circle");
    double stackScaleReduction = 0.35;   // 0.0 to 0.70
    double stackOpacityReduction = 0.40; // 0.0 to 0.70
    double stackBadgeGap = 8.0;
    bool targetLabelVisible = false;
    QColor targetLabelBg;
    QColor targetLabelTextColor;
    bool laserGlow = true;
    double laserGlowIntensity = 1.0;
    QString laserDashStyle = QStringLiteral("solid");
    bool fixedLetterColors = true;
};

struct WindowTargetItem {
    void *windowId = nullptr;
    QRectF geometry;
    QChar preferredKey;
    QString title;
    QString appClass;
    QString customLabel;
    bool isVisibleOnScreen = true;
    bool isCurrentFocused = false;
    QRectF hudCardRect;
    QColor customColor;
    int zOrder = 0;         // Stacking index in KWin (higher = frontmost/top, lower = underneath)
    bool isOccluded = false; // Whether it is occluded by another window
};

struct KeycapGeometry {
    QChar key;
    QRectF rect;
    int row = 0;
    int col = 0;
};

struct KeyboardOverlayLayout {
    QRectF panelRect;
    QRectF hudBounds;
    QList<KeycapGeometry> keycaps;
    QMap<QChar, KeycapGeometry> keyMap;
};

inline KeyboardOverlayLayout computeOverlayGeometry(const QRectF &screenRect,
                                                    const QRectF &hudBounds = QRectF(),
                                                    const OverlayDisplayConfig &cfg = OverlayDisplayConfig())
{
    KeyboardOverlayLayout geom;
    geom.hudBounds = hudBounds;
    const qreal keyW = qBound(32.0, cfg.keySize, 96.0);
    const qreal keyH = qBound(46.0, keyW * (54.0 / 64.0), 84.0);
    const qreal gap = qBound(4.0, keyW * 0.10, 8.0);
    const qreal panelPaddingX = qBound(18.0, keyW * 0.38, 36.0);
    const qreal panelPaddingY = qBound(10.0, keyH * 0.25, 20.0);
    const qreal headerHeight = 26.0;

    // Standard QWERTY 3-row layout:
    // Row 1: 10 keys (QWERTYUIOP)
    // Row 2:  9 keys (ASDFGHJKL)
    // Row 3:  7 keys (ZXCVBNM)
    const QString rows[3] = {
        QStringLiteral("QWERTYUIOP"),
        QStringLiteral("ASDFGHJKL"),
        QStringLiteral("ZXCVBNM")
    };

    const qreal maxRowWidth = 10 * keyW + 9 * gap;
    const qreal panelW = maxRowWidth + 2 * panelPaddingX;
    const qreal panelH = 3 * keyH + 2 * gap + 2 * panelPaddingY + headerHeight;

    // Position panel horizontally centered
    const qreal panelX = screenRect.left() + (screenRect.width() - panelW) / 2.0;
    qreal panelY = screenRect.bottom() - panelH - 34.0;

    // If HUD dock is present at the bottom, float cleanly above it to prevent collision
    if (hudBounds.isValid() && hudBounds.bottom() >= screenRect.bottom() - 160.0) {
        const qreal elevatedY = hudBounds.top() - panelH - 12.0;
        if (elevatedY > screenRect.top() + 60.0) {
            panelY = elevatedY;
        }
    }

    geom.panelRect = QRectF(panelX, panelY, panelW, panelH);

    // Keyboard physical stagger offsets for rows 2 and 3
    const qreal rowOffsets[3] = {
        0.0,
        (keyW + gap) * 0.45,  // ~27px stagger for home row
        (keyW + gap) * 1.15   // ~69px stagger for bottom row
    };

    const qreal contentStartX = panelX + panelPaddingX;
    const qreal contentStartY = panelY + panelPaddingY + headerHeight;

    for (int r = 0; r < 3; ++r) {
        const QString &keysInRow = rows[r];
        const qreal rowY = contentStartY + r * (keyH + gap);
        const qreal rowX = contentStartX + rowOffsets[r];
        for (int c = 0; c < keysInRow.length(); ++c) {
            const QChar ch = keysInRow.at(c);
            const qreal kx = rowX + c * (keyW + gap);
            KeycapGeometry kg{
                ch,
                QRectF(kx, rowY, keyW, keyH),
                r,
                c
            };
            geom.keycaps.append(kg);
            geom.keyMap.insert(ch, kg);
        }
    }

    return geom;
}

inline bool segmentsIntersect(const QPointF &p1, const QPointF &p2, const QPointF &p3, const QPointF &p4)
{
    auto ccw = [](const QPointF &a, const QPointF &b, const QPointF &c) -> qreal {
        return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
    };
    const qreal cp1 = ccw(p1, p2, p3);
    const qreal cp2 = ccw(p1, p2, p4);
    const qreal cp3 = ccw(p3, p4, p1);
    const qreal cp4 = ccw(p3, p4, p2);

    if (((cp1 > 0 && cp2 < 0) || (cp1 < 0 && cp2 > 0)) &&
        ((cp3 > 0 && cp4 < 0) || (cp3 < 0 && cp4 > 0))) {
        return true;
    }
    return false;
}

// Computes the closest point on the boundary/perimeter of a (rounded) rectangle from an external point (shortest path)
inline QPointF closestPointOnRoundedRectPerimeter(const QPointF &from, const QRectF &rect, qreal cornerRadius = 8.0, qreal padding = 0.0)
{
    if (!rect.isValid()) {
        return from;
    }
    const qreal cr = qBound(0.0, cornerRadius, qMin(rect.width(), rect.height()) / 2.0);
    const QRectF r = rect.adjusted(-padding, -padding, padding, padding);

    // If 'from' is inside 'r' (fallback), push to nearest exterior edge
    if (r.contains(from)) {
        const qreal dLeft = from.x() - r.left();
        const qreal dRight = r.right() - from.x();
        const qreal dTop = from.y() - r.top();
        const qreal dBottom = r.bottom() - from.y();
        const qreal minD = qMin(qMin(dLeft, dRight), qMin(dTop, dBottom));
        if (minD == dLeft) return QPointF(r.left(), from.y());
        if (minD == dRight) return QPointF(r.right(), from.y());
        if (minD == dTop) return QPointF(from.x(), r.top());
        return QPointF(from.x(), r.bottom());
    }

    // Check corner zones if cornerRadius > 0
    if (cr > 0.0) {
        const qreal rEff = cr + padding;
        const qreal leftArcX = rect.left() + cr;
        const qreal rightArcX = rect.right() - cr;
        const qreal topArcY = rect.top() + cr;
        const qreal bottomArcY = rect.bottom() - cr;

        QPointF center;
        bool inCorner = false;
        if (from.x() < leftArcX && from.y() < topArcY) {
            center = QPointF(leftArcX, topArcY);
            inCorner = true;
        } else if (from.x() > rightArcX && from.y() < topArcY) {
            center = QPointF(rightArcX, topArcY);
            inCorner = true;
        } else if (from.x() < leftArcX && from.y() > bottomArcY) {
            center = QPointF(leftArcX, bottomArcY);
            inCorner = true;
        } else if (from.x() > rightArcX && from.y() > bottomArcY) {
            center = QPointF(rightArcX, bottomArcY);
            inCorner = true;
        }

        if (inCorner) {
            const qreal cdx = from.x() - center.x();
            const qreal cdy = from.y() - center.y();
            const qreal cdist = std::hypot(cdx, cdy);
            if (cdist > 0.001) {
                return QPointF(center.x() + (cdx / cdist) * rEff,
                               center.y() + (cdy / cdist) * rEff);
            }
            return center;
        }
    }

    // Straight edge zones: orthogonal clamping strictly minimizes Euclidean distance (shortest path)
    const qreal qx = qBound(r.left(), from.x(), r.right());
    const qreal qy = qBound(r.top(), from.y(), r.bottom());
    return QPointF(qx, qy);
}

// Spatial key matching with Planar Untangling (eliminates crossing laser lines)
inline QMap<QChar, WindowTargetItem> assignSpatialKeys(const QList<WindowTargetItem> &windows, const QRectF &screenRect, bool fixedLetterColors = true, const QSet<QChar> &reservedKeys = {})
{
    QMap<QChar, WindowTargetItem> assigned;
    if (windows.isEmpty()) {
        return assigned;
    }

    const auto &palette = distinctArrowColors();
    QSet<QChar> usedKeys = reservedKeys;
    QList<WindowTargetItem> pendingWindows;

    struct AssignmentEntry {
        QChar key;
        WindowTargetItem item;
        QPointF targetCenter;
        bool isUserLocked = false;
    };
    QList<AssignmentEntry> assignedList;

    // 1. Process explicit user marks / preferred keys first
    for (const auto &win : windows) {
        if (!win.preferredKey.isNull() && win.preferredKey.isLetter()) {
            const QChar upperKey = win.preferredKey.toUpper();
            if (!usedKeys.contains(upperKey)) {
                usedKeys.insert(upperKey);
                WindowTargetItem item = win;
                item.preferredKey = upperKey;
                QPointF center = item.geometry.center();
                if (!item.isVisibleOnScreen && item.hudCardRect.isValid()) {
                    center = item.hudCardRect.center();
                }
                assignedList.append({upperKey, item, center, true});
                continue;
            }
        }
        pendingWindows.append(win);
    }

    // Normalized ideal coordinates (0.0 .. 1.0) for each key in QWERTY layout
    struct KeySpatialPos {
        QChar key;
        qreal normX;
        qreal normY;
    };

    static const QList<KeySpatialPos> keyPositions = {
        // Row 1 (Top row, normY ~ 0.20)
        { QChar('Q'), 0.05, 0.20 }, { QChar('W'), 0.15, 0.20 }, { QChar('E'), 0.25, 0.20 },
        { QChar('R'), 0.35, 0.20 }, { QChar('T'), 0.45, 0.20 }, { QChar('Y'), 0.55, 0.20 },
        { QChar('U'), 0.65, 0.20 }, { QChar('I'), 0.75, 0.20 }, { QChar('O'), 0.85, 0.20 }, { QChar('P'), 0.95, 0.20 },
        // Row 2 (Home row, normY ~ 0.50)
        { QChar('A'), 0.08, 0.50 }, { QChar('S'), 0.18, 0.50 }, { QChar('D'), 0.28, 0.50 },
        { QChar('F'), 0.38, 0.50 }, { QChar('G'), 0.48, 0.50 }, { QChar('H'), 0.58, 0.50 },
        { QChar('J'), 0.68, 0.50 }, { QChar('K'), 0.78, 0.50 }, { QChar('L'), 0.88, 0.50 },
        // Row 3 (Bottom row, normY ~ 0.80)
        { QChar('Z'), 0.15, 0.80 }, { QChar('X'), 0.25, 0.80 }, { QChar('C'), 0.35, 0.80 },
        { QChar('V'), 0.45, 0.80 }, { QChar('B'), 0.55, 0.80 }, { QChar('N'), 0.65, 0.80 }, { QChar('M'), 0.75, 0.80 }
    };

    const qreal sW = qMax<qreal>(100.0, screenRect.width());
    const qreal sH = qMax<qreal>(100.0, screenRect.height());

    // Sort pending windows: top-to-bottom, left-to-right (matching HUD badge order for overlapping windows)
    std::sort(pendingWindows.begin(), pendingWindows.end(), [](const WindowTargetItem &a, const WindowTargetItem &b) {
        const bool hasHudA = a.hudCardRect.isValid();
        const bool hasHudB = b.hudCardRect.isValid();
        if (hasHudA && hasHudB && a.geometry.intersects(b.geometry)) {
            const qreal hudDx = a.hudCardRect.center().x() - b.hudCardRect.center().x();
            if (qAbs(hudDx) > 1.0) {
                return hudDx < 0.0;
            }
        }
        if (qAbs(a.geometry.center().y() - b.geometry.center().y()) > 80.0) {
            return a.geometry.center().y() < b.geometry.center().y();
        }
        return a.geometry.center().x() < b.geometry.center().x();
    });

    for (const auto &win : pendingWindows) {
        QPointF center = win.geometry.center();
        if (!win.isVisibleOnScreen && win.hudCardRect.isValid()) {
            center = win.hudCardRect.center();
        }
        const qreal winNormX = qBound(0.0, (center.x() - screenRect.left()) / sW, 1.0);
        const qreal winNormY = qBound(0.0, (center.y() - screenRect.top()) / sH, 1.0);

        QChar bestKey;
        qreal bestDistSq = 999999.0;

        for (const auto &kpos : keyPositions) {
            if (usedKeys.contains(kpos.key)) {
                continue;
            }
            const qreal dx = winNormX - kpos.normX;
            const qreal dy = winNormY - kpos.normY;
            const qreal distSq = (dx * dx * 1.6) + (dy * dy * 1.0);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                bestKey = kpos.key;
            }
        }

        if (!bestKey.isNull()) {
            usedKeys.insert(bestKey);
            WindowTargetItem item = win;
            item.preferredKey = bestKey;
            assignedList.append({bestKey, item, center, false});
        }
    }

    if (assignedList.isEmpty()) {
        return assigned;
    }

    // 2-opt Planar Untangling Algorithm (eliminates crossing laser lines)
    const KeyboardOverlayLayout layout = computeOverlayGeometry(screenRect);
    auto keyStartPt = [&](const QChar &k) -> QPointF {
        if (layout.keyMap.contains(k)) {
            const auto &kg = layout.keyMap.value(k);
            return QPointF(kg.rect.center().x(), kg.rect.top() + 3.0);
        }
        return QPointF();
    };

    bool untangled = true;
    int pass = 0;
    while (untangled && pass < 80) {
        untangled = false;
        pass++;
        for (int i = 0; i < assignedList.size(); ++i) {
            for (int j = i + 1; j < assignedList.size(); ++j) {
                if (assignedList[i].isUserLocked && assignedList[j].isUserLocked) {
                    continue;
                }
                const QPointF pKa = keyStartPt(assignedList[i].key);
                const QPointF pWa = assignedList[i].targetCenter;
                const QPointF pKb = keyStartPt(assignedList[j].key);
                const QPointF pWb = assignedList[j].targetCenter;

                bool intersect = segmentsIntersect(pKa, pWa, pKb, pWb);

                // Also check laser extension lines to HUD badges
                const bool hasHudA = assignedList[i].item.hudCardRect.isValid();
                const bool hasHudB = assignedList[j].item.hudCardRect.isValid();
                if (!intersect && hasHudA && hasHudB) {
                    const QPointF pHudA = closestPointOnRoundedRectPerimeter(pWa, assignedList[i].item.hudCardRect, 8.0, 2.0);
                    const QPointF pHudB = closestPointOnRoundedRectPerimeter(pWb, assignedList[j].item.hudCardRect, 8.0, 2.0);
                    if (segmentsIntersect(pWa, pHudA, pWb, pHudB)) {
                        intersect = true;
                    }
                }

                const qreal curDist = QLineF(pKa, pWa).length() + QLineF(pKb, pWb).length();
                const qreal swapDist = QLineF(pKa, pWb).length() + QLineF(pKb, pWa).length();

                // Swap keys if laser lines intersect, or if swapping reduces total line length
                if (intersect || (swapDist < curDist - 2.0 && !assignedList[i].isUserLocked && !assignedList[j].isUserLocked)) {
                    if (!assignedList[i].isUserLocked && !assignedList[j].isUserLocked) {
                        std::swap(assignedList[i].key, assignedList[j].key);
                        untangled = true;
                    }
                }
            }
        }
    }

    for (int i = 0; i < assignedList.size(); ++i) {
        assignedList[i].item.preferredKey = assignedList[i].key;
        if (!assignedList[i].item.customColor.isValid()) {
            if (fixedLetterColors) {
                assignedList[i].item.customColor = colorForLetter(assignedList[i].key);
            } else {
                assignedList[i].item.customColor = palette[i % palette.size()];
            }
        }
        assigned.insert(assignedList[i].key, assignedList[i].item);
    }

    return assigned;
}

struct TargetBadgePlacement {
    QChar key;
    WindowTargetItem item;
    QPointF center;
    qreal radius = 18.0;
    qreal opacity = 1.0;
    int stackLevel = 0; // 0 = deepest underneath in stack
    int stackTotal = 1; // total stacked windows in cluster
};

// Computes side-by-side placements and sizes for window target badges.
// If windows are occluded or stacked:
// - The deepest window underneath (lowest z-order) is positioned on the far left with the smallest badge and faded opacity.
// - The frontmost window on surface (highest z-order) is positioned on the far right with the largest badge and full opacity.
// - Intermediate windows are spaced side-by-side with monotonically increasing sizes and opacities.
inline QMap<QChar, TargetBadgePlacement> computeTargetBadgePlacements(
    const QMap<QChar, WindowTargetItem> &assignedWindows,
    const QRectF &screenRect,
    const KeyboardOverlayLayout &layout,
    const OverlayDisplayConfig &cfg = OverlayDisplayConfig())
{
    QMap<QChar, TargetBadgePlacement> placements;
    if (assignedWindows.isEmpty()) {
        return placements;
    }

    const QList<QChar> keys = assignedWindows.keys();
    const int n = keys.size();

    auto windowsOverlap = [&](const WindowTargetItem &a, const WindowTargetItem &b) -> bool {
        if (!a.isVisibleOnScreen || !b.isVisibleOnScreen) {
            return false;
        }
        // Focused windows have no laser targets and must never drag other windows into clusters
        if (a.isCurrentFocused || b.isCurrentFocused) {
            return false;
        }
        if (!a.geometry.intersects(b.geometry)) {
            return false;
        }
        // Two windows are only considered stacked / occluded for side-by-side clustering
        // if their badge locations would physically collide/overlap on screen.
        // Partial tile/background overlaps with distinct center locations must NEVER be glued together.
        const qreal collisionDist = qMax(65.0, 2.0 * cfg.badgeRadius + cfg.stackBadgeGap + 15.0);
        const qreal dist = std::hypot(a.geometry.center().x() - b.geometry.center().x(),
                                      a.geometry.center().y() - b.geometry.center().y());
        return (dist < collisionDist);
    };

    QVector<bool> visited(n, false);
    QList<QList<int>> clusters;

    for (int i = 0; i < n; ++i) {
        if (visited[i]) continue;
        QList<int> cluster;
        QList<int> queue = { i };
        visited[i] = true;
        while (!queue.isEmpty()) {
            int curr = queue.takeFirst();
            cluster.append(curr);
            for (int j = 0; j < n; ++j) {
                if (!visited[j] && windowsOverlap(assignedWindows.value(keys[curr]), assignedWindows.value(keys[j]))) {
                    visited[j] = true;
                    queue.append(j);
                }
            }
        }
        clusters.append(cluster);
    }

    for (const auto &clusterIndices : clusters) {
        const int clusterSize = clusterIndices.size();
        if (clusterSize == 1) {
            const QChar key = keys[clusterIndices.first()];
            const WindowTargetItem &item = assignedWindows.value(key);
            TargetBadgePlacement bp;
            bp.key = key;
            bp.item = item;
            bp.center = item.geometry.center();
            bp.radius = cfg.badgeRadius;
            bp.opacity = 1.0;
            bp.stackLevel = 0;
            bp.stackTotal = 1;
            bp.center.setX(qBound(screenRect.left() + 25.0, bp.center.x(), screenRect.right() - 25.0));
            qreal minY = screenRect.top() + 25.0;
            qreal maxY = layout.panelRect.top() - 35.0;
            if (layout.hudBounds.isValid()) {
                if (layout.hudBounds.top() > screenRect.center().y()) {
                    maxY = qMin(maxY, layout.hudBounds.top() - bp.radius - 12.0);
                } else {
                    minY = qMax(minY, layout.hudBounds.bottom() + bp.radius + 12.0);
                }
            }
            if (item.hudCardRect.isValid()) {
                if (item.hudCardRect.top() > screenRect.center().y()) {
                    maxY = qMin(maxY, item.hudCardRect.top() - bp.radius - 12.0);
                } else {
                    minY = qMax(minY, item.hudCardRect.bottom() + bp.radius + 12.0);
                }
            }
            bp.center.setY(qBound(minY, bp.center.y(), maxY));
            placements.insert(key, bp);
            continue;
        }

        // Multiple occluded / stacked windows in this cluster!
        // Sort cluster items so that their side-by-side target order matches the layout of their badges in HUD.
        // If HUD badges are present, sort cluster items by their HUD badge positions (left-to-right along HUD),
        // preventing laser extension lines between target badges and HUD badges from crossing / tangling!
        QList<int> sortedCluster = clusterIndices;
        std::sort(sortedCluster.begin(), sortedCluster.end(), [&](int a, int b) {
            const auto &itemA = assignedWindows.value(keys[a]);
            const auto &itemB = assignedWindows.value(keys[b]);
            const bool hasHudA = itemA.hudCardRect.isValid();
            const bool hasHudB = itemB.hudCardRect.isValid();
            if (hasHudA && hasHudB) {
                const qreal dx = itemA.hudCardRect.center().x() - itemB.hudCardRect.center().x();
                if (qAbs(dx) > 1.0) {
                    return dx < 0.0;
                }
                const qreal dy = itemA.hudCardRect.center().y() - itemB.hudCardRect.center().y();
                if (qAbs(dy) > 1.0) {
                    return dy < 0.0;
                }
            } else if (hasHudA != hasHudB) {
                return hasHudA;
            }
            if (itemA.zOrder != itemB.zOrder) {
                return itemA.zOrder < itemB.zOrder;
            }
            return itemA.geometry.center().x() < itemB.geometry.center().x();
        });

        // Compute each window's depth rank in cluster based on zOrder (0 = deepest underneath, clusterSize - 1 = topmost)
        QVector<int> stackRanks(clusterSize, 0);
        for (int i = 0; i < clusterSize; ++i) {
            int rank = 0;
            const auto &itemI = assignedWindows.value(keys[sortedCluster[i]]);
            for (int j = 0; j < clusterSize; ++j) {
                if (i == j) continue;
                const auto &itemJ = assignedWindows.value(keys[sortedCluster[j]]);
                if (itemJ.zOrder < itemI.zOrder) {
                    rank++;
                } else if (itemJ.zOrder == itemI.zOrder && j < i) {
                    rank++;
                }
            }
            stackRanks[i] = rank;
        }

        // Radius & Opacity scale monotonically from bottommost (smallest, faded) to topmost (largest, opaque)
        const qreal maxRadius = (qAbs(cfg.badgeRadius - 18.0) < 0.001)
            ? 21.0
            : qMax(10.0, cfg.badgeRadius * (21.0 / 18.0));
        const qreal scaleFactor = qBound(0.0, cfg.stackScaleReduction, 0.70);
        const qreal minRadius = (qAbs(cfg.stackScaleReduction - 0.35) < 0.001 && qAbs(cfg.badgeRadius - 18.0) < 0.001)
            ? 12.0
            : qMax(6.0, maxRadius * (1.0 - scaleFactor));
        const qreal badgeGap = (cfg.stackBadgeGap > 0.0) ? cfg.stackBadgeGap : 8.0;

        QVector<qreal> radii(clusterSize);
        QVector<qreal> opacities(clusterSize);
        const qreal opacityFactor = qBound(0.0, cfg.stackOpacityReduction, 0.70);
        for (int r = 0; r < clusterSize; ++r) {
            const qreal progress = (clusterSize > 1)
                ? static_cast<qreal>(r) / static_cast<qreal>(clusterSize - 1)
                : 1.0;
            radii[r] = minRadius + progress * (maxRadius - minRadius);
            opacities[r] = (1.0 - opacityFactor) + progress * opacityFactor;
        }

        qreal totalWidth = 0.0;
        for (int i = 0; i < clusterSize; ++i) {
            const int rank = stackRanks[i];
            totalWidth += 2.0 * radii[rank];
            if (i > 0) {
                totalWidth += badgeGap;
            }
        }

        qreal sumX = 0.0;
        qreal sumY = 0.0;
        for (int idx : sortedCluster) {
            const auto &it = assignedWindows.value(keys[idx]);
            sumX += it.geometry.center().x();
            sumY += it.geometry.center().y();
        }
        const qreal avgX = sumX / clusterSize;
        const qreal avgY = sumY / clusterSize;

        const qreal clampedCenterX = qBound(screenRect.left() + totalWidth / 2.0 + 20.0,
                                            avgX,
                                            screenRect.right() - totalWidth / 2.0 - 20.0);
        qreal minY = screenRect.top() + maxRadius + 20.0;
        qreal maxY = layout.panelRect.top() - maxRadius - 35.0;
        if (layout.hudBounds.isValid()) {
            if (layout.hudBounds.top() > screenRect.center().y()) {
                maxY = qMin(maxY, layout.hudBounds.top() - maxRadius - 12.0);
            } else {
                minY = qMax(minY, layout.hudBounds.bottom() + maxRadius + 12.0);
            }
        }
        const qreal clampedCenterY = qBound(minY, avgY, maxY);

        // Position from left to right matching the HUD badge layout order
        qreal currLeft = clampedCenterX - totalWidth / 2.0;
        for (int i = 0; i < clusterSize; ++i) {
            const QChar key = keys[sortedCluster[i]];
            const WindowTargetItem &item = assignedWindows.value(keys[sortedCluster[i]]);
            const int rank = stackRanks[i];
            const qreal r = radii[rank];
            const qreal cx = currLeft + r;
            const qreal cy = clampedCenterY;

            TargetBadgePlacement bp;
            bp.key = key;
            bp.item = item;
            bp.center = QPointF(cx, cy);
            bp.radius = r;
            bp.opacity = opacities[rank];
            bp.stackLevel = rank;
            bp.stackTotal = clusterSize;
            placements.insert(key, bp);

            currLeft += 2.0 * r + badgeGap;
        }
    }

    return placements;
}

struct TargetLabelPlacement {
    QChar key;
    QRectF rect;
    QString text;
    QPointF badgeCenter;
    qreal badgeRadius = 18.0;
    QColor borderColor;
    QColor bgColor;
    QColor textColor;
    bool hasConnector = false;
    QPointF connectorFrom;
    QPointF connectorTo;
};

// Computes non-overlapping positions for target labels avoiding collision between labels,
// avoiding collision with target badge circles, and avoiding offscreen/panel clipping.
// Labels are guaranteed to be 100% opaque (no transparency) for maximum contrast and readability.
inline QList<TargetLabelPlacement> computeTargetLabelPlacements(
    const QMap<QChar, TargetBadgePlacement> &placements,
    const QRectF &screenRect,
    const KeyboardOverlayLayout &layout,
    const OverlayDisplayConfig &cfg = OverlayDisplayConfig())
{
    QList<TargetLabelPlacement> result;
    if (!cfg.targetLabelVisible || placements.isEmpty()) {
        return result;
    }

    const QString effFontFamily = cfg.fontFamily.isEmpty() ? QStringLiteral("sans-serif") : cfg.fontFamily;
    const int effLabelFontSize = qBound(6, cfg.targetLabelFontSize, 24);
    QFont tagFont(effFontFamily, effLabelFontSize, QFont::Bold);
    tagFont.setStyleStrategy(QFont::PreferAntialias);
    QFontMetrics fm(tagFont);

    // Requirement 2: Labels must have NO transparency (alpha = 255) for crisp, clear readability
    QColor baseBg = cfg.targetLabelBg.isValid() ? cfg.targetLabelBg : QColor(14, 18, 26);
    baseBg.setAlpha(255);

    QColor baseTextCol = cfg.targetLabelTextColor.isValid() ? cfg.targetLabelTextColor : QColor(255, 255, 255);
    baseTextCol.setAlpha(255);

    struct CandidateItem {
        QChar key;
        TargetBadgePlacement bp;
        QString displayLabel;
        int tagW = 0;
        int tagH = 0;
        QColor borderCol;
        QColor textCol;
    };

    QList<CandidateItem> itemsToPlace;

    for (auto it = placements.constBegin(); it != placements.constEnd(); ++it) {
        const TargetBadgePlacement &bp = it.value();
        if (bp.item.isCurrentFocused) {
            continue;
        }

        // Only place labels on windows physically visible on screen with on-screen target badges
        if (!bp.item.isVisibleOnScreen && cfg.extendToHud && bp.item.hudCardRect.isValid()) {
            continue;
        }

        const QString fullLabel = !bp.item.customLabel.isEmpty()
            ? bp.item.customLabel
            : (!bp.item.title.isEmpty() ? bp.item.title : bp.item.appClass);

        if (fullLabel.isEmpty()) {
            continue;
        }

        const qreal maxLabelW = qMax(180.0, bp.item.geometry.width() > 50.0 ? bp.item.geometry.width() * 0.85 : 220.0);
        const QString displayLabel = fm.elidedText(fullLabel, Qt::ElideMiddle, maxLabelW);
        const int tagW = fm.horizontalAdvance(displayLabel) + 14;
        const int tagH = fm.height() + 6;

        const QColor baseArrowCol = bp.item.customColor.isValid() ? bp.item.customColor : QColor(0, 229, 255);
        QColor borderCol(baseArrowCol.red(), baseArrowCol.green(), baseArrowCol.blue(), 255);
        QColor textColor = cfg.targetLabelTextColor.isValid() ? baseTextCol : borderCol;
        textColor.setAlpha(255);

        CandidateItem ci;
        ci.key = it.key();
        ci.bp = bp;
        ci.displayLabel = displayLabel;
        ci.tagW = tagW;
        ci.tagH = tagH;
        ci.borderCol = borderCol;
        ci.textCol = textColor;
        itemsToPlace.append(ci);
    }

    if (itemsToPlace.isEmpty()) {
        return result;
    }

    // Collect all badge bounding circles and HUD cards as protected obstacles so labels never cover key letters or HUD cards/badges
    QList<QRectF> badgeObstacles;
    for (auto it = placements.constBegin(); it != placements.constEnd(); ++it) {
        if (!it.value().item.isCurrentFocused) {
            const QPointF &c = it.value().center;
            const qreal r = it.value().radius + 2.0;
            badgeObstacles.append(QRectF(c.x() - r, c.y() - r, r * 2.0, r * 2.0));
            if (it.value().item.hudCardRect.isValid()) {
                badgeObstacles.append(it.value().item.hudCardRect.adjusted(-2.0, -2.0, 2.0, 2.0));
            }
        }
    }
    if (layout.hudBounds.isValid()) {
        badgeObstacles.append(layout.hudBounds.adjusted(-2.0, -2.0, 2.0, 2.0));
    }

    qreal minScreenY = screenRect.top() + 6.0;
    qreal maxScreenY = layout.panelRect.isValid() ? (layout.panelRect.top() - 6.0) : (screenRect.bottom() - 6.0);
    if (layout.hudBounds.isValid()) {
        if (layout.hudBounds.top() > screenRect.center().y()) {
            maxScreenY = qMin(maxScreenY, layout.hudBounds.top() - 6.0);
        } else {
            minScreenY = qMax(minScreenY, layout.hudBounds.bottom() + 6.0);
        }
    }
    const qreal minScreenX = screenRect.left() + 6.0;
    const qreal maxScreenX = screenRect.right() - 6.0;

    for (const auto &item : itemsToPlace) {
        const QPointF &bc = item.bp.center;
        const qreal r = item.bp.radius;
        const qreal w = item.tagW;
        const qreal h = item.tagH;

        // Generate prioritized candidate positions
        QList<QRectF> candidates;

        // In clusters, alternate: even stackLevel prefers Above, odd prefers Below
        bool preferAbove = (item.bp.stackTotal > 1) && (item.bp.stackLevel % 2 == 1);
        if (bc.y() - r - h - 4.0 < minScreenY) {
            preferAbove = false;
        } else if (bc.y() + r + h + 4.0 > maxScreenY) {
            preferAbove = true;
        }

        if (preferAbove) {
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() - r - h - 4.0, w, h));
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() + r + 4.0, w, h));
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() - r - 2.0 * h - 8.0, w, h));
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() + r + 4.0 + h + 4.0, w, h));
        } else {
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() + r + 4.0, w, h));
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() - r - h - 4.0, w, h));
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() + r + 4.0 + h + 4.0, w, h));
            candidates.append(QRectF(bc.x() - w / 2.0, bc.y() - r - 2.0 * h - 8.0, w, h));
        }

        candidates.append(QRectF(bc.x() - w / 2.0, bc.y() + r + 4.0 + 2.0 * (h + 4.0), w, h));
        candidates.append(QRectF(bc.x() - w / 2.0, bc.y() - r - 3.0 * h - 12.0, w, h));
        candidates.append(QRectF(bc.x() - w - r - 6.0, bc.y() - h / 2.0, w, h));
        candidates.append(QRectF(bc.x() + r + 6.0, bc.y() - h / 2.0, w, h));

        QRectF bestRect;
        qreal bestScore = 1e12;

        for (int cIdx = 0; cIdx < candidates.size(); ++cIdx) {
            QRectF cand = candidates[cIdx];

            // Horizontal clamping inside screen
            if (cand.left() < minScreenX) {
                cand.moveLeft(minScreenX);
            }
            if (cand.right() > maxScreenX) {
                cand.moveRight(maxScreenX);
            }

            qreal penalty = 0.0;

            // Penalty for screen vertical boundaries
            if (cand.top() < minScreenY) {
                penalty += 10000000.0 * (minScreenY - cand.top());
            }
            if (cand.bottom() > maxScreenY) {
                penalty += 10000000.0 * (cand.bottom() - maxScreenY);
            }

            // Penalty for covering any target badge (key letter)
            for (const QRectF &bObs : badgeObstacles) {
                if (cand.intersects(bObs)) {
                    const QRectF overlap = cand.intersected(bObs);
                    penalty += 5000000.0 + (overlap.width() * overlap.height() * 200.0);
                }
            }

            // Penalty for overlapping already placed labels
            for (const auto &placed : result) {
                const QRectF paddedPlaced = placed.rect.adjusted(-2.0, -2.0, 2.0, 2.0);
                if (cand.intersects(paddedPlaced)) {
                    const QRectF overlap = cand.intersected(paddedPlaced);
                    penalty += 2000000.0 + (overlap.width() * overlap.height() * 500.0);
                }
            }

            // Preference penalty (earlier candidates preferred)
            penalty += cIdx * 150.0;
            penalty += std::hypot(cand.center().x() - bc.x(), cand.center().y() - bc.y()) * 0.5;

            if (penalty < bestScore) {
                bestScore = penalty;
                bestRect = cand;
            }
        }

        TargetLabelPlacement tlp;
        tlp.key = item.key;
        tlp.rect = bestRect;
        tlp.text = item.displayLabel;
        tlp.badgeCenter = bc;
        tlp.badgeRadius = r;
        tlp.borderColor = item.borderCol;
        tlp.bgColor = baseBg;
        tlp.textColor = item.textCol;

        // If staggered away from badge, calculate connector line coordinates
        if (bestRect.top() > bc.y() + r + 6.0) {
            tlp.hasConnector = true;
            tlp.connectorFrom = QPointF(bc.x(), bc.y() + r);
            tlp.connectorTo = QPointF(bc.x(), bestRect.top());
        } else if (bestRect.bottom() < bc.y() - r - 6.0) {
            tlp.hasConnector = true;
            tlp.connectorFrom = QPointF(bc.x(), bc.y() - r);
            tlp.connectorTo = QPointF(bc.x(), bestRect.bottom());
        }

        result.append(tlp);
    }

    // Iterative relaxation pass: if any remaining collision exists, separate them along the Y axis
    for (int pass = 0; pass < 20; ++pass) {
        bool collisionFound = false;
        for (int i = 0; i < result.size(); ++i) {
            for (int j = i + 1; j < result.size(); ++j) {
                if (result[i].rect.adjusted(-2.0, -2.0, 2.0, 2.0).intersects(result[j].rect)) {
                    collisionFound = true;
                    if (result[j].rect.top() >= result[i].rect.top()) {
                        const qreal shift = (result[i].rect.bottom() + 4.0) - result[j].rect.top();
                        if (result[j].rect.bottom() + shift <= maxScreenY) {
                            result[j].rect.translate(0, shift);
                            if (result[j].hasConnector) result[j].connectorTo.setY(result[j].rect.top());
                        } else {
                            result[i].rect.translate(0, -shift);
                            if (result[i].hasConnector) result[i].connectorTo.setY(result[i].rect.bottom());
                        }
                    } else {
                        const qreal shift = (result[j].rect.bottom() + 4.0) - result[i].rect.top();
                        if (result[i].rect.bottom() + shift <= maxScreenY) {
                            result[i].rect.translate(0, shift);
                            if (result[i].hasConnector) result[i].connectorTo.setY(result[i].rect.top());
                        } else {
                            result[j].rect.translate(0, -shift);
                            if (result[j].hasConnector) result[j].connectorTo.setY(result[j].rect.bottom());
                        }
                    }
                }
            }
        }
        if (!collisionFound) {
            break;
        }
    }

    return result;
}

inline QString elideMiddleTitle(const QString &text, const QFontMetrics &fm, qreal availableWidth)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || availableWidth <= 8.0) {
        return QString();
    }

    if (fm.horizontalAdvance(trimmed) <= availableWidth) {
        return trimmed;
    }

    const QString elided = fm.elidedText(trimmed, Qt::ElideMiddle, qRound(availableWidth));
    if (!elided.isEmpty() && fm.horizontalAdvance(elided) <= availableWidth) {
        return elided;
    }

    // Dynamic prefix/suffix fallback
    const int len = trimmed.length();
    for (int k = qMin(6, len / 2); k >= 2; --k) {
        const QString candidate = trimmed.left(k) + QStringLiteral("…") + trimmed.right(k);
        if (fm.horizontalAdvance(candidate) <= availableWidth) {
            return candidate;
        }
    }

    return trimmed.left(qMax(2, qMin(4, len))) + QStringLiteral("…");
}

inline QImage renderOverlayImage(const KeyboardOverlayLayout &layout,
                                 const QMap<QChar, WindowTargetItem> &assignedWindows,
                                 const QRectF &screenRect,
                                 qreal scale = 1.0,
                                 const OverlayDisplayConfig &cfg = OverlayDisplayConfig(),
                                 const QChar &invalidKey = QChar())
{
    const int imgW = qMax(100, qRound(screenRect.width() * scale));
    const int imgH = qMax(100, qRound(screenRect.height() * scale));

    QImage img(imgW, imgH, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    img.setDevicePixelRatio(scale);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QString effFontFamily = cfg.fontFamily.isEmpty() ? QStringLiteral("sans-serif") : cfg.fontFamily;

    // -------------------------------------------------------------
    // 1. Draw Keyboard Dock Panel
    // -------------------------------------------------------------
    const QRectF &pr = layout.panelRect;

    // Panel soft shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 130));
    p.drawRoundedRect(pr.adjusted(-3.0, 3.0, 3.0, 7.0), 20.0, 20.0);

    // Panel acrylic background fill
    QLinearGradient panelGrad(pr.topLeft(), pr.bottomLeft());
    panelGrad.setColorAt(0.0, QColor(20, 24, 34, 240));
    panelGrad.setColorAt(1.0, QColor(11, 14, 20, 250));
    p.setBrush(panelGrad);
    p.setPen(QPen(QColor(255, 255, 255, 32), 1.6));
    p.drawRoundedRect(pr, 18.0, 18.0);

    // Header hint bar text
    QFont headerFont(effFontFamily, 8, QFont::DemiBold);
    p.setFont(headerFont);
    p.setPen(QColor(170, 185, 210, 190));
    const QRectF headerRect(pr.left(), pr.top() + 6.0, pr.width(), 20.0);
    p.drawText(headerRect, Qt::AlignCenter, QStringLiteral("LEAPK SPATIAL KEYBOARD  •  PRESS KEY TO LEAP  •  [ESC: EXIT]"));

    // Subtle divider line below header
    p.setPen(QPen(QColor(255, 255, 255, 18), 1.0));
    p.drawLine(QPointF(pr.left() + 20.0, pr.top() + 26.0), QPointF(pr.right() - 20.0, pr.top() + 26.0));

    // -------------------------------------------------------------
    // 2. Draw All 26 Keycaps (Unassigned = Translucent, Assigned = Glowing)
    // -------------------------------------------------------------
    const int effKeyFontSize = qBound(8, cfg.keyFontSize, 28);
    QFont keyFont(effFontFamily, effKeyFontSize, QFont::Bold);
    keyFont.setStyleStrategy(QFont::PreferAntialias);
    QFont subFont(effFontFamily, qMax(6, effKeyFontSize / 2), QFont::Bold);
    subFont.setStyleStrategy(QFont::PreferAntialias);

    for (const KeycapGeometry &kg : layout.keycaps) {
        const bool isAssigned = assignedWindows.contains(kg.key);
        const bool isKeyInvalid = (!invalidKey.isNull() && kg.key == invalidKey);

        if (isKeyInvalid) {
            // Negative feedback aura on keycap when invalid key was pressed
            p.setPen(QPen(QColor(235, 50, 60, 230), 3.0));
            p.setBrush(QColor(235, 50, 60, 75));
            p.drawRoundedRect(kg.rect.adjusted(-2.0, -2.0, 2.0, 2.0), 9.0, 9.0);
        }

        if (!isAssigned) {
            // UNASSIGNED KEY: Semi-transparent ghosted keycap
            p.setPen(QPen(QColor(255, 255, 255, 16), 1.0));
            p.setBrush(isKeyInvalid ? QColor(80, 20, 28, 120) : QColor(28, 32, 42, 55));
            p.drawRoundedRect(kg.rect, 8.0, 8.0);

            p.setFont(keyFont);
            if (isKeyInvalid) {
                p.setPen(QColor(255, 120, 130, 240));
            } else if (cfg.fixedLetterColors) {
                QColor unassignedCol = colorForLetter(kg.key);
                unassignedCol.setAlpha(70);
                p.setPen(unassignedCol);
            } else {
                p.setPen(QColor(255, 255, 255, 48));
            }
            p.drawText(kg.rect, Qt::AlignCenter, QString(kg.key));
        } else {
            // ASSIGNED KEY: Highlighted, illuminated, with distinct vibrant color
            const WindowTargetItem &item = assignedWindows.value(kg.key);
            const QColor col = item.customColor.isValid() ? item.customColor : QColor(0, 229, 255);

            // Key outer glow
            p.setPen(QPen(QColor(col.red(), col.green(), col.blue(), 80), 3.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(kg.rect.adjusted(-1.0, -1.0, 1.0, 1.0), 9.0, 9.0);

            // Keycap body gradient
            QLinearGradient keyGrad(kg.rect.topLeft(), kg.rect.bottomLeft());
            keyGrad.setColorAt(0.0, QColor(32, 38, 52, 250));
            keyGrad.setColorAt(1.0, QColor(16, 20, 28, 250));
            p.setPen(QPen(col, 2.2));
            p.setBrush(keyGrad);
            p.drawRoundedRect(kg.rect, 8.0, 8.0);

            // Colored top accent bar
            p.setPen(Qt::NoPen);
            p.setBrush(col);
            const QRectF topAccent(kg.rect.left() + 4.0, kg.rect.top() + 2.0, kg.rect.width() - 8.0, 3.0);
            p.drawRoundedRect(topAccent, 1.5, 1.5);

            // Resolve window display title (prefer custom label, then window title, then app class)
            QString rawTitle;
            if (!item.customLabel.trimmed().isEmpty()) {
                rawTitle = item.customLabel.trimmed();
            } else if (!item.title.trimmed().isEmpty()) {
                rawTitle = item.title.trimmed();
            } else if (!item.appClass.trimmed().isEmpty()) {
                rawTitle = item.appClass.trimmed();
            }

            const bool hasTitle = !rawTitle.isEmpty();

            // Key letter (crisp bold white with slight shadow in upper portion)
            p.setFont(keyFont);
            p.setPen(QColor(0, 0, 0, 180));
            const QRectF letterRect = hasTitle
                ? QRectF(kg.rect.left(), kg.rect.top() + 4.0, kg.rect.width(), kg.rect.height() - 22.0)
                : kg.rect;
            const QRectF textShadowRect = letterRect.adjusted(0, 1.0, 0, 1.0);
            p.drawText(textShadowRect, Qt::AlignCenter, QString(kg.key));

            p.setPen(cfg.fixedLetterColors ? col : Qt::white);
            p.drawText(letterRect, Qt::AlignCenter, QString(kg.key));

            // Window title pill at bottom of keycap
            if (hasTitle) {
                const QRectF labelPill(kg.rect.left() + 3.0, kg.rect.bottom() - 17.5, kg.rect.width() - 6.0, 15.0);

                // Contrast glassmorphic pill background
                p.setPen(QPen(QColor(col.red(), col.green(), col.blue(), 130), 1.0));
                p.setBrush(QColor(10, 14, 22, 235));
                p.drawRoundedRect(labelPill, 3.5, 3.5);

                // Dynamic font and middle-truncation (macOS-style: "Terminal (bash) - neovim" => "Termin...neovim")
                const int labelFontSize = qBound(6, qRound(effKeyFontSize * 0.52), 10);
                QFont labelFont(effFontFamily, labelFontSize, QFont::DemiBold);
                labelFont.setStyleStrategy(QFont::PreferAntialias);
                const QFontMetrics fm(labelFont);

                const qreal availW = labelPill.width() - 4.0;
                const QString displayTitle = elideMiddleTitle(rawTitle, fm, availW);

                p.setFont(labelFont);
                p.setPen(col);
                p.drawText(labelPill, Qt::AlignCenter, displayTitle);
            }
        }
    }

    // -------------------------------------------------------------
    // 3. Draw Directional Lines & Target Badges ON TOP of the Keyboard
    // -------------------------------------------------------------
    const bool drawLaserLines = !(cfg.limitByWindowCount && cfg.maxWindows > 0 && assignedWindows.size() > cfg.maxWindows);
    const auto placements = computeTargetBadgePlacements(assignedWindows, screenRect, layout, cfg);
    const qreal effLineWidth = qBound(1.0, cfg.lineWidth, 10.0);
    const Qt::PenStyle lineStyle = (cfg.laserDashStyle == QStringLiteral("dashed"))
        ? Qt::DashLine
        : ((cfg.laserDashStyle == QStringLiteral("dotted")) ? Qt::DotLine : Qt::SolidLine);

    for (auto it = placements.constBegin(); it != placements.constEnd(); ++it) {
        const QChar key = it.key();
        const TargetBadgePlacement &bp = it.value();
        if (!layout.keyMap.contains(key)) {
            continue;
        }

        // Point 2: Do not draw line or arrow for the currently focused window
        if (bp.item.isCurrentFocused) {
            continue;
        }

        const KeycapGeometry &kg = layout.keyMap.value(key);
        // Point 5: Emerge from the top edge of the keycap so the letter in the center is completely unobstructed
        const QPointF pStart(kg.rect.center().x(), kg.rect.top() + 3.0);
        const QColor baseArrowCol = bp.item.customColor.isValid() ? bp.item.customColor : QColor(0, 229, 255);
        const qreal effOpacity = qBound(0.2, bp.opacity, 1.0);
        const QColor arrowCol(baseArrowCol.red(), baseArrowCol.green(), baseArrowCol.blue(), qRound(baseArrowCol.alpha() * effOpacity));

        if (drawLaserLines) {
            // Draw glowing launch anchor node right on top accent bar
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), qRound(160 * effOpacity)));
            p.drawEllipse(pStart, 5.0, 5.0);
            p.setBrush(arrowCol);
            p.drawEllipse(pStart, 3.0, 3.0);
            p.setBrush(QColor(255, 255, 255, qRound(255 * effOpacity)));
            p.drawEllipse(pStart, 1.5, 1.5);
        }

        QPointF pEnd = bp.center;
        const bool isTargetInHud = (!bp.item.isVisibleOnScreen && bp.item.hudCardRect.isValid());

        QPointF pTargetPerimeter;
        qreal dx = 0.0;
        qreal dy = 0.0;
        qreal dist = 0.0;
        qreal theta = 0.0;

        if (isTargetInHud) {
            if (cfg.extendToHud) {
                // Shortest path directly to the HUD badge boundary (never connecting to the center)
                pTargetPerimeter = closestPointOnRoundedRectPerimeter(pStart, bp.item.hudCardRect, 8.0, 3.0);
                dx = pTargetPerimeter.x() - pStart.x();
                dy = pTargetPerimeter.y() - pStart.y();
                dist = std::hypot(dx, dy);
                theta = std::atan2(dy, dx);
            }
        } else {
            dx = pEnd.x() - pStart.x();
            dy = pEnd.y() - pStart.y();
            dist = std::hypot(dx, dy);
            if (dist > 0.001) {
                const qreal ux = dx / dist;
                const qreal uy = dy / dist;
                theta = std::atan2(dy, dx);
                pTargetPerimeter = QPointF(pEnd.x() - ux * bp.radius, pEnd.y() - uy * bp.radius);
            }
        }

        if (drawLaserLines && dist > 20.0) {
            // Ambient Laser Glow (if enabled by theme)
            if (cfg.laserGlow) {
                QPen glowPen(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), qRound(50 * effOpacity * cfg.laserGlowIntensity)),
                             effLineWidth + 6.0, lineStyle, Qt::RoundCap, Qt::RoundJoin);
                p.setPen(glowPen);
                p.drawLine(pStart, pTargetPerimeter);
            }

            // Pass A: Contrast drop-shadow for clear visibility across wallpaper and windows
            QPen shadowPen(QColor(0, 0, 0, qRound(190 * effOpacity)), effLineWidth + 3.0, lineStyle, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(shadowPen);
            p.drawLine(pStart, pTargetPerimeter);

            // Pass B: Vibrant neon laser arrow line
            QPen colorPen(arrowCol, effLineWidth, lineStyle, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(colorPen);
            p.drawLine(pStart, pTargetPerimeter);

            // Arrowhead at pTargetPerimeter pointing directly along line of shortest path
            const qreal arrowLen = qBound(12.0, bp.radius * 0.85, 18.0);
            const qreal arrowSpread = 0.48;
            const QPointF tip = pTargetPerimeter;
            const QPointF leftBase(tip.x() - std::cos(theta - arrowSpread) * arrowLen,
                                   tip.y() - std::sin(theta - arrowSpread) * arrowLen);
            const QPointF rightBase(tip.x() - std::cos(theta + arrowSpread) * arrowLen,
                                    tip.y() - std::sin(theta + arrowSpread) * arrowLen);
            const QPointF centerNotch(tip.x() - std::cos(theta) * (arrowLen * 0.65),
                                      tip.y() - std::sin(theta) * (arrowLen * 0.65));

            QPolygonF arrowHead;
            arrowHead << tip << leftBase << centerNotch << rightBase;

            p.setPen(QPen(QColor(0, 0, 0, qRound(200 * effOpacity)), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(arrowCol);
            p.drawPolygon(arrowHead);
        }

        // Point 3: Extend line from window target badge to its card in the HUD dock (if available and enabled)
        if (drawLaserLines && bp.item.isVisibleOnScreen && cfg.extendToHud && bp.item.hudCardRect.isValid()) {
            // Shortest path from window target badge to the HUD badge perimeter (never connecting to the center)
            const QPointF pHud = closestPointOnRoundedRectPerimeter(bp.center, bp.item.hudCardRect, 8.0, 2.0);
            const qreal hdx = pHud.x() - bp.center.x();
            const qreal hdy = pHud.y() - bp.center.y();
            const qreal hdist = std::hypot(hdx, hdy);
            if (hdist > 25.0) {
                const qreal hux = hdx / hdist;
                const qreal huy = hdy / hdist;
                const QPointF pFrom(bp.center.x() + hux * bp.radius, bp.center.y() + huy * bp.radius);

                QPen extShadow(QColor(0, 0, 0, qRound(180 * effOpacity)), effLineWidth + 1.8, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
                p.setPen(extShadow);
                p.drawLine(pFrom, pHud);

                QPen extCol(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), qRound(200 * effOpacity)), qMax(1.5, effLineWidth * 0.70), Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
                p.setPen(extCol);
                p.drawLine(pFrom, pHud);

                // Small connector dot on HUD card outer perimeter (outside badge)
                p.setPen(Qt::NoPen);
                p.setBrush(arrowCol);
                p.drawEllipse(pHud, 3.0, 3.0);
            }
        }

        // Target badge at window target position (skipped if target is in HUD so it never covers the HUD badge)
        if (!isTargetInHud) {
            const qreal ringRadius = bp.radius;
            const QRectF badgeRect(pEnd.x() - ringRadius, pEnd.y() - ringRadius, ringRadius * 2.0, ringRadius * 2.0);

            // Outer ambient glow
            if (cfg.laserGlow) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), qRound(60 * effOpacity * cfg.laserGlowIntensity)));
                p.drawEllipse(pEnd, ringRadius + 5.0, ringRadius + 5.0);
            }

            // Target badge backing & border based on cfg.targetBadgeShape
            QColor badgeBg(14, 18, 26, qRound(245 * effOpacity));
            QPen badgeBorderPen(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), qRound(255 * effOpacity)), 2.6);
            p.setPen(badgeBorderPen);
            p.setBrush(badgeBg);

            if (cfg.targetBadgeShape == QStringLiteral("ring")) {
                p.setBrush(QColor(14, 18, 26, qRound(150 * effOpacity)));
                badgeBorderPen.setWidthF(3.2);
                p.setPen(badgeBorderPen);
                p.drawEllipse(pEnd, ringRadius, ringRadius);
            } else if (cfg.targetBadgeShape == QStringLiteral("rounded_rect")) {
                p.drawRoundedRect(badgeRect, 6.0, 6.0);
            } else if (cfg.targetBadgeShape == QStringLiteral("pill")) {
                p.drawRoundedRect(badgeRect, ringRadius, ringRadius);
            } else if (cfg.targetBadgeShape == QStringLiteral("sharp")) {
                p.drawRect(badgeRect);
            } else if (cfg.targetBadgeShape == QStringLiteral("chamfered")) {
                const qreal ch = qBound(3.0, ringRadius * 0.35, 7.0);
                QPolygonF poly;
                poly << QPointF(badgeRect.left() + ch, badgeRect.top())
                     << QPointF(badgeRect.right() - ch, badgeRect.top())
                     << QPointF(badgeRect.right(), badgeRect.top() + ch)
                     << QPointF(badgeRect.right(), badgeRect.bottom() - ch)
                     << QPointF(badgeRect.right() - ch, badgeRect.bottom())
                     << QPointF(badgeRect.left() + ch, badgeRect.bottom())
                     << QPointF(badgeRect.left(), badgeRect.bottom() - ch)
                     << QPointF(badgeRect.left(), badgeRect.top() + ch);
                p.drawPolygon(poly);
            } else {
                // Default "circle"
                p.drawEllipse(pEnd, ringRadius, ringRadius);
            }

            // Key letter inside target badge (font scaled proportionally to badge radius or config)
            const QColor targetLetterCol = cfg.fixedLetterColors
                ? QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), qRound(255 * effOpacity))
                : QColor(255, 255, 255, qRound(255 * effOpacity));
            p.setPen(targetLetterCol);
            const int effBadgeFontSize = (cfg.badgeFontSize > 0) ? cfg.badgeFontSize : qBound(9, qRound(ringRadius * 0.72), 16);
            QFont letterFont(effFontFamily, effBadgeFontSize, QFont::Bold);
            letterFont.setStyleStrategy(QFont::PreferAntialias);
            p.setFont(letterFont);
            const QRectF badgeTextRect(pEnd.x() - ringRadius, pEnd.y() - ringRadius, ringRadius * 2.0, ringRadius * 2.0);
            p.drawText(badgeTextRect, Qt::AlignCenter, QString(key));
        }
    }

    // -------------------------------------------------------------
    // 4. Draw Collision-Free Target Labels (100% Solid & Opaque)
    // -------------------------------------------------------------
    if (cfg.targetLabelVisible) {
        const auto labelPlacements = computeTargetLabelPlacements(placements, screenRect, layout, cfg);

        const int effLabelFontSize = qBound(6, cfg.targetLabelFontSize, 24);
        QFont tagFont(effFontFamily, effLabelFontSize, QFont::Bold);
        tagFont.setStyleStrategy(QFont::PreferAntialias);
        p.setFont(tagFont);

        for (const auto &lbl : labelPlacements) {
            // Connector line if staggered away from badge
            if (lbl.hasConnector) {
                p.setPen(QPen(lbl.borderColor, 1.2, Qt::DotLine, Qt::RoundCap));
                p.drawLine(lbl.connectorFrom, lbl.connectorTo);
            }

            // Contrast drop shadow
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 180));
            p.drawRoundedRect(lbl.rect.translated(0.0, 1.5), 4.0, 4.0);

            // Requirement 2: Solid, 100% opaque background & solid border (no transparency for clear readability)
            p.setPen(QPen(lbl.borderColor, 1.4));
            p.setBrush(lbl.bgColor);
            p.drawRoundedRect(lbl.rect, 4.0, 4.0);

            // Requirement 2: Solid, 100% opaque label text
            p.setPen(lbl.textColor);
            p.drawText(lbl.rect, Qt::AlignCenter, lbl.text);
        }
    }

    p.end();
    return img;
}

} // namespace LeapKKeyboardOverlay
