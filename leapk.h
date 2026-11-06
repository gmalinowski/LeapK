/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <effect/effect.h>
#include <effect/globals.h>
#include <opengl/glshader.h>
#include <opengl/glvertexbuffer.h>
#include "badgehelper.h"
#include "historyhelper.h"
#include "audiotracker.h"
#include "fuzzymatcher.h"
#include "soundhelper.h"
#include "nerdstatshelper.h"

#include <QColor>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QRect>
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
#include "customshaderhelper.h"
#include "visibilitystate.h"
#include "keyboardlayouthelper.h"
#include "persistentmarkhelper.h"
#include "workspacehelper.h"
#include "tilinghelper.h"
#include "keyboardoverlayhelper.h"

class QFileSystemWatcher;

namespace KWin
{

using OutlineColor::ColorStop;

class SceneView;
class GLFramebuffer;
class GLTexture;
class VirtualDesktop;
class Tile;
class TileManager;
class RootTile;

#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
using OutlineRegion = QRegion;
using OutlineRect = QRect;
using OutlineOutput = Output;
#else
using OutlineRegion = Region;
using OutlineRect = Rect;
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
    qreal borderWidth = 0;
};


class ActiveWindowOutlineEffect : public Effect
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.ActiveWindowOutline")

public:
    ActiveWindowOutlineEffect();
    ~ActiveWindowOutlineEffect() override;

    void reconfigure(ReconfigureFlags flags) override;

public Q_SLOTS:
    void toggleOutline();
    void setOutlineEnabled(bool enabled);
    bool isOutlineEnabled() const;
    void toggleAmbilight();
    bool isAmbilightEnabled() const;
    void toggleGlow();
    bool isGlowEnabled() const;
    void triggerJumpMode();
    void toggleWindowMark();
    void markWindowWithColor();
    void openWorkspaceManager();
    void openMarkerManager();
    void clearAllMarks();
    void prevRecentWindow();
    void nextRecentWindow();
    void applyPreset(const QString &presetName);
    void setBadgePreview(bool active, bool showWindowBadges, bool showHud, const QString &jsonConfig = QString());
    void clearBadgePreview();

Q_SIGNALS:
    void stateChanged();
    void badgePreviewChanged(bool active);

public:
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

