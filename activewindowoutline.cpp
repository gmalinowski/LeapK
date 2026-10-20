/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline.h"
#include "popupfilter.h"
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <window.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/glvertexbuffer.h>
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
uniform float glitchFrequency;
uniform float glitchIntensity;

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

    if (glitchActive > 0.5) {
        float intensityNorm = glitchIntensity / 100.0;

        // 1. Gentle continuous holographic scanlines moving smoothly at 60 FPS
        float scanline = 0.92 + 0.08 * sin(position0.y * 0.35 - time * (2.8 * glitchSpeed));

        // 2. Slow rolling holographic beam sweep
        float beamCoord = fract(position0.y * 0.0018 - time * (0.05 * glitchSpeed));
        float beam = smoothstep(0.06, 0.0, abs(beamCoord - 0.5));

        // 3. Subtle ambient micro-texture (sub-pixel, keeps the outline straight without snaking/waving)
        float ambientMicro = sin(position0.y * 0.6 + time * (2.5 * glitchSpeed))
                           * cos(position0.y * 1.2 - time * (1.8 * glitchSpeed)) * 0.15;

        // 4. Glitch surge envelope determined by glitchFrequency (how often anomalies trigger)
        float anomalyPhase = time * (0.4 + 0.25 * glitchFrequency);
        float beat = sin(anomalyPhase) * sin(anomalyPhase * 0.618 + 1.5);
        float surge = smoothstep(0.48, 0.95, beat);

        // 5. Brief, sharp holographic micro-jitter during anomaly surges (subtle, 1-1.5px max)
        float jitter = sin(position0.y * 0.8 + time * (16.0 * glitchSpeed))
                     * cos(position0.y * 1.6 - time * (11.0 * glitchSpeed));
        float surgeShift = jitter * surge * (intensityNorm * 1.4);

        float totalShift = ambientMicro * intensityNorm + surgeShift;

        float dynamicDist = distanceToOutline + totalShift;
        float normDist = max(dynamicDist, 0.0) / sigma;
        float dynamicFalloff = exp(-0.5 * normDist * normDist);
        float dynamicMask = smoothstep(-1.0, 0.0, dynamicDist);

        // Subtle holographic luminescence and energy flare
        float energy = 1.0 + intensityNorm * (0.2 * (scanline - 0.92) + 0.35 * beam + 0.7 * surge);
        dynamicFalloff *= energy;

        // Subtle electric cyan/blue ionization shimmer
        vec3 holoEmission = vec3(0.08, 0.18, 0.28) * (intensityNorm * (0.6 * surge + 0.3 * beam));
        vec3 finalRgb = min(vec3(1.0), geometryColor.rgb * energy + holoEmission);

        fragColor = vec4(finalRgb,
                         geometryColor.a * 0.38 * dynamicFalloff * dynamicMask);
    } else {
        float normalized = max(distanceToOutline, 0.0) / sigma;
        float falloff = exp(-0.5 * normalized * normalized);
        float outsideMask = smoothstep(-1.0, 0.0, distanceToOutline);

        fragColor = vec4(geometryColor.rgb,
                         geometryColor.a * 0.38 * falloff * outsideMask);
    }
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

    initShortcuts();
}

