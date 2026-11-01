// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QList>
#include <QRectF>

namespace KWin
{

class Window;
class VirtualDesktop;
class LogicalOutput;
class CustomTile;
class RootTile;

class Tile : public QObject
{
public:
    enum class LayoutDirection {
        Floating = 0,
        Horizontal = 1,
        Vertical = 2
    };

    bool manage(Window *window);
    bool unmanage(Window *window);
    QList<Tile *> childTiles() const;
    QList<Tile *> descendants() const;
    bool isLayout() const;
    QRectF windowGeometry() const;
    QRectF absoluteGeometry() const;
    QRectF relativeGeometry() const;
    int childCount() const;
    Tile *childTile(int row);
    void destroyChild(Tile *tile);
    Tile *parentTile() const;
};

class CustomTile : public Tile
{
public:
    void remove();
    Tile::LayoutDirection layoutDirection() const;
};

class RootTile : public CustomTile
{
public:
    Tile *tileForWindow(Window *window);
    Tile *pick(const QPointF &point) const;
};

class TileManager : public QObject
{
public:
    RootTile *rootTile(VirtualDesktop *desktop) const;
    RootTile *rootTile() const;
    void readSettings(RootTile *rootTile);
    void saveSettings();
};

} // namespace KWin
