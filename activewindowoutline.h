/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <effect/effect.h>
#include <effect/globals.h>
#include <opengl/glshader.h>
#include <opengl/glvertexbuffer.h>

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVector2D>
#include <memory>

#include "ambilightalgorithm.h"
#include "colortransition.h"
#include "geometryhelper.h"
#include "shadereffects.h"
#include "visibilitystate.h"

class QFileSystemWatcher;

namespace KWin
{

using OutlineColor::ColorStop;

class SceneView;
class GLFramebuffer;
class GLTexture;

#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
using OutlineRegion = QRegion;
using OutlineOutput = Output;
#else
using OutlineRegion = Region;
using OutlineOutput = LogicalOutput;
#endif

struct OutlineLayer {
    QList<QVector2D> vertices;
    qreal opacity = 1.0;
};

struct OutlineCache {
    QList<OutlineLayer> layers;
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
    ~ActiveWindowOutlineEffect() override;

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

#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
    bool paintScreen(const RenderTarget &renderTarget,
#else
    void paintScreen(const RenderTarget &renderTarget,
#endif
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
    void slotWindowFullScreenChanged(KWin::EffectWindow *w);
    void slotShowingDesktopChanged();
    void slotToggleAction();
    void slotAnimationTick();

private:
    void prepareScreenPaint(ScreenPrePaintData &data);
    void initShortcuts();
    bool shouldOutlineWindow(EffectWindow *w) const;
    bool isBlockingPopup(EffectWindow *w) const;
    bool popupAcceptsInput(EffectWindow *w) const;
    bool hasVisibleBlockingPopup() const;
    void updatePopupBlockingState();
    OutlineRegion panelOcclusionRegion(EffectWindow *w, QString *source = nullptr) const;
    void repaintAllOutlines();

    QRectF expandedGeometryFor(EffectWindow *w) const;

    void generateRoundedRectStrip(const QRectF &outer, qreal outerRadius,
                                  const QRectF &inner, qreal innerRadius,
                                  qreal scale,
                                  QList<QVector2D> &verts);

    void renderOutline(const RenderTarget &renderTarget,
                       const RenderViewport &viewport,
                       const OutlineCache &cache,
                       const OutlineRegion &clipRegion,
                       EffectWindow *outlinedWindow,
                       qreal windowOpacity = 1.0);

    void cacheWindowOutline(EffectWindow *w, const QRectF &geo);

    QColor resolveColor(qreal windowOpacity = 1.0, EffectWindow *w = nullptr) const;
    QColor currentCycleColor(qreal elapsedSec);
    QColor currentAmbilightColor(qreal elapsedSec);
    bool sampleAmbilightColor(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w);
    void applyNewAmbilightColor(const QColor &newColor);
    bool isAnimationActive() const;
    void updateAnimationTimer();
    void parseColorList(const QString &str);
    qreal computeBreatheFactor(qreal elapsedSec) const;
    qreal computeNeonFactor(qreal elapsedSec) const;
    qreal computeFlashFactor(qreal elapsedSec) const;
    qreal computeGlitchFactor(qreal elapsedSec) const;

    EffectWindow *m_activeWindow = nullptr;
    QList<EffectWindow *> m_recentWindows;
    mutable QMap<EffectWindow *, QColor> m_recentAmbilightColors;
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    QPointer<SceneView> m_sceneView;
#endif
    // Active windows whose outline is currently suppressed. Used to detect
    // when minimizing or showing the desktop first hides a window so the
    // now-stale outline band can be repainted away exactly once.
    QSet<EffectWindow *> m_suppressedWindows;
    QMap<EffectWindow *, QRectF> m_lastGeometry;
    QMap<EffectWindow *, OutlineCache> m_outlineCache;
    std::unique_ptr<GLShader> m_glowShader;
    std::unique_ptr<GLShader> m_fadeCornersShader;
    QFileSystemWatcher *m_configWatcher = nullptr;
    QTimer *m_configReloadTimer = nullptr;
    QString m_configPath;

    // Config - Geometry & Base
    bool m_useAccentColor = true;
    int m_accentOpacity = 66;
    QColor m_customColor = QColor(61, 174, 233);
    bool m_ensurePerimeterContrast = true;
    int m_width = 2;
    int m_gap = 0;
    int m_radius = 6;
    bool m_fadeCorners = true;
    bool m_drawInside = true;
    bool m_smoothEdges = true;
    bool m_glowEnabled = true;
    int m_glowRadius = 6;
    bool m_outlineEnabled = true;
    bool m_hideOnFullScreen = true;

    // Config - Ambilight
    bool m_ambilightEnabled = false;
    int m_ambilightMode = 0; // 0: Smart Adaptive, 1: Dominant, 2: Balanced
    int m_ambilightOpacity = 100;
    qreal m_ambilightFrequency = 1.0;
    qreal m_ambilightDuration = 0.6;
    int m_ambilightSaturationLeveling = 65;
    int m_ambilightFallbackStrength = 50;
    int m_ambilightMinBrightness = 25;
    int m_ambilightMaxBrightness = 85;
    bool m_ambilightEnsureContrast = true;

    // Config - Color Cycling
    bool m_colorCycleEnabled = false;
    QList<ColorStop> m_colorStops;
    qreal m_colorTransitionDuration = 2.0;
    int m_colorCycleOrder = 0; // 0: Sequential, 1: Random
    int m_colorCycleEasing = 0; // 0: EaseInOut, 1: Linear, 2: EaseIn, 3: EaseOut

    // Config - Shader & Visual Effects
    bool m_breatheEnabled = false;
    qreal m_breatheSpeed = 0.5;
    int m_breatheIntensity = 40;

    bool m_glitchEnabled = false;
    qreal m_glitchSpeed = 1.0;
    int m_glitchIntensity = 50;
    bool m_glitchAnomalyEnabled = false;
    int m_glitchFrequency = 3;

    bool m_neonFlickerEnabled = false;
    qreal m_neonFlickerInterval = 3.0;
    qreal m_neonFlickerDuration = 0.25;
    int m_neonFlickerIntensity = 30;

    bool m_flashEnabled = false;
    qreal m_flashInterval = 4.0;
    qreal m_flashDuration = 0.15;

    // Config - Behavior & Delays
    int m_fadeDuration = 200;
    int m_menuHideDelay = 300;
    int m_menuShowDelay = 300;
    int m_windowHideDelay = 0;
    int m_windowShowDelay = 0;
    bool m_highlightRecentWindow = false;
    int m_recentWindowsCount = 1;
    int m_recentWindowColorMode = 0;
    QColor m_recentWindowCustomColor = QColor(127, 140, 141);
    int m_recentWindowOpacity = 20;
    int m_recentWindowHueShift = 0;
    int m_recentWindowSaturation = 0;

    // Runtime delay, buffer & visibility transition state
    OutlineVisibility::StateMachine m_visibilityState;
    OutlineVisibility::Config m_visibilityConfig;
    QTimer *m_menuHideTimer = nullptr;
    QTimer *m_menuShowTimer = nullptr;
    QTimer *m_windowHideTimer = nullptr;
    QTimer *m_windowShowTimer = nullptr;
    EffectWindow *m_delayedHideWindow = nullptr;
    OutlineVisibility::DelayedHideState m_delayedHideState;

    // Runtime animation state
    QTimer *m_animationTimer = nullptr;
    mutable QElapsedTimer m_animationClock;
    mutable int m_currentColorIndex = 0;
    mutable int m_nextColorIndex = 0;
    mutable bool m_isHoldingColor = true;
    mutable qreal m_stateStartTime = 0.0;

    // Runtime ambilight state
    QTimer *m_ambilightTimer = nullptr;
    mutable bool m_ambilightSampleNeeded = true;
    mutable qreal m_lastAmbilightSampleTime = 0.0;
    mutable QColor m_ambilightCurrentColor;
    mutable QColor m_ambilightTargetColor;
    mutable QColor m_ambilightTransitionStartColor;
    mutable qreal m_ambilightTransitionStartTime = 0.0;
    mutable bool m_ambilightInTransition = false;
    std::unique_ptr<GLTexture> m_ambilightTexture;
    std::unique_ptr<GLFramebuffer> m_ambilightFbo;
    std::unique_ptr<GLTexture> m_outsideTexture;
    std::unique_ptr<GLFramebuffer> m_outsideFbo;
};

} // namespace KWin
