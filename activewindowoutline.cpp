/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline.h"
#include "popupfilter.h"
#include <QPainter>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <window.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/glvertexbuffer.h>
#include <opengl/glframebuffer.h>
#include <opengl/gltexture.h>
#include <opengl/eglcontext.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#ifndef ACTIVE_WINDOW_OUTLINE_X11
#ifdef ACTIVE_WINDOW_OUTLINE_SURFACE_BLUR_REGION
#include <core/region.h>
#else
#include <wayland/blur.h>
#include <wayland/contrast.h>
#endif
#include <wayland/surface.h>
#endif
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
#include <core/rect.h>
#include "cursoroverlay.h"
#include <scene/workspacescene.h>
#endif
#include <KConfigGroup>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QPalette>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>
#include <KGlobalAccel>
#include <QAction>
#include <QRandomGenerator>
#include <cmath>
#include <utility>

namespace KWin
{

static QByteArray glowFragmentSource()
{
    const EglContext *context = EglContext::currentContext();
    const bool isOpenGLES = !epoxy_is_desktop_gl();
    const bool modern = isOpenGLES
        ? context->glslVersion() >= Version(3, 0)
        : context->glslVersion() >= Version(1, 40);

    QByteArray source;
    if (isOpenGLES) {
        if (modern) {
            source += "#version 300 es\n";
        }
        source += "precision highp float;\n";
    } else if (modern) {
        source += "#version 140\n";
    }
    source += modern
        ? "in vec2 position0;\nout vec4 fragColor;\n"
        : "varying vec2 position0;\n#define fragColor gl_FragColor\n";
    source += R"GLSL(
uniform vec4 geometryColor;
uniform vec4 outerBox;
uniform vec4 innerBox;
uniform float outerRadius;
uniform float innerRadius;
uniform float glowRadius;
uniform float time;
uniform float glitchActive;
uniform float glitchSpeed;
uniform float glitchIntensity;
uniform float glitchAnomalyActive;
uniform float glitchFrequency;
uniform float fadeCorners;
uniform float cornerCutoff;
uniform float fadeDistance;

float roundedBoxDistance(vec2 point, vec4 box, float radius)
{
    vec2 q = abs(point - box.xy) - box.zw + vec2(radius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
    float outside = roundedBoxDistance(position0, outerBox, outerRadius);
    float inside = -roundedBoxDistance(position0, innerBox, innerRadius);
    float distanceToOutline = max(outside, inside);
    float sigma = max(glowRadius * 0.42, 0.5);

    float cornerFade = 1.0;
    if (fadeCorners > 0.5) {
        vec2 p = abs(position0 - outerBox.xy);
        vec2 d = outerBox.zw - p;
        float edgeDist = max(d.x, d.y);
        cornerFade = smoothstep(cornerCutoff, cornerCutoff + fadeDistance, edgeDist);
    }

    if (glitchActive > 0.5) {
        float intensityNorm = glitchIntensity / 100.0;

        // 1. Clear, distinct continuous holographic scanlines (multi-frequency interference)
        float scanline1 = sin(position0.y * 0.35 - time * (3.0 * glitchSpeed));
        float scanline2 = sin(position0.y * 0.9 + time * (1.8 * glitchSpeed));
        float scanlines = 0.5 + 0.5 * (scanline1 * 0.7 + scanline2 * 0.3);

        // 2. Rolling holographic sweep beam (clearly visible, smooth 60 FPS sweep)
        float beamCoord = fract(position0.y * 0.002 - time * (0.12 * glitchSpeed));
        float beam = smoothstep(0.08, 0.0, abs(beamCoord - 0.5));

        // 3. Crisp continuous holographic micro-jitter (subtle, keeps line straight without wavy noodle)
        float holoJitter = sin(position0.y * 0.5 + time * (3.5 * glitchSpeed))
                         * cos(position0.y * 1.1 - time * (2.5 * glitchSpeed));
        float totalShift = holoJitter * (intensityNorm * 0.5);

        // 4. Optional periodic glitch disturbance (active ONLY when glitchAnomalyActive > 0.5)
        float surge = 0.0;
        if (glitchAnomalyActive > 0.5) {
            float anomalyPhase = time * (0.35 + 0.2 * glitchFrequency);
            float beat = sin(anomalyPhase) * sin(anomalyPhase * 0.618 + 1.5);
            surge = smoothstep(0.5, 0.95, beat);

            if (surge > 0.0) {
                float slice = sin(position0.y * 1.2 + time * (16.0 * glitchSpeed))
                            * cos(position0.y * 2.2 - time * (11.0 * glitchSpeed));
                totalShift += slice * surge * (intensityNorm * 1.5);
            }
        }

        float dynamicDist = distanceToOutline + totalShift;
        float normDist = max(dynamicDist, 0.0) / sigma;
        float dynamicFalloff = exp(-0.5 * normDist * normDist);
        float dynamicMask = smoothstep(-1.0, 0.0, dynamicDist);

        // Holographic luminescence and radiance (rich & clearly visible!)
        float energy = 1.0 + intensityNorm * (0.45 * (scanlines - 0.5) + 0.6 * beam + 0.8 * surge);
        dynamicFalloff *= max(0.2, energy);

        // Vibrant electric cyan/blue holographic ionization
        vec3 holoEmission = vec3(0.12, 0.28, 0.42) * (intensityNorm * (0.35 + 0.65 * beam + 0.8 * surge));
        vec3 finalRgb = min(vec3(1.0), geometryColor.rgb * energy + holoEmission);

        fragColor = vec4(finalRgb,
                         geometryColor.a * 0.38 * dynamicFalloff * dynamicMask * cornerFade);
    } else {
        float normalized = max(distanceToOutline, 0.0) / sigma;
        float falloff = exp(-0.5 * normalized * normalized);
        float outsideMask = smoothstep(-1.0, 0.0, distanceToOutline);

        fragColor = vec4(geometryColor.rgb,
                         geometryColor.a * 0.38 * falloff * outsideMask * cornerFade);
    }
}
)GLSL";
    return source;
}

static QByteArray outlineFragmentSource()
{
    const EglContext *context = EglContext::currentContext();
    const bool isOpenGLES = !epoxy_is_desktop_gl();
    const bool modern = isOpenGLES
        ? context->glslVersion() >= Version(3, 0)
        : context->glslVersion() >= Version(1, 40);

    QByteArray source;
    if (isOpenGLES) {
        if (modern) {
            source += "#version 300 es\n";
        }
        source += "precision highp float;\n";
    } else if (modern) {
        source += "#version 140\n";
    }
    source += modern
        ? "in vec2 position0;\nout vec4 fragColor;\n"
        : "varying vec2 position0;\n#define fragColor gl_FragColor\n";
    source += R"GLSL(
uniform vec4 geometryColor;
uniform vec4 outerBox;
uniform float cornerCutoff;
uniform float fadeDistance;

void main()
{
    vec2 p = abs(position0 - outerBox.xy);
    vec2 d = outerBox.zw - p;
    float edgeDist = max(d.x, d.y);
    float cornerFade = smoothstep(cornerCutoff, cornerCutoff + fadeDistance, edgeDist);
    fragColor = vec4(geometryColor.rgb, geometryColor.a * cornerFade);
}
)GLSL";
    return source;
}

#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
// KWin before 6.6 has no Border shader trait. Supply the position varying
// explicitly instead of relying on that trait's generated vertex shader.
static QByteArray glowVertexSource()
{
    const EglContext *context = EglContext::currentContext();
    const bool isOpenGLES = !epoxy_is_desktop_gl();
    const bool modern = isOpenGLES
        ? context->glslVersion() >= Version(3, 0)
        : context->glslVersion() >= Version(1, 40);

    QByteArray source;
    if (isOpenGLES && modern) {
        source += "#version 300 es\n";
    } else if (!isOpenGLES && modern) {
        source += "#version 140\n";
    }
    source += modern
        ? "in vec4 position;\nout vec2 position0;\n"
        : "attribute vec4 position;\nvarying vec2 position0;\n";
    source += R"GLSL(
uniform mat4 modelViewProjectionMatrix;
void main()
{
    position0 = position.xy;
    gl_Position = modelViewProjectionMatrix * position;
}
)GLSL";
    return source;
}
#endif

KWIN_EFFECT_FACTORY_SUPPORTED(ActiveWindowOutlineEffect, "metadata.json",
                              return ActiveWindowOutlineEffect::supported();)

