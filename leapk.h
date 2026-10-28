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
#include "badgehelper.h"
#include "historyhelper.h"
#include "audiotracker.h"

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

class QFileSystemWatcher;

namespace KWin
{

using OutlineColor::ColorStop;

class SceneView;
class GLFramebuffer;
class GLTexture;

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
    void slotWindowDamaged(KWin::EffectWindow *w);
    void slotShowingDesktopChanged();
    void slotActiveFullScreenEffectChanged();
    void slotToggleAction();
    void slotMarkWindowToggle();
    void slotMarkWindowWithColor();
    void slotOpenWorkspaceManager();
    void slotOpenMarkerManager();
    void slotToggleHudGrouping();
    void slotTogglePinActiveWindow();
    void slotCycleHudView(int delta = 1);
    void slotTriggerJumpMode();
    void slotTriggerAudioFilterMode();
    void slotJumpToRecentWindow();
    void slotPrevRecentWindow();
    void slotNextRecentWindow();
    void slotCommitHistoryNavigation();
    void slotAnimationTick();

private:
    void prepareScreenPaint(ScreenPrePaintData &data);
    void initShortcuts();
    bool shouldOutlineWindow(EffectWindow *w) const;
    bool isEligibleOutlineWindow(EffectWindow *w) const;
    bool isBlockingPopup(EffectWindow *w) const;
    bool popupAcceptsInput(EffectWindow *w) const;
    bool hasVisibleBlockingPopup() const;
    bool isSystemOverlay(EffectWindow *w) const;
    bool isSessionLockOrLogoutActive() const;
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
    void enterSubLeapForGroup(QChar keyChar, const QList<EffectWindow *> &group);
    void exitSubLeapMode();
    bool isWindowObscured(EffectWindow *target) const;
    bool isBadgeOccluded(EffectWindow *target, const QRectF &badgeRect, const QList<QRectF> &hudRects = {}) const;
    bool isWindowInStash(EffectWindow *w) const;
    bool isWindowPinned(EffectWindow *w) const;
    void pinWindow(EffectWindow *w);
    void unpinWindow(EffectWindow *w, int targetWorkspace = 0);
    void toggleWindowPin(EffectWindow *w);
    int findWorkspaceForWindow(EffectWindow *target) const;
    void toggleSuperJumpMode();
    void exitSuperJumpMode();
    void toggleAudioFilterMode();
    void exitAudioFilterMode();
    bool isWorkspacePlayingAudio(int wsIndex) const;
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
                     bool isRecent = false);
    void renderOffscreenJumpTargets(const RenderTarget &renderTarget,
                                   const RenderViewport &viewport,
                                   const QList<QPair<QChar, EffectWindow *>> &targets,
                                   int badgeSize,
                                   int dividerAfterIndex = -1);
    QRectF renderHudHintBar(const RenderTarget &renderTarget,
                            const RenderViewport &viewport,
                            const QRectF &pillsBounds = QRectF());
    void renderHudModeHeader(const RenderTarget &renderTarget,
                             const RenderViewport &viewport,
                             const QRectF &pillsBounds = QRectF(),
                             const QRectF &hintBarBounds = QRectF());
    void applyWorkspaceViewIsolation(int hudViewIndex);
    void enterStashMode();
    void exitStashMode();
    void addWindowToWorkspace(EffectWindow *w, int workspaceIndex);
    void createEmptyWorkspace(const QString &suggestedName = QString());
    void slotCreateNewWorkspacePrompt();
    void slotCloseActiveWorkspace();
    void applyWorkspaceDeletion(const QString &deletedWorkspaceId, const QString &windowAction);
    void renderBadgePreview(const RenderTarget &renderTarget,
                            const RenderViewport &viewport);
    QImage captureWindowThumbnail(EffectWindow *target,
                                  const RenderTarget &renderTarget,
                                  const RenderViewport &viewport,
                                  const QSize &targetSize = QSize(120, 75),
                                  bool forceBlit = false);

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
    OutlineBadge::BadgeConfig m_previewBadgeConfig;
    int m_previewBadgePosition = 0;
    int m_previewBadgeHudPosition = 0;
    std::unique_ptr<QTimer> m_previewSafetyTimer;

    // Jump to Window
    bool m_jumpMode = false;
    int m_jumpTargets = 0; // 0 = All visible windows, 1 = Marked windows only
    int m_cursorWarpMode = 0; // 0 = Disabled, 1 = Center of Window, 2 = Top-Left Titlebar
    bool m_badgeHudGroupWindows = true;
    qreal m_badgeHudGroupBorderWidth = 2.0;
    int m_badgeHudGroupFontSize = 0;
    int m_badgeHudGroupCornerRadius = 8;
    bool m_badgeHudGroupShaderEnabled = true;
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
    std::shared_ptr<GLTexture> m_hudHintBarTexture;
    int m_hudHintBarCacheKey = -1;
    std::shared_ptr<GLTexture> m_hudHeaderTexture;
    int m_hudHeaderCacheKey = -1;
    QSet<EffectWindow *> m_workspaceMinimizedWindows;
    bool m_stashModeActive = false;
    QSet<EffectWindow *> m_stashedWindows;
    QSet<EffectWindow *> m_pinnedWindows;
    bool m_superJumpActive = false;
    int m_superJumpTargetWorkspace = 0;
    bool m_audioFilterModeActive = false;
    OutlineAudio::AudioTracker m_audioTracker;
    QTimer *m_audioRefreshTimer = nullptr;
    bool m_globalShortcutsBlocked = false;

    // Sub-Leap Disambiguation (multi-instance window disambiguation)
    bool m_subLeapActive = false;
    QChar m_subLeapBaseKey;
    QList<EffectWindow *> m_subLeapWindows;
    QMap<QChar, EffectWindow *> m_subLeapLetterToWindow;
    QList<QPair<QChar, EffectWindow *>> m_subLeapTargets;

    // Persistent Marks (saved across desktop sessions)
    QList<LeapKMarks::PersistentMarkRule> m_persistentMarks;

    // Workspaces (Smart Workspaces / project spaces)
    QList<LeapKWorkspace::WorkspaceInfo> m_workspaces;

    // History Navigation
    bool m_isHistoryNavigating = false;
    int m_historyNavIndex = 0;
    QList<EffectWindow *> m_windowHistory;
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