ActiveWindowOutlineEffect::~ActiveWindowOutlineEffect()
{
    if (m_animationTimer) {
        m_animationTimer->stop();
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
    m_width = qBound(1, config.readEntry("BorderWidth", 2), 20);
    m_gap = qBound(0, config.readEntry("OutlineGap", 0), 30);
    m_radius = qBound(0, config.readEntry("BorderRadius", 6), 30);
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
    m_breatheSpeed = qMax<qreal>(0.1, config.readEntry("BreatheSpeed", 1.5));
    m_breatheIntensity = qBound(10, config.readEntry("BreatheIntensity", 40), 100);

    m_glitchEnabled = config.readEntry("GlitchEnabled", false);
    m_glitchSpeed = qBound<qreal>(0.1, config.readEntry("GlitchSpeed", 1.0), 5.0);
    m_glitchFrequency = qBound(1, config.readEntry("GlitchFrequency", 3), 10);
    m_glitchIntensity = qBound(10, config.readEntry("GlitchIntensity", 50), 100);

    m_neonFlickerEnabled = config.readEntry("NeonFlickerEnabled", false);
    m_neonFlickerIntensity = qBound(10, config.readEntry("NeonFlickerIntensity", 30), 100);

    m_flashEnabled = config.readEntry("FlashEnabled", false);
    m_flashInterval = qMax<qreal>(0.5, config.readEntry("FlashInterval", 4.0));
    m_flashDuration = qBound<qreal>(0.05, config.readEntry("FlashDuration", 0.15), 1.0);

    m_outlineCache.clear();
    updateAnimationTimer();
    effects->addRepaintFull();
}

bool ActiveWindowOutlineEffect::isAnimationActive() const
{
    return m_outlineEnabled && (m_colorCycleEnabled || m_breatheEnabled
        || m_glitchEnabled || m_neonFlickerEnabled || m_flashEnabled);
}

void ActiveWindowOutlineEffect::updateAnimationTimer()
{
    if (isAnimationActive()) {
        if (!m_animationClock.isValid()) {
            m_animationClock.start();
        }
        if (!m_animationTimer->isActive()) {
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
    if (m_activeWindow && shouldOutlineWindow(m_activeWindow)) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

void ActiveWindowOutlineEffect::parseColorList(const QString &str)
{
    m_colorStops.clear();
    const QStringList parts = str.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QStringList tokens = part.split(QLatin1Char(':'));
        if (tokens.size() >= 1) {
            const QColor col(tokens[0].trimmed());
            qreal hold = 3.0;
            if (tokens.size() >= 2) {
                bool ok = false;
                qreal val = tokens[1].trimmed().toDouble(&ok);
                if (ok && val > 0.0) {
                    hold = val;
                }
            }
            if (col.isValid()) {
                m_colorStops.append({col, hold});
            }
        }
    }
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

        qreal f = progress;
        switch (m_colorCycleEasing) {
        case 0: // Ease In Out (Smoothstep)
            f = progress * progress * (3.0 - 2.0 * progress);
            break;
        case 1: // Linear
            f = progress;
            break;
        case 2: // Ease In
            f = progress * progress;
            break;
        case 3: // Ease Out
            f = progress * (2.0 - progress);
            break;
        }

        const QColor c1 = m_colorStops[m_currentColorIndex].color;
        const QColor c2 = m_colorStops[m_nextColorIndex].color;

        int r = qRound(c1.red() + (c2.red() - c1.red()) * f);
        int g = qRound(c1.green() + (c2.green() - c1.green()) * f);
        int b = qRound(c1.blue() + (c2.blue() - c1.blue()) * f);
        int a = qRound(c1.alpha() + (c2.alpha() - c1.alpha()) * f);

        if (stateElapsed >= transDuration) {
            m_currentColorIndex = m_nextColorIndex;
            m_isHoldingColor = true;
            m_stateStartTime = elapsedSec;
        }

        return QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), qBound(0, a, 255));
    }
}

qreal ActiveWindowOutlineEffect::computeBreatheFactor(qreal elapsedSec) const
{
    if (!m_breatheEnabled) {
        return 1.0;
    }
    const qreal depth = m_breatheIntensity / 100.0;
    const qreal wave = 0.5 + 0.5 * std::sin(2.0 * M_PI * m_breatheSpeed * elapsedSec);
    return 1.0 - depth * (1.0 - wave);
}

qreal ActiveWindowOutlineEffect::computeNeonFactor(qreal elapsedSec) const
{
    if (!m_neonFlickerEnabled) {
        return 1.0;
    }
    const qreal noise = std::fmod(std::sin(elapsedSec * 78.233 + std::sin(elapsedSec * 133.0)) * 43758.5453, 1.0);
    const qreal positiveNoise = std::abs(noise);
    const qreal depth = m_neonFlickerIntensity / 100.0;
    if (positiveNoise < 0.08) {
        return qMax<qreal>(0.1, 1.0 - depth);
    }
    return 1.0 - depth * 0.15 * positiveNoise;
}

qreal ActiveWindowOutlineEffect::computeFlashFactor(qreal elapsedSec) const
{
    if (!m_flashEnabled) {
        return 0.0;
    }
    const qreal interval = qMax<qreal>(0.5, m_flashInterval);
    const qreal phase = std::fmod(elapsedSec, interval);
    if (phase < m_flashDuration) {
        const qreal progress = phase / m_flashDuration;
        return 0.8 * std::exp(-4.0 * progress);
    }
    return 0.0;
}