ActiveWindowOutlineEffect::ActiveWindowOutlineEffect()
{
    m_animationTimer = new QTimer(this);
    m_animationTimer->setInterval(16);
    connect(m_animationTimer, &QTimer::timeout,
            this, &ActiveWindowOutlineEffect::slotAnimationTick);
    m_animationClock.start();

    m_menuHideTimer = new QTimer(this);
    m_menuHideTimer->setSingleShot(true);
    connect(m_menuHideTimer, &QTimer::timeout, this, [this] {
        m_visibilityState.handleMenuHideTimerTimeout(hasVisibleBlockingPopup(), m_visibilityConfig);
        updateAnimationTimer();
        if (m_activeWindow) {
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_menuShowTimer = new QTimer(this);
    m_menuShowTimer->setSingleShot(true);
    connect(m_menuShowTimer, &QTimer::timeout, this, [this] {
        m_visibilityState.handleMenuShowTimerTimeout(hasVisibleBlockingPopup(), m_visibilityConfig);
        updateAnimationTimer();
        if (m_activeWindow && shouldOutlineWindow(m_activeWindow)) {
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_windowHideTimer = new QTimer(this);
    m_windowHideTimer->setSingleShot(true);
    connect(m_windowHideTimer, &QTimer::timeout, this, [this] {
        if (m_delayedHideWindow) {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_delayedHideState.startFade(m_visibilityConfig.fadeDuration);
                updateAnimationTimer();
                effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
            } else {
                effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
                m_suppressedWindows.remove(m_delayedHideWindow);
                m_lastGeometry.remove(m_delayedHideWindow);
                m_outlineCache.remove(m_delayedHideWindow);
                m_delayedHideWindow = nullptr;
                m_delayedHideState.reset();
            }
        }
    });

    m_windowShowTimer = new QTimer(this);
    m_windowShowTimer->setSingleShot(true);
    connect(m_windowShowTimer, &QTimer::timeout, this, [this] {
        if (m_activeWindow && shouldOutlineWindow(m_activeWindow)
            && !m_visibilityState.popupBlocked && !hasVisibleBlockingPopup()) {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_visibilityState.visibilityOpacity = 0.0;
            }
            m_visibilityState.setTargetOpacity(1.0, m_visibilityConfig.fadeDuration);
            updateAnimationTimer();
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    m_ambilightTimer = new QTimer(this);
    m_ambilightTimer->setInterval(1000);
    connect(m_ambilightTimer, &QTimer::timeout, this, [this] {
        if ((m_ambilightEnabled || m_ensurePerimeterContrast) && m_activeWindow && !m_activeWindow->isMinimized() && m_activeWindow->isOnCurrentDesktop()) {
            m_ambilightSampleNeeded = true;
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    });

    reconfigure(ReconfigureAll);

    // Some third-party effect KCMs are not reliably reconfigured by KWin's
    // D-Bus hook on every Plasma version. Watch kwinrc as a fallback so a
    // saved setting always reaches the running effect without a logout.
    m_configPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/kwinrc");
    m_configWatcher = new QFileSystemWatcher(this);
    if (QFileInfo::exists(m_configPath)) {
        m_configWatcher->addPath(m_configPath);
    }

    m_configReloadTimer = new QTimer(this);
    m_configReloadTimer->setSingleShot(true);
    m_configReloadTimer->setInterval(150);

    const auto scheduleConfigReload = [this] {
        m_configReloadTimer->start();
    };
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged,
            this, scheduleConfigReload);
    connect(m_configWatcher, &QFileSystemWatcher::directoryChanged,
            this, scheduleConfigReload);
    connect(m_configReloadTimer, &QTimer::timeout, this, [this] {
        const QString configDirectory = QFileInfo(m_configPath).absolutePath();
        if (QFileInfo::exists(m_configPath)
            && !m_configWatcher->files().contains(m_configPath)) {
            m_configWatcher->addPath(m_configPath);
        }
        if (m_configWatcher->directories().contains(configDirectory)) {
            m_configWatcher->removePath(configDirectory);
        }
        effects->config()->reparseConfiguration();
        reconfigure(ReconfigureAll);
    });

    // KConfig may replace the file atomically. When that happens the file
    // watch disappears, so temporarily watch its parent until it reappears.
    connect(m_configWatcher, &QFileSystemWatcher::fileChanged,
            this, [this] {
        if (!m_configWatcher->files().contains(m_configPath)) {
            const QString configDirectory = QFileInfo(m_configPath).absolutePath();
            if (!m_configWatcher->directories().contains(configDirectory)) {
                m_configWatcher->addPath(configDirectory);
            }
        }
    });

    m_activeWindow = effects->activeWindow();
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        connect(w, &EffectWindow::windowFrameGeometryChanged,
                this, &ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged);
        connect(w, &EffectWindow::minimizedChanged,
                this, &ActiveWindowOutlineEffect::slotWindowMinimizedChanged);
        connect(w, &EffectWindow::windowHiddenChanged,
                this, &ActiveWindowOutlineEffect::slotWindowHiddenChanged);
        connect(w, &EffectWindow::windowFullScreenChanged,
                this, &ActiveWindowOutlineEffect::slotWindowFullScreenChanged);
    }

    connect(effects, &EffectsHandler::windowActivated,
            this, &ActiveWindowOutlineEffect::slotWindowActivated);
    connect(effects, &EffectsHandler::windowAdded,
            this, &ActiveWindowOutlineEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &ActiveWindowOutlineEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::showingDesktopChanged,
            this, &ActiveWindowOutlineEffect::slotShowingDesktopChanged);
    connect(effects, &EffectsHandler::stackingOrderChanged,
            this, &ActiveWindowOutlineEffect::repaintAllOutlines);

    initShortcuts();
}

ActiveWindowOutlineEffect::~ActiveWindowOutlineEffect()
{
    if (m_animationTimer) {
        m_animationTimer->stop();
    }
    if (m_menuHideTimer) {
        m_menuHideTimer->stop();
    }
    if (m_menuShowTimer) {
        m_menuShowTimer->stop();
    }
    if (m_windowHideTimer) {
        m_windowHideTimer->stop();
    }
    if (m_windowShowTimer) {
        m_windowShowTimer->stop();
    }
    if (m_ambilightTimer) {
        m_ambilightTimer->stop();
    }
}

void ActiveWindowOutlineEffect::initShortcuts()
{
    QAction *toggleAction = new QAction(this);
    toggleAction->setObjectName(QStringLiteral("ToggleActiveWindowOutline"));
    toggleAction->setText(QStringLiteral("Toggle Active Window Outline"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_O));
    KGlobalAccel::self()->setGlobalShortcut(toggleAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_O));
    connect(toggleAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleAction);
}

bool ActiveWindowOutlineEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void ActiveWindowOutlineEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-active-window-outline"));
    m_useAccentColor = config.readEntry("UseAccentColor", true);
    m_accentOpacity = qBound(0, config.readEntry("AccentOpacity", 66), 100);
    m_customColor = config.readEntry("CustomColor", QColor(61, 174, 233));
    m_ensurePerimeterContrast = config.readEntry("EnsurePerimeterContrast", config.readEntry("AmbilightEnsureContrast", true));
    m_ambilightEnsureContrast = m_ensurePerimeterContrast;
    m_ambilightEnabled = config.readEntry("AmbilightEnabled", false);
    m_ambilightMode = qBound(0, config.readEntry("AmbilightMode", 0), 2);
    m_ambilightOpacity = qBound(0, config.readEntry("AmbilightOpacity", 100), 100);
    m_ambilightFrequency = qBound<qreal>(0.1, config.readEntry("AmbilightFrequency", 1.5), 10.0);
    m_ambilightDuration = qBound<qreal>(0.1, config.readEntry("AmbilightDuration", 0.5), 5.0);
    m_ambilightSaturationLeveling = qBound(0, config.readEntry("AmbilightSaturationLeveling", 65), 100);
    m_ambilightFallbackStrength = qBound(0, config.readEntry("AmbilightFallbackStrength", 50), 100);
    m_ambilightMinBrightness = qBound(0, config.readEntry("AmbilightMinBrightness", 25), 50);
    m_ambilightMaxBrightness = qBound(50, config.readEntry("AmbilightMaxBrightness", 85), 100);

    if (m_ambilightTimer) {
        m_ambilightTimer->setInterval(qRound(1000.0 / m_ambilightFrequency));
    }

    if (m_ambilightEnabled || m_ensurePerimeterContrast) {
        m_ambilightSampleNeeded = true;
        m_ambilightTargetColor = QColor();
        m_ambilightCurrentColor = QColor();
        if (m_ambilightTimer && !m_ambilightTimer->isActive()) {
            m_ambilightTimer->start();
        }
    } else {
        if (m_ambilightTimer && m_ambilightTimer->isActive()) {
            m_ambilightTimer->stop();
        }
        m_ambilightInTransition = false;
        m_ambilightTargetColor = QColor();
        m_ambilightCurrentColor = QColor();
    }

    m_width = qBound(1, config.readEntry("BorderWidth", 2), 20);
    m_gap = qBound(0, config.readEntry("OutlineGap", 0), 30);
    m_radius = qBound(0, config.readEntry("BorderRadius", 6), 30);
    m_fadeCorners = config.readEntry("FadeCorners", true);
    m_drawInside = config.readEntry("DrawInside", true);
    m_smoothEdges = config.readEntry("SmoothEdges", true);
    m_glowEnabled = config.readEntry("GlowEnabled", true);
    m_glowRadius = qBound(1, config.readEntry("GlowRadius", 6), 30);
    m_hideOnFullScreen = config.readEntry("HideOnFullScreen", true);

    m_colorCycleEnabled = config.readEntry("ColorCycleEnabled", false);
    parseColorList(config.readEntry("ColorList", QStringLiteral("#3daee9:3.0;#2ecc71:3.0;#9b59b6:3.0")));
    m_colorTransitionDuration = qMax<qreal>(0.1, config.readEntry("ColorTransitionDuration", 2.0));
    m_colorCycleOrder = config.readEntry("ColorCycleOrder", 0);
    m_colorCycleEasing = config.readEntry("ColorCycleEasing", 0);

    m_breatheEnabled = config.readEntry("BreatheEnabled", false);
    m_breatheSpeed = qBound<qreal>(0.01, config.readEntry("BreatheSpeed", 0.5), 20.0);
    m_breatheIntensity = qBound(10, config.readEntry("BreatheIntensity", 40), 100);

    m_glitchEnabled = config.readEntry("GlitchEnabled", false);
    m_glitchSpeed = qBound<qreal>(0.1, config.readEntry("GlitchSpeed", 1.0), 5.0);
    m_glitchIntensity = qBound(10, config.readEntry("GlitchIntensity", 50), 100);
    m_glitchAnomalyEnabled = config.readEntry("GlitchAnomalyEnabled", false);
    m_glitchFrequency = qBound(1, config.readEntry("GlitchFrequency", 3), 10);

    m_neonFlickerEnabled = config.readEntry("NeonFlickerEnabled", false);
    m_neonFlickerInterval = qMax<qreal>(0.5, config.readEntry("NeonFlickerInterval", 3.0));
    m_neonFlickerDuration = qBound<qreal>(0.05, config.readEntry("NeonFlickerDuration", 0.25), 2.0);
    m_neonFlickerIntensity = qBound(10, config.readEntry("NeonFlickerIntensity", 30), 100);

    m_flashEnabled = config.readEntry("FlashEnabled", false);
    m_flashInterval = qMax<qreal>(0.5, config.readEntry("FlashInterval", 4.0));
    m_flashDuration = qBound<qreal>(0.05, config.readEntry("FlashDuration", 0.15), 1.0);

    m_fadeDuration = qBound(0, config.readEntry("FadeDuration", 200), 2000);
    m_menuHideDelay = qBound(0, config.readEntry("MenuHideDelay", 300), 2000);
    m_menuShowDelay = qBound(0, config.readEntry("MenuShowDelay", 300), 2000);
    m_windowHideDelay = qBound(0, config.readEntry("WindowHideDelay", 0), 2000);
    m_windowShowDelay = qBound(0, config.readEntry("WindowShowDelay", 0), 2000);
    m_highlightRecentWindow = config.readEntry("HighlightRecentWindow", false);
    m_recentWindowsCount = qBound(1, config.readEntry("RecentWindowsCount", 1), 5);
    m_recentWindowColorMode = qBound(0, config.readEntry("RecentWindowColorMode", 0), 1);
    m_recentWindowCustomColor = config.readEntry("RecentWindowCustomColor", QColor(127, 140, 141));
    m_recentWindowOpacity = qBound(10, config.readEntry("RecentWindowOpacity", 50), 100);
    if (!m_highlightRecentWindow) {
        for (EffectWindow *recent : m_recentWindows) {
            if (recent) {
                effects->addRepaint(expandedGeometryFor(recent));
                m_outlineCache.remove(recent);
                m_lastGeometry.remove(recent);
            }
        }
        m_recentWindows.clear();
        m_recentAmbilightColors.clear();
    } else {
        while (m_recentWindows.size() > m_recentWindowsCount) {
            EffectWindow *oldWindow = m_recentWindows.takeLast();
            m_recentAmbilightColors.remove(oldWindow);
            if (oldWindow) {
                effects->addRepaint(expandedGeometryFor(oldWindow));
                m_outlineCache.remove(oldWindow);
                m_lastGeometry.remove(oldWindow);
            }
        }
    }

    m_visibilityConfig.fadeDuration = m_fadeDuration;
    m_visibilityConfig.menuHideDelay = m_menuHideDelay;
    m_visibilityConfig.menuShowDelay = m_menuShowDelay;
    m_visibilityConfig.windowHideDelay = m_windowHideDelay;
    m_visibilityConfig.windowShowDelay = m_windowShowDelay;

    m_visibilityState.popupBlocked = hasVisibleBlockingPopup();
    m_visibilityState.targetOpacity = m_visibilityState.popupBlocked ? 0.0 : 1.0;
    if (m_visibilityState.targetOpacity > 0.5 && m_visibilityState.visibilityOpacity < 0.001) {
        m_visibilityState.visibilityOpacity = 1.0;
    }

    m_outlineCache.clear();
    updateAnimationTimer();
    effects->addRepaintFull();
}

bool ActiveWindowOutlineEffect::isAnimationActive() const
{
    if (!m_outlineEnabled) {
        return false;
    }
    if (m_visibilityState.isTransitionActive(m_visibilityConfig.fadeDuration)) {
        return true;
    }
    if (m_delayedHideWindow && m_delayedHideState.isTransitionActive(m_visibilityConfig.fadeDuration)) {
        return true;
    }
    const bool hasDelayed = (m_delayedHideWindow && !m_delayedHideWindow->isDeleted()
        && m_delayedHideWindow->isOnCurrentDesktop() && !m_delayedHideWindow->isMinimized()
        && !m_delayedHideWindow->isHiddenByShowDesktop() && m_delayedHideState.isEffectivelyVisible());
    if (m_visibilityState.isEffectivelyVisible() || hasDelayed) {
        return m_colorCycleEnabled || m_breatheEnabled
            || m_glitchEnabled || m_neonFlickerEnabled || m_flashEnabled
            || m_ambilightInTransition;
    }
    return false;
}

void ActiveWindowOutlineEffect::updateAnimationTimer()
{
    if (isAnimationActive()) {
        if (!m_animationClock.isValid()) {
            m_animationClock.start();
        }
        if (!m_animationTimer->isActive()) {
            m_visibilityState.lastTickSec = -1.0;
            m_animationTimer->start();
        }
    } else {
        if (m_animationTimer && m_animationTimer->isActive()) {
            m_animationTimer->stop();
        }
    }
}

void ActiveWindowOutlineEffect::slotAnimationTick()
{
    const float elapsedSec = m_animationClock.isValid() ? float(m_animationClock.elapsed() / 1000.0) : 0.0f;
    const bool opacityChanged = m_visibilityState.stepTransition(elapsedSec, m_visibilityConfig.fadeDuration);

    if (m_visibilityState.needsSelfHeal(hasVisibleBlockingPopup())) {
        updatePopupBlockingState();
    }

    const bool showPending = (m_windowShowTimer && m_windowShowTimer->isActive());
    const bool wantsOutline = m_visibilityState.isEffectivelyVisible()
        || m_visibilityState.isTransitionActive(m_visibilityConfig.fadeDuration);
    if (!showPending && m_activeWindow && shouldOutlineWindow(m_activeWindow) && wantsOutline) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    } else if (opacityChanged && m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }

    if (m_delayedHideWindow && !m_delayedHideWindow->isDeleted()
        && m_delayedHideWindow->isOnCurrentDesktop() && !m_delayedHideWindow->isMinimized()) {
        const bool delayedOpacityChanged = m_delayedHideState.stepTransition(elapsedSec, m_visibilityConfig.fadeDuration);
        if (m_delayedHideState.isEffectivelyVisible()) {
            effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
        } else if (delayedOpacityChanged) {
            effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
            m_suppressedWindows.remove(m_delayedHideWindow);
            m_lastGeometry.remove(m_delayedHideWindow);
            m_outlineCache.remove(m_delayedHideWindow);
            m_delayedHideWindow = nullptr;
            m_delayedHideState.reset();
        }
    }

    if (!isAnimationActive() && m_animationTimer->isActive()) {
        m_animationTimer->stop();
    }
}

void ActiveWindowOutlineEffect::parseColorList(const QString &str)
{
    m_colorStops = OutlineColor::parseColorStops(str);
    m_currentColorIndex = 0;
    m_nextColorIndex = (m_colorStops.size() > 1) ? 1 : 0;
    m_isHoldingColor = true;
    m_stateStartTime = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;
}

QColor ActiveWindowOutlineEffect::currentCycleColor(qreal elapsedSec)
{
    if (m_colorStops.isEmpty()) {
        return m_customColor;
    }
    if (m_colorStops.size() == 1) {
        return m_colorStops[0].color;
    }

    if (m_currentColorIndex >= m_colorStops.size()) {
        m_currentColorIndex = 0;
    }
    if (m_nextColorIndex >= m_colorStops.size()) {
        m_nextColorIndex = (m_currentColorIndex + 1) % m_colorStops.size();
    }

    const qreal stateElapsed = elapsedSec - m_stateStartTime;

    if (m_isHoldingColor) {
        const qreal holdDuration = m_colorStops[m_currentColorIndex].holdTime;
        if (stateElapsed >= holdDuration) {
            m_isHoldingColor = false;
            m_stateStartTime = elapsedSec;
            if (m_colorCycleOrder == 0) {
                m_nextColorIndex = (m_currentColorIndex + 1) % m_colorStops.size();
            } else {
                if (m_colorStops.size() > 1) {
                    int next = m_currentColorIndex;
                    while (next == m_currentColorIndex) {
                        next = QRandomGenerator::global()->bounded(m_colorStops.size());
                    }
                    m_nextColorIndex = next;
                }
            }
        }
        return m_colorStops[m_currentColorIndex].color;
    } else {
        const qreal transDuration = qMax<qreal>(0.01, m_colorTransitionDuration);
        const qreal progress = qBound<qreal>(0.0, stateElapsed / transDuration, 1.0);

        const QColor c1 = m_colorStops[m_currentColorIndex].color;
        const QColor c2 = m_colorStops[m_nextColorIndex].color;
        const QColor result = OutlineColor::interpolateColors(c1, c2, progress, m_colorCycleEasing);

        if (stateElapsed >= transDuration) {
            m_currentColorIndex = m_nextColorIndex;
            m_isHoldingColor = true;
            m_stateStartTime = elapsedSec;
        }

        return result;
    }
}

qreal ActiveWindowOutlineEffect::computeBreatheFactor(qreal elapsedSec) const
{
    return OutlineShader::computeBreatheFactor(elapsedSec, m_breatheEnabled, m_breatheSpeed, m_breatheIntensity);
}

qreal ActiveWindowOutlineEffect::computeNeonFactor(qreal elapsedSec) const
{
    return OutlineShader::computeNeonFactor(elapsedSec, m_neonFlickerEnabled, m_neonFlickerInterval, m_neonFlickerDuration, m_neonFlickerIntensity);
}

qreal ActiveWindowOutlineEffect::computeFlashFactor(qreal elapsedSec) const
{
    return OutlineShader::computeFlashFactor(elapsedSec, m_flashEnabled, m_flashInterval, m_flashDuration);
}

qreal ActiveWindowOutlineEffect::computeGlitchFactor(qreal elapsedSec) const
{
    return OutlineShader::computeGlitchFactor(elapsedSec, m_glitchEnabled, m_glitchAnomalyEnabled, m_glitchSpeed, m_glitchFrequency, m_glitchIntensity);
}

QColor ActiveWindowOutlineEffect::resolveColor(qreal windowOpacity, EffectWindow *w) const
{
    QColor color;
    const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;
    const bool isRecent = (w != nullptr && m_recentWindows.contains(w) && w != m_activeWindow);

    if (isRecent && m_recentWindowColorMode == 1) {
        color = m_recentWindowCustomColor;
    } else if (m_colorCycleEnabled && !m_colorStops.isEmpty()) {
        color = const_cast<ActiveWindowOutlineEffect *>(this)->currentCycleColor(elapsedSec);
    } else if (m_ambilightEnabled) {
        if (isRecent && m_recentAmbilightColors.contains(w) && m_recentAmbilightColors.value(w).isValid()) {
            color = m_recentAmbilightColors.value(w);
        } else {
            color = const_cast<ActiveWindowOutlineEffect *>(this)->currentAmbilightColor(elapsedSec);
        }
        color.setAlphaF(color.alphaF() * (m_ambilightOpacity / 100.0));
    } else if (m_ensurePerimeterContrast && (isRecent && m_recentAmbilightColors.contains(w) ? m_recentAmbilightColors.value(w).isValid() : m_ambilightCurrentColor.isValid())) {
        if (isRecent && m_recentAmbilightColors.contains(w) && m_recentAmbilightColors.value(w).isValid()) {
            color = m_recentAmbilightColors.value(w);
        } else {
            color = const_cast<ActiveWindowOutlineEffect *>(this)->currentAmbilightColor(elapsedSec);
        }
        const qreal opacityFraction = m_useAccentColor
            ? (m_accentOpacity / 100.0)
            : (m_customColor.alphaF());
        color.setAlphaF(color.alphaF() * opacityFraction);
    } else if (m_useAccentColor) {
        color = qApp->palette().color(QPalette::Highlight);
        color.setAlphaF(color.alphaF() * m_accentOpacity / 100.0);
    } else {
        color = m_customColor;
    }

    if (!isRecent && (m_breatheEnabled || m_neonFlickerEnabled || m_flashEnabled || m_glitchEnabled)) {
        const qreal breathe = computeBreatheFactor(elapsedSec);
        const qreal neon = computeNeonFactor(elapsedSec);
        const qreal flash = computeFlashFactor(elapsedSec);
        const qreal glitch = computeGlitchFactor(elapsedSec);

        qreal totalFactor = breathe * neon * glitch + flash;
        totalFactor = qBound<qreal>(0.05, totalFactor, 2.0);

        int a = qRound(color.alpha() * totalFactor);
        const qreal glowBoost = flash + (glitch - 1.0) * 0.8;
        int r = qRound(qMin<qreal>(255.0, color.red() * neon * (1.0 + glowBoost)));
        int g = qRound(qMin<qreal>(255.0, color.green() * neon * (1.0 + glowBoost)));
        int b = qRound(qMin<qreal>(255.0, color.blue() * neon * (1.0 + glowBoost)));

        color = QColor(r, g, b, qBound(0, a, 255));
    }

    // Apply visibility transition opacity
    const qreal visOpacity = qBound<qreal>(0.0, windowOpacity, 1.0);
    color.setAlpha(qBound(0, qRound(color.alpha() * visOpacity), 255));

    return color;
}

void ActiveWindowOutlineEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowFrameGeometryChanged,
            this, &ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged);
    connect(w, &EffectWindow::minimizedChanged,
            this, &ActiveWindowOutlineEffect::slotWindowMinimizedChanged);
    connect(w, &EffectWindow::windowHiddenChanged,
            this, &ActiveWindowOutlineEffect::slotWindowHiddenChanged);
    connect(w, &EffectWindow::windowFullScreenChanged,
            this, &ActiveWindowOutlineEffect::slotWindowFullScreenChanged);

    updatePopupBlockingState();
}

bool ActiveWindowOutlineEffect::shouldOutlineWindow(EffectWindow *w) const
{
    return m_outlineEnabled && w && w == m_activeWindow && !w->isDesktop() && !w->isDock()
        && !(m_hideOnFullScreen && w->isFullScreen())
        && !isBlockingPopup(w);
}

bool ActiveWindowOutlineEffect::isBlockingPopup(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    // Native xdg_popup menus may have WindowType::Unknown. KWin's own
    // titlebar menu is an internal window, not necessarily a semantic menu
    // either. A grab is sufficient, but not necessary: Firefox also opens
    // interactive menus without one. The fallback checks input and ownership
    // instead of treating every generic hover popup as a blocking menu.
    return w->isMenu() || w->isDropdownMenu() || w->isPopupMenu()
        || w->isComboBox() || w->isAppletPopup()
        || (w->isPopupWindow() && w->window() && w->window()->hasPopupGrab())
        || OutlinePopupFilter::isOwnedInteractivePopup(w, m_activeWindow,
                                                       popupAcceptsInput(w));
}

bool ActiveWindowOutlineEffect::popupAcceptsInput(EffectWindow *w) const
{
#ifndef ACTIVE_WINDOW_OUTLINE_X11
    if (w && !w->isDeleted() && w->isPopupWindow() && w->surface()) {
        return !w->surface()->input().isEmpty();
    }
#endif
    return false;
}

bool ActiveWindowOutlineEffect::hasVisibleBlockingPopup() const
{
    // Query the current stacking order on every paint instead of remembering
    // popup state from windowAdded. Toolkits often create a popup before it is
    // visible, so checking only at creation time misses context menus.
    for (EffectWindow *candidate : effects->stackingOrder()) {
        if (isBlockingPopup(candidate) && !candidate->isDeleted()
            && candidate->isVisible() && !candidate->isMinimized()
            && candidate->isOnCurrentDesktop()
            && !candidate->isHiddenByShowDesktop()) {
            return true;
        }
    }
    return false;
}

void ActiveWindowOutlineEffect::slotWindowDeleted(EffectWindow *w)
{
    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
    }
    if (m_activeWindow == w) {
        m_activeWindow = nullptr;
        if (m_windowShowTimer) {
            m_windowShowTimer->stop();
        }
    }
    m_recentWindows.removeAll(w);
    m_recentAmbilightColors.remove(w);
    m_suppressedWindows.remove(w);
    m_lastGeometry.remove(w);
    m_outlineCache.remove(w);

    updatePopupBlockingState();
    repaintAllOutlines();
}

