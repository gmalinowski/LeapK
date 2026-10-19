/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <effect/effect.h>
#include <opengl/glshader.h>
#include <QVector2D>
#include <QMap>
#include <chrono>
#include <memory>

class QFileSystemWatcher;
class QTimer;

namespace KWin
{

#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
using OutlineRegion = QRegion;
using OutlineOutput = Output;
#else
using OutlineRegion = Region;
using OutlineOutput = LogicalOutput;
#endif

struct OutlineLayer {
    QList<QVector2D> vertices;
    QColor color;
};

struct OutlineCache {
    QList<OutlineLayer> layers;
    QColor sourceColor;
    QRectF outerRect;
    QRectF innerRect;
    qreal outerRadius = 0;
    qreal innerRadius = 0;
};

class ActiveWindowOutlineEffect : public Effect
{
    Q_OBJECT

public:
    ActiveWindowOutlineEffect();

    void reconfigure(ReconfigureFlags flags) override;

    // The outline is drawn in a screen-level pass (after all windows) rather
    // than per-window. Drawing per window required marking the window
    // transformed so the border could spill outside its bounds, but the
    // PAINT_WINDOW_TRANSFORMED flag makes KWin's blur/background-contrast
    // effects skip the window — which cleared the blurred background behind
    // active windows. Painting at screen level avoids the flag entirely.
#ifdef ACTIVE_WINDOW_OUTLINE_PREPAINT_PRESENT_TIME
    void prePaintScreen(ScreenPrePaintData &data,
                        std::chrono::milliseconds presentTime) override;
#else
    void prePaintScreen(ScreenPrePaintData &data) override;
#endif

    void paintScreen(const RenderTarget &renderTarget,
                     const RenderViewport &viewport,
                     int mask,
                     const OutlineRegion &deviceRegion,
                     OutlineOutput *screen) override;

    bool isActive() const override;
    QString debug(const QString &parameter) const override;

    static bool supported();

private Q_SLOTS:
    void slotWindowActivated(KWin::EffectWindow *w);
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w,
                                        const QRectF &oldGeometry);
    void slotWindowMinimizedChanged(KWin::EffectWindow *w);
    void slotWindowHiddenChanged(KWin::EffectWindow *w);
    void slotShowingDesktopChanged();

private:
    void prepareScreenPaint(ScreenPrePaintData &data);
    bool shouldOutlineWindow(EffectWindow *w) const;
    bool isBlockingPopup(EffectWindow *w) const;
    bool hasVisibleBlockingPopup() const;
    void repaintAllOutlines();

    QRectF expandedGeometryFor(EffectWindow *w) const;

    void generateRoundedRectStrip(const QRectF &outer, qreal outerRadius,
                                  const QRectF &inner, qreal innerRadius,
                                  qreal scale,
                                  QList<QVector2D> &verts);

    void renderOutline(const RenderTarget &renderTarget,
                       const RenderViewport &viewport,
                       const OutlineCache &cache,
                       const OutlineRegion &clipRegion);

    void cacheWindowOutline(EffectWindow *w, const QRectF &geo);

    QColor resolveColor() const;

    EffectWindow *m_activeWindow = nullptr;
    // Active windows whose outline is currently suppressed. Used to detect
    // when minimizing or showing the desktop first hides a window so the
    // now-stale outline band can be repainted away exactly once.
    QSet<EffectWindow *> m_suppressedWindows;
    QMap<EffectWindow *, QRectF> m_lastGeometry;
    QMap<EffectWindow *, OutlineCache> m_outlineCache;
    std::unique_ptr<GLShader> m_glowShader;
    QFileSystemWatcher *m_configWatcher = nullptr;
    QTimer *m_configReloadTimer = nullptr;
    QString m_configPath;

    // Config
    bool m_useAccentColor = true;
    int m_accentOpacity = 66;
    QColor m_customColor = QColor(61, 174, 233);
    int m_width = 2;
    int m_gap = 0;
    int m_radius = 6;
    bool m_drawInside = true;
    bool m_smoothEdges = true;
    bool m_glowEnabled = true;
    int m_glowRadius = 6;
};

} // namespace KWin
