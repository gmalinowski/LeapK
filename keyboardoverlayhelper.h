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

struct WindowTargetItem {
    void *windowId = nullptr;
    QRectF geometry;
    QChar preferredKey;
    QString title;
    QString appClass;
    bool isVisibleOnScreen = true;
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
    QList<KeycapGeometry> keycaps;
    QMap<QChar, KeycapGeometry> keyMap;
};

inline KeyboardOverlayLayout computeOverlayGeometry(const QRectF &screenRect, const QRectF &hudBounds = QRectF())
{
    KeyboardOverlayLayout geom;
    const qreal keyW = 54.0;
    const qreal keyH = 46.0;
    const qreal gap = 6.0;
    const qreal panelPaddingX = 26.0;
    const qreal panelPaddingY = 14.0;
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

    const qreal maxRowWidth = 10 * keyW + 9 * gap; // 540 + 54 = 594 px
    const qreal panelW = maxRowWidth + 2 * panelPaddingX; // 646 px
    const qreal panelH = 3 * keyH + 2 * gap + 2 * panelPaddingY + headerHeight; // 138 + 12 + 28 + 26 = 204 px

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

// Spatial key matching:
// Assigns keys based on the spatial positions of windows on the screen,
// so that left windows get left keys, right windows get right keys, top windows get top row.
// Prevents criss-crossing arrows!
inline QMap<QChar, WindowTargetItem> assignSpatialKeys(const QList<WindowTargetItem> &windows, const QRectF &screenRect)
{
    QMap<QChar, WindowTargetItem> assigned;
    if (windows.isEmpty()) {
        return assigned;
    }

    const auto &palette = distinctArrowColors();
    QSet<QChar> usedKeys;
    QList<WindowTargetItem> pendingWindows;

    // 1. Process explicit user marks / preferred keys first
    for (const auto &win : windows) {
        if (!win.preferredKey.isNull() && win.preferredKey.isLetter()) {
            const QChar upperKey = win.preferredKey.toUpper();
            if (!usedKeys.contains(upperKey)) {
                usedKeys.insert(upperKey);
                WindowTargetItem item = win;
                item.preferredKey = upperKey;
                if (!item.customColor.isValid()) {
                    item.customColor = palette[assigned.size() % palette.size()];
                }
                assigned.insert(upperKey, item);
                continue;
            }
        }
        pendingWindows.append(win);
    }

    if (pendingWindows.isEmpty()) {
        return assigned;
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

    // Sort pending windows: top-to-bottom, left-to-right
    std::sort(pendingWindows.begin(), pendingWindows.end(), [](const WindowTargetItem &a, const WindowTargetItem &b) {
        if (qAbs(a.geometry.center().y() - b.geometry.center().y()) > 80.0) {
            return a.geometry.center().y() < b.geometry.center().y();
        }
        return a.geometry.center().x() < b.geometry.center().x();
    });

    for (const auto &win : pendingWindows) {
        const QPointF center = win.geometry.center();
        const qreal winNormX = qBound(0.0, (center.x() - screenRect.left()) / sW, 1.0);
        const qreal winNormY = qBound(0.0, (center.y() - screenRect.top()) / sH, 1.0);

        QChar bestKey;
        qreal bestDistSq = 999999.0;

        for (const auto &kpos : keyPositions) {
            if (usedKeys.contains(kpos.key)) {
                continue;
            }
            // Weight horizontal alignment slightly more to maintain left-to-right order
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
            if (!item.customColor.isValid()) {
                item.customColor = palette[assigned.size() % palette.size()];
            }
            assigned.insert(bestKey, item);
        }
    }

    return assigned;
}

struct TargetBadgePlacement {
    QChar key;
    WindowTargetItem item;
    QPointF center;
    qreal radius = 18.0;
    int stackLevel = 0; // 0 = deepest underneath in stack
    int stackTotal = 1; // total stacked windows in cluster
};

// Computes side-by-side placements and sizes for window target badges.
// If windows are occluded or stacked:
// - The deepest window underneath (lowest z-order) is positioned on the far left with the smallest badge.
// - The frontmost window on surface (highest z-order) is positioned on the far right with the largest badge.
// - Intermediate windows are spaced side-by-side with monotonically increasing sizes.
inline QMap<QChar, TargetBadgePlacement> computeTargetBadgePlacements(
    const QMap<QChar, WindowTargetItem> &assignedWindows,
    const QRectF &screenRect,
    const KeyboardOverlayLayout &layout)
{
    QMap<QChar, TargetBadgePlacement> placements;
    if (assignedWindows.isEmpty()) {
        return placements;
    }

    const QList<QChar> keys = assignedWindows.keys();
    const int n = keys.size();

    auto windowsOverlap = [](const WindowTargetItem &a, const WindowTargetItem &b) -> bool {
        if (!a.isVisibleOnScreen || !b.isVisibleOnScreen) {
            return false;
        }
        if (a.geometry.intersects(b.geometry)) {
            const QRectF inter = a.geometry.intersected(b.geometry);
            if (inter.width() >= 35.0 && inter.height() >= 35.0) {
                return true;
            }
        }
        const qreal dist = std::hypot(a.geometry.center().x() - b.geometry.center().x(),
                                      a.geometry.center().y() - b.geometry.center().y());
        return (dist < 75.0);
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
            bp.radius = 18.0;
            bp.stackLevel = 0;
            bp.stackTotal = 1;
            bp.center.setX(qBound(screenRect.left() + 25.0, bp.center.x(), screenRect.right() - 25.0));
            bp.center.setY(qBound(screenRect.top() + 25.0, bp.center.y(), layout.panelRect.top() - 35.0));
            placements.insert(key, bp);
            continue;
        }

        // Multiple occluded / stacked windows in this cluster!
        // Sort cluster items by zOrder ascending:
        // Index 0 = lowest z-order (deepest underneath) -> placed on the far LEFT
        // Index clusterSize - 1 = highest z-order (topmost on surface) -> placed on the far RIGHT
        QList<int> sortedCluster = clusterIndices;
        std::sort(sortedCluster.begin(), sortedCluster.end(), [&](int a, int b) {
            const auto &itemA = assignedWindows.value(keys[a]);
            const auto &itemB = assignedWindows.value(keys[b]);
            if (itemA.zOrder != itemB.zOrder) {
                return itemA.zOrder < itemB.zOrder;
            }
            return itemA.geometry.center().x() < itemB.geometry.center().x();
        });

        // Radius scales monotonically from bottommost (smallest) to topmost (largest)
        const qreal minRadius = 12.0;
        const qreal maxRadius = 21.0;
        const qreal badgeGap = 8.0;

        QVector<qreal> radii(clusterSize);
        qreal totalWidth = 0.0;
        for (int i = 0; i < clusterSize; ++i) {
            const qreal progress = static_cast<qreal>(i) / static_cast<qreal>(clusterSize - 1);
            radii[i] = minRadius + progress * (maxRadius - minRadius);
            totalWidth += 2.0 * radii[i];
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
        const qreal clampedCenterY = qBound(screenRect.top() + maxRadius + 20.0,
                                            avgY,
                                            layout.panelRect.top() - maxRadius - 35.0);

        // Position from left to right:
        // Index 0 (bottommost) on the far left, Index clusterSize - 1 (topmost) on the far right
        qreal currLeft = clampedCenterX - totalWidth / 2.0;
        for (int i = 0; i < clusterSize; ++i) {
            const QChar key = keys[sortedCluster[i]];
            const WindowTargetItem &item = assignedWindows.value(key);
            const qreal r = radii[i];
            const qreal cx = currLeft + r;
            const qreal cy = clampedCenterY;

            TargetBadgePlacement bp;
            bp.key = key;
            bp.item = item;
            bp.center = QPointF(cx, cy);
            bp.radius = r;
            bp.stackLevel = i;
            bp.stackTotal = clusterSize;
            placements.insert(key, bp);

            currLeft += 2.0 * r + badgeGap;
        }
    }

    return placements;
}

inline QImage renderOverlayImage(const KeyboardOverlayLayout &layout,
                                 const QMap<QChar, WindowTargetItem> &assignedWindows,
                                 const QRectF &screenRect,
                                 qreal scale = 1.0)
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
    QFont headerFont(QStringLiteral("sans-serif"), 8, QFont::DemiBold);
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
    QFont keyFont(QStringLiteral("sans-serif"), 14, QFont::Bold);
    keyFont.setStyleStrategy(QFont::PreferAntialias);
    QFont subFont(QStringLiteral("sans-serif"), 7, QFont::Bold);
    subFont.setStyleStrategy(QFont::PreferAntialias);

    for (const KeycapGeometry &kg : layout.keycaps) {
        const bool isAssigned = assignedWindows.contains(kg.key);

        if (!isAssigned) {
            // UNASSIGNED KEY: Semi-transparent ghosted keycap
            p.setPen(QPen(QColor(255, 255, 255, 16), 1.0));
            p.setBrush(QColor(28, 32, 42, 55));
            p.drawRoundedRect(kg.rect, 8.0, 8.0);

            p.setFont(keyFont);
            p.setPen(QColor(255, 255, 255, 48));
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

            // Key letter (crisp bold white with slight shadow)
            p.setFont(keyFont);
            p.setPen(QColor(0, 0, 0, 180));
            const QRectF textShadowRect = kg.rect.adjusted(0, 1.0, 0, 1.0);
            p.drawText(textShadowRect, Qt::AlignHCenter | Qt::AlignVCenter, QString(kg.key));

            p.setPen(Qt::white);
            p.drawText(kg.rect, Qt::AlignHCenter | Qt::AlignVCenter, QString(kg.key));

            // Short app caption at bottom of keycap
            if (!item.appClass.isEmpty()) {
                p.setFont(subFont);
                p.setPen(col);
                const QRectF subRect(kg.rect.left(), kg.rect.bottom() - 11.0, kg.rect.width(), 10.0);
                const QString truncated = item.appClass.left(5);
                p.drawText(subRect, Qt::AlignCenter, truncated);
            }
        }
    }

    // -------------------------------------------------------------
    // 3. Draw Directional Lines & Target Badges ON TOP of the Keyboard
    // -------------------------------------------------------------
    const auto placements = computeTargetBadgePlacements(assignedWindows, screenRect, layout);

    for (auto it = placements.constBegin(); it != placements.constEnd(); ++it) {
        const QChar key = it.key();
        const TargetBadgePlacement &bp = it.value();
        if (!layout.keyMap.contains(key)) {
            continue;
        }

        const KeycapGeometry &kg = layout.keyMap.value(key);
        // Start line directly at keycap center (overlays visibly on top of keycap and keyboard dock)
        const QPointF pStart = kg.rect.center();
        const QPointF pEnd = bp.center;
        const QColor arrowCol = bp.item.customColor.isValid() ? bp.item.customColor : QColor(0, 229, 255);

        // Draw glowing launch anchor node right on top of the keycap
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), 150));
        p.drawEllipse(pStart, 6.0, 6.0);
        p.setBrush(arrowCol);
        p.drawEllipse(pStart, 3.5, 3.5);
        p.setBrush(Qt::white);
        p.drawEllipse(pStart, 1.8, 1.8);

        const qreal dx = pEnd.x() - pStart.x();
        const qreal dy = pEnd.y() - pStart.y();
        const qreal dist = std::hypot(dx, dy);

        if (dist > 20.0) {
            const qreal ux = dx / dist;
            const qreal uy = dy / dist;
            const qreal theta = std::atan2(dy, dx);

            // Arrow line stops at the perimeter of the target badge
            const QPointF pTargetPerimeter(pEnd.x() - ux * bp.radius, pEnd.y() - uy * bp.radius);

            // Pass A: Contrast drop-shadow for clear visibility across wallpaper and windows
            QPen shadowPen(QColor(0, 0, 0, 200), 6.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(shadowPen);
            p.drawLine(pStart, pTargetPerimeter);

            // Pass B: Vibrant neon laser arrow line
            QPen colorPen(arrowCol, 3.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(colorPen);
            p.drawLine(pStart, pTargetPerimeter);

            // Arrowhead at pTargetPerimeter pointing directly at target badge center
            const qreal arrowLen = qBound(12.0, bp.radius * 0.85, 17.0);
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

            p.setPen(QPen(QColor(0, 0, 0, 210), 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(arrowCol);
            p.drawPolygon(arrowHead);
        }

        // Target badge at window target position (scaled dynamically for stacking depth)
        const qreal ringRadius = bp.radius;

        // Outer ambient glow
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(arrowCol.red(), arrowCol.green(), arrowCol.blue(), 60));
        p.drawEllipse(pEnd, ringRadius + 5.0, ringRadius + 5.0);

        // Dark circular glass backing
        p.setPen(QPen(arrowCol, 2.6));
        p.setBrush(QColor(14, 18, 26, 245));
        p.drawEllipse(pEnd, ringRadius, ringRadius);

        // Key letter inside target badge (font scaled proportionally to badge radius)
        p.setPen(Qt::white);
        const int fontSize = qBound(9, qRound(ringRadius * 0.72), 15);
        QFont letterFont(QStringLiteral("sans-serif"), fontSize, QFont::Bold);
        letterFont.setStyleStrategy(QFont::PreferAntialias);
        p.setFont(letterFont);
        const QRectF badgeTextRect(pEnd.x() - ringRadius, pEnd.y() - ringRadius, ringRadius * 2.0, ringRadius * 2.0);
        p.drawText(badgeTextRect, Qt::AlignCenter, QString(key));

        // App class caption pill below target badge
        if (!bp.item.appClass.isEmpty()) {
            QString appTag = bp.item.appClass.toUpper();
            if (bp.stackTotal > 1 && appTag.length() > 4) {
                appTag = appTag.left(4);
            }
            QFont tagFont(QStringLiteral("sans-serif"), 7, QFont::Bold);
            p.setFont(tagFont);
            QFontMetrics fm(tagFont);
            const int tagW = fm.horizontalAdvance(appTag) + 10;
            const int tagH = 16;
            const QRectF tagRect(pEnd.x() - tagW / 2.0, pEnd.y() + ringRadius + 3.0, tagW, tagH);

            p.setPen(QPen(arrowCol, 1.2));
            p.setBrush(QColor(12, 15, 22, 235));
            p.drawRoundedRect(tagRect, 4.0, 4.0);

            p.setPen(arrowCol);
            p.drawText(tagRect, Qt::AlignCenter, appTag);
        }
    }

    p.end();
    return img;
}

} // namespace LeapKKeyboardOverlay