void ActiveWindowOutlineEffect::slotWindowHiddenChanged(EffectWindow *w)
{
    Q_UNUSED(w);
    updatePopupBlockingState();
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
    for (EffectWindow *recent : m_recentWindows) {
        if (recent) {
            effects->addRepaint(expandedGeometryFor(recent));
        }
    }
}

void ActiveWindowOutlineEffect::updatePopupBlockingState()
{
    const bool currentlyHasPopup = hasVisibleBlockingPopup();
    const auto action = m_visibilityState.handlePopupChange(currentlyHasPopup, m_visibilityConfig);

    switch (action) {
    case OutlineVisibility::StateMachine::TimerAction::StartHideTimer:
        if (m_menuHideTimer) {
            m_menuHideTimer->start(m_visibilityConfig.menuHideDelay);
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::StartShowTimer:
        if (m_menuShowTimer) {
            m_menuShowTimer->start(m_visibilityConfig.menuShowDelay);
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::CancelHideTimer:
        if (m_menuHideTimer) {
            m_menuHideTimer->stop();
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::CancelShowTimer:
        if (m_menuShowTimer) {
            m_menuShowTimer->stop();
        }
        break;
    case OutlineVisibility::StateMachine::TimerAction::None:
        break;
    }

    updateAnimationTimer();
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

void ActiveWindowOutlineEffect::repaintAllOutlines()
{
    if (m_delayedHideWindow && !m_delayedHideWindow->isDeleted()
        && m_delayedHideWindow->isOnCurrentDesktop() && !m_delayedHideWindow->isMinimized()) {
        effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
    }
    for (EffectWindow *recent : m_recentWindows) {
        if (recent && !recent->isDeleted()
            && recent->isOnCurrentDesktop() && !recent->isMinimized()) {
            effects->addRepaint(expandedGeometryFor(recent));
        }
    }
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

OutlineRegion ActiveWindowOutlineEffect::panelOcclusionRegion(EffectWindow *w,
                                                             QString *source) const
{
    const auto label = [source](const char *value) {
        if (source) {
            *source = QString::fromLatin1(value);
        }
    };
#ifndef ACTIVE_WINDOW_OUTLINE_X11
    if (SurfaceInterface *surface = w->surface()) {
        OutlineRegion region;
        OutlineRegion input;
        // These are surface-local coordinates, not decoration/frame-local.
        const QPointF origin = w->bufferGeometry().topLeft();
#ifdef ACTIVE_WINDOW_OUTLINE_SURFACE_BLUR_REGION
        region = surface->blurRegion().translated(origin).roundedOut();
        input = surface->input().translated(origin).roundedOut();
#else
        if (BlurInterface *blur = surface->blur()) {
            region = blur->region();
        }
        if (region.isEmpty()) {
            if (ContrastInterface *contrast = surface->contrast()) {
                region = contrast->region();
            }
        }
        region.translate(origin.toPoint());
        input = surface->input().translated(origin.toPoint());
#endif
        // Clamp protocol regions (which can cover the entire coordinate
        // space) before converting them to render-target coordinates.
        const OutlineRegion bounds(QRegion(QRectF(w->bufferGeometry()).toAlignedRect()));
        region &= bounds;
        input &= bounds;
        if (!region.isEmpty()) {
            label("background-mask");
            return region;
        }
        if (!input.isEmpty()) {
            // Plasma updates this even when its theme disables blur. The
            // desktop-facing edge follows the animated panel background.
            // The screen-facing gap is included for Fitts's law, so this is
            // a conservative fallback, not an exact visual/alpha mask.
            label("input-fallback");
            return input;
        }
    }
#endif
    label("frame-fallback");
    return OutlineRegion(QRegion(QRectF(w->frameGeometry()).toAlignedRect()));
}

void ActiveWindowOutlineEffect::renderOutline(const RenderTarget &renderTarget,
                                           const RenderViewport &viewport,
                                           const OutlineCache &cache,
                                           const OutlineRegion &clipRegion,
                                           EffectWindow *outlinedWindow,
                                           qreal windowOpacity)
{
    if (cache.layers.isEmpty()) {
        return;
    }

    const qreal scale = viewport.scale();
    static constexpr GLVertexAttrib layout{
        .attributeIndex = VA_Position,
        .componentCount = 2,
        .type = GL_FLOAT,
        .relativeOffset = 0,
    };

    // KWin's other effects share this OpenGL context. Preserve both the blend
    // enable flag and factors; leaving an additive factor behind can change
    // how subsequently repainted hover highlights and windows are composited.
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint previousSrcRgb = GL_ONE;
    GLint previousDstRgb = GL_ZERO;
    GLint previousSrcAlpha = GL_ONE;
    GLint previousDstAlpha = GL_ZERO;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    // GLVertexBuffer::render(region, ..., true) sets a scissor box for each
    // damaged rectangle, but its caller must enable GL_SCISSOR_TEST. Without
    // it translucent layers can accumulate over stale pixels during a small
    // hover or typing repaint.
    const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLint previousScissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, previousScissorBox);
    glEnable(GL_SCISSOR_TEST);

    // The VBO's hardware clipping expects render-target-local coordinates.
    // Older KWin passes a logical region and exposes the conversion directly.
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    OutlineRegion scissorRegion = viewport.mapToRenderTarget(clipRegion);
#else
    // Newer KWin passes a device-space damage region. Match
    // ItemRendererOpenGL's conversion, including output rotation and the
    // inset used by an intermediate render target.
    const QSize bufferOffset = renderTarget.transform().map(
        QSize(viewport.renderOffset().x(), viewport.renderOffset().y()));
    OutlineRegion scissorRegion = Rect(
        QPoint(bufferOffset.width(), bufferOffset.height()),
        renderTarget.size() - 2 * bufferOffset);
    scissorRegion &= viewport.transform().map(
        clipRegion & renderTarget.transformedRect(), renderTarget.transformedSize());
#endif

    // Draw only in the final screen pass so the existing popup/window suppression
    // applies to every part of the outline. Any window positioned above the
    // outlined window in stacking order (e.g. notifications, KeepAbove/Always-on-top
    // windows, OSD, popups, or overlapping dialogs) must occlude the outline so
    // it never bleeds through surfaces in front of it.
    bool aboveOutlinedWindow = false;
    for (EffectWindow *candidate : effects->stackingOrder()) {
        if (candidate == outlinedWindow) {
            aboveOutlinedWindow = true;
            continue;
        }
        if (aboveOutlinedWindow && !candidate->isDeleted()
            && candidate->isVisible() && !candidate->isMinimized()
            && candidate->isOnCurrentDesktop()
            && !candidate->isHiddenByShowDesktop()
            && !candidate->isOutline()
            && !candidate->isDesktop()) {
            if (candidate->isDock()) {
                scissorRegion -= viewport.mapToRenderTarget(panelOcclusionRegion(candidate));
            } else if (candidate->isNotification() || candidate->isCriticalNotification() || candidate->isOnScreenDisplay()) {
                scissorRegion -= viewport.mapToRenderTarget(panelOcclusionRegion(candidate));
            } else {
                const OutlineRegion occludingRegion(QRegion(QRectF(candidate->frameGeometry()).toAlignedRect()));
                scissorRegion -= viewport.mapToRenderTarget(occludingRegion);
            }
        }
    }

    if (scissorRegion.isEmpty()) {
        glScissor(previousScissorBox[0], previousScissorBox[1],
                  previousScissorBox[2], previousScissorBox[3]);
        if (!scissorWasEnabled) {
            glDisable(GL_SCISSOR_TEST);
        }
        return;
    }

    const QColor dynamicColor = resolveColor(windowOpacity, outlinedWindow);

    const auto box = [scale](const QRectF &rect) {
        return QVector4D(rect.center().x() * scale,
                         rect.center().y() * scale,
                         rect.width() * scale / 2.0,
                         rect.height() * scale / 2.0);
    };
    const float cornerRadiusPx = float(cache.outerRadius * scale);
    const float minCutoff = qMax<float>(4.0f * float(scale), cornerRadiusPx);
    const float cornerCutoff = m_drawInside ? qMax<float>(minCutoff, float(m_width * scale)) : minCutoff;
    const float edgeHalfMin = float(qMin<qreal>(cache.outerRect.width(), cache.outerRect.height()) * scale * 0.5);
    const float maxFade = qMax<float>(15.0f * float(scale), edgeHalfMin - cornerCutoff - 10.0f * float(scale));
    const float fadeDist = std::clamp<float>(120.0f * float(scale), 20.0f * float(scale), maxFade);

    if (m_glowEnabled && dynamicColor.alpha() > 0) {
        if (!m_glowShader) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
            m_glowShader = ShaderManager::instance()->generateCustomShader(
                ShaderTrait::UniformColor, glowVertexSource(), glowFragmentSource());
#else
            m_glowShader = ShaderManager::instance()->generateCustomShader(
                ShaderTrait::Border, QByteArray(), glowFragmentSource());
#endif
        }
#ifdef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
        if (m_glowShader) {
#else
        if (m_glowShader && m_glowShader->isValid()) {
#endif
            const qreal reach = m_glowRadius;
            const QRectF farOuter = cache.outerRect.adjusted(-reach, -reach, reach, reach);
            const QRectF farInner = cache.innerRect.adjusted(reach, reach, -reach, -reach);
            QList<QVector2D> glowVertices;
            if (farInner.width() > 0 && farInner.height() > 0) {
                generateRoundedRectStrip(farOuter, cache.outerRadius + reach,
                                         farInner, qMax<qreal>(0.0, cache.innerRadius - reach),
                                         1.0, glowVertices);
            } else {
                const float x0 = farOuter.left();
                const float y0 = farOuter.top();
                const float x1 = farOuter.right();
                const float y1 = farOuter.bottom();
                glowVertices = {{x0, y0}, {x1, y0}, {x0, y1},
                                {x1, y0}, {x1, y1}, {x0, y1}};
            }

            QList<QVector2D> scaledVerts;
            scaledVerts.reserve(glowVertices.size());
            for (const QVector2D &vertex : std::as_const(glowVertices)) {
                scaledVerts.append(vertex * scale);
            }
            auto vbo = GLVertexBuffer::streamingBuffer();
            auto mapped = vbo->map<QVector2D>(scaledVerts.size());
            if (mapped) {
                memcpy(mapped->data(), scaledVerts.data(), scaledVerts.size() * sizeof(QVector2D));
                vbo->unmap();
                vbo->setVertexCount(scaledVerts.size());
                vbo->setAttribLayout(std::span(&layout, 1), sizeof(QVector2D));

                ShaderBinder glowBinder(m_glowShader.get());
                GLShader *shader = glowBinder.shader();
                shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                                   viewport.projectionMatrix());
                shader->setUniform(GLShader::ColorUniform::Color, dynamicColor);
                shader->setUniform("outerBox", box(cache.outerRect));
                shader->setUniform("innerBox", box(cache.innerRect));
                shader->setUniform("outerRadius", float(cache.outerRadius * scale));
                shader->setUniform("innerRadius", float(cache.innerRadius * scale));
                shader->setUniform("glowRadius", float(m_glowRadius * scale));
                shader->setUniform("fadeCorners", m_fadeCorners ? 1.0f : 0.0f);
                shader->setUniform("cornerCutoff", cornerCutoff);
                shader->setUniform("fadeDistance", fadeDist);

                const float elapsedSec = m_animationClock.isValid() ? float(m_animationClock.elapsed() / 1000.0) : 0.0f;
                shader->setUniform("time", elapsedSec);
                shader->setUniform("glitchActive", m_glitchEnabled ? 1.0f : 0.0f);
                shader->setUniform("glitchSpeed", float(m_glitchSpeed));
                shader->setUniform("glitchIntensity", float(m_glitchIntensity));
                shader->setUniform("glitchAnomalyActive", (m_glitchEnabled && m_glitchAnomalyEnabled) ? 1.0f : 0.0f);
                shader->setUniform("glitchFrequency", float(m_glitchFrequency));

                glEnable(GL_BLEND);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                vbo->render(scissorRegion, GL_TRIANGLES, true);
            }
        }
    }

    if (m_fadeCorners) {
        if (!m_fadeCornersShader) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
            m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                ShaderTrait::UniformColor, glowVertexSource(), outlineFragmentSource());
#else
            m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                ShaderTrait::Border, QByteArray(), outlineFragmentSource());
#endif
        }
    }

    const bool useCustomFade = m_fadeCorners &&
#ifdef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
        m_fadeCornersShader;
#else
        m_fadeCornersShader && m_fadeCornersShader->isValid();
#endif

    std::unique_ptr<ShaderBinder> customBinder;
    std::unique_ptr<ShaderBinder> defaultBinder;
    GLShader *activeShader = nullptr;

    if (useCustomFade) {
        customBinder = std::make_unique<ShaderBinder>(m_fadeCornersShader.get());
        activeShader = customBinder->shader();
        activeShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                                 viewport.projectionMatrix());
        activeShader->setUniform("outerBox", box(cache.outerRect));
        activeShader->setUniform("cornerCutoff", cornerCutoff);
        activeShader->setUniform("fadeDistance", fadeDist);
    } else {
        defaultBinder = std::make_unique<ShaderBinder>(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
        activeShader = defaultBinder->shader();
        activeShader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                                 viewport.projectionMatrix());
        activeShader->setColorspaceUniforms(ColorDescription::sRGB,
                                            renderTarget.colorDescription(),
                                            RenderingIntent::Perceptual);
    }

    for (const OutlineLayer &layer : cache.layers) {
        if (layer.vertices.isEmpty()) {
            continue;
        }

        QColor layerColor = dynamicColor;
        layerColor.setAlpha(qBound(0, qRound(dynamicColor.alpha() * layer.opacity), 255));
        if (layerColor.alpha() == 0) {
            continue;
        }

        auto vbo = GLVertexBuffer::streamingBuffer();
        QList<QVector2D> scaledVerts;
        scaledVerts.reserve(layer.vertices.size());
        for (const QVector2D &vertex : layer.vertices) {
            scaledVerts.append(QVector2D(vertex.x() * scale, vertex.y() * scale));
        }

        auto mapped = vbo->map<QVector2D>(scaledVerts.size());
        if (!mapped) {
            continue;
        }
        memcpy(mapped->data(), scaledVerts.data(), scaledVerts.size() * sizeof(QVector2D));
        vbo->unmap();
        vbo->setVertexCount(scaledVerts.size());
        vbo->setAttribLayout(std::span(&layout, 1), sizeof(QVector2D));

        activeShader->setUniform(GLShader::ColorUniform::Color, layerColor);
        const bool blended = useCustomFade || layerColor.alpha() < 255;
        if (blended) {
            glEnable(GL_BLEND);
            // The uniform contains straight (unpremultiplied) RGB and alpha.
            // Alpha-over keeps the hue of the outline; additive blending
            // drives light backgrounds toward white.
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        vbo->render(scissorRegion, GL_TRIANGLES, true);
    }

    glBlendFuncSeparate(previousSrcRgb, previousDstRgb,
                        previousSrcAlpha, previousDstAlpha);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    glScissor(previousScissorBox[0], previousScissorBox[1],
              previousScissorBox[2], previousScissorBox[3]);
    if (!scissorWasEnabled) {
        glDisable(GL_SCISSOR_TEST);
    }
}