#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
    bool paintWindow(const RenderTarget &renderTarget,
#else
    void paintWindow(const RenderTarget &renderTarget,
#endif
                     const RenderViewport &viewport,
                     EffectWindow *w,
                     int mask,
                     const OutlineRegion &deviceRegion,
                     WindowPaintData &data) override;

#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
    bool drawWindow(const RenderTarget &renderTarget,
#else
    void drawWindow(const RenderTarget &renderTarget,
#endif
                    const RenderViewport &viewport,
                    EffectWindow *w,
                    int mask,
                    const OutlineRegion &deviceRegion,
                    WindowPaintData &data) override;

    bool isActive() const override;
    QString debug(const QString &parameter) const override;
    void grabbedKeyboardEvent(QKeyEvent *e) override;
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_WINDOW_INPUT_MOUSE_EVENT
    void windowInputMouseEvent(QEvent *e) override;
#endif

    static bool supported();

private Q_SLOTS:
    void slotMouseChanged(const QPointF &pos, const QPointF &oldpos,
                          Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                          Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers);
    void slotWindowActivated(KWin::EffectWindow *w);
    void slotWindowAdded(KWin::EffectWindow *w);
    void slotWindowDeleted(KWin::EffectWindow *w);
    void slotWindowFrameGeometryChanged(KWin::EffectWindow *w,
                                        const QRectF &oldGeometry);
    void slotWindowMinimizedChanged(KWin::EffectWindow *w);
    void slotWindowHiddenChanged(KWin::EffectWindow *w);
    void slotWindowFullScreenChanged(KWin::EffectWindow *w);
    void slotWindowDamaged(KWin::EffectWindow *w);
    void slotWindowStartUserMovedResized(KWin::EffectWindow *w);
    void slotWindowStepUserMovedResized(KWin::EffectWindow *w, const QRectF &geometry);
    void slotWindowFinishUserMovedResized(KWin::EffectWindow *w);
    void slotShowingDesktopChanged();
    void slotActiveFullScreenEffectChanged();
    void slotToggleAction();
    void slotMarkWindowToggle();
    void slotMarkWindowWithColor();
    void slotOpenWorkspaceManager();
    void slotOpenMarkerManager();
    void slotToggleHudGrouping();
    void slotToggleHudHints();
    void slotTogglePinActiveWindow();
    void slotCycleHudView(int delta = 1);
    void slotCycleBadge(int delta = 1);
    void slotTriggerJumpMode();
    void slotTriggerCloseWindowMode();
    void slotTriggerPinMode();
    void slotToggleThumbnailZoom();
    void slotTriggerAudioFilterMode();
    void slotTriggerSearchMode();
    void slotTriggerTilingPresetsMode();
    void slotJumpToRecentWindow();
    void slotJumpToRecentWindow(int keyToRelease);
    void slotPrevRecentWindow();
    void slotNextRecentWindow();
    void slotCommitHistoryNavigation();
    void traverseRecentWindows(int direction);
    void slotAnimationTick();
    void slotMprisPropertiesChanged(const QString &interfaceName,
                                   const QVariantMap &changedProperties,
                                   const QStringList &invalidatedProperties);
    void slotMprisNameOwnerChanged(const QString &name,
                                  const QString &oldOwner,
                                  const QString &newOwner);

private:
    // --- Subsystem: Paint & Screen Pipeline (leapk_paint.cpp) ---
    void prepareScreenPaint(ScreenPrePaintData &data);
    QRectF expandedGeometryFor(EffectWindow *w) const;

    // --- Subsystem: Core Lifecycle & Popups (leapk.cpp) ---
    void initShortcuts();
    bool shouldOutlineWindow(EffectWindow *w) const;
    bool isEligibleOutlineWindow(EffectWindow *w) const;
    QList<QPair<pid_t, QString>> getKnownWindowsInfo() const;
    bool isBlockingPopup(EffectWindow *w) const;
    bool popupAcceptsInput(EffectWindow *w) const;
    bool hasVisibleBlockingPopup() const;
    bool isSystemOverlay(EffectWindow *w) const;
    bool isSessionLockOrLogoutActive() const;
    void updatePopupBlockingState();
    OutlineRegion panelOcclusionRegion(EffectWindow *w, QString *source = nullptr) const;
    void repaintAllOutlines();

    // --- Subsystem: Shaders & OpenGL Geometry (leapk_shaders.cpp) ---
    void generateRoundedRectStrip(const QRectF &outer, qreal outerRadius,
                                  const QRectF &inner, qreal innerRadius,
                                  qreal scale,
                                  QList<QVector2D> &verts);

    void ensureAudioShadersCompiled();
    void renderHudPillAudioShader(const RenderTarget &renderTarget,
                                  const RenderViewport &viewport,
                                  const QRectF &rect,
                                  qreal scale,
                                  const OutlineBadge::BadgeConfig &hudCfg,
                                  bool isOutlinePass);

    void renderOutline(const RenderTarget &renderTarget,
                       const RenderViewport &viewport,
                       const OutlineCache &cache,
                       const OutlineRegion &clipRegion,
                       EffectWindow *outlinedWindow,
                       qreal windowOpacity = 1.0);

    void cacheWindowOutline(EffectWindow *w, const QRectF &geo, qreal customWidth = -1);
    void toggleWindowMark(EffectWindow *w);
    void setWindowMarkColor(EffectWindow *w, const QColor &color);
    void setWindowMark(EffectWindow *w, const QColor &color, const QChar &letter = QChar(), const QString &label = QString());
    void applyPersistentMarksToWindow(EffectWindow *w);
    void refreshPersistentMarksForWindows();
    void refreshWorkspaces();
    QColor generateRandomMarkColor() const;
    QColor detectSmartAccentForWindow(EffectWindow *w) const;
    QChar nextAvailableLetter() const;
    bool isEligibleTargetWindow(EffectWindow *w) const;
    QMap<QChar, EffectWindow *> assignJumpLetters() const;
    void cancelJumpMode();
    void cancelJumpMode(int keyToRelease);
    void activateJumpTargetWindow(EffectWindow *target, int keyToPass = 0);
    void finishKeyboardGrab();
    void enterSubLeapForGroup(QChar keyChar, const QList<EffectWindow *> &group);
    void exitSubLeapMode();
    bool isWindowObscured(EffectWindow *target) const;
    bool isBadgeOccluded(EffectWindow *target, const QRectF &badgeRect, const QList<QRectF> &hudRects = {}) const;
    QRectF findBestBadgePositionForWindow(EffectWindow *target,
                                          const QRectF &visibleWindowGeo,
                                          int defaultPosition,
                                          qreal badgeW,
                                          qreal badgeH,
                                          qreal margin,
                                          const QRectF &screenRect,
                                          const QList<QRectF> &hudRects,
                                          bool *outFoundVisible = nullptr) const;
    bool isWindowInStash(EffectWindow *w) const;
    bool isWindowPinned(EffectWindow *w, int wsIndex = 0) const;
    QSet<int> getWindowPinnedWorkspaces(EffectWindow *w) const;
    void playThemedSound(OutlineSound::SoundEffect effect);
    void pinWindow(EffectWindow *w);
    void pinWindowToWorkspace(EffectWindow *w, int wsIndex);
    void pinWindowToWorkspaces(EffectWindow *w, const QSet<int> &wsIndices);
    void toggleWindowPinWorkspace(EffectWindow *w, int wsIndex);
    void unpinWindow(EffectWindow *w, int targetWorkspace = 0);
    void toggleWindowPin(EffectWindow *w);
    int findWorkspaceForWindow(EffectWindow *target) const;
    void toggleSuperJumpMode();
    void exitSuperJumpMode();
    void enterCrossWsActionMode(EffectWindow *target);
    void exitCrossWsActionMode();
    void executeCrossWsAction(int actionType);
    void triggerExternalCrossWorkspacePrompt(EffectWindow *target, EffectWindow *previous);
    void cancelCrossWsActionMode();
    bool isCrossWorkspaceTarget(EffectWindow *target) const;
    bool handleSuperJumpCrossWorkspaceSelection(EffectWindow *target, int keyToPass = 0);
    void renderHudCrossWsActionOverlay(const RenderTarget &renderTarget, const RenderViewport &viewport);
    void enterPinWsSelectionMode(EffectWindow *target);
    void exitPinWsSelectionMode();
    void renderHudPinWsSelectionOverlay(const RenderTarget &renderTarget, const RenderViewport &viewport);
    void enterCloseWindowMode();
    void exitCloseWindowMode();
    void enterPinMode();
    void exitPinMode();
    void toggleHudThumbnailZoom();
    void toggleSpatialKeyboard();
    void toggleNerdStats();
    void slotTriggerNerdStats();
    void updateNerdStatsMetrics(qint64 elapsedNs);
    void renderHudNerdStatsOverlay(const RenderTarget &renderTarget, const RenderViewport &viewport);
    void renderHudMetaOverlay(const RenderTarget &renderTarget, const RenderViewport &viewport);
    void toggleAudioFilterMode();
    void exitAudioFilterMode();
    bool isWorkspacePlayingAudio(int wsIndex) const;
    void updateAudioTrackingState(bool forceImmediateRefresh = false);
    void enterSearchMode();
    void exitSearchMode();
    void toggleSearchMode();
    void updateSearchMatches();
    void setKdeGlobalShortcutsBlocked(bool blocked);
    QString getWindowLocationString(EffectWindow *target) const;
    QList<QRectF> computeHudPillRects(const QList<QPair<QChar, EffectWindow *>> &targets,
                                      const RenderViewport &viewport,
                                      int badgeSize,
                                      const OutlineBadge::BadgeConfig &cfg,
                                      int dividerAfterIndex = -1,
                                      QList<QRectF> *outDividerRects = nullptr) const;
    EffectWindow *findRecentWindow() const;
    void warpCursorToWindow(EffectWindow *target);
    void updateRecentWindowsFromHistory();
    void renderBadge(const RenderTarget &renderTarget,
                     const RenderViewport &viewport,
                     const QRectF &badgeRect,
                     const QChar &letter,
                     const QColor &bgColor,
                     const QColor &textColor,
                     bool isJumpMode,
                     int badgeSize = 28,
                     const QString &customLabel = QString(),
                     EffectWindow *target = nullptr,
                     const QString &recentKey = QString(),
                     bool isRecent = false,
                     qreal opacity = 1.0);
    void renderFloatingPill(const RenderTarget &renderTarget,
                            const RenderViewport &viewport,
                            const QString &text,
                            const QPointF &centerPos,
                            const QColor &bgColor = QColor(),
                            const QColor &textColor = QColor(),
                            int fontSizePt = 12,
                            qreal maxAllowedWidth = 0.0,
                            const QString &shapeOverride = QString());
    void renderKeyboardArrowsOverlay(const RenderTarget &renderTarget,
                                    const RenderViewport &viewport);
    void renderOffscreenJumpTargets(const RenderTarget &renderTarget,
                                   const RenderViewport &viewport,
                                   const QList<QPair<QChar, EffectWindow *>> &targets,
                                   int badgeSize,
                                   int dividerAfterIndex = -1);
    QList<QRectF> computeAllHudCollisionRects(const QList<QPair<QChar, EffectWindow *>> &targets,
                                              const RenderViewport &viewport,
                                              int badgeSize,
                                              const OutlineBadge::BadgeConfig &cfg,
                                              int dividerAfterIndex = -1) const;
    QRectF computeActiveMarkerRectForTargets(const QList<QRectF> &pillRects,
                                            const QList<QPair<QChar, EffectWindow *>> &targets,
                                            int dividerAfterIndex,
                                            int hudPos) const;
    QSizeF ensureHudHintBarTexture(qreal scale, const QRectF &screenRect) const;
    qreal computeHudEffectiveMargin(int hudPos, qreal scale, const QRectF &screenRect, qreal headerH) const;
    QRectF computeHudHintBarRect(const RenderViewport &viewport,
                                 const QRectF &pillsBounds = QRectF()) const;
    QRectF renderHudHintBar(const RenderTarget &renderTarget,
                            const RenderViewport &viewport,
                            const QRectF &pillsBounds = QRectF());
    QSizeF ensureHudHeaderTexture(qreal scale) const;
    QRectF computeHudHeaderRect(const RenderViewport &viewport,
                                const QRectF &pillsBounds = QRectF(),
                                const QRectF &hintBarBounds = QRectF()) const;
    QRectF renderHudModeHeader(const RenderTarget &renderTarget,
                              const RenderViewport &viewport,
                              const QRectF &pillsBounds = QRectF(),
                              const QRectF &hintBarBounds = QRectF());
    void applyWorkspaceViewIsolation(int hudViewIndex, VirtualDesktop *targetDesktop = nullptr);
    void switchToWorkspaceView(int newHudViewIndex);
    QString workspaceIdForView(int hudViewIndex) const;
    void enterStashMode();
    void exitStashMode();
    void addWindowToWorkspace(EffectWindow *w, int workspaceIndex);
    void createEmptyWorkspace(const QString &suggestedName = QString());
    void slotCreateNewWorkspacePrompt();
    void slotCloseActiveWorkspace();
    void applyWorkspaceDeletion(const QString &deletedWorkspaceId, const QString &windowAction);

    // --- Subsystem: Tiling Engine (leapk_tiling.cpp) ---
    void enterTilingPresetsMode();
    void exitTilingPresetsMode();
    void refreshTilingPresetsWithKdeCustom();
    void slotSaveCurrentTilingLayout();
    void deleteTilingPreset(QChar triggerKey);
    void resetTilingPresetsToDefaults();
    QList<LeapKTiling::TileSlot> captureCurrentLayoutSlots() const;
    void upsertTilingPreset(const QString &name, QChar triggerKey, const QList<LeapKTiling::TileSlot> &tileSlots);
    void slotOpenKdeTileEditor();
    void slotToggleActiveWindowFloating();
    void activateTilingPreset(const LeapKTiling::TilingPreset &preset, bool preserveJumpMode = false);
    void deactivateTiling(EffectWindow *skipRestoreWindow = nullptr, bool preserveWorkspaceState = false);
    QList<Tile *> getLiveKdeLeafTiles() const;
    void applyWindowToTilingSlot(EffectWindow *ew, const QRectF &geo, Tile *targetTile = nullptr);
    void handleTiledWindowResized(EffectWindow *w, const QRectF &newGeo);
    void syncActiveTilingSlotsToKdeTiles();
    bool isWindowInTileSlot(EffectWindow *w) const;
    int findTileSlotIndexForWindow(EffectWindow *w) const;
    void performTilingSwap(EffectWindow *target);
    void ensureTilingNoMinSizeRule();
    void slotToggleTilingEditMode();
    void enterTilingEditMode();
    void exitTilingEditMode();
    void refreshTilingSlotWindows();
    EffectWindow *findWindowInSlot(int slotIdx) const;
    void splitActiveTileSlot(Qt::Orientation orientation);
    void mergeTileSlots(int idx1, int idx2);
    void deleteTileSlot(int slotIdx);
    void performTileSlotSwap(int idx1, int idx2);
    void performTilingResize(LeapKTiling::ResizeDirection dir, int count, LeapKTiling::TileEdge edge = LeapKTiling::TileEdge::Auto);
    void renderTilingEditOverlay(const RenderTarget &renderTarget,
                                 const RenderViewport &viewport);
    void renderTilingPresetPills(const RenderTarget &renderTarget,
                                const RenderViewport &viewport,
                                const QList<LeapKTiling::TilingPreset> &presets);
    void renderTilingToast(const RenderTarget &renderTarget,
                           const RenderViewport &viewport);
    void showTilingToast(const QString &message);
    void checkKdeTilesEditorState();
    void onKdeTilesEditorClosed();

    // --- Subsystem: HUD Badge Preview (leapk_hud.cpp) ---
    void renderBadgePreview(const RenderTarget &renderTarget,
                            const RenderViewport &viewport);

    // --- Subsystem: Thumbnail Capture (leapk_paint.cpp) ---
    QImage captureWindowThumbnail(EffectWindow *target,
                                  const RenderTarget &renderTarget,
                                  const RenderViewport &viewport,
                                  const QSize &targetSize = QSize(120, 75),
                                  bool forceBlit = false);

    // --- Subsystem: Color, Ambilight & Animations (leapk_color.cpp) ---
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
    mutable QMap<EffectWindow *, QColor> m_recentCycleColors;
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
    std::unique_ptr<GLShader> m_audioGlowShader;
    std::unique_ptr<GLShader> m_audioOutlineShader;
    QFileSystemWatcher *m_configWatcher = nullptr;
    QTimer *m_configReloadTimer = nullptr;
    QString m_configPath;
    QFileSystemWatcher *m_marksWatcher = nullptr;
    QTimer *m_marksReloadTimer = nullptr;

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
    bool m_glowEnabled = false;
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

    bool m_customShaderEnabled = false;
    QString m_activeCustomShaderId;
    LeapKShader::CustomShaderEffect m_activeCustomShader;
    bool m_audioShaderRuleEnabled = false;
    QString m_audioShaderId;
    LeapKShader::CustomShaderEffect m_audioShaderEffect;
    QString m_audioWindowFrameShaderMode = QStringLiteral("always");
    bool m_audioHudShaderEnabled = true;

    // Config - Behavior & Delays
    int m_fadeDuration = 200;
    int m_menuHideDelay = 500;
    int m_menuShowDelay = 500;
    int m_windowHideDelay = 0;
    int m_windowShowDelay = 0;
    bool m_highlightRecentWindow = false;
    int m_recentWindowsCount = 1;
    int m_recentWindowColorMode = 0;
    QColor m_recentWindowCustomColor = QColor(127, 140, 141);
    int m_recentWindowOpacity = 20;
    int m_recentWindowHueShift = 0;
    int m_recentWindowSaturation = 0;
    int m_recentWindowWidthReduction = 0;
    bool m_recentWindowDashed = false;
    int m_recentWindowGapGrowth = 6;
    bool m_recentWindowDashSmooth = false;

    // Config - Mark Window
    QColor m_markDefaultColor = QColor(QStringLiteral("#e74c3c"));
    bool m_markSmartAdaptive = true;
    bool m_markRandomColor = false;
    int m_markInactiveWidthReduction = 1;
    bool m_markInactiveDashed = true;
    int m_markInactiveDashGap = 6;
    bool m_markInactiveDashSmooth = true;

    // Config - Style Precedence
    QStringList m_stylePrecedence = {
        QStringLiteral("Marked"),
        QStringLiteral("Active"),
        QStringLiteral("Recent")
    };

    // Runtime mark window state
    QMap<EffectWindow *, QColor> m_markedWindows;
    QMap<EffectWindow *, QChar> m_markedWindowLetters;
    QMap<EffectWindow *, QString> m_markedWindowLabels;
    mutable QMap<EffectWindow *, QColor> m_smartAccentCache;
    QColor m_lastMarkColor = QColor(QStringLiteral("#e74c3c"));
    bool m_markShowLetterBadges = true;
    bool m_badgesAlwaysVisible = false;
    int m_badgeDisplayMode = 0; // 0 = Only during jump, 1 = Always visible, 2 = Disabled
    bool m_badgeWindowBadgesEnabled = true;
    int m_badgePosition = 1;    // 0 = TopLeft, 1 = Top, 2 = TopRight, 3 = Left, 4 = Center, 5 = Right, 6 = BottomLeft, 7 = Bottom, 8 = BottomRight
    int m_badgeHudPosition = 0; // 0 = Bottom, 1 = Top, 2 = Left, 3 = Right
    int m_badgeSize = 28;       // Height in pixels (16 to 500)
    int m_badgeMargin = 0;      // Margin from window edge in pixels (0 to 500)
    bool m_badgeShowIcon = false;
    bool m_badgeShowThumbnail = false;
    bool m_badgeHudIncludeAll = false;
    bool m_badgeLivePreviews = false;
    int m_badgeTimeout = 5;
    QString m_jumpRecentKey = QStringLiteral("Space");
    OutlineBadge::BadgeConfig m_badgeConfig;

    // Badge Live Preview
    bool m_badgePreviewActive = false;
    bool m_previewShowWindowBadges = false;
    bool m_previewShowHud = false;
    bool m_previewMouseIntercepting = false;
    QList<QRectF> m_previewWindowBadgeRects;
    QRectF m_previewHudBounds;
    OutlineBadge::BadgeConfig m_previewBadgeConfig;
    int m_previewBadgePosition = 0;
    int m_previewBadgeHudPosition = 0;
    std::unique_ptr<QTimer> m_previewSafetyTimer;
    bool isPosInsideBadgePreview(const QPointF &pos) const;

    // Jump to Window
    bool m_jumpMode = false;
    QPointer<EffectWindow> m_hudOriginWindow;
    int m_jumpTargets = 0; // 0 = All visible windows, 1 = Marked windows only
    int m_cursorWarpMode = 0; // 0 = Disabled, 1 = Center of Window, 2 = Top-Left Titlebar
    bool m_soundFeedbackEnabled = false;
    int m_soundFeedbackVolume = 30;
    VirtualDesktop *m_lastDesktop = nullptr;
    bool m_badgeHudGroupWindows = true;
    qreal m_badgeHudGroupBorderWidth = 2.0;
    int m_badgeHudGroupFontSize = 0;
    int m_badgeHudGroupCornerRadius = 8;
    bool m_badgeHudGroupShaderEnabled = true;
    bool m_badgeHudShowHints = true;
    int m_badgeHudNavFontSize = 0;
    int m_badgeHudHintsFontSize = 0;
    QMap<QChar, EffectWindow *> m_jumpLetterToWindow;
    mutable QMap<QChar, QList<EffectWindow *>> m_letterWindowGroups;
    mutable QMap<QChar, QList<EffectWindow *>> m_groupJumpLetters;
    mutable QMap<EffectWindow *, QChar> m_windowToGroupLetter;
    mutable QMap<EffectWindow *, QChar> m_windowToDirectLetter;
    QTimer *m_jumpTimeoutTimer = nullptr;
    QPointer<EffectWindow> m_expectedJumpTarget;
    QTimer *m_expectedJumpTargetTimer = nullptr;
    int m_activeHudView = 0;
    QSet<EffectWindow *> m_closingWindows;
    mutable std::shared_ptr<GLTexture> m_hudHintBarTexture;
    mutable int m_hudHintBarCacheKey = -1;
    mutable std::shared_ptr<GLTexture> m_hudHeaderTexture;
    mutable int m_hudHeaderCacheKey = -1;
    QSet<EffectWindow *> m_workspaceMinimizedWindows;
    bool m_stashModeActive = false;
    bool m_closeWindowModeActive = false;
    bool m_pinModeActive = false;
    bool m_hudMetaHeld = false;
    mutable std::shared_ptr<GLTexture> m_hudMetaOverlayTexture;
    mutable qint64 m_hudMetaOverlayCacheKey = -1;
    QSet<EffectWindow *> m_stashedWindows;
    QSet<EffectWindow *> m_pinnedWindows;
    QMap<EffectWindow *, QSet<int>> m_windowPinnedWorkspaces;
    QMap<EffectWindow *, int> m_windowWorkspace;
    bool m_pinWsSelectionActive = false;
    QPointer<EffectWindow> m_pinTargetWindow;
    QRectF m_pinWsDialogScreenRect;
    QList<QRectF> m_pinWsRowScreenRects;
    QList<QRectF> m_pinWsRelRowRects;
    QRectF m_pinWsRelCancelRect;
    QRectF m_pinWsCancelScreenRect;
    mutable std::shared_ptr<GLTexture> m_pinWsOverlayTexture;
    mutable qint64 m_pinWsOverlayCacheKey = -1;
    int m_hudThumbnailMode = 0; // 0 = Default, 1 = Zoomed, 2 = Off
    bool m_hudThumbnailZoomActive = false;
    bool m_nerdStatsEnabled = false;
    bool m_nerdStatsActive = false;
    LeapKNerdStats::RollingAverage m_avgPowerWatts{0.08};
    LeapKNerdStats::RollingAverage m_avgPackagePowerWatts{0.08};
    LeapKNerdStats::RollingAverage m_avgCpuPercent{0.08};
    LeapKNerdStats::RollingAverage m_avgFrameLatencyMs{0.08};
    qint64 m_cumulativePaintTimeNs = 0;
    QElapsedTimer m_cpuMeasurementTimer;
    QElapsedTimer m_powerQueryTimer;
    QTimer *m_nerdStatsTimer = nullptr;
    mutable std::shared_ptr<GLTexture> m_nerdStatsOverlayTexture;
    mutable qint64 m_nerdStatsCacheKey = -1;
    QRectF m_nerdStatsScreenRect;
    bool m_superJumpActive = false;
    int m_superJumpTargetWorkspace = 0;
    int m_superJumpCrossWsAction = 0;
    bool m_crossWsActionActive = false;
    QPointer<EffectWindow> m_crossWsTargetWindow;
    QPointer<EffectWindow> m_crossWsOriginWindow;
    int m_crossWsTargetWorkspace = 0;
    int m_crossWsCurrentWorkspace = 0;
    QString m_crossWsTargetWsName;
    QString m_crossWsCurrentWsName;
    QRectF m_crossWsDialogScreenRect;
    QRectF m_crossWsJumpScreenRect;
    QRectF m_crossWsMoveScreenRect;
    QRectF m_crossWsPinScreenRect;
    QRectF m_crossWsCancelScreenRect;
    QRectF m_crossWsRelJumpRect;
    QRectF m_crossWsRelMoveRect;
    QRectF m_crossWsRelPinRect;
    QRectF m_crossWsRelCancelRect;
    mutable std::shared_ptr<GLTexture> m_crossWsActionOverlayTexture;
    mutable qint64 m_crossWsActionOverlayCacheKey = -1;
    bool m_audioFilterModeActive = false;
    OutlineAudio::AudioTracker m_audioTracker;
    QTimer *m_audioRefreshTimer = nullptr;
    bool m_searchModeActive = false;
    QString m_searchQuery;
    int m_searchScope = 1;
    int m_searchSelectedIndex = 0;
    QList<OutlineFuzzy::SearchCandidate> m_searchMatches;
    bool m_keyboardOverlayEnabled = true;
    bool m_keyboardOverlayTilingOnly = false;
    bool isKeyboardOverlayActive() const {
        return LeapKKeyboardOverlay::isKeyboardOverlayActive(m_keyboardOverlayEnabled, m_keyboardOverlayTilingOnly, m_tilingActive);
    }
    bool m_keyboardOverlayExtendToHud = true;
    int m_keyboardOverlayKeySize = 54;
    int m_keyboardOverlayKeyFontSize = 14;
    qreal m_keyboardOverlayLineWidth = 3.4;
    int m_keyboardOverlayBadgeRadius = 18;
    int m_keyboardOverlayBadgeFontSize = 13;
    QString m_keyboardOverlayFontFamily;
    int m_keyboardOverlayTargetLabelFontSize = 10;
    int m_keyboardOverlayStackScaleReduction = 35;
    int m_keyboardOverlayStackOpacityReduction = 40;
    LeapKKeyboardOverlay::KeyboardOverlayLayout m_keyboardOverlayLayout;
    QMap<QChar, EffectWindow *> m_keyboardOverlayWindowMap;
    QChar m_invalidJumpKey;
    QElapsedTimer m_invalidJumpKeyTimer;
    void triggerInvalidKeyFeedback(const QChar &key);
    bool m_globalShortcutsBlocked = false;
    bool m_mouseIntercepting = false;
    bool m_keyboardGrabActive = false;
    int m_releaseKeyToConsume = 0;
    QTimer *m_keyReleaseSafetyTimer = nullptr;
    mutable QRectF m_hudCloseButtonLocalRect;
    QRectF m_hudCloseButtonScreenRect;
    mutable QRectF m_hudHeaderScreenRect;
    mutable QRectF m_hudHintBarScreenRect;
    QList<QPair<QRectF, EffectWindow *>> m_hudTargetPillRects;
    QElapsedTimer m_hudSelectAnimTimer;
    bool m_hudSelectAnimActive = false;
    QRectF m_hudCurrentMarkerRect;
    QRectF m_hudPrevMarkerRect;
    QRectF m_hudLastDrawnMarkerRect;
    int m_hudCurrentActiveDockIndex = -1;
    int m_hudPrevActiveDockIndex = -1;
    QPointer<EffectWindow> m_hudPrevActiveWindow;

    // Sub-Leap Disambiguation (multi-instance window disambiguation)
    bool m_subLeapActive = false;
    QChar m_subLeapBaseKey;
    QList<EffectWindow *> m_subLeapWindows;
    QMap<QChar, EffectWindow *> m_subLeapLetterToWindow;
    QList<QPair<QChar, EffectWindow *>> m_subLeapTargets;

    // Tiling Presets & Smart Swap
    bool m_tilingActive = false;
    LeapKTiling::TilingPreset m_activeTilingPreset;
    struct ActiveTilingSlot {
        QRectF geometry;
        QPointer<EffectWindow> window;
        QRectF originalGeometry;
    };
    QList<ActiveTilingSlot> m_activeTilingSlots;
    QMap<QPointer<EffectWindow>, QRectF> m_preTilingGeometries;
    bool m_tilingPresetsModeActive = false;
    QList<LeapKTiling::TilingPreset> m_tilingPresets;
    int m_tilingGaps = 6;
    int m_tilingMargins = 8;
    bool m_tilingRemoveMinSizeLimit = true;
    bool m_tilingEditModeActive = false;
    bool m_tilingMergeModeActive = false;
    int m_tilingEditMultiplier = 0;
    QList<QPair<QRectF, LeapKTiling::TilingPreset>> m_hudTilingPresetPillRects;
    QSet<EffectWindow *> m_floatingWindows;
    QJsonObject m_savedKdeCustomTilesJson;
    bool m_kdeTilesEditorWasActive = false;
    QTimer *m_kdeTilesEditorWatchTimer = nullptr;
    QElapsedTimer m_tilingToastTimer;
    QString m_tilingToastMessage;
    QTimer *m_tilingToastClearTimer = nullptr;

    // Persistent Marks (saved across desktop sessions)
    QList<LeapKMarks::PersistentMarkRule> m_persistentMarks;

    // Workspaces (Smart Workspaces / project spaces)
    QList<LeapKWorkspace::WorkspaceInfo> m_workspaces;
    QMap<QString, QString> m_workspaceLastVirtualDesktopId;
    QMap<QString, bool> m_workspaceTilingActive;
    QMap<QString, LeapKTiling::TilingPreset> m_workspaceTilingPreset;
    mutable QList<QRectF> m_hudTabLocalRects;
    QList<QRectF> m_hudTabScreenRects;

    // History Navigation
    bool m_isHistoryNavigating = false;
    bool m_metaSpaceTraverseActive = false;
    int m_historyNavIndex = 0;
    QList<EffectWindow *> m_windowHistory;
    QList<EffectWindow *> m_hudSnapshotHistory;
    QTimer *m_historyCommitTimer = nullptr;

    // Badge OpenGL texture cache
    mutable QMap<quint64, std::shared_ptr<GLTexture>> m_badgeTextureCache;
    mutable QMap<EffectWindow *, QImage> m_thumbnailCache;
    mutable QHash<EffectWindow *, qint64> m_lastCaptureTimestamp;
    std::unique_ptr<GLTexture> m_thumbTexture;
    std::unique_ptr<GLFramebuffer> m_thumbFbo;
    bool m_inThumbnailCapture = false;

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