qreal ActiveWindowOutlineEffect::computeGlitchFactor(qreal elapsedSec) const
{
    if (!m_glitchEnabled) {
        return 1.0;
    }
    const qreal anomalyPhase = elapsedSec * (0.4 + 0.25 * m_glitchFrequency);
    const qreal beat = std::sin(anomalyPhase) * std::sin(anomalyPhase * 0.618 + 1.5);
    if (beat > 0.48) {
        const qreal surge = (beat - 0.48) / 0.52;
        const qreal microJitter = 0.5 + 0.5 * std::sin(elapsedSec * (16.0 * m_glitchSpeed));
        const qreal depth = (m_glitchIntensity / 100.0) * 0.15;
        return 1.0 + depth * surge * (0.7 + 0.3 * microJitter);
    }
    return 1.0;
}

QColor ActiveWindowOutlineEffect::resolveColor() const
{
    QColor color;
    const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;

    if (m_colorCycleEnabled && !m_colorStops.isEmpty()) {
        color = const_cast<ActiveWindowOutlineEffect *>(this)->currentCycleColor(elapsedSec);
    } else if (m_useAccentColor) {
        color = qApp->palette().color(QPalette::Highlight);
        color.setAlphaF(color.alphaF() * m_accentOpacity / 100.0);
    } else {
        color = m_customColor;
    }

    if (m_breatheEnabled || m_neonFlickerEnabled || m_flashEnabled || m_glitchEnabled) {
        const qreal breathe = computeBreatheFactor(elapsedSec);
        const qreal neon = computeNeonFactor(elapsedSec);
        const qreal flash = computeFlashFactor(elapsedSec);
        const qreal glitch = computeGlitchFactor(elapsedSec);

        qreal totalFactor = breathe * neon * glitch + flash;
        totalFactor = qBound<qreal>(0.05, totalFactor, 2.0);

        int a = qRound(color.alpha() * totalFactor);
        const qreal glowBoost = flash + (glitch - 1.0) * 0.8;
        int r = qRound(qMin<qreal>(255.0, color.red() * (1.0 + glowBoost)));
        int g = qRound(qMin<qreal>(255.0, color.green() * (1.0 + glowBoost)));
        int b = qRound(qMin<qreal>(255.0, color.blue() * (1.0 + glowBoost)));

        color = QColor(r, g, b, qBound(0, a, 255));
    }

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

    if (isBlockingPopup(w)) {
        repaintAllOutlines();
    }
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
    if (isBlockingPopup(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (m_activeWindow == w) {
        m_activeWindow = nullptr;
    }
    m_suppressedWindows.remove(w);
    m_lastGeometry.remove(w);
    m_outlineCache.remove(w);
    repaintAllOutlines();
}

void ActiveWindowOutlineEffect::slotWindowHiddenChanged(EffectWindow *w)
{
    if (!isBlockingPopup(w)) {
        return;
    }
    effects->addRepaint(expandedGeometryFor(w));
    repaintAllOutlines();
}

void ActiveWindowOutlineEffect::repaintAllOutlines()
{
    if (shouldOutlineWindow(m_activeWindow)) {
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
                                           EffectWindow *outlinedWindow)
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

    // Draw only in the final screen pass so the existing popup suppression
    // applies to every part of the outline. Floating Plasma panels have a
    // transparent margin inside their frame; their blur/contrast region tracks
    // the actual animated panel background more closely than frameGeometry().
    bool aboveOutlinedWindow = false;
    for (EffectWindow *candidate : effects->stackingOrder()) {
        if (candidate == outlinedWindow) {
            aboveOutlinedWindow = true;
            continue;
        }
        if (aboveOutlinedWindow && candidate->isDock() && !candidate->isDeleted()
            && candidate->isVisible() && !candidate->isMinimized()
            && candidate->isOnCurrentDesktop()
            && !candidate->isHiddenByShowDesktop()) {
            scissorRegion -= viewport.mapToRenderTarget(panelOcclusionRegion(candidate));
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

    const QColor dynamicColor = resolveColor();

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
                const auto box = [scale](const QRectF &rect) {
                    return QVector4D(rect.center().x() * scale,
                                     rect.center().y() * scale,
                                     rect.width() * scale / 2.0,
                                     rect.height() * scale / 2.0);
                };
                shader->setUniform("outerBox", box(cache.outerRect));
                shader->setUniform("innerBox", box(cache.innerRect));
                shader->setUniform("outerRadius", float(cache.outerRadius * scale));
                shader->setUniform("innerRadius", float(cache.innerRadius * scale));
                shader->setUniform("glowRadius", float(m_glowRadius * scale));

                const float elapsedSec = m_animationClock.isValid() ? float(m_animationClock.elapsed() / 1000.0) : 0.0f;
                shader->setUniform("time", elapsedSec);
                shader->setUniform("glitchActive", m_glitchEnabled ? 1.0f : 0.0f);
                shader->setUniform("glitchSpeed", float(m_glitchSpeed));
                shader->setUniform("glitchFrequency", float(m_glitchFrequency));
                shader->setUniform("glitchIntensity", float(m_glitchIntensity));

                glEnable(GL_BLEND);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                vbo->render(scissorRegion, GL_TRIANGLES, true);
            }
        }
    }

    ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                                viewport.projectionMatrix());
    binder.shader()->setColorspaceUniforms(ColorDescription::sRGB,
                                           renderTarget.colorDescription(),
                                           RenderingIntent::Perceptual);

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

        binder.shader()->setUniform(GLShader::ColorUniform::Color, layerColor);
        const bool blended = layerColor.alpha() < 255;
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
    EffectWindow *previous = m_activeWindow;
    if (previous == w) {
        return;
    }

    if (previous) {
        effects->addRepaint(expandedGeometryFor(previous));
        m_suppressedWindows.remove(previous);
        m_lastGeometry.remove(previous);
        m_outlineCache.remove(previous);
    }

    m_activeWindow = w;
    if (shouldOutlineWindow(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
}

void ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged(EffectWindow *w,
                                                            const QRectF &oldGeometry)
{
    if (!shouldOutlineWindow(w) && !isBlockingPopup(w)) {
        return;
    }
    const qreal glowExtent = m_glowEnabled ? m_glowRadius : 0;
    const qreal extent = m_drawInside
        ? glowExtent + 1.0
        : m_width + m_gap + glowExtent + 1.0;
    // Repaint the old location to erase the previous outline, and the new one
    // to draw it. Without setTransformed there is nothing else extending the
    // window's damage out to the border, so we schedule it explicitly.
    effects->addRepaint(oldGeometry.adjusted(-extent, -extent, extent, extent));
    effects->addRepaint(expandedGeometryFor(w));
    if (isBlockingPopup(w)) {
        repaintAllOutlines();
    }
}

void ActiveWindowOutlineEffect::slotToggleAction()
{
    m_outlineEnabled = !m_outlineEnabled;
    updateAnimationTimer();
    if (m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(m_activeWindow));
    }
}

void ActiveWindowOutlineEffect::slotWindowMinimizedChanged(EffectWindow *w)
{
    if (!shouldOutlineWindow(w)) {
        return;
    }
    // Minimizing hides the outline and un-minimizing brings it back; in both
    // cases the border area needs to be repainted.
    effects->addRepaint(expandedGeometryFor(w));
}

void ActiveWindowOutlineEffect::slotWindowFullScreenChanged(EffectWindow *w)
{
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
    const qreal extent = m_drawInside
        ? glowExtent + 1.0
        : m_width + m_gap + glowExtent + 1.0;
    return w->frameGeometry().adjusted(-extent, -extent, extent, extent);
}

bool ActiveWindowOutlineEffect::isActive() const
{
    return shouldOutlineWindow(m_activeWindow);
}

QString ActiveWindowOutlineEffect::debug(const QString &parameter) const
{
    QString result = QStringLiteral("version=" ACTIVE_WINDOW_OUTLINE_VERSION
                                   " build=" ACTIVE_WINDOW_OUTLINE_BUILD_ID
                                   " kwinHeaders=" ACTIVE_WINDOW_OUTLINE_KWIN_VERSION "\n");
    result += QStringLiteral("width=%1 offset=%2 radius=%3 drawInside=%4 smoothEdges=%5 glow=%6 glowRadius=%7 accentOpacity=%8")
        .arg(m_width)
        .arg(m_gap)
        .arg(m_radius)
        .arg(m_drawInside ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_smoothEdges ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_glowEnabled ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_glowRadius)
        .arg(m_accentOpacity);
    if (parameter != QStringLiteral("windows")) {
        return result;
    }
    const auto rectText = [](const QRectF &rect) {
        return QStringLiteral("[%1,%2 %3x%4]")
            .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
    };
    result += QStringLiteral("\nblockingPopup=%1").arg(hasVisibleBlockingPopup());
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
    EffectWindow *w = m_activeWindow;
    if (shouldOutlineWindow(w)) {
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
    const bool drawOutline = shouldOutlineWindow(w) && !hasVisibleBlockingPopup()
        && !w->isMinimized() && !w->isHiddenByShowDesktop() && w->isOnCurrentDesktop();
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    std::unique_ptr<OutlineCursorOverlay> cursorOverlay;
    // Only defer the cursor for the ordinary output pass. Transformed/offscreen
    // effects own their own cursor handling and must retain their scene filters.
    if (drawOutline && m_sceneView && m_sceneView->scene() == effects->scene()
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

    if (!drawOutline) {
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        return true;
#else
        return;
#endif
    }

    const QRectF currentGeo = w->frameGeometry();
    if (!m_outlineCache.contains(w) || m_lastGeometry.value(w) != currentGeo) {
        cacheWindowOutline(w, currentGeo);
        m_lastGeometry[w] = currentGeo;
    }

    renderOutline(renderTarget, viewport, m_outlineCache[w], deviceRegion, w);
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

// Generate a triangle strip between two concentric rounded rectangles.
// 'outer' is the outer rect, 'inner' is the inner rect.
// outerRadius/innerRadius are the corner radii for each.
void ActiveWindowOutlineEffect::generateRoundedRectStrip(
    const QRectF &outer, qreal outerRadius,
    const QRectF &inner, qreal innerRadius,
    qreal scale,
    QList<QVector2D> &verts)
{
    constexpr int cornerSegments = 8;

    // Clamp radii so they don't exceed half the rect dimensions
    outerRadius = qMin(outerRadius, qMin(outer.width(), outer.height()) / 2.0);
    innerRadius = qMin(innerRadius, qMin(inner.width(), inner.height()) / 2.0);

    const QPointF innerCenters[4] = {
        {inner.x() + innerRadius, inner.y() + innerRadius},             // top-left
        {inner.right() - innerRadius, inner.y() + innerRadius},         // top-right
        {inner.right() - innerRadius, inner.bottom() - innerRadius},    // bottom-right
        {inner.x() + innerRadius, inner.bottom() - innerRadius},        // bottom-left
    };
    const QPointF outerCenters[4] = {
        {outer.x() + outerRadius, outer.y() + outerRadius},             // top-left
        {outer.right() - outerRadius, outer.y() + outerRadius},         // top-right
        {outer.right() - outerRadius, outer.bottom() - outerRadius},    // bottom-right
        {outer.x() + outerRadius, outer.bottom() - outerRadius},        // bottom-left
    };

    // Start angles for each corner in screen coordinates (Y points down).
    // Going clockwise: top-left, top-right, bottom-right, bottom-left.
    const qreal startAngles[4] = {M_PI, 1.5 * M_PI, 0.0, 0.5 * M_PI};

    struct PointPair {
        QPointF outer, inner;
    };
    QList<PointPair> ring;

    for (int corner = 0; corner < 4; ++corner) {
        const qreal startAngle = startAngles[corner];
        for (int i = 0; i <= cornerSegments; ++i) {
            const qreal angle = startAngle + (M_PI / 2.0) * i / cornerSegments;
            const qreal cosA = qCos(angle);
            const qreal sinA = qSin(angle);

            PointPair pp;
            pp.inner = innerCenters[corner] + QPointF(innerRadius * cosA, innerRadius * sinA);
            pp.outer = outerCenters[corner] + QPointF(outerRadius * cosA, outerRadius * sinA);
            ring.append(pp);
        }
    }

    // Close the ring by repeating the first pair
    ring.append(ring.first());

    // Convert the ring into triangles (2 per segment)
    for (int i = 0; i < ring.size() - 1; ++i) {
        const auto &a = ring[i];
        const auto &b = ring[i + 1];

        // Triangle 1: a.outer, b.outer, a.inner
        verts.append(QVector2D(a.outer.x() * scale, a.outer.y() * scale));
        verts.append(QVector2D(b.outer.x() * scale, b.outer.y() * scale));
        verts.append(QVector2D(a.inner.x() * scale, a.inner.y() * scale));

        // Triangle 2: b.outer, b.inner, a.inner
        verts.append(QVector2D(b.outer.x() * scale, b.outer.y() * scale));
        verts.append(QVector2D(b.inner.x() * scale, b.inner.y() * scale));
        verts.append(QVector2D(a.inner.x() * scale, a.inner.y() * scale));
    }
}

} // namespace KWin

#include "activewindowoutline.moc"