void ActiveWindowOutlineEffect::cacheWindowOutline(EffectWindow *w, const QRectF &geo)
{
    OutlineCache cache;

    const qreal minDimension = qMin(geo.width(), geo.height());
    const qreal gap = m_drawInside
        ? qMin<qreal>(m_gap, qMax<qreal>(0.0, minDimension / 2.0 - 1.0))
        : m_gap;
    const qreal maxInsideWidth = qMax<qreal>(0.0, minDimension / 2.0 - gap);
    const qreal bw = m_drawInside ? qMin<qreal>(m_width, maxInsideWidth) : m_width;
    if (bw <= 0.0) {
        m_outlineCache[w] = cache;
        return;
    }
    const qreal r = m_radius;

    const QRectF outerRect = m_drawInside
        ? geo.adjusted(gap, gap, -gap, -gap)
        : geo.adjusted(-gap - bw, -gap - bw, gap + bw, gap + bw);
    const QRectF innerRect = m_drawInside
        ? geo.adjusted(gap + bw, gap + bw, -gap - bw, -gap - bw)
        : geo.adjusted(-gap, -gap, gap, gap);
    const qreal outerRadius = m_drawInside ? qMax<qreal>(0.0, r - gap) : r + gap + bw;
    const qreal innerRadius = m_drawInside ? qMax<qreal>(0.0, r - gap - bw) : r + gap;
    cache.outerRect = outerRect;
    cache.innerRect = innerRect;
    cache.outerRadius = outerRadius;
    cache.innerRadius = innerRadius;

    const auto appendLayer = [this, &cache](const QRectF &outer, qreal outerR,
                                            const QRectF &inner, qreal innerR,
                                            qreal opacity) {
        if (outer.width() <= 0.0 || outer.height() <= 0.0
            || inner.width() <= 0.0 || inner.height() <= 0.0) {
            return;
        }
        OutlineLayer layer;
        generateRoundedRectStrip(outer, outerR, inner, innerR, 1.0, layer.vertices);
        layer.opacity = opacity;
        cache.layers.append(std::move(layer));
    };

    if (m_smoothEdges) {
        constexpr qreal feather = 0.85;
        appendLayer(outerRect.adjusted(-feather, -feather, feather, feather),
                    outerRadius + feather, outerRect, outerRadius,
                    0.38);
        appendLayer(innerRect, innerRadius,
                    innerRect.adjusted(feather, feather, -feather, -feather),
                    qMax<qreal>(0.0, innerRadius - feather),
                    0.38);
    }

    appendLayer(outerRect, outerRadius, innerRect, innerRadius, 1.0);

    m_outlineCache[w] = cache;
}

void ActiveWindowOutlineEffect::slotWindowActivated(EffectWindow *w)
{
    if (isBlockingPopup(w)) {
        return;
    }

    EffectWindow *previous = m_activeWindow;
    if (previous == w) {
        return;
    }

    if (m_ambilightEnabled || m_ensurePerimeterContrast) {
        m_ambilightSampleNeeded = true;
    }

    if (m_windowShowTimer && m_windowShowTimer->isActive()) {
        m_windowShowTimer->stop();
    }

    if (m_highlightRecentWindow) {
        if (previous && !previous->isDeleted() && !previous->isDesktop() && !previous->isDock()
            && !(m_hideOnFullScreen && previous->isFullScreen()) && !isBlockingPopup(previous)) {
            m_recentWindows.removeAll(previous);
            m_recentWindows.prepend(previous);
            if (m_ambilightCurrentColor.isValid()) {
                m_recentAmbilightColors[previous] = m_ambilightCurrentColor;
            }
        }

        if (w) {
            m_recentWindows.removeAll(w);
            m_recentAmbilightColors.remove(w);
        }

        while (m_recentWindows.size() > m_recentWindowsCount) {
            EffectWindow *oldWindow = m_recentWindows.takeLast();
            m_recentAmbilightColors.remove(oldWindow);
            if (oldWindow) {
                effects->addRepaint(expandedGeometryFor(oldWindow));
                m_outlineCache.remove(oldWindow);
                m_lastGeometry.remove(oldWindow);
            }
        }

        for (EffectWindow *recent : m_recentWindows) {
            if (recent) {
                effects->addRepaint(expandedGeometryFor(recent));
            }
        }

        if (m_delayedHideWindow) {
            if (!m_recentWindows.contains(m_delayedHideWindow) && m_delayedHideWindow != w) {
                effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
                m_outlineCache.remove(m_delayedHideWindow);
                m_lastGeometry.remove(m_delayedHideWindow);
            }
            m_delayedHideWindow = nullptr;
            m_delayedHideState.reset();
            if (m_windowHideTimer) {
                m_windowHideTimer->stop();
            }
        }
    } else {
        if (!m_recentWindows.isEmpty()) {
            for (EffectWindow *recent : m_recentWindows) {
                if (recent) {
                    effects->addRepaint(expandedGeometryFor(recent));
                    m_outlineCache.remove(recent);
                    m_lastGeometry.remove(recent);
                }
            }
            m_recentWindows.clear();
            m_recentAmbilightColors.clear();
        }

        if (previous) {
            const bool willDelayOrFade = (!previous->isDeleted() && previous->isOnCurrentDesktop())
                && (m_windowHideDelay > 0 || m_visibilityConfig.fadeDuration > 0);

            if (willDelayOrFade) {
                if (m_delayedHideWindow && m_delayedHideWindow != previous) {
                    effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
                    m_suppressedWindows.remove(m_delayedHideWindow);
                    m_lastGeometry.remove(m_delayedHideWindow);
                    m_outlineCache.remove(m_delayedHideWindow);
                    m_delayedHideWindow = nullptr;
                    m_delayedHideState.reset();
                }
                m_delayedHideWindow = previous;
                effects->addRepaint(expandedGeometryFor(previous));

                if (m_windowHideDelay > 0) {
                    m_delayedHideState.startBuffer();
                    if (m_windowHideTimer) {
                        m_windowHideTimer->start(m_windowHideDelay);
                    }
                } else {
                    m_delayedHideState.startFade(m_visibilityConfig.fadeDuration);
                    updateAnimationTimer();
                }
            } else {
                effects->addRepaint(expandedGeometryFor(previous));
                m_suppressedWindows.remove(previous);
                m_lastGeometry.remove(previous);
                m_outlineCache.remove(previous);
                if (m_delayedHideWindow == previous) {
                    m_delayedHideWindow = nullptr;
                    m_delayedHideState.reset();
                    if (m_windowHideTimer) {
                        m_windowHideTimer->stop();
                    }
                }
            }
        }
    }

    m_activeWindow = w;

    if (!w) {
        m_visibilityState.setTargetOpacity(0.0, m_visibilityConfig.fadeDuration);
        updateAnimationTimer();
        return;
    }

    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
    }

    // If a popup menu is currently active or buffering, let popup timers manage outline visibility
    if (hasVisibleBlockingPopup() || m_visibilityState.popupBlocked
        || m_visibilityState.menuShowTimerActive || (m_menuShowTimer && m_menuShowTimer->isActive())) {
        return;
    }

    if (shouldOutlineWindow(w)) {
        if (m_windowShowDelay > 0) {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_visibilityState.visibilityOpacity = 0.0;
            }
            m_visibilityState.targetOpacity = 0.0;
            if (m_windowShowTimer) {
                m_windowShowTimer->start(m_windowShowDelay);
            }
        } else {
            if (m_visibilityConfig.fadeDuration > 0) {
                m_visibilityState.visibilityOpacity = 0.0;
            }
            m_visibilityState.setTargetOpacity(1.0, m_visibilityConfig.fadeDuration);
            updateAnimationTimer();
            effects->addRepaint(expandedGeometryFor(w));
        }
    }
}

void ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged(EffectWindow *w,
                                                            const QRectF &oldGeometry)
{
    if (m_activeWindow && w != m_activeWindow) {
        const QRectF activeGeo = expandedGeometryFor(m_activeWindow);
        if (oldGeometry.intersects(activeGeo) || w->frameGeometry().intersects(activeGeo)) {
            effects->addRepaint(activeGeo);
        }
    }
    const bool isRecent = (m_highlightRecentWindow && m_recentWindows.contains(w));
    if (!shouldOutlineWindow(w) && !isBlockingPopup(w) && m_delayedHideWindow != w && !isRecent) {
        return;
    }
    const qreal glowExtent = m_glowEnabled ? m_glowRadius : 0;
    const qreal extent = m_drawInside
        ? glowExtent + 1.0
        : m_width + m_gap + glowExtent + 1.0;
    effects->addRepaint(oldGeometry.adjusted(-extent, -extent, extent, extent));
    effects->addRepaint(expandedGeometryFor(w));
    if (w == m_activeWindow && (m_ambilightEnabled || m_ensurePerimeterContrast)) {
        m_ambilightSampleNeeded = true;
    }
    if (isBlockingPopup(w)) {
        updatePopupBlockingState();
    }
}

void ActiveWindowOutlineEffect::slotToggleAction()
{
    m_outlineEnabled = !m_outlineEnabled;
    if (m_outlineEnabled) {
        if (m_visibilityConfig.fadeDuration > 0) {
            m_visibilityState.visibilityOpacity = 0.0;
        }
        m_visibilityState.targetOpacity = hasVisibleBlockingPopup() ? 0.0 : 1.0;
        updateAnimationTimer();
        if (m_activeWindow) {
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
        for (EffectWindow *recent : m_recentWindows) {
            if (recent) {
                effects->addRepaint(expandedGeometryFor(recent));
            }
        }
    } else {
        m_visibilityState.targetOpacity = 0.0;
        m_visibilityState.visibilityOpacity = 0.0;
        updateAnimationTimer();
        if (m_windowShowTimer) {
            m_windowShowTimer->stop();
        }
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
        if (m_menuShowTimer) {
            m_menuShowTimer->stop();
        }
        if (m_menuHideTimer) {
            m_menuHideTimer->stop();
        }
        for (EffectWindow *recent : m_recentWindows) {
            if (recent) {
                effects->addRepaint(expandedGeometryFor(recent));
            }
        }
        if (m_delayedHideWindow) {
            effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
            m_suppressedWindows.remove(m_delayedHideWindow);
            m_lastGeometry.remove(m_delayedHideWindow);
            m_outlineCache.remove(m_delayedHideWindow);
            m_delayedHideWindow = nullptr;
        }
        m_delayedHideState.reset();
        if (m_activeWindow) {
            effects->addRepaint(expandedGeometryFor(m_activeWindow));
        }
    }
}

void ActiveWindowOutlineEffect::slotWindowMinimizedChanged(EffectWindow *w)
{
    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_recentWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (shouldOutlineWindow(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    } else if (m_activeWindow && w != m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

void ActiveWindowOutlineEffect::slotWindowFullScreenChanged(EffectWindow *w)
{
    if (m_delayedHideWindow == w) {
        m_delayedHideWindow = nullptr;
        m_delayedHideState.reset();
        if (m_windowHideTimer) {
            m_windowHideTimer->stop();
        }
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_recentWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (w && w == m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(w));
    }
}

void ActiveWindowOutlineEffect::slotShowingDesktopChanged()
{
    // KWin's show-desktop animation does not minimize windows: it marks them
    // hiddenByShowDesktop and transforms/fades them in a separate effect. The
    // outline is painted at screen level, so explicitly damage its old band on
    // both transitions instead of leaving it above the desktop.
    repaintAllOutlines();
}

QRectF ActiveWindowOutlineEffect::expandedGeometryFor(EffectWindow *w) const
{
    if (!w) {
        return QRectF();
    }
    const qreal glowExtent = m_glowEnabled ? m_glowRadius : 0;
    qreal extent = m_drawInside
        ? glowExtent + 1.0
        : m_width + m_gap + glowExtent + 1.0;
    if (m_ensurePerimeterContrast) {
        extent = std::max(extent, 18.0);
    }
    return w->frameGeometry().adjusted(-extent, -extent, extent, extent);
}

bool ActiveWindowOutlineEffect::isActive() const
{
    const bool showPending = (m_windowShowTimer && m_windowShowTimer->isActive());
    const bool activeEligible = shouldOutlineWindow(m_activeWindow) && m_visibilityState.isEffectivelyVisible();
    const bool delayedEligible = (m_delayedHideWindow && !m_delayedHideWindow->isDeleted()
        && !m_delayedHideWindow->isMinimized() && !m_delayedHideWindow->isHiddenByShowDesktop()
        && m_delayedHideWindow->isOnCurrentDesktop() && m_delayedHideState.isEffectivelyVisible());
    bool recentEligible = false;
    if (m_highlightRecentWindow) {
        for (EffectWindow *recent : m_recentWindows) {
            if (recent && !recent->isDeleted() && !recent->isMinimized()
                && !recent->isHiddenByShowDesktop() && recent->isOnCurrentDesktop()) {
                recentEligible = true;
                break;
            }
        }
    }
    return m_outlineEnabled && (recentEligible || OutlineVisibility::StateMachine::isEffectActive(m_outlineEnabled, showPending, activeEligible, delayedEligible));
}

QString ActiveWindowOutlineEffect::debug(const QString &parameter) const
{
    QString result = QStringLiteral("version=" ACTIVE_WINDOW_OUTLINE_VERSION
                                   " build=" ACTIVE_WINDOW_OUTLINE_BUILD_ID
                                   " kwinHeaders=" ACTIVE_WINDOW_OUTLINE_KWIN_VERSION "\n");
    result += QStringLiteral("width=%1 offset=%2 radius=%3 drawInside=%4 smoothEdges=%5 glow=%6 glowRadius=%7 accentOpacity=%8"
                             " fadeDuration=%9 menuHideDelay=%10 menuShowDelay=%11 windowHideDelay=%12 windowShowDelay=%13 fadeCorners=%14")
        .arg(m_width)
        .arg(m_gap)
        .arg(m_radius)
        .arg(m_drawInside ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_smoothEdges ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_glowEnabled ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_glowRadius)
        .arg(m_accentOpacity)
        .arg(m_fadeDuration)
        .arg(m_menuHideDelay)
        .arg(m_menuShowDelay)
        .arg(m_windowHideDelay)
        .arg(m_windowShowDelay)
        .arg(m_fadeCorners ? QStringLiteral("true") : QStringLiteral("false"));
    if (parameter != QStringLiteral("windows")) {
        return result;
    }
    const auto rectText = [](const QRectF &rect) {
        return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    };
    result += QStringLiteral("\nblockingPopup=%1 popupBlocked=%2 visibilityOpacity=%3 targetOpacity=%4 delayedHideWindow=%5 delayedHideOpacity=%6 showTimerActive=%7 recentCount=%8 recentOpacity=%9 effectActive=%10")
        .arg(hasVisibleBlockingPopup())
        .arg(m_visibilityState.popupBlocked)
        .arg(m_visibilityState.visibilityOpacity)
        .arg(m_visibilityState.targetOpacity)
        .arg(m_delayedHideWindow != nullptr)
        .arg(m_delayedHideState.opacity)
        .arg(m_windowShowTimer && m_windowShowTimer->isActive())
        .arg(m_recentWindows.size())
        .arg(m_recentWindowOpacity)
        .arg(isActive());
    int index = 0;
    // Intentionally omit window titles, application IDs and document names.
    // Indexes describe the current stacking order, not persistent identities.
    for (EffectWindow *w : effects->stackingOrder()) {
        result += QStringLiteral("\nwindow=%1 active=%2 deleted=%3 visible=%4 dock=%5 popup=%6 tooltip=%7 grab=%8 blocking=%9")
            .arg(index++).arg(w == m_activeWindow).arg(w->isDeleted())
            .arg(w->isVisible()).arg(w->isDock()).arg(w->isPopupWindow())
            .arg(w->isTooltip())
            .arg(!w->isDeleted() && w->window() && w->window()->hasPopupGrab())
            .arg(isBlockingPopup(w));
        result += QStringLiteral(" internal=%1 frame=%2 buffer=%3")
            .arg(w->internalWindow() != nullptr)
            .arg(rectText(w->frameGeometry()), rectText(w->bufferGeometry()));
        if (w->isPopupWindow() && !w->isDeleted()) {
            const bool acceptsInput = popupAcceptsInput(w);
            result += QStringLiteral(" popupInput=%1 ownedInteractive=%2")
                .arg(acceptsInput)
                .arg(OutlinePopupFilter::isOwnedInteractivePopup(w, m_activeWindow,
                                                                acceptsInput));
        }
        if (w->isDock() && !w->isDeleted()) {
            QString source;
            const auto region = panelOcclusionRegion(w, &source);
            result += QStringLiteral(" clipSource=%1 clipBounds=%2")
                .arg(source, rectText(QRectF(region.boundingRect())));
        }
    }
    return result + QLatin1Char('\n');
}

void ActiveWindowOutlineEffect::prepareScreenPaint(ScreenPrePaintData &data)
{
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    m_sceneView = qobject_cast<SceneView *>(data.view);
#endif
    if (m_visibilityState.needsSelfHeal(hasVisibleBlockingPopup())) {
        updatePopupBlockingState();
    }

    EffectWindow *w = m_activeWindow;
    const bool showPending = (m_windowShowTimer && m_windowShowTimer->isActive());
    const bool wantsOutline = m_visibilityState.isEffectivelyVisible()
        || m_visibilityState.isTransitionActive(m_visibilityConfig.fadeDuration);
    if (!showPending && shouldOutlineWindow(w) && wantsOutline) {
        if (w->isMinimized() || w->isHiddenByShowDesktop()) {
            // The outline sits in a band just *outside* the window's rectangle.
            // Hiding a window only damages the window rectangle itself, not
            // that surrounding band, so the outline would be left behind as a
            // ghost. Repaint the band once in the same pass so it is cleared.
            if (!m_suppressedWindows.contains(w)) {
                m_suppressedWindows.insert(w);
                data.paint += expandedGeometryFor(w).toAlignedRect();
            }
            return;
        }

        m_suppressedWindows.remove(w);
        // Make sure the border area is part of the region being painted this
        // frame, otherwise our outline would be scissored away.
        if (w->isOnCurrentDesktop()) {
            data.paint += expandedGeometryFor(w).toAlignedRect();
        }
    }

    if (m_delayedHideWindow && m_delayedHideWindow != w && !m_delayedHideWindow->isDeleted()
        && m_delayedHideWindow->isOnCurrentDesktop() && !m_delayedHideWindow->isMinimized()
        && !m_delayedHideWindow->isHiddenByShowDesktop() && m_delayedHideState.isEffectivelyVisible()) {
        data.paint += expandedGeometryFor(m_delayedHideWindow).toAlignedRect();
    }

    if (m_highlightRecentWindow) {
        for (EffectWindow *recent : m_recentWindows) {
            if (recent && recent != w && !recent->isDeleted()
                && recent->isOnCurrentDesktop() && !recent->isMinimized()
                && !recent->isHiddenByShowDesktop()
                && !(m_hideOnFullScreen && recent->isFullScreen())
                && !isBlockingPopup(recent)) {
                data.paint += expandedGeometryFor(recent).toAlignedRect();
            }
        }
    }
}

#ifdef ACTIVE_WINDOW_OUTLINE_PREPAINT_PRESENT_TIME
void ActiveWindowOutlineEffect::prePaintScreen(ScreenPrePaintData &data,
                                            std::chrono::milliseconds presentTime)
{
    prepareScreenPaint(data);
    effects->prePaintScreen(data, presentTime);
}
#else
void ActiveWindowOutlineEffect::prePaintScreen(ScreenPrePaintData &data)
{
    prepareScreenPaint(data);
    effects->prePaintScreen(data);
}
#endif

#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
bool ActiveWindowOutlineEffect::paintScreen(const RenderTarget &renderTarget,
#else
void ActiveWindowOutlineEffect::paintScreen(const RenderTarget &renderTarget,
#endif
                                         const RenderViewport &viewport,
                                         int mask,
                                         const OutlineRegion &deviceRegion,
                                         OutlineOutput *screen)
{
    EffectWindow *w = m_activeWindow;
    const bool showPending = (m_windowShowTimer && m_windowShowTimer->isActive());
    const bool drawOutline = !showPending && shouldOutlineWindow(w) && m_visibilityState.isEffectivelyVisible()
        && !w->isMinimized() && !w->isHiddenByShowDesktop() && w->isOnCurrentDesktop();
    const bool drawDelayed = (m_delayedHideWindow && m_delayedHideWindow != w
        && !m_delayedHideWindow->isDeleted() && !m_delayedHideWindow->isMinimized()
        && !m_delayedHideWindow->isHiddenByShowDesktop() && m_delayedHideWindow->isOnCurrentDesktop()
        && m_delayedHideState.isEffectivelyVisible());
    bool hasRecentToDraw = false;
    if (m_highlightRecentWindow && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked) {
        for (EffectWindow *recent : m_recentWindows) {
            if (recent && recent != w && !recent->isDeleted() && !recent->isMinimized()
                && !recent->isHiddenByShowDesktop() && recent->isOnCurrentDesktop()
                && !(m_hideOnFullScreen && recent->isFullScreen())
                && !isBlockingPopup(recent)) {
                hasRecentToDraw = true;
                break;
            }
        }
    }
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    std::unique_ptr<OutlineCursorOverlay> cursorOverlay;
    // Only defer the cursor for the ordinary output pass. Transformed/offscreen
    // effects own their own cursor handling and must retain their scene filters.
    if ((drawOutline || drawDelayed || hasRecentToDraw) && m_sceneView && m_sceneView->scene() == effects->scene()
        && m_sceneView->logicalOutput() == screen
        && m_sceneView->viewport() == viewport.renderRect()
        && m_sceneView->scale() == viewport.scale()
        && !(mask & PAINT_SCREEN_TRANSFORMED) && !effects->activeFullScreenEffect()) {
        Item *cursor = effects->scene()->cursorItem();
        if (cursor && cursor->isVisible() && m_sceneView->shouldRenderItem(cursor)) {
            cursorOverlay = std::make_unique<OutlineCursorOverlay>(m_sceneView, cursor);
        }
    }
#endif
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
    if (!effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen)) {
        return false;
    }
#else
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
#endif

    const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;
    const qreal sampleInterval = 1.0 / qMax<qreal>(0.1, m_ambilightFrequency);
    const bool timeToSample = m_ambilightSampleNeeded || ((elapsedSec - m_lastAmbilightSampleTime) >= sampleInterval);

    if ((m_ambilightEnabled || m_ensurePerimeterContrast) && m_activeWindow && timeToSample) {
        const QPoint windowCenter = m_activeWindow->frameGeometry().center().toPoint();
        const bool isScreenForWindow = !screen || QRect(screen->geometry()).contains(windowCenter);
        if (isScreenForWindow) {
            if (sampleAmbilightColor(renderTarget, viewport, m_activeWindow)) {
                m_ambilightSampleNeeded = false;
                m_lastAmbilightSampleTime = elapsedSec;
            }
        }
    }

    if (!drawOutline && !drawDelayed && !hasRecentToDraw) {
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        return true;
#else
        return;
#endif
    }

    if (drawDelayed) {
        const QRectF delayedGeo = m_delayedHideWindow->frameGeometry();
        if (!m_outlineCache.contains(m_delayedHideWindow) || m_lastGeometry.value(m_delayedHideWindow) != delayedGeo) {
            cacheWindowOutline(m_delayedHideWindow, delayedGeo);
            m_lastGeometry[m_delayedHideWindow] = delayedGeo;
        }
        renderOutline(renderTarget, viewport, m_outlineCache[m_delayedHideWindow], deviceRegion, m_delayedHideWindow, m_delayedHideState.opacity);
    }

    if (hasRecentToDraw) {
        for (int i = m_recentWindows.size() - 1; i >= 0; --i) {
            EffectWindow *recent = m_recentWindows.at(i);
            if (!recent || recent == w || recent->isDeleted() || recent->isMinimized()
                || recent->isHiddenByShowDesktop() || !recent->isOnCurrentDesktop()
                || (m_hideOnFullScreen && recent->isFullScreen())
                || isBlockingPopup(recent)) {
                continue;
            }

            const QRectF recentGeo = recent->frameGeometry();
            if (!m_outlineCache.contains(recent) || m_lastGeometry.value(recent) != recentGeo) {
                cacheWindowOutline(recent, recentGeo);
                m_lastGeometry[recent] = recentGeo;
            }

            const qreal stepFactor = (m_recentWindowsCount > 1) ? std::max(0.4, 1.0 - i * 0.2) : 1.0;
            const qreal recentOpacity = (m_recentWindowOpacity / 100.0) * stepFactor * m_visibilityState.visibilityOpacity;
            renderOutline(renderTarget, viewport, m_outlineCache[recent], deviceRegion, recent, recentOpacity);
        }
    }

    if (drawOutline) {
        const QRectF currentGeo = w->frameGeometry();
        if (!m_outlineCache.contains(w) || m_lastGeometry.value(w) != currentGeo) {
            cacheWindowOutline(w, currentGeo);
            m_lastGeometry[w] = currentGeo;
        }
        renderOutline(renderTarget, viewport, m_outlineCache[w], deviceRegion, w, m_visibilityState.visibilityOpacity);
    }

#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    if (cursorOverlay) {
        const bool success = cursorOverlay->render(renderTarget, viewport, deviceRegion);
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        return success;
#else
        Q_UNUSED(success);
#endif
    }
#endif
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
    return true;
#endif
}

bool ActiveWindowOutlineEffect::sampleAmbilightColor(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w)
{
    if (!w) {
        return false;
    }

    const QRectF frameGeo = w->frameGeometry();
    if (frameGeo.width() < 10 || frameGeo.height() < 10) {
        return false;
    }

    constexpr int kSampleDim = 32;
    constexpr int kOutsideDim = 8;
    const QSize contentSize(kSampleDim, kSampleDim);

    // Inset sample geometry to strictly sample window content and avoid rounded corners / drop shadows
    // where desktop wallpaper bleed can occur.
    QRectF sampleGeo = w->clientGeometry();
    if (sampleGeo.isEmpty() || sampleGeo.width() < 20 || sampleGeo.height() < 20) {
        sampleGeo = frameGeo;
    }
    const int insetX = std::min(8, static_cast<int>(sampleGeo.width() / 4));
    const int insetY = std::min(8, static_cast<int>(sampleGeo.height() / 4));
    sampleGeo = sampleGeo.adjusted(insetX, insetY, -insetX, -insetY);

    // If running with a software/QImage render target:
    if (renderTarget.image()) {
        const QImage *img = renderTarget.image();
        const QRect windowRect = sampleGeo.toRect();
        const QRect targetRect = windowRect.intersected(img->rect());
        if (targetRect.isEmpty() || targetRect.width() < 10 || targetRect.height() < 10) {
            return false;
        }
        const QImage scaled = img->copy(targetRect).scaled(contentSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGBA8888);
        OutlineAmbilight::Settings settings;
        settings.mode = static_cast<OutlineAmbilight::Mode>(m_ambilightMode);
        settings.saturationLeveling = m_ambilightSaturationLeveling / 100.0;
        settings.fallbackStrength = m_ambilightFallbackStrength / 100.0;
        settings.minBrightness = m_ambilightMinBrightness / 100.0;
        settings.maxBrightness = m_ambilightMaxBrightness / 100.0;
        settings.ensureContrast = m_ensurePerimeterContrast;
        settings.sampleWidth = contentSize.width();
        settings.sampleHeight = contentSize.height();

        QImage outsideCombined;
        if (m_ensurePerimeterContrast) {
            constexpr int margin = 16;
            const int left = static_cast<int>(std::floor(frameGeo.left()));
            const int top = static_cast<int>(std::floor(frameGeo.top()));
            const int right = static_cast<int>(std::ceil(frameGeo.right()));
            const int bottom = static_cast<int>(std::ceil(frameGeo.bottom()));
            const int width = right - left + 1;
            const int height = bottom - top + 1;

            const QRect topSrc = QRect(left, top - margin, width, margin).intersected(img->rect());
            const QRect botSrc = QRect(left, bottom + 1, width, margin).intersected(img->rect());
            const QRect leftSrc = QRect(left - margin, top, margin, height).intersected(img->rect());
            const QRect rightSrc = QRect(right + 1, top, margin, height).intersected(img->rect());

            outsideCombined = QImage(kSampleDim, kOutsideDim, QImage::Format_RGBA8888);
            outsideCombined.fill(0);
            QPainter p(&outsideCombined);
            if (!topSrc.isEmpty()) p.drawImage(QRect(0, 0, 32, 2), img->copy(topSrc));
            if (!botSrc.isEmpty()) p.drawImage(QRect(0, 2, 32, 2), img->copy(botSrc));
            if (!leftSrc.isEmpty()) p.drawImage(QRect(0, 4, 32, 2), img->copy(leftSrc));
            if (!rightSrc.isEmpty()) p.drawImage(QRect(0, 6, 32, 2), img->copy(rightSrc));
            p.end();

            settings.outsideRgba = outsideCombined.constBits();
            settings.outsidePixelCount = kSampleDim * kOutsideDim;
        }

        if (m_ambilightTargetColor.isValid()) {
            float prevH = 0.0f, prevS = 0.0f, prevL = 0.0f;
            m_ambilightTargetColor.getHslF(&prevH, &prevS, &prevL);
            if (prevS > 0.1f) {
                settings.previousHue = prevH;
            }
        }

        QColor newTargetColor;
        if (m_ambilightEnabled) {
            newTargetColor = OutlineAmbilight::processRgbaSamples(scaled.constBits(), contentSize.width() * contentSize.height(), settings);
        } else {
            const QColor baseColor = m_useAccentColor ? qApp->palette().color(QPalette::Highlight) : m_customColor;
            newTargetColor = OutlineAmbilight::enforcePerimeterContrast(baseColor, scaled.constBits(), contentSize.width(), contentSize.height(), settings);
        }
        applyNewAmbilightColor(newTargetColor);
        return true;
    }

    // OpenGL compositing path:
    if (!m_ambilightTexture || m_ambilightTexture->size() != contentSize) {
        m_ambilightTexture = GLTexture::allocate(GL_RGBA8, contentSize);
        if (m_ambilightTexture) {
            m_ambilightFbo = std::make_unique<GLFramebuffer>(m_ambilightTexture.get());
        }
    }

    if (!m_ambilightFbo || !m_ambilightFbo->valid()) {
        return false;
    }

    const Rect windowRect(sampleGeo.toRect());
    const Rect destContentRect(0, 0, kSampleDim, kSampleDim);
    GLFramebuffer *savedFbo = GLFramebuffer::currentFramebuffer();
    const bool blitOk = m_ambilightFbo->blitFromRenderTarget(renderTarget, viewport, windowRect, destContentRect);
    if (!blitOk) {
        if (savedFbo && GLFramebuffer::currentFramebuffer() != savedFbo) {
            GLFramebuffer::pushFramebuffer(savedFbo);
        }
        return false;
    }

    uint8_t windowPixels[kSampleDim * kSampleDim * 4];
    GLFramebuffer::pushFramebuffer(m_ambilightFbo.get());
    glReadPixels(0, 0, kSampleDim, kSampleDim, GL_RGBA, GL_UNSIGNED_BYTE, windowPixels);
    GLFramebuffer::popFramebuffer();

    uint8_t outsidePixels[kSampleDim * kOutsideDim * 4];
    bool hasOutside = false;

    if (m_ensurePerimeterContrast) {
        const QSize outsideSize(kSampleDim, kOutsideDim);
        if (!m_outsideTexture || m_outsideTexture->size() != outsideSize) {
            m_outsideTexture = GLTexture::allocate(GL_RGBA8, outsideSize);
            if (m_outsideTexture) {
                m_outsideFbo = std::make_unique<GLFramebuffer>(m_outsideTexture.get());
            }
        }

        if (m_outsideFbo && m_outsideFbo->valid()) {
            constexpr int margin = 16;
            const QRect viewRect = QRectF(viewport.renderRect()).toRect();
            const int left = static_cast<int>(std::floor(frameGeo.left()));
            const int top = static_cast<int>(std::floor(frameGeo.top()));
            const int right = static_cast<int>(std::ceil(frameGeo.right()));
            const int bottom = static_cast<int>(std::ceil(frameGeo.bottom()));
            const int width = right - left + 1;
            const int height = bottom - top + 1;

            const QRect topSrc = QRect(left, top - margin, width, margin).intersected(viewRect);
            const QRect botSrc = QRect(left, bottom + 1, width, margin).intersected(viewRect);
            const QRect leftSrc = QRect(left - margin, top, margin, height).intersected(viewRect);
            const QRect rightSrc = QRect(right + 1, top, margin, height).intersected(viewRect);

            if (topSrc.width() >= 4 && topSrc.height() >= 1) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, Rect(topSrc), Rect(0, 0, 32, 2));
            }
            if (botSrc.width() >= 4 && botSrc.height() >= 1) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, Rect(botSrc), Rect(0, 2, 32, 2));
            }
            if (leftSrc.width() >= 1 && leftSrc.height() >= 4) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, Rect(leftSrc), Rect(0, 4, 32, 2));
            }
            if (rightSrc.width() >= 1 && rightSrc.height() >= 4) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, Rect(rightSrc), Rect(0, 6, 32, 2));
            }

            GLFramebuffer::pushFramebuffer(m_outsideFbo.get());
            glReadPixels(0, 0, kSampleDim, kOutsideDim, GL_RGBA, GL_UNSIGNED_BYTE, outsidePixels);
            GLFramebuffer::popFramebuffer();
            hasOutside = true;
        }
    }

    if (savedFbo && GLFramebuffer::currentFramebuffer() != savedFbo) {
        GLFramebuffer::pushFramebuffer(savedFbo);
    }

    OutlineAmbilight::Settings settings;
    settings.mode = static_cast<OutlineAmbilight::Mode>(m_ambilightMode);
    settings.saturationLeveling = m_ambilightSaturationLeveling / 100.0;
    settings.fallbackStrength = m_ambilightFallbackStrength / 100.0;
    settings.minBrightness = m_ambilightMinBrightness / 100.0;
    settings.maxBrightness = m_ambilightMaxBrightness / 100.0;
    settings.ensureContrast = m_ensurePerimeterContrast;
    settings.sampleWidth = kSampleDim;
    settings.sampleHeight = kSampleDim;
    if (m_ensurePerimeterContrast && hasOutside) {
        settings.outsideRgba = outsidePixels;
        settings.outsidePixelCount = kSampleDim * kOutsideDim;
    }
    if (m_ambilightTargetColor.isValid()) {
        float prevH = 0.0f, prevS = 0.0f, prevL = 0.0f;
        m_ambilightTargetColor.getHslF(&prevH, &prevS, &prevL);
        if (prevS > 0.1f) {
            settings.previousHue = prevH;
        }
    }

    QColor newTargetColor;
    if (m_ambilightEnabled) {
        newTargetColor = OutlineAmbilight::processRgbaSamples(windowPixels, kSampleDim * kSampleDim, settings);
    } else {
        const QColor baseColor = m_useAccentColor ? qApp->palette().color(QPalette::Highlight) : m_customColor;
        newTargetColor = OutlineAmbilight::enforcePerimeterContrast(baseColor, windowPixels, kSampleDim, kSampleDim, settings);
    }
    applyNewAmbilightColor(newTargetColor);
    return true;
}

void ActiveWindowOutlineEffect::applyNewAmbilightColor(const QColor &newColor)
{
    const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;

    if (!m_ambilightCurrentColor.isValid()) {
        m_ambilightCurrentColor = newColor;
        m_ambilightTargetColor = newColor;
        m_ambilightInTransition = false;
        repaintAllOutlines();
        return;
    }

    if (newColor == m_ambilightTargetColor) {
        return;
    }

    m_ambilightTransitionStartColor = m_ambilightCurrentColor;
    m_ambilightTargetColor = newColor;
    m_ambilightTransitionStartTime = elapsedSec;
    m_ambilightInTransition = true;
    updateAnimationTimer();
    repaintAllOutlines();
}

QColor ActiveWindowOutlineEffect::currentAmbilightColor(qreal elapsedSec)
{
    if (!m_ambilightInTransition) {
        return m_ambilightTargetColor.isValid() ? m_ambilightTargetColor : m_customColor;
    }

    const qreal duration = qMax<qreal>(0.05, m_ambilightDuration);
    const qreal progress = qBound<qreal>(0.0, (elapsedSec - m_ambilightTransitionStartTime) / duration, 1.0);

    const QColor result = OutlineColor::interpolateColors(m_ambilightTransitionStartColor, m_ambilightTargetColor, progress, 0 /* EaseInOut */);

    if (progress >= 1.0) {
        m_ambilightInTransition = false;
        m_ambilightCurrentColor = m_ambilightTargetColor;
    } else {
        m_ambilightCurrentColor = result;
    }

    return result;
}

// Generate a triangle strip between two concentric rounded rectangles.
// 'outer' is the outer rect, 'inner' is the inner rect.
// outerRadius/innerRadius are the corner radii for each.
void ActiveWindowOutlineEffect::generateRoundedRectStrip(
    const QRectF &outer, qreal outerRadius,
    const QRectF &inner, qreal innerRadius,
    qreal scale,
    QList<QVector2D> &verts)
{
    OutlineGeometry::buildRoundedRectStrip(outer, outerRadius, inner, innerRadius, scale, verts);
}

} // namespace KWin

#include "activewindowoutline.moc"
