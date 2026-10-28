/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk.h"
#include "popupfilter.h"
#include "precedencehelper.h"
#include <QKeyEvent>
#include <QPainter>
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <window.h>
#include <virtualdesktops.h>
#include <QLocale>
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
#include <scene/windowitem.h>
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_OPENGL_SURFACE_TEXTURE
#include <scene/surfaceitem.h>
#include <core/graphicsbuffer.h>
#endif
#endif
#include <KConfigGroup>
#include <QGuiApplication>
#include <QFileInfo>
#include <QDir>
#include <QFileSystemWatcher>
#include <QPalette>
#include <QColorDialog>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>
#include <KGlobalAccel>
#include <QAction>
#include <QRandomGenerator>
#include <QDBusConnection>
#include <QJsonDocument>
#include <QJsonObject>
#include "presethelper.h"
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
uniform float dashActive;
uniform float dashLength;
uniform float dashPeriod;
uniform float dashSmooth;

float roundedBoxDistance(vec2 point, vec4 box, float radius)
{
    vec2 q = abs(point - box.xy) - box.zw + vec2(radius);
    return length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

float computePerimeterDistance(vec2 pt, vec4 box, float r)
{
    vec2 q = pt - box.xy;
    float w0 = max(0.0, box.z - r);
    float h0 = max(0.0, box.w - r);
    float Lx = 2.0 * w0;
    float Ly = 2.0 * h0;
    float Lc = 1.57079632679 * r;

    if (q.x > w0 && q.y < -h0) {
        float angle = atan(q.x - w0, -(q.y + h0));
        return Lx + r * angle;
    }
    if (q.x > w0 && q.y > h0) {
        float angle = atan(q.y - h0, q.x - w0);
        return Lx + Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y > h0) {
        float angle = atan(-(q.x + w0), q.y - h0);
        return 2.0 * Lx + 2.0 * Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y < -h0) {
        float angle = atan(-(q.y + h0), -(q.x + w0));
        return 2.0 * Lx + 3.0 * Lc + 2.0 * Ly + r * angle;
    }

    if (q.y < -h0) {
        return clamp(q.x + w0, 0.0, Lx);
    }
    if (q.x > w0) {
        return Lx + Lc + clamp(q.y + h0, 0.0, Ly);
    }
    if (q.y > h0) {
        return Lx + Lc + Ly + Lc + clamp(w0 - q.x, 0.0, Lx);
    }
    return 2.0 * Lx + 3.0 * Lc + Ly + clamp(h0 - q.y, 0.0, Ly);
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

    if (dashActive > 0.5 && dashPeriod > 0.0) {
        float s = computePerimeterDistance(position0, outerBox, outerRadius);
        float d = mod(s, dashPeriod);
        float edgeDist;
        if (d < dashLength) {
            edgeDist = min(d, dashLength - d);
        } else {
            float gapLength = dashPeriod - dashLength;
            float distFromDash = d - dashLength;
            edgeDist = -min(distFromDash, gapLength - distFromDash);
        }

        float dashAlpha;
        if (dashSmooth > 0.5) {
            float fadeWidth = max(1.0, min(dashLength * 0.5, (dashPeriod - dashLength) * 0.5));
            dashAlpha = smoothstep(-fadeWidth, fadeWidth, edgeDist);
        } else {
            dashAlpha = clamp(edgeDist + 0.5, 0.0, 1.0);
        }

        if (dashAlpha <= 0.001) {
            discard;
        }
        fragColor.a *= dashAlpha;
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
uniform float outerRadius;
uniform float fadeCorners;
uniform float cornerCutoff;
uniform float fadeDistance;
uniform float dashActive;
uniform float dashLength;
uniform float dashPeriod;
uniform float dashSmooth;

float computePerimeterDistance(vec2 pt, vec4 box, float r)
{
    vec2 q = pt - box.xy;
    float w0 = max(0.0, box.z - r);
    float h0 = max(0.0, box.w - r);
    float Lx = 2.0 * w0;
    float Ly = 2.0 * h0;
    float Lc = 1.57079632679 * r;

    if (q.x > w0 && q.y < -h0) {
        float angle = atan(q.x - w0, -(q.y + h0));
        return Lx + r * angle;
    }
    if (q.x > w0 && q.y > h0) {
        float angle = atan(q.y - h0, q.x - w0);
        return Lx + Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y > h0) {
        float angle = atan(-(q.x + w0), q.y - h0);
        return 2.0 * Lx + 2.0 * Lc + Ly + r * angle;
    }
    if (q.x < -w0 && q.y < -h0) {
        float angle = atan(-(q.y + h0), -(q.x + w0));
        return 2.0 * Lx + 3.0 * Lc + 2.0 * Ly + r * angle;
    }

    if (q.y < -h0) {
        return clamp(q.x + w0, 0.0, Lx);
    }
    if (q.x > w0) {
        return Lx + Lc + clamp(q.y + h0, 0.0, Ly);
    }
    if (q.y > h0) {
        return Lx + Lc + Ly + Lc + clamp(w0 - q.x, 0.0, Lx);
    }
    return 2.0 * Lx + 3.0 * Lc + Ly + clamp(h0 - q.y, 0.0, Ly);
}

void main()
{
    float cornerFade = 1.0;
    if (fadeCorners > 0.5) {
        vec2 p = abs(position0 - outerBox.xy);
        vec2 d = outerBox.zw - p;
        float edgeDist = max(d.x, d.y);
        cornerFade = smoothstep(cornerCutoff, cornerCutoff + fadeDistance, edgeDist);
    }

    if (dashActive > 0.5 && dashPeriod > 0.0) {
        float s = computePerimeterDistance(position0, outerBox, outerRadius);
        float d = mod(s, dashPeriod);
        float edgeDist;
        if (d < dashLength) {
            edgeDist = min(d, dashLength - d);
        } else {
            float gapLength = dashPeriod - dashLength;
            float distFromDash = d - dashLength;
            edgeDist = -min(distFromDash, gapLength - distFromDash);
        }

        float dashAlpha;
        if (dashSmooth > 0.5) {
            float fadeWidth = max(1.0, min(dashLength * 0.5, (dashPeriod - dashLength) * 0.5));
            dashAlpha = smoothstep(-fadeWidth, fadeWidth, edgeDist);
        } else {
            dashAlpha = clamp(edgeDist + 0.5, 0.0, 1.0);
        }

        if (dashAlpha <= 0.001) {
            discard;
        }
        fragColor = vec4(geometryColor.rgb, geometryColor.a * cornerFade * dashAlpha);
    } else {
        fragColor = vec4(geometryColor.rgb, geometryColor.a * cornerFade);
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

    m_jumpTimeoutTimer = new QTimer(this);
    m_jumpTimeoutTimer->setSingleShot(true);
    connect(m_jumpTimeoutTimer, &QTimer::timeout, this, &ActiveWindowOutlineEffect::cancelJumpMode);

    m_expectedJumpTargetTimer = new QTimer(this);
    m_expectedJumpTargetTimer->setSingleShot(true);
    connect(m_expectedJumpTargetTimer, &QTimer::timeout, this, [this]() {
        m_expectedJumpTarget = nullptr;
    });

    m_historyCommitTimer = new QTimer(this);
    m_historyCommitTimer->setSingleShot(true);
    connect(m_historyCommitTimer, &QTimer::timeout, this, &ActiveWindowOutlineEffect::slotCommitHistoryNavigation);

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
        connect(w, &EffectWindow::windowDamaged,
                this, &ActiveWindowOutlineEffect::slotWindowDamaged);
    }

    connect(effects, &EffectsHandler::windowActivated,
            this, &ActiveWindowOutlineEffect::slotWindowActivated);
    connect(effects, &EffectsHandler::windowAdded,
            this, &ActiveWindowOutlineEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &ActiveWindowOutlineEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::showingDesktopChanged,
            this, &ActiveWindowOutlineEffect::slotShowingDesktopChanged);
    connect(effects, &EffectsHandler::activeFullScreenEffectChanged,
            this, &ActiveWindowOutlineEffect::slotActiveFullScreenEffectChanged);
    connect(effects, &EffectsHandler::hasActiveFullScreenEffectChanged,
            this, &ActiveWindowOutlineEffect::slotActiveFullScreenEffectChanged);
    connect(effects, &EffectsHandler::stackingOrderChanged,
            this, &ActiveWindowOutlineEffect::repaintAllOutlines);
    connect(effects, &EffectsHandler::desktopChanged,
            this, [this]() {
                repaintAllOutlines();
            });
    connect(effects, &EffectsHandler::screenLockingChanged,
            this, [this](bool locked) {
                if (locked) {
                    if (m_jumpMode) {
                        cancelJumpMode();
                    }
                    m_badgeTextureCache.clear();
                    m_thumbnailCache.clear();
                    m_lastCaptureTimestamp.clear();
                }
                repaintAllOutlines();
            });
    connect(effects, &EffectsHandler::screenAboutToLock,
            this, [this]() {
                if (m_jumpMode) {
                    cancelJumpMode();
                }
                repaintAllOutlines();
            });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(effects, &EffectsHandler::sessionStateChanged,
            this, [this]() {
                if (m_jumpMode) {
                    cancelJumpMode();
                }
                m_badgeTextureCache.clear();
                m_thumbnailCache.clear();
                m_lastCaptureTimestamp.clear();
                repaintAllOutlines();
            });
#endif

    initShortcuts();

    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/LeapK"),
        QStringLiteral("org.kde.LeapK"),
        this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/KLeap"),
        QStringLiteral("org.kde.KLeap"),
        this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );
    QDBusConnection::sessionBus().registerObject(
        QStringLiteral("/ActiveWindowOutline"),
        QStringLiteral("org.kde.ActiveWindowOutline"),
        this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals
    );
}

ActiveWindowOutlineEffect::~ActiveWindowOutlineEffect()
{
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/LeapK"));
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/KLeap"));
    QDBusConnection::sessionBus().unregisterObject(QStringLiteral("/ActiveWindowOutline"));
    if (m_jumpMode) {
        cancelJumpMode();
    }
    if (m_jumpTimeoutTimer) {
        m_jumpTimeoutTimer->stop();
    }
    if (m_expectedJumpTargetTimer) {
        m_expectedJumpTargetTimer->stop();
    }
    if (m_historyCommitTimer) {
        m_historyCommitTimer->stop();
    }
    if (m_previewSafetyTimer && m_previewSafetyTimer->isActive()) {
        m_previewSafetyTimer->stop();
    }
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();

    if (m_animationTimer) {
        m_animationTimer->stop();
    }
    for (EffectWindow *w : m_workspaceMinimizedWindows) {
        if (w && !w->isDeleted() && w->isMinimized()) {
            w->setMinimized(false);
        }
    }
    m_workspaceMinimizedWindows.clear();
    m_stashedWindows.clear();
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
    toggleAction->setText(QStringLiteral("LeapK: Toggle Window Highlight"));
    KGlobalAccel::self()->setDefaultShortcut(toggleAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_O));
    KGlobalAccel::self()->setGlobalShortcut(toggleAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_O));
    connect(toggleAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleAction);

    QAction *markAction = new QAction(this);
    markAction->setObjectName(QStringLiteral("MarkActiveWindow"));
    markAction->setText(QStringLiteral("LeapK: Toggle Mark Window (Quick)"));
    KGlobalAccel::self()->setDefaultShortcut(markAction, QList<QKeySequence>());
    KGlobalAccel::self()->setGlobalShortcut(markAction, QKeySequence());
    connect(markAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotMarkWindowToggle);

    QAction *markWithColorAction = new QAction(this);
    markWithColorAction->setObjectName(QStringLiteral("MarkActiveWindowWithColor"));
    markWithColorAction->setText(QStringLiteral("LeapK: Mark Window with Color Picker"));
    KGlobalAccel::self()->setDefaultShortcut(markWithColorAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_Space));
    KGlobalAccel::self()->setGlobalShortcut(markWithColorAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_Space));
    connect(markWithColorAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotMarkWindowWithColor);

    QAction *workspaceManagerAction = new QAction(this);
    workspaceManagerAction->setObjectName(QStringLiteral("OpenWorkspaceManager"));
    workspaceManagerAction->setText(QStringLiteral("LeapK: Open Workspace Manager"));
    KGlobalAccel::self()->setDefaultShortcut(workspaceManagerAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Space));
    KGlobalAccel::self()->setGlobalShortcut(workspaceManagerAction, QKeySequence(Qt::META | Qt::SHIFT | Qt::Key_Space));
    connect(workspaceManagerAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotOpenWorkspaceManager);

    QAction *toggleGroupAction = new QAction(this);
    toggleGroupAction->setObjectName(QStringLiteral("ToggleHudGrouping"));
    toggleGroupAction->setText(QStringLiteral("LeapK: Toggle HUD Window Grouping"));
    KGlobalAccel::self()->setDefaultShortcut(toggleGroupAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_G));
    KGlobalAccel::self()->setGlobalShortcut(toggleGroupAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_G));
    connect(toggleGroupAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotToggleHudGrouping);

    QAction *jumpAction = new QAction(this);
    jumpAction->setObjectName(QStringLiteral("JumpToWindow"));
    jumpAction->setText(QStringLiteral("LeapK: Jump to Window"));
    KGlobalAccel::self()->setDefaultShortcut(jumpAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::Key_Space));
    KGlobalAccel::self()->setGlobalShortcut(jumpAction, QKeySequence(Qt::META | Qt::Key_Space));
    connect(jumpAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotTriggerJumpMode);

    QAction *prevRecentAction = new QAction(this);
    prevRecentAction->setObjectName(QStringLiteral("PrevRecentWindow"));
    prevRecentAction->setText(QStringLiteral("LeapK: Previous Recent Window"));
    KGlobalAccel::self()->setDefaultShortcut(prevRecentAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_BracketLeft));
    KGlobalAccel::self()->setGlobalShortcut(prevRecentAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_BracketLeft));
    connect(prevRecentAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotPrevRecentWindow);

    QAction *nextRecentAction = new QAction(this);
    nextRecentAction->setObjectName(QStringLiteral("NextRecentWindow"));
    nextRecentAction->setText(QStringLiteral("LeapK: Next Recent Window"));
    KGlobalAccel::self()->setDefaultShortcut(nextRecentAction, QList<QKeySequence>() << QKeySequence(Qt::META | Qt::CTRL | Qt::Key_BracketRight));
    KGlobalAccel::self()->setGlobalShortcut(nextRecentAction, QKeySequence(Qt::META | Qt::CTRL | Qt::Key_BracketRight));
    connect(nextRecentAction, &QAction::triggered, this, &ActiveWindowOutlineEffect::slotNextRecentWindow);
}

bool ActiveWindowOutlineEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void ActiveWindowOutlineEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
    KConfigGroup fallbackConfig = effects->config()->group(QStringLiteral("Effect-active-window-outline"));
    if (!effects->config()->hasGroup(QStringLiteral("Effect-leapk")) && effects->config()->hasGroup(QStringLiteral("Effect-active-window-outline"))) {
        fallbackConfig.copyTo(&primaryConfig);
        primaryConfig.sync();
    }
    KConfigGroup config = primaryConfig.exists() ? primaryConfig : fallbackConfig;
    const bool configOutlineEnabled = config.readEntry("OutlineEnabled", true);
    if (m_outlineEnabled != configOutlineEnabled) {
        slotToggleAction();
    }
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

    m_width = qBound(1, config.readEntry("BorderWidth", 2), 100);
    m_gap = qBound(0, config.readEntry("OutlineGap", 0), 200);
    m_radius = qBound(0, config.readEntry("BorderRadius", 6), 200);
    m_fadeCorners = config.readEntry("FadeCorners", true);
    m_drawInside = config.readEntry("DrawInside", true);
    m_smoothEdges = config.readEntry("SmoothEdges", true);
    m_glowEnabled = config.readEntry("GlowEnabled", false);
    m_glowRadius = qBound(1, config.readEntry("GlowRadius", 6), 200);
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

    m_customShaderEnabled = config.readEntry("CustomShaderEnabled", false);
    m_activeCustomShaderId = config.readEntry("ActiveCustomShaderId", QString());
    m_glowShader.reset();
    m_fadeCornersShader.reset();
    if (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty()) {
        LeapKShader::loadShaderById(m_activeCustomShaderId, m_activeCustomShader);
    }

    m_fadeDuration = qBound(0, config.readEntry("FadeDuration", 200), 2000);
    m_menuHideDelay = qBound(0, config.readEntry("MenuHideDelay", 500), 2000);
    m_menuShowDelay = qBound(0, config.readEntry("MenuShowDelay", 500), 2000);
    m_windowHideDelay = qBound(0, config.readEntry("WindowHideDelay", 0), 2000);
    m_windowShowDelay = qBound(0, config.readEntry("WindowShowDelay", 0), 2000);
    m_highlightRecentWindow = config.readEntry("HighlightRecentWindow", false);
    m_recentWindowsCount = qBound(1, config.readEntry("RecentWindowsCount", 1), 5);
    m_recentWindowColorMode = qBound(0, config.readEntry("RecentWindowColorMode", 0), 1);
    m_recentWindowCustomColor = config.readEntry("RecentWindowCustomColor", QColor(127, 140, 141));
    m_recentWindowOpacity = qBound(5, config.readEntry("RecentWindowOpacity", 20), 100);
    m_recentWindowHueShift = qBound(-180, config.readEntry("RecentWindowHueShift", 0), 180);
    m_recentWindowSaturation = qBound(0, config.readEntry("RecentWindowSaturation", 0), 100);
    m_recentWindowWidthReduction = qBound(0, config.readEntry("RecentWindowWidthReduction", 0), 10);
    m_recentWindowDashed = config.readEntry("RecentWindowDashed", false);
    m_recentWindowGapGrowth = qBound(0, config.readEntry("RecentWindowGapGrowth", 6), 30);
    m_recentWindowDashSmooth = config.readEntry("RecentWindowDashSmooth", false);
    m_markDefaultColor = config.readEntry("MarkDefaultColor", QColor(QStringLiteral("#e74c3c")));
    m_markSmartAdaptive = config.readEntry("MarkSmartAdaptive", true);
    m_markRandomColor = config.readEntry("MarkRandomColor", false);
    m_markInactiveWidthReduction = qBound(0, config.readEntry("MarkInactiveWidthReduction", 1), 10);
    m_markInactiveDashed = config.readEntry("MarkInactiveDashed", true);
    m_markInactiveDashGap = qBound(1, config.readEntry("MarkInactiveDashGap", 6), 40);
    m_markInactiveDashSmooth = config.readEntry("MarkInactiveDashSmooth", true);
    m_stylePrecedence = OutlinePrecedence::parsePrecedence(config.readEntry("StylePrecedence", QStringLiteral("Marked,Active,Recent")));
     m_badgeDisplayMode = qBound(0, config.readEntry("BadgeDisplayMode", 0), 2);
    if (config.hasKey("BadgesAlwaysVisible") && !config.hasKey("BadgeDisplayMode")) {
        if (!config.readEntry("MarkShowLetterBadges", true)) {
            m_badgeDisplayMode = 2;
        } else if (config.readEntry("BadgesAlwaysVisible", false)) {
            m_badgeDisplayMode = 1;
        } else {
            m_badgeDisplayMode = 0;
        }
    }
    m_badgeWindowBadgesEnabled = config.readEntry("BadgeWindowBadgesEnabled", true);
    m_badgeConfig.windowBadgesEnabled = m_badgeWindowBadgesEnabled;
    m_badgePosition = qBound(0, config.readEntry("BadgePosition", 1), 8);
    m_badgeHudPosition = qBound(0, config.readEntry("BadgeHudPosition", 0), 3);
    m_badgeSize = qBound(16, config.readEntry("BadgeSize", 28), 500);
    m_badgeMargin = qBound(0, config.readEntry("BadgeMargin", 0), 500);
    m_badgeConfig.windowShowIcon = config.readEntry("BadgeWindowShowIcon", false);
    m_badgeConfig.windowShowThumbnail = config.readEntry("BadgeWindowShowThumbnail", false);
    m_badgeConfig.windowBadgeSize = m_badgeSize;
    m_badgeConfig.jumpBadgeSize = qBound(16, config.readEntry("BadgeJumpSize", 34), 500);
    m_badgeConfig.windowIconSize = qBound(12, config.readEntry("BadgeWindowIconSize", 20), 500);
    const int defWinThumb = qMax(config.readEntry("BadgeWindowThumbnailWidth", 56), config.readEntry("BadgeWindowThumbnailHeight", 32));
    const int winThumbSize = qBound(16, config.readEntry("BadgeWindowThumbnailSize", defWinThumb), 2500);
    m_badgeConfig.windowThumbnailSize = winThumbSize;
    m_badgeConfig.windowThumbnailWidth = winThumbSize;
    m_badgeConfig.windowThumbnailHeight = winThumbSize;
    m_badgeConfig.windowLetterFontSize = qBound(0, config.readEntry("BadgeWindowLetterFontSize", 0), 200);
    m_badgeConfig.windowLabelFontSize = qBound(0, config.readEntry("BadgeWindowLabelFontSize", 0), 200);

    m_badgeConfig.hudShowIcon = config.readEntry("BadgeHudShowIcon", true);
    m_badgeConfig.hudShowThumbnail = config.readEntry("BadgeHudShowThumbnail", true);
    m_badgeConfig.hudBadgeSize = qBound(24, config.readEntry("BadgeHudSize", 36), 500);
    m_badgeConfig.hudIconSize = qBound(12, config.readEntry("BadgeHudIconSize", 24), 500);
    const int defHudThumb = qMax(config.readEntry("BadgeHudThumbnailWidth", 80), config.readEntry("BadgeHudThumbnailHeight", 48));
    const int hudThumbSize = qBound(24, config.readEntry("BadgeHudThumbnailSize", defHudThumb), 3500);
    m_badgeConfig.hudThumbnailSize = hudThumbSize;
    m_badgeConfig.hudThumbnailWidth = hudThumbSize;
    m_badgeConfig.hudThumbnailHeight = hudThumbSize;
    m_badgeConfig.hudLetterFontSize = qBound(0, config.readEntry("BadgeHudLetterFontSize", 0), 200);
    m_badgeConfig.hudCaptionFontSize = qBound(0, config.readEntry("BadgeHudCaptionFontSize", 0), 200);
    m_badgeConfig.hudLocationFontSize = qBound(0, config.readEntry("BadgeHudLocationFontSize", 0), 200);

    m_badgeHudIncludeAll = config.readEntry("BadgeHudIncludeAll", false);
    m_badgeConfig.hudIncludeAll = m_badgeHudIncludeAll;
    m_badgeHudGroupWindows = config.readEntry("BadgeHudGroupWindows", true);
    m_badgeConfig.hudGroupWindows = m_badgeHudGroupWindows;
    m_badgeHudGroupBorderWidth = qBound<qreal>(0.5, config.readEntry("BadgeHudGroupBorderWidth", 2.0), 20.0);
    m_badgeConfig.hudGroupBorderWidth = m_badgeHudGroupBorderWidth;
    m_badgeHudGroupFontSize = qBound(0, config.readEntry("BadgeHudGroupFontSize", 0), 200);
    m_badgeConfig.hudGroupFontSize = m_badgeHudGroupFontSize;
    m_badgeHudGroupCornerRadius = qBound(0, config.readEntry("BadgeHudGroupCornerRadius", 8), 50);
    m_badgeConfig.hudGroupCornerRadius = m_badgeHudGroupCornerRadius;
    m_badgeHudGroupShaderEnabled = config.readEntry("BadgeHudGroupShaderEnabled", true);
    m_badgeConfig.hudGroupShaderEnabled = m_badgeHudGroupShaderEnabled;
    m_badgeLivePreviews = config.readEntry("BadgeLivePreviews", false);
    m_badgeConfig.livePreviews = m_badgeLivePreviews;
    m_badgeTimeout = qBound(0, config.readEntry("BadgeTimeout", 5), 300);
    m_badgeConfig.badgeTimeout = m_badgeTimeout;
    m_jumpRecentKey = config.readEntry("JumpRecentKey", QStringLiteral("Space"));
    m_badgeConfig.jumpRecentKey = m_jumpRecentKey;

    m_badgeConfig.iconSize = m_badgeConfig.windowIconSize;
    m_badgeConfig.thumbnailSize = m_badgeConfig.windowThumbnailSize;
    m_badgeConfig.thumbnailWidth = m_badgeConfig.windowThumbnailWidth;
    m_badgeConfig.thumbnailHeight = m_badgeConfig.windowThumbnailHeight;
    m_badgeConfig.showIcon = m_badgeConfig.windowShowIcon;
    m_badgeConfig.showThumbnail = m_badgeConfig.windowShowThumbnail;
    m_badgeShowIcon = m_badgeConfig.windowShowIcon;
    m_badgeShowThumbnail = m_badgeConfig.windowShowThumbnail;

    m_badgeConfig.margin = m_badgeMargin;
    m_badgeConfig.cornerRadius = qBound(0, config.readEntry("BadgeCornerRadius", 6), 200);
    m_badgeConfig.opacityPercent = qBound(10, config.readEntry("BadgeOpacity", 92), 100);
    m_badgeConfig.hudPosition = m_badgeHudPosition;
    m_badgeConfig.hudMarkColorMode = qBound(0, config.readEntry("BadgeHudMarkColorMode", 0), 2);
    m_badgeConfig.defaultBgColor = config.readEntry("BadgeDefaultColor", QColor(QStringLiteral("#232629")));
    m_badgeConfig.letterColorAuto = config.readEntry("BadgeLetterColorAuto", true);
    m_badgeConfig.letterColor = config.readEntry("BadgeLetterColor", QColor(QStringLiteral("#ffffff")));
    m_badgeConfig.labelColorAuto = config.readEntry("BadgeLabelColorAuto", true);
    m_badgeConfig.labelUnmarkedColor = config.readEntry("BadgeLabelUnmarkedColor", QColor(QStringLiteral("#f1c40f")));
    m_badgeConfig.captionColorAuto = config.readEntry("BadgeCaptionColorAuto", true);
    m_badgeConfig.captionColor = config.readEntry("BadgeCaptionColor", QColor(QStringLiteral("#dce1e6")));
    m_badgeConfig.locationColorAuto = config.readEntry("BadgeLocationColorAuto", true);
    m_badgeConfig.locationColor = config.readEntry("BadgeLocationColor", QColor(QStringLiteral("#f1c40f")));
    m_badgeConfig.letterFontFamily = config.readEntry("BadgeLetterFont", QString());
    m_badgeConfig.labelFontFamily = config.readEntry("BadgeLabelFont", QString());
    m_badgeConfig.captionFontFamily = config.readEntry("BadgeCaptionFont", QString());
    m_badgeConfig.locationFontFamily = config.readEntry("BadgeLocationFont", QString());
    m_badgeConfig.letterFontSize = qBound(0, config.readEntry("BadgeLetterFontSize", 0), 200);
    m_badgeConfig.labelFontSize = qBound(0, config.readEntry("BadgeLabelFontSize", 0), 200);
    m_badgeConfig.captionFontSize = qBound(0, config.readEntry("BadgeCaptionFontSize", 0), 200);
    m_badgeConfig.locationFontSize = qBound(0, config.readEntry("BadgeLocationFontSize", 0), 200);
    m_markShowLetterBadges = (m_badgeDisplayMode != 2);
    m_badgesAlwaysVisible = (m_badgeDisplayMode == 1);
    m_jumpTargets = qBound(0, config.readEntry("JumpTargets", 0), 3);
    m_activeHudView = config.readEntry("ActiveHudView", 0);
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    if (!m_lastMarkColor.isValid()) {
        m_lastMarkColor = m_markDefaultColor;
    }
    updateRecentWindowsFromHistory();

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

    m_persistentMarks = LeapKMarks::loadPersistentMarks();
    refreshWorkspaces();
    for (EffectWindow *w : effects->stackingOrder()) {
        applyPersistentMarksToWindow(w);
    }

    effects->addRepaintFull();
    Q_EMIT stateChanged();
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
            || m_ambilightInTransition
            || (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty());
    }
    if (m_jumpMode && m_badgeHudGroupShaderEnabled && (m_breatheEnabled || m_neonFlickerEnabled || (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty()))) {
        return true;
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
    const bool isFocus = (w == m_activeWindow);
    const bool isRecentInHistory = (w != nullptr && m_recentWindows.contains(w) && !isFocus);
    const bool isMarked = (w != nullptr && m_markedWindows.contains(w));

    int rankActive = m_stylePrecedence.indexOf(QStringLiteral("Active"));
    int rankMarked = m_stylePrecedence.indexOf(QStringLiteral("Marked"));
    int rankRecent = m_stylePrecedence.indexOf(QStringLiteral("Recent"));
    if (rankActive < 0) rankActive = 0;
    if (rankMarked < 0) rankMarked = 1;
    if (rankRecent < 0) rankRecent = 2;

    const bool activeOverridesMark = isFocus && (rankActive < rankMarked);
    const bool recentOverridesMark = isRecentInHistory && (rankRecent < rankMarked);

    const bool treatAsMarked = isMarked && !activeOverridesMark && !recentOverridesMark;
    const bool treatAsRecent = isRecentInHistory && (!isMarked || recentOverridesMark);

    if (treatAsMarked) {
        color = m_markedWindows.value(w);
    } else if (treatAsRecent && m_recentWindowColorMode == 1) {
        color = m_recentWindowCustomColor;
    } else if (m_colorCycleEnabled && !m_colorStops.isEmpty()) {
        if (treatAsRecent && m_recentCycleColors.contains(w) && m_recentCycleColors.value(w).isValid()) {
            color = m_recentCycleColors.value(w);
        } else {
            color = const_cast<ActiveWindowOutlineEffect *>(this)->currentCycleColor(elapsedSec);
        }
    } else if (m_ambilightEnabled) {
        if (treatAsRecent && m_recentAmbilightColors.contains(w) && m_recentAmbilightColors.value(w).isValid()) {
            color = m_recentAmbilightColors.value(w);
        } else {
            color = const_cast<ActiveWindowOutlineEffect *>(this)->currentAmbilightColor(elapsedSec);
        }
        color.setAlphaF(color.alphaF() * (m_ambilightOpacity / 100.0));
    } else if (m_ensurePerimeterContrast && (treatAsRecent && m_recentAmbilightColors.contains(w) ? m_recentAmbilightColors.value(w).isValid() : m_ambilightCurrentColor.isValid())) {
        if (treatAsRecent && m_recentAmbilightColors.contains(w) && m_recentAmbilightColors.value(w).isValid()) {
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

    if (treatAsRecent) {
        const int recentIdx = m_recentWindows.indexOf(w);
        if (recentIdx >= 0) {
            const bool isCustom = (m_recentWindowColorMode == 1);
            color = OutlineColor::applyRecentWindowStyling(color, recentIdx, m_recentWindowsCount, m_recentWindowHueShift, m_recentWindowSaturation, isCustom);
        }
    }

    if (w == m_activeWindow && !treatAsMarked && (m_breatheEnabled || m_neonFlickerEnabled || m_flashEnabled || m_glitchEnabled)) {
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

void ActiveWindowOutlineEffect::applyPersistentMarksToWindow(EffectWindow *w)
{
    if (!w || w->isDeleted() || m_markedWindows.contains(w) || !isEligibleTargetWindow(w)) {
        return;
    }
    const auto desktops = w->desktops();
    const int desktopNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
    const auto *rule = LeapKMarks::findBestMatchingRule(m_persistentMarks, w->windowClass(), w->caption(), desktopNum, w->pid());
    if (rule) {
        setWindowMark(w, rule->color, rule->letter, rule->label);
    }
}

void ActiveWindowOutlineEffect::refreshPersistentMarksForWindows()
{
    if (m_persistentMarks.isEmpty() || !effects) {
        return;
    }
    const auto windows = effects->stackingOrder();
    for (EffectWindow *w : windows) {
        if (!w || w->isDeleted() || !isEligibleTargetWindow(w)) {
            continue;
        }
        const auto desktops = w->desktops();
        const int desktopNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
        const auto *rule = LeapKMarks::findBestMatchingRule(m_persistentMarks, w->windowClass(), w->caption(), desktopNum, w->pid());
        if (rule) {
            const bool marked = m_markedWindows.contains(w);
            const QChar curLetter = m_markedWindowLetters.value(w);
            const QColor curColor = m_markedWindows.value(w);
            const QString curLabel = m_markedWindowLabels.value(w);
            if (!marked || curLetter != rule->letter || curColor != rule->color || curLabel != rule->label) {
                setWindowMark(w, rule->color, rule->letter, rule->label);
            }
        }
    }
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
    connect(w, &EffectWindow::windowDamaged,
            this, &ActiveWindowOutlineEffect::slotWindowDamaged);

    applyPersistentMarksToWindow(w);

    if (isSystemOverlay(w)) {
        if (m_jumpMode) {
            cancelJumpMode();
        }
        m_badgeTextureCache.clear();
        m_thumbnailCache.clear();
        m_lastCaptureTimestamp.clear();
    }

    updatePopupBlockingState();
    if (m_badgeDisplayMode == 1) {
        effects->addRepaintFull();
    }

    refreshWorkspaces();
    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        if (isEligibleOutlineWindow(w) && !w->isSkipSwitcher() && w->acceptsFocus()) {
            if (w->windowClass().isEmpty() && w->caption().isEmpty()) {
                QPointer<EffectWindow> pw(w);
                const int currentWsView = m_activeHudView;
                QTimer::singleShot(60, this, [this, pw, currentWsView]() {
                    if (pw && !pw->isDeleted() && m_activeHudView == currentWsView && m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
                        addWindowToWorkspace(pw.data(), currentWsView);
                    }
                });
            } else {
                addWindowToWorkspace(w, m_activeHudView);
            }
        }
    }
}

bool ActiveWindowOutlineEffect::isEligibleOutlineWindow(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    if (isSessionLockOrLogoutActive() || isSystemOverlay(w)) {
        return false;
    }
    if (w->isDesktop() || w->isDock() || w->isSpecialWindow() || w->isLockScreen()) {
        return false;
    }
    if (w->isPopupWindow() || w->isTooltip() || w->isNotification()
        || w->isCriticalNotification() || w->isOnScreenDisplay()) {
        return false;
    }
    if (effects && effects->isScreenLocked()) {
        return false;
    }
    return true;
}

bool ActiveWindowOutlineEffect::shouldOutlineWindow(EffectWindow *w) const
{
    return m_outlineEnabled && w && w == m_activeWindow && isEligibleOutlineWindow(w)
        && !(m_hideOnFullScreen && w->isFullScreen())
        && !isBlockingPopup(w)
        && !isSessionLockOrLogoutActive();
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

bool ActiveWindowOutlineEffect::isSystemOverlay(EffectWindow *w) const
{
    if (!w || w->isDeleted()) {
        return false;
    }
    if (w->isLockScreen()) {
        return true;
    }
    const QString cls = w->windowClass();
    if (cls.contains(QStringLiteral("ksmserver-logout-greeter"), Qt::CaseInsensitive)
        || cls.contains(QStringLiteral("logout-greeter"), Qt::CaseInsensitive)
        || cls.contains(QStringLiteral("logoutgreeter"), Qt::CaseInsensitive)
        || cls.contains(QStringLiteral("kscreenlocker"), Qt::CaseInsensitive)) {
        return true;
    }
    const QString role = w->windowRole();
    if (role.contains(QStringLiteral("logout"), Qt::CaseInsensitive)
        || role.contains(QStringLiteral("greeter"), Qt::CaseInsensitive)
        || role.contains(QStringLiteral("lockscreen"), Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

bool ActiveWindowOutlineEffect::isSessionLockOrLogoutActive() const
{
    if (!effects) {
        return false;
    }
    if (effects->isScreenLocked()) {
        return true;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (effects->sessionState() != KWin::SessionState::Normal) {
        return true;
    }
#endif
    for (EffectWindow *w : effects->stackingOrder()) {
        if (isSystemOverlay(w) && w->isVisible()) {
            return true;
        }
    }
    return false;
}

bool ActiveWindowOutlineEffect::hasVisibleBlockingPopup() const
{
    if (isSessionLockOrLogoutActive()) {
        return true;
    }
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
    m_windowHistory.removeAll(w);
    m_recentAmbilightColors.remove(w);
    m_recentCycleColors.remove(w);
    m_smartAccentCache.remove(w);
    m_thumbnailCache.remove(w);
    m_lastCaptureTimestamp.remove(w);
    m_markedWindows.remove(w);
    m_markedWindowLetters.remove(w);
    m_markedWindowLabels.remove(w);
    m_closingWindows.remove(w);
    m_workspaceMinimizedWindows.remove(w);
    m_stashedWindows.remove(w);
    for (auto it = m_jumpLetterToWindow.begin(); it != m_jumpLetterToWindow.end(); ) {
        if (it.value() == w) {
            it = m_jumpLetterToWindow.erase(it);
        } else {
            ++it;
        }
    }
    m_suppressedWindows.remove(w);
    m_lastGeometry.remove(w);
    m_outlineCache.remove(w);

    updatePopupBlockingState();
    updateRecentWindowsFromHistory();
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
    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        if (it.key()) {
            effects->addRepaint(expandedGeometryFor(it.key()));
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
    if (effects && effects->isScreenLocked()) {
        return;
    }
    if (m_badgeDisplayMode == 1 || m_jumpMode) {
        effects->addRepaintFull();
    }
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
    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        EffectWindow *marked = it.key();
        if (marked && !marked->isDeleted()
            && marked->isOnCurrentDesktop() && !marked->isMinimized()) {
            effects->addRepaint(expandedGeometryFor(marked));
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
    OutlineRegion scissorRegion = OutlineRect(
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
    const qreal effectiveWidth = (cache.borderWidth > 0.0) ? cache.borderWidth : m_width;
    const float cornerCutoff = m_drawInside ? qMax<float>(minCutoff, float(effectiveWidth * scale)) : minCutoff;
    const float edgeHalfMin = float(qMin<qreal>(cache.outerRect.width(), cache.outerRect.height()) * scale * 0.5);
    const float maxFade = qMax<float>(15.0f * float(scale), edgeHalfMin - cornerCutoff - 10.0f * float(scale));
    const float fadeDist = std::clamp<float>(120.0f * float(scale), 20.0f * float(scale), maxFade);

    const bool isFocus = (outlinedWindow == m_activeWindow);
    const bool isMarked = (outlinedWindow != nullptr && m_markedWindows.contains(outlinedWindow));
    const bool isRecentInHistory = (outlinedWindow != nullptr && m_recentWindows.contains(outlinedWindow) && !isFocus);

    int rankActive = m_stylePrecedence.indexOf(QStringLiteral("Active"));
    int rankMarked = m_stylePrecedence.indexOf(QStringLiteral("Marked"));
    int rankRecent = m_stylePrecedence.indexOf(QStringLiteral("Recent"));
    if (rankActive < 0) rankActive = 0;
    if (rankMarked < 0) rankMarked = 1;
    if (rankRecent < 0) rankRecent = 2;

    const bool activeOverridesMark = isFocus && (rankActive < rankMarked);
    const bool recentOverridesMark = isRecentInHistory && (rankRecent < rankMarked);

    const bool treatAsMarked = isMarked && !activeOverridesMark && !recentOverridesMark;
    const bool treatAsRecent = isRecentInHistory && (!isMarked || recentOverridesMark);
    const int recentIdx = treatAsRecent ? m_recentWindows.indexOf(outlinedWindow) : -1;

    bool isDashed = false;
    bool isSmooth = false;
    qreal effDash = 12.0 * scale;
    qreal effPeriod = 20.0 * scale;

    if (treatAsMarked && !isFocus) {
        isDashed = m_markInactiveDashed;
        isSmooth = m_markInactiveDashSmooth;
        if (isDashed) {
            const qreal perimeter = OutlineGeometry::computeRoundedRectPerimeter(cache.outerRect, cache.outerRadius, scale);
            OutlineGeometry::computeCustomDashPattern(12.0, m_markInactiveDashGap, perimeter, scale, effDash, effPeriod);
        }
    } else if (treatAsRecent) {
        isDashed = m_recentWindowDashed && (recentIdx >= 0);
        isSmooth = m_recentWindowDashSmooth;
        if (isDashed) {
            const qreal perimeter = OutlineGeometry::computeRoundedRectPerimeter(cache.outerRect, cache.outerRadius, scale);
            OutlineGeometry::computeRecentWindowDashPattern(recentIdx, perimeter, scale, effDash, effPeriod, m_recentWindowGapGrowth);
        }
    }

    const bool customGlow = m_customShaderEnabled && !m_activeCustomShaderId.isEmpty()
        && (m_activeCustomShader.target == QStringLiteral("glow") || m_activeCustomShader.target == QStringLiteral("both"));
    const bool enableGlow = isFocus ? (m_glowEnabled || m_glitchEnabled || customGlow) : (m_glowEnabled || customGlow);

    if (enableGlow && dynamicColor.alpha() > 0) {
        if (!m_glowShader) {
            const EglContext *context = EglContext::currentContext();
            const bool isOpenGLES = !epoxy_is_desktop_gl();
            const bool modern = isOpenGLES
                ? (context ? context->glslVersion() >= Version(3, 0) : true)
                : (context ? context->glslVersion() >= Version(1, 40) : true);

            if (customGlow) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                m_glowShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), LeapKShader::buildGlowFragmentSource(m_activeCustomShader, modern, isOpenGLES));
#else
                m_glowShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), LeapKShader::buildGlowFragmentSource(m_activeCustomShader, modern, isOpenGLES));
#endif
                if (!m_glowShader
#ifndef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
                    || !m_glowShader->isValid()
#endif
                ) {
                    qWarning() << "ActiveWindowOutline: Custom glow shader failed to compile, falling back.";
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                    m_glowShader = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::UniformColor, glowVertexSource(), glowFragmentSource());
#else
                    m_glowShader = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::Border, QByteArray(), glowFragmentSource());
#endif
                }
            } else {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                m_glowShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), glowFragmentSource());
#else
                m_glowShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), glowFragmentSource());
#endif
            }
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
                shader->setUniform("dashActive", isDashed ? 1.0f : 0.0f);
                shader->setUniform("dashLength", float(effDash));
                shader->setUniform("dashPeriod", float(effPeriod));
                shader->setUniform("dashSmooth", isSmooth ? 1.0f : 0.0f);

                const float elapsedSec = m_animationClock.isValid() ? float(m_animationClock.elapsed() / 1000.0) : 0.0f;
                shader->setUniform("time", elapsedSec);
                const bool applyGlitch = m_glitchEnabled && isFocus;
                shader->setUniform("glitchActive", applyGlitch ? 1.0f : 0.0f);
                shader->setUniform("glitchSpeed", float(m_glitchSpeed));
                shader->setUniform("glitchIntensity", float(m_glitchIntensity));
                shader->setUniform("glitchAnomalyActive", (applyGlitch && m_glitchAnomalyEnabled) ? 1.0f : 0.0f);
                shader->setUniform("glitchFrequency", float(m_glitchFrequency));

                if (customGlow) {
                    for (const auto &param : m_activeCustomShader.parameters) {
                        const QByteArray uniformName = param.id.toUtf8();
                        if (param.type == QStringLiteral("float")) {
                            shader->setUniform(uniformName.constData(), float(param.currentValue));
                        } else if (param.type == QStringLiteral("int")) {
                            shader->setUniform(uniformName.constData(), int(param.currentValue));
                        } else if (param.type == QStringLiteral("bool")) {
                            shader->setUniform(uniformName.constData(), param.currentValue > 0.5 ? 1.0f : 0.0f);
                        } else if (param.type == QStringLiteral("color")) {
                            const QColor c(param.colorCurrent);
                            shader->setUniform(uniformName.constData(), QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
                        }
                    }
                }

                glEnable(GL_BLEND);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                    GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                vbo->render(scissorRegion, GL_TRIANGLES, true);
            }
        }
    }

    const bool customOutline = m_customShaderEnabled && !m_activeCustomShaderId.isEmpty()
        && (m_activeCustomShader.target == QStringLiteral("outline") || m_activeCustomShader.target == QStringLiteral("both"));
    const bool useCustomShader = m_fadeCorners || isDashed || customOutline;
    if (useCustomShader) {
        if (!m_fadeCornersShader) {
            const EglContext *context = EglContext::currentContext();
            const bool isOpenGLES = !epoxy_is_desktop_gl();
            const bool modern = isOpenGLES
                ? (context ? context->glslVersion() >= Version(3, 0) : true)
                : (context ? context->glslVersion() >= Version(1, 40) : true);

            if (customOutline) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), LeapKShader::buildOutlineFragmentSource(m_activeCustomShader, modern, isOpenGLES));
#else
                m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), LeapKShader::buildOutlineFragmentSource(m_activeCustomShader, modern, isOpenGLES));
#endif
                if (!m_fadeCornersShader
#ifndef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
                    || !m_fadeCornersShader->isValid()
#endif
                ) {
                    qWarning() << "ActiveWindowOutline: Custom outline shader failed to compile, falling back.";
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                    m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::UniformColor, glowVertexSource(), outlineFragmentSource());
#else
                    m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::Border, QByteArray(), outlineFragmentSource());
#endif
                }
            } else {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), outlineFragmentSource());
#else
                m_fadeCornersShader = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), outlineFragmentSource());
#endif
            }
        }
    }

    const bool useCustomFade = useCustomShader &&
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
        activeShader->setUniform("outerRadius", float(cache.outerRadius * scale));
        activeShader->setUniform("fadeCorners", m_fadeCorners ? 1.0f : 0.0f);
        activeShader->setUniform("cornerCutoff", cornerCutoff);
        activeShader->setUniform("fadeDistance", fadeDist);
        activeShader->setUniform("dashActive", isDashed ? 1.0f : 0.0f);
        activeShader->setUniform("dashLength", float(effDash));
        activeShader->setUniform("dashPeriod", float(effPeriod));
        activeShader->setUniform("dashSmooth", isSmooth ? 1.0f : 0.0f);

        if (customOutline) {
            const float elapsedSec = m_animationClock.isValid() ? float(m_animationClock.elapsed() / 1000.0) : 0.0f;
            activeShader->setUniform("time", elapsedSec);
            for (const auto &param : m_activeCustomShader.parameters) {
                const QByteArray uniformName = param.id.toUtf8();
                if (param.type == QStringLiteral("float")) {
                    activeShader->setUniform(uniformName.constData(), float(param.currentValue));
                } else if (param.type == QStringLiteral("int")) {
                    activeShader->setUniform(uniformName.constData(), int(param.currentValue));
                } else if (param.type == QStringLiteral("bool")) {
                    activeShader->setUniform(uniformName.constData(), param.currentValue > 0.5 ? 1.0f : 0.0f);
                } else if (param.type == QStringLiteral("color")) {
                    const QColor c(param.colorCurrent);
                    activeShader->setUniform(uniformName.constData(), QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
                }
            }
        }
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

void ActiveWindowOutlineEffect::cacheWindowOutline(EffectWindow *w, const QRectF &geo, qreal customWidth)
{
    OutlineCache cache;

    const qreal minDimension = qMin(geo.width(), geo.height());
    const qreal gap = m_drawInside
        ? qMin<qreal>(m_gap, qMax<qreal>(0.0, minDimension / 2.0 - 1.0))
        : m_gap;
    const qreal maxInsideWidth = qMax<qreal>(0.0, minDimension / 2.0 - gap);
    const qreal baseW = (customWidth > 0.0) ? customWidth : m_width;
    const qreal bw = m_drawInside ? qMin<qreal>(baseW, maxInsideWidth) : baseW;
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
    cache.borderWidth = bw;

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
    if (isBlockingPopup(w) || isSystemOverlay(w)) {
        if (isSystemOverlay(w) && m_jumpMode) {
            cancelJumpMode();
        }
        return;
    }

    if (m_expectedJumpTarget) {
        if (!w || w->isDeleted()) {
            return;
        }
        if (w != m_expectedJumpTarget) {
            // Intermediate window activation caused by virtual desktop switch before target window is activated. Ignore!
            return;
        }
        m_expectedJumpTarget = nullptr;
        if (m_expectedJumpTargetTimer && m_expectedJumpTargetTimer->isActive()) {
            m_expectedJumpTargetTimer->stop();
        }
    }

    if (m_isHistoryNavigating) {
        if (!w || w->isDeleted()) {
            return;
        }
        if (m_historyNavIndex >= 0 && m_historyNavIndex < m_windowHistory.size()
            && w == m_windowHistory.at(m_historyNavIndex)) {
            m_activeWindow = w;
            updateRecentWindowsFromHistory();
            repaintAllOutlines();
            return;
        }
        if (m_historyCommitTimer && m_historyCommitTimer->isActive()) {
            return;
        }
        m_isHistoryNavigating = false;
    }

    EffectWindow *previous = m_activeWindow;
    if (previous == w) {
        return;
    }

    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        if (isEligibleOutlineWindow(w) && !w->isSkipSwitcher() && w->acceptsFocus()) {
            const auto &ws = m_workspaces.at(m_activeHudView - 1);
            const auto desktops = w->desktops();
            const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
            if (!LeapKWorkspace::matchesWorkspace(ws, w->windowClass(), w->caption(), dNum)
                && !m_workspaceMinimizedWindows.contains(w)
                && !m_stashedWindows.contains(w)) {
                addWindowToWorkspace(w, m_activeHudView);
            }
        }
    }

    if (m_ambilightEnabled || m_ensurePerimeterContrast) {
        m_ambilightSampleNeeded = true;
    }

    if (m_windowShowTimer && m_windowShowTimer->isActive()) {
        m_windowShowTimer->stop();
    }

    if (previous && isEligibleOutlineWindow(previous)
        && !(m_hideOnFullScreen && previous->isFullScreen()) && !isBlockingPopup(previous)) {
        m_windowHistory.removeAll(previous);
        m_windowHistory.prepend(previous);
        if (m_highlightRecentWindow) {
            if (m_ambilightCurrentColor.isValid()) {
                m_recentAmbilightColors[previous] = m_ambilightCurrentColor;
            }
            if (m_colorCycleEnabled && !m_colorStops.isEmpty()) {
                const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;
                m_recentCycleColors[previous] = currentCycleColor(elapsedSec);
            }
        }
    }

    if (w && isEligibleOutlineWindow(w)) {
        m_windowHistory.removeAll(w);
        m_windowHistory.prepend(w);
        m_recentAmbilightColors.remove(w);
        m_recentCycleColors.remove(w);
        m_activeWindow = w;
    } else {
        m_activeWindow = nullptr;
    }

    m_historyNavIndex = 0;
    while (m_windowHistory.size() > 50) {
        m_windowHistory.removeLast();
    }
    updateRecentWindowsFromHistory();

    if (m_highlightRecentWindow) {
        if (previous && isEligibleOutlineWindow(previous)) {
            effects->addRepaint(expandedGeometryFor(previous));
            m_outlineCache.remove(previous);
            m_lastGeometry.remove(previous);
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
        if (previous && isEligibleOutlineWindow(previous)) {
            if (m_markedWindows.contains(previous)) {
                m_outlineCache.remove(previous);
                m_lastGeometry.remove(previous);
                effects->addRepaint(expandedGeometryFor(previous));
            } else {
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
    }

    if (w) {
        m_outlineCache.remove(w);
        m_lastGeometry.remove(w);
        if (m_markedWindows.contains(w)) {
            effects->addRepaint(expandedGeometryFor(w));
        }
    }

    if (!m_activeWindow) {
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

    if (m_badgeDisplayMode == 1) {
        effects->addRepaintFull();
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
    const bool isMarked = m_markedWindows.contains(w);
    const bool hasContinuousBadges = (m_badgeDisplayMode == 1) && (w != m_activeWindow) && !w->isDeleted() && w->isOnCurrentDesktop();
    if (!shouldOutlineWindow(w) && !isBlockingPopup(w) && m_delayedHideWindow != w && !isRecent && !isMarked && !hasContinuousBadges) {
        return;
    }
    const bool hasGlow = (w == m_activeWindow) ? (m_glowEnabled || m_glitchEnabled) : m_glowEnabled;
    const qreal glowExtent = hasGlow ? m_glowRadius : 0;
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
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-leapk"));
    config.writeEntry("OutlineEnabled", m_outlineEnabled);
    config.sync();

    if (m_outlineEnabled) {
        if (m_visibilityConfig.fadeDuration > 0) {
            m_visibilityState.visibilityOpacity = 0.0;
        }
        m_visibilityState.targetOpacity = hasVisibleBlockingPopup() ? 0.0 : 1.0;
        updateAnimationTimer();
        repaintAllOutlines();
        effects->addRepaintFull();
    } else {
        if (m_jumpMode) {
            cancelJumpMode();
        }
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
        if (m_delayedHideWindow) {
            effects->addRepaint(expandedGeometryFor(m_delayedHideWindow));
            m_suppressedWindows.remove(m_delayedHideWindow);
            m_lastGeometry.remove(m_delayedHideWindow);
            m_outlineCache.remove(m_delayedHideWindow);
            m_delayedHideWindow = nullptr;
        }
        m_delayedHideState.reset();
        m_outlineCache.clear();
        m_badgeTextureCache.clear();
        m_thumbnailCache.clear();
        m_lastCaptureTimestamp.clear();
        repaintAllOutlines();
        effects->addRepaintFull();
    }
    Q_EMIT stateChanged();
}

void ActiveWindowOutlineEffect::toggleOutline()
{
    slotToggleAction();
}

void ActiveWindowOutlineEffect::setOutlineEnabled(bool enabled)
{
    if (m_outlineEnabled != enabled) {
        slotToggleAction();
    }
}

bool ActiveWindowOutlineEffect::isOutlineEnabled() const
{
    return m_outlineEnabled;
}

void ActiveWindowOutlineEffect::toggleAmbilight()
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-leapk"));
    const bool current = config.readEntry("AmbilightEnabled", false);
    config.writeEntry("AmbilightEnabled", !current);
    config.sync();
    reconfigure(ReconfigureAll);
    Q_EMIT stateChanged();
}

bool ActiveWindowOutlineEffect::isAmbilightEnabled() const
{
    return m_ambilightEnabled;
}

void ActiveWindowOutlineEffect::toggleGlow()
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-leapk"));
    const bool current = config.readEntry("GlowEnabled", false);
    config.writeEntry("GlowEnabled", !current);
    config.sync();
    reconfigure(ReconfigureAll);
    Q_EMIT stateChanged();
}

bool ActiveWindowOutlineEffect::isGlowEnabled() const
{
    return m_glowEnabled;
}

void ActiveWindowOutlineEffect::triggerJumpMode()
{
    slotTriggerJumpMode();
}

void ActiveWindowOutlineEffect::toggleWindowMark()
{
    slotMarkWindowToggle();
}

void ActiveWindowOutlineEffect::markWindowWithColor()
{
    slotMarkWindowWithColor();
}

void ActiveWindowOutlineEffect::openWorkspaceManager()
{
    slotOpenWorkspaceManager();
}

void ActiveWindowOutlineEffect::clearAllMarks()
{
    m_markedWindows.clear();
    m_markedWindowLetters.clear();
    m_markedWindowLabels.clear();
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    repaintAllOutlines();
    effects->addRepaintFull();
    Q_EMIT stateChanged();
}

void ActiveWindowOutlineEffect::prevRecentWindow()
{
    slotPrevRecentWindow();
}

void ActiveWindowOutlineEffect::nextRecentWindow()
{
    slotNextRecentWindow();
}

void ActiveWindowOutlineEffect::applyPreset(const QString &presetName)
{
    const auto presets = OutlinePreset::loadPresets(OutlinePreset::defaultPresetsDirectory());
    for (const auto &p : presets) {
        if (p.name == presetName) {
            // Apply embedded shader bundle if available
            if (!p.shader.isEmpty()) {
                LeapKShader::applyShaderBundle(p.shader);
            } else if (p.settings.contains(QStringLiteral("ShaderEffect"))) {
                LeapKShader::applyShaderBundle(p.settings.value(QStringLiteral("ShaderEffect")).toObject());
            }

            KConfigGroup group = effects->config()->group(QStringLiteral("Effect-leapk"));
            for (auto it = p.settings.constBegin(); it != p.settings.constEnd(); ++it) {
                if (it.key() == QStringLiteral("ShaderEffect")) continue;
                group.writeEntry(it.key(), it.value().toVariant());
            }
            group.sync();
            reconfigure(ReconfigureAll);
            Q_EMIT stateChanged();
            break;
        }
    }
}

static void parseBadgeConfigFromJson(OutlineBadge::BadgeConfig &cfg, int &badgePos, int &hudPos, const QJsonObject &obj)
{
    if (obj.contains(QStringLiteral("BadgePosition"))) {
        badgePos = qBound(0, obj.value(QStringLiteral("BadgePosition")).toInt(1), 8);
    }
    if (obj.contains(QStringLiteral("BadgeHudPosition"))) {
        hudPos = qBound(0, obj.value(QStringLiteral("BadgeHudPosition")).toInt(0), 3);
        cfg.hudPosition = hudPos;
    }
    if (obj.contains(QStringLiteral("BadgeMargin"))) {
        cfg.margin = qBound(0, obj.value(QStringLiteral("BadgeMargin")).toInt(0), 500);
    }
    if (obj.contains(QStringLiteral("BadgeCornerRadius"))) {
        cfg.cornerRadius = qBound(0, obj.value(QStringLiteral("BadgeCornerRadius")).toInt(6), 200);
    }
    if (obj.contains(QStringLiteral("BadgeOpacity"))) {
        cfg.opacityPercent = qBound(10, obj.value(QStringLiteral("BadgeOpacity")).toInt(92), 100);
    }

    if (obj.contains(QStringLiteral("BadgeWindowBadgesEnabled"))) {
        cfg.windowBadgesEnabled = obj.value(QStringLiteral("BadgeWindowBadgesEnabled")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeWindowShowIcon"))) {
        cfg.windowShowIcon = obj.value(QStringLiteral("BadgeWindowShowIcon")).toBool(false);
    }
    if (obj.contains(QStringLiteral("BadgeWindowShowThumbnail"))) {
        cfg.windowShowThumbnail = obj.value(QStringLiteral("BadgeWindowShowThumbnail")).toBool(false);
    }
    if (obj.contains(QStringLiteral("BadgeSize"))) {
        cfg.windowBadgeSize = qBound(16, obj.value(QStringLiteral("BadgeSize")).toInt(28), 500);
    }
    if (obj.contains(QStringLiteral("BadgeJumpSize"))) {
        cfg.jumpBadgeSize = qBound(16, obj.value(QStringLiteral("BadgeJumpSize")).toInt(34), 500);
    }
    if (obj.contains(QStringLiteral("BadgeWindowIconSize"))) {
        cfg.windowIconSize = qBound(12, obj.value(QStringLiteral("BadgeWindowIconSize")).toInt(20), 500);
    }
    if (obj.contains(QStringLiteral("BadgeWindowThumbnailSize"))) {
        const int s = qBound(16, obj.value(QStringLiteral("BadgeWindowThumbnailSize")).toInt(56), 2500);
        cfg.windowThumbnailSize = s;
        cfg.windowThumbnailWidth = s;
        cfg.windowThumbnailHeight = s;
    }
    if (obj.contains(QStringLiteral("BadgeWindowLetterFontSize"))) {
        cfg.windowLetterFontSize = qBound(0, obj.value(QStringLiteral("BadgeWindowLetterFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeWindowLabelFontSize"))) {
        cfg.windowLabelFontSize = qBound(0, obj.value(QStringLiteral("BadgeWindowLabelFontSize")).toInt(0), 200);
    }

    if (obj.contains(QStringLiteral("BadgeHudShowIcon"))) {
        cfg.hudShowIcon = obj.value(QStringLiteral("BadgeHudShowIcon")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeHudShowThumbnail"))) {
        cfg.hudShowThumbnail = obj.value(QStringLiteral("BadgeHudShowThumbnail")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeHudSize"))) {
        cfg.hudBadgeSize = qBound(24, obj.value(QStringLiteral("BadgeHudSize")).toInt(36), 500);
    }
    if (obj.contains(QStringLiteral("BadgeHudIconSize"))) {
        cfg.hudIconSize = qBound(12, obj.value(QStringLiteral("BadgeHudIconSize")).toInt(24), 500);
    }
    if (obj.contains(QStringLiteral("BadgeHudThumbnailSize"))) {
        const int s = qBound(24, obj.value(QStringLiteral("BadgeHudThumbnailSize")).toInt(80), 3500);
        cfg.hudThumbnailSize = s;
        cfg.hudThumbnailWidth = s;
        cfg.hudThumbnailHeight = s;
    }
    if (obj.contains(QStringLiteral("BadgeHudLetterFontSize"))) {
        cfg.hudLetterFontSize = qBound(0, obj.value(QStringLiteral("BadgeHudLetterFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeHudCaptionFontSize"))) {
        cfg.hudCaptionFontSize = qBound(0, obj.value(QStringLiteral("BadgeHudCaptionFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeHudLocationFontSize"))) {
        cfg.hudLocationFontSize = qBound(0, obj.value(QStringLiteral("BadgeHudLocationFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeHudIncludeAll"))) {
        cfg.hudIncludeAll = obj.value(QStringLiteral("BadgeHudIncludeAll")).toBool(false);
    }
    if (obj.contains(QStringLiteral("BadgeLivePreviews"))) {
        cfg.livePreviews = obj.value(QStringLiteral("BadgeLivePreviews")).toBool(false);
    }
    if (obj.contains(QStringLiteral("BadgeTimeout"))) {
        cfg.badgeTimeout = qBound(0, obj.value(QStringLiteral("BadgeTimeout")).toInt(5), 300);
    }
    if (obj.contains(QStringLiteral("JumpRecentKey"))) {
        cfg.jumpRecentKey = obj.value(QStringLiteral("JumpRecentKey")).toString(QStringLiteral("Space"));
    }
    if (obj.contains(QStringLiteral("BadgeHudMarkColorMode"))) {
        cfg.hudMarkColorMode = qBound(0, obj.value(QStringLiteral("BadgeHudMarkColorMode")).toInt(0), 2);
    }

    if (obj.contains(QStringLiteral("BadgeDefaultColor"))) {
        cfg.defaultBgColor = QColor(obj.value(QStringLiteral("BadgeDefaultColor")).toString(QStringLiteral("#232629")));
    }
    if (obj.contains(QStringLiteral("BadgeLetterColorAuto"))) {
        cfg.letterColorAuto = obj.value(QStringLiteral("BadgeLetterColorAuto")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeLetterColor"))) {
        cfg.letterColor = QColor(obj.value(QStringLiteral("BadgeLetterColor")).toString(QStringLiteral("#ffffff")));
    }
    if (obj.contains(QStringLiteral("BadgeLabelColorAuto"))) {
        cfg.labelColorAuto = obj.value(QStringLiteral("BadgeLabelColorAuto")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeLabelUnmarkedColor"))) {
        cfg.labelUnmarkedColor = QColor(obj.value(QStringLiteral("BadgeLabelUnmarkedColor")).toString(QStringLiteral("#f1c40f")));
    }
    if (obj.contains(QStringLiteral("BadgeCaptionColorAuto"))) {
        cfg.captionColorAuto = obj.value(QStringLiteral("BadgeCaptionColorAuto")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeCaptionColor"))) {
        cfg.captionColor = QColor(obj.value(QStringLiteral("BadgeCaptionColor")).toString(QStringLiteral("#dce1e6")));
    }
    if (obj.contains(QStringLiteral("BadgeLocationColorAuto"))) {
        cfg.locationColorAuto = obj.value(QStringLiteral("BadgeLocationColorAuto")).toBool(true);
    }
    if (obj.contains(QStringLiteral("BadgeLocationColor"))) {
        cfg.locationColor = QColor(obj.value(QStringLiteral("BadgeLocationColor")).toString(QStringLiteral("#f1c40f")));
    }

    if (obj.contains(QStringLiteral("BadgeLetterFont"))) {
        cfg.letterFontFamily = obj.value(QStringLiteral("BadgeLetterFont")).toString();
    }
    if (obj.contains(QStringLiteral("BadgeLabelFont"))) {
        cfg.labelFontFamily = obj.value(QStringLiteral("BadgeLabelFont")).toString();
    }
    if (obj.contains(QStringLiteral("BadgeCaptionFont"))) {
        cfg.captionFontFamily = obj.value(QStringLiteral("BadgeCaptionFont")).toString();
    }
    if (obj.contains(QStringLiteral("BadgeLocationFont"))) {
        cfg.locationFontFamily = obj.value(QStringLiteral("BadgeLocationFont")).toString();
    }

    if (obj.contains(QStringLiteral("BadgeLetterFontSize"))) {
        cfg.letterFontSize = qBound(0, obj.value(QStringLiteral("BadgeLetterFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeLabelFontSize"))) {
        cfg.labelFontSize = qBound(0, obj.value(QStringLiteral("BadgeLabelFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeCaptionFontSize"))) {
        cfg.captionFontSize = qBound(0, obj.value(QStringLiteral("BadgeCaptionFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeLocationFontSize"))) {
        cfg.locationFontSize = qBound(0, obj.value(QStringLiteral("BadgeLocationFontSize")).toInt(0), 200);
    }
}

void ActiveWindowOutlineEffect::setBadgePreview(bool active, bool showWindowBadges, bool showHud, const QString &jsonConfig)
{
    if (!active || (!showWindowBadges && !showHud)) {
        clearBadgePreview();
        return;
    }

    m_badgePreviewActive = true;
    m_previewShowWindowBadges = showWindowBadges;
    m_previewShowHud = showHud;

    m_previewBadgeConfig = m_badgeConfig;
    m_previewBadgePosition = m_badgePosition;
    m_previewBadgeHudPosition = m_badgeHudPosition;

    if (!jsonConfig.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(jsonConfig.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            parseBadgeConfigFromJson(m_previewBadgeConfig, m_previewBadgePosition, m_previewBadgeHudPosition, doc.object());
            m_badgeLivePreviews = m_previewBadgeConfig.livePreviews;
        }
    }

    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();

    if (!m_previewSafetyTimer) {
        m_previewSafetyTimer = std::make_unique<QTimer>();
        m_previewSafetyTimer->setSingleShot(true);
        connect(m_previewSafetyTimer.get(), &QTimer::timeout, this, [this]() {
            clearBadgePreview();
        });
    }
    m_previewSafetyTimer->start(60000);

    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::clearBadgePreview()
{
    if (!m_badgePreviewActive) {
        return;
    }
    m_badgePreviewActive = false;
    m_previewShowWindowBadges = false;
    m_previewShowHud = false;
    if (m_previewSafetyTimer && m_previewSafetyTimer->isActive()) {
        m_previewSafetyTimer->stop();
    }
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::renderBadgePreview(const RenderTarget &renderTarget,
                                                   const RenderViewport &viewport)
{
    if (!m_badgePreviewActive || (!m_previewShowWindowBadges && !m_previewShowHud)) {
        return;
    }
    if (isSessionLockOrLogoutActive()) {
        return;
    }

    const auto windows = effects->stackingOrder();
    QList<EffectWindow *> targetWindows;
    for (EffectWindow *w : windows) {
        if (!isEligibleTargetWindow(w)) {
            continue;
        }
        if (m_closingWindows.contains(w)) {
            continue;
        }
        targetWindows.append(w);
    }

    if (targetWindows.isEmpty()) {
        return;
    }

    static const QString kLetters = QStringLiteral("ASDFJKLGHQWERUIOPZXCVBNM1234567890");
    QMap<EffectWindow *, QChar> winLetters;
    for (int i = 0; i < targetWindows.size(); ++i) {
        EffectWindow *w = targetWindows.at(i);
        QChar ch = (i < kLetters.size()) ? kLetters.at(i) : QChar('?');
        winLetters[w] = ch;
    }

    const QRectF screenRect = viewport.renderRect();

    QList<QPair<QChar, EffectWindow *>> hudTargets;
    QList<QRectF> hudRects;
    if (m_previewShowHud) {
        for (EffectWindow *target : targetWindows) {
            if (!target || target->isDeleted()) {
                continue;
            }
            hudTargets.append(qMakePair(winLetters.value(target, 'A'), target));
        }
        if (!hudTargets.isEmpty()) {
            hudRects = computeHudPillRects(hudTargets, viewport, m_previewBadgeConfig.hudBadgeSize, m_previewBadgeConfig);
        }
    }

    // 1. Render Window Badges if enabled
    if (m_previewShowWindowBadges && m_previewBadgeConfig.windowBadgesEnabled) {
        EffectWindow *recentWin = findRecentWindow();
        if (!recentWin && !targetWindows.isEmpty()) {
            recentWin = targetWindows.first();
        }
        const int badgeH = m_previewBadgeConfig.windowBadgeSize;
        for (EffectWindow *target : targetWindows) {
            if (!target || target->isDeleted() || !target->isOnCurrentDesktop()
                || target->isMinimized() || !target->isVisible()
                || (m_hideOnFullScreen && target->isFullScreen())) {
                continue;
            }

            const QRectF geo = target->frameGeometry();
            const QRectF visibleWindowGeo = geo.intersected(screenRect);
            if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50) {
                continue;
            }

            const bool isRecent = (target == recentWin);
            const QString recentKey = isRecent ? m_previewBadgeConfig.jumpRecentKey : QString();
            const QColor bgCol = m_markedWindows.contains(target)
                ? m_markedWindows.value(target)
                : (isRecent ? (m_recentWindowCustomColor.isValid() ? m_recentWindowCustomColor : QColor(52, 152, 219)) : m_previewBadgeConfig.defaultBgColor);
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);
            const QString customLabel = m_markedWindowLabels.value(target);
            const qreal badgeW = OutlineBadge::computeBadgeWidth(m_previewBadgeConfig, badgeH, customLabel, recentKey);
            QRectF badgeRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, m_previewBadgePosition, badgeW, badgeH, m_previewBadgeConfig.margin);
            if (badgeRect.left() < screenRect.left() + 4.0) badgeRect.moveLeft(screenRect.left() + 4.0);
            if (badgeRect.right() > screenRect.right() - 4.0) badgeRect.moveRight(screenRect.right() - 4.0);
            if (badgeRect.top() < screenRect.top() + 4.0) badgeRect.moveTop(screenRect.top() + 4.0);
            if (badgeRect.bottom() > screenRect.bottom() - 4.0) badgeRect.moveBottom(screenRect.bottom() - 4.0);

            if (isBadgeOccluded(target, badgeRect, hudRects)) {
                const int altPositions[] = { 4 /* Center */, 1 /* Top */, 0 /* TopLeft */, 2 /* TopRight */, 7 /* Bottom */, 6 /* BottomLeft */, 8 /* BottomRight */, 3 /* Left */, 5 /* Right */ };
                bool foundVisiblePosition = false;
                for (int altPos : altPositions) {
                    if (altPos == m_previewBadgePosition) continue;
                    QRectF altRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, altPos, badgeW, badgeH, m_previewBadgeConfig.margin);
                    if (altRect.left() < screenRect.left() + 4.0) altRect.moveLeft(screenRect.left() + 4.0);
                    if (altRect.right() > screenRect.right() - 4.0) altRect.moveRight(screenRect.right() - 4.0);
                    if (altRect.top() < screenRect.top() + 4.0) altRect.moveTop(screenRect.top() + 4.0);
                    if (altRect.bottom() > screenRect.bottom() - 4.0) altRect.moveBottom(screenRect.bottom() - 4.0);

                    if (!isBadgeOccluded(target, altRect, hudRects)) {
                        badgeRect = altRect;
                        foundVisiblePosition = true;
                        break;
                    }
                }
                if (!foundVisiblePosition) {
                    continue;
                }
            }

            renderBadge(renderTarget, viewport, badgeRect, winLetters.value(target, 'A'), bgCol, textCol, false, badgeH, customLabel, target, recentKey, isRecent);
        }
    }

    // 2. Render HUD Badges if enabled
    if (m_previewShowHud && !hudTargets.isEmpty()) {
        renderOffscreenJumpTargets(renderTarget, viewport, hudTargets, m_previewBadgeConfig.hudBadgeSize);
    }
}

void ActiveWindowOutlineEffect::setWindowMark(EffectWindow *w, const QColor &color, const QChar &letter, const QString &label)
{
    if (!isEligibleOutlineWindow(w) || !color.isValid()) {
        return;
    }
    m_markedWindows[w] = color;
    m_lastMarkColor = color;

    QChar finalLetter = letter.toUpper();
    if (finalLetter.isNull() || !finalLetter.isLetterOrNumber()) {
        m_markedWindowLetters.remove(w);
    } else {
        m_markedWindowLetters[w] = finalLetter;
    }

    const QString cleanLabel = label.trimmed();
    if (cleanLabel.isEmpty()) {
        m_markedWindowLabels.remove(w);
    } else {
        m_markedWindowLabels[w] = cleanLabel;
    }

    effects->addRepaint(expandedGeometryFor(w));
    if (m_badgeDisplayMode == 1 || m_jumpMode) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::setWindowMarkColor(EffectWindow *w, const QColor &color)
{
    setWindowMark(w, color, m_markedWindowLetters.value(w), m_markedWindowLabels.value(w));
}

QChar ActiveWindowOutlineEffect::nextAvailableLetter() const
{
    QList<QChar> used;
    for (auto it = m_markedWindowLetters.constBegin(); it != m_markedWindowLetters.constEnd(); ++it) {
        used.append(it.value());
    }
    return OutlineBadge::findNextAvailableLetter(used);
}

QColor ActiveWindowOutlineEffect::generateRandomMarkColor() const
{
    return OutlineBadge::generateRandomMarkColor(m_markedWindows.values());
}

QColor ActiveWindowOutlineEffect::detectSmartAccentForWindow(EffectWindow *w) const
{
    if (!w) {
        return generateRandomMarkColor();
    }

    QColor candidate;

    // 1. Cached live Smart Adaptive color sampled from window pixels
    if (m_smartAccentCache.contains(w) && m_smartAccentCache.value(w).isValid()) {
        candidate = m_smartAccentCache.value(w);
    }

    // 2. Extract smart adaptive accent from window icon using OutlineBadge helper
    if (!candidate.isValid()) {
        const QIcon icon = w->icon();
        if (!icon.isNull()) {
            const QPixmap pix = icon.pixmap(QSize(32, 32));
            if (!pix.isNull()) {
                const QColor iconColor = OutlineBadge::extractSmartAccentFromImage(
                    pix.toImage(),
                    m_ambilightSaturationLeveling / 100.0,
                    m_ambilightFallbackStrength / 100.0,
                    m_ambilightMinBrightness / 100.0,
                    m_ambilightMaxBrightness / 100.0);
                if (iconColor.isValid() && iconColor.saturationF() > 0.10) {
                    m_smartAccentCache[w] = iconColor;
                    candidate = iconColor;
                }
            }
        }
    }

    // 3. Window live ambilight color
    if (!candidate.isValid()) {
        if (w == m_activeWindow && m_ambilightCurrentColor.isValid()) {
            candidate = m_ambilightCurrentColor;
        } else if (m_recentAmbilightColors.contains(w) && m_recentAmbilightColors.value(w).isValid()) {
            candidate = m_recentAmbilightColors.value(w);
        }
    }

    // Check candidate against existing marked window colors to prevent duplicate/colliding accents (e.g. repeated Tuxedo green)
    const auto existingColors = m_markedWindows.values();
    if (candidate.isValid()) {
        bool collides = false;
        for (const QColor &used : existingColors) {
            if (OutlineBadge::isColorTooClose(candidate, used)) {
                collides = true;
                break;
            }
        }
        if (!collides) {
            return candidate;
        }

        // Collision detected: try shifting hue in steps of 60 degrees to find a contrasting distinct hue
        if (candidate.saturationF() > 0.15) {
            for (int step = 1; step <= 5; ++step) {
                const int shiftedHue = (candidate.hue() + step * 60) % 360;
                const QColor shifted = QColor::fromHsv(shiftedHue, candidate.saturation(), candidate.value());
                bool shiftCollision = false;
                for (const QColor &used : existingColors) {
                    if (OutlineBadge::isColorTooClose(shifted, used)) {
                        shiftCollision = true;
                        break;
                    }
                }
                if (!shiftCollision) {
                    return shifted;
                }
            }
        }
    }

    // 4. Fallback: random mark color guaranteed not to collide with used colors
    return generateRandomMarkColor();
}

void ActiveWindowOutlineEffect::toggleWindowMark(EffectWindow *w)
{
    if (!isEligibleOutlineWindow(w)) {
        return;
    }
    if (m_markedWindows.contains(w)) {
        m_markedWindows.remove(w);
        m_markedWindowLetters.remove(w);
        m_markedWindowLabels.remove(w);
        effects->addRepaint(expandedGeometryFor(w));
    } else {
        const QColor markCol = m_markedWindows.contains(w)
            ? m_markedWindows.value(w)
            : (m_markRandomColor
                ? generateRandomMarkColor()
                : (m_markSmartAdaptive
                    ? detectSmartAccentForWindow(w)
                    : (m_lastMarkColor.isValid() ? m_lastMarkColor : m_markDefaultColor)));
        const QChar letter = nextAvailableLetter();
        setWindowMark(w, markCol, letter);
    }
}

void ActiveWindowOutlineEffect::slotMarkWindowToggle()
{
    EffectWindow *w = effects->activeWindow();
    if (!isEligibleOutlineWindow(w)) {
        w = m_activeWindow;
    }
    if (isEligibleOutlineWindow(w)) {
        toggleWindowMark(w);
    }
}

void ActiveWindowOutlineEffect::slotMarkWindowWithColor()
{
    EffectWindow *target = effects->activeWindow();
    if (!isEligibleOutlineWindow(target)) {
        target = m_activeWindow;
    }
    if (!isEligibleOutlineWindow(target)) {
        return;
    }

    QPointer<EffectWindow> targetWindow(target);
    const QColor smartCol = detectSmartAccentForWindow(target);
    const QColor initialColor = m_markedWindows.contains(target)
        ? m_markedWindows.value(target)
        : (m_markRandomColor
            ? generateRandomMarkColor()
            : ((m_markSmartAdaptive && smartCol.isValid())
                ? smartCol
                : (m_lastMarkColor.isValid() ? m_lastMarkColor : m_markDefaultColor)));
    const QChar initialLetter = m_markedWindowLetters.contains(target)
        ? m_markedWindowLetters.value(target)
        : nextAvailableLetter();
    const QString initialLabel = m_markedWindowLabels.value(target);

    const auto applyMark = [this, targetWindow](const QColor &pickedCol, const QChar &pickedLetter, const QString &pickedLabel, bool useColor = true, bool useLetter = true) {
        if (!targetWindow) {
            return;
        }
        EffectWindow *w = targetWindow.data();
        const QColor markCol = (useColor && pickedCol.isValid())
            ? pickedCol
            : (m_markedWindows.contains(w)
                ? m_markedWindows.value(w)
                : (m_markRandomColor
                    ? generateRandomMarkColor()
                    : (m_markSmartAdaptive
                        ? detectSmartAccentForWindow(w)
                        : (m_lastMarkColor.isValid() ? m_lastMarkColor : m_markDefaultColor))));

        const QChar letter = (useLetter && pickedLetter.isLetterOrNumber())
            ? pickedLetter.toUpper()
            : QChar();

        setWindowMark(w, markCol, letter, pickedLabel);
    };

    // 1. Dedicated Qt6 color picker helper (natively supports alpha channel, letter badge, and custom label)
    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString customPicker = QStandardPaths::findExecutable(QStringLiteral("leapk-custom-marker"), candidateDirs);
    if (customPicker.isEmpty()) {
        customPicker = QStandardPaths::findExecutable(QStringLiteral("leapk-colorpicker"), candidateDirs);
    }
    if (customPicker.isEmpty()) {
        customPicker = QStandardPaths::findExecutable(QStringLiteral("kleap-custom-marker"), candidateDirs);
    }
    if (customPicker.isEmpty()) {
        customPicker = QStandardPaths::findExecutable(QStringLiteral("kleap-colorpicker"), candidateDirs);
    }
    if (customPicker.isEmpty()) {
        customPicker = QStandardPaths::findExecutable(QStringLiteral("active-window-outline-colorpicker"), candidateDirs);
    }
    if (!customPicker.isEmpty()) {
        QProcess *process = new QProcess(this);
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [this, process, applyMark, targetWindow](int exitCode, QProcess::ExitStatus exitStatus) {
            process->deleteLater();
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    QString colorStr;
                    QString letterStr;
                    QString labelStr;
                    bool persist = true;
                    bool groupByApp = true;
                    QString matchCaption;

                    int matchDesktop = 0;
                    bool reassign = false;

                    if (out.startsWith(QLatin1Char('{'))) {
                        const QJsonDocument doc = QJsonDocument::fromJson(out.toUtf8());
                        if (doc.isObject()) {
                            const QJsonObject obj = doc.object();
                            if (obj.value(QStringLiteral("action")).toString() == QStringLiteral("remove")) {
                                const bool removePersistent = obj.value(QStringLiteral("removePersistent")).toBool(true);
                                if (targetWindow) {
                                    EffectWindow *w = targetWindow.data();
                                    m_markedWindows.remove(w);
                                    m_markedWindowLetters.remove(w);
                                    m_markedWindowLabels.remove(w);
                                    if (removePersistent) {
                                        const auto desktops = w->desktops();
                                        const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
                                        for (int i = m_persistentMarks.size() - 1; i >= 0; --i) {
                                            const auto &r = m_persistentMarks.at(i);
                                            if (r.matchApp.compare(w->windowClass(), Qt::CaseInsensitive) == 0) {
                                                if (r.matchDesktop == 0 || r.matchDesktop == dNum) {
                                                    m_persistentMarks.removeAt(i);
                                                }
                                            }
                                        }
                                        LeapKMarks::savePersistentMarks(m_persistentMarks);
                                    }
                                    if (effects) {
                                        effects->addRepaint(expandedGeometryFor(w));
                                        effects->addRepaintFull();
                                    }
                                }
                                return;
                            }
                            colorStr = obj.value(QStringLiteral("color")).toString();
                            letterStr = obj.value(QStringLiteral("letter")).toString();
                            labelStr = obj.value(QStringLiteral("label")).toString();
                            persist = obj.value(QStringLiteral("persist")).toBool(true);
                            groupByApp = obj.value(QStringLiteral("groupByApp")).toBool(true);
                            matchCaption = obj.value(QStringLiteral("matchCaption")).toString().trimmed();
                            matchDesktop = obj.value(QStringLiteral("matchDesktop")).toInt(0);
                            reassign = obj.value(QStringLiteral("reassign")).toBool(false);
                        }
                    } else {
                        // Legacy space-separated fallback
                        const int firstSpace = out.indexOf(QLatin1Char(' '));
                        if (firstSpace != -1) {
                            colorStr = out.left(firstSpace).trimmed();
                            const int secondSpace = out.indexOf(QLatin1Char(' '), firstSpace + 1);
                            if (secondSpace != -1) {
                                letterStr = out.mid(firstSpace + 1, secondSpace - firstSpace - 1).trimmed();
                                const int thirdSpace = out.lastIndexOf(QLatin1Char(' '));
                                if (thirdSpace > secondSpace) {
                                    labelStr = out.mid(secondSpace + 1, thirdSpace - secondSpace - 1).trimmed();
                                    const QString persistStr = out.mid(thirdSpace + 1).trimmed();
                                    persist = (persistStr == QStringLiteral("1") || persistStr == QStringLiteral("true"));
                                } else {
                                    labelStr = out.mid(secondSpace + 1).trimmed();
                                }
                            } else {
                                letterStr = out.mid(firstSpace + 1).trimmed();
                            }
                        }
                    }

                    if (labelStr == QStringLiteral("-")) {
                        labelStr.clear();
                    }

                    const bool useColor = (colorStr.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0
                                        && colorStr.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0
                                        && colorStr != QStringLiteral("-"));
                    const bool useLetter = (!letterStr.isEmpty()
                                         && letterStr.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0
                                         && letterStr.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0
                                         && letterStr != QStringLiteral("-"));

                    const QColor picked = useColor ? QColor(colorStr) : QColor();
                    const QChar pickedLetter = useLetter ? letterStr.at(0).toUpper() : QChar();

                    applyMark(picked, pickedLetter, labelStr, useColor, useLetter);

                    if (reassign && targetWindow && !pickedLetter.isNull()) {
                        EffectWindow *w = targetWindow.data();

                        // Reassign: if any other window belonging to a DIFFERENT application held this letter, release it
                        if (effects) {
                            for (auto it = m_markedWindowLetters.begin(); it != m_markedWindowLetters.end(); ) {
                                EffectWindow *other = it.key();
                                if (other && other != w && it.value() == pickedLetter) {
                                    if (other->windowClass().compare(w->windowClass(), Qt::CaseInsensitive) != 0) {
                                        it = m_markedWindowLetters.erase(it);
                                        effects->addRepaint(expandedGeometryFor(other));
                                        continue;
                                    }
                                }
                                ++it;
                            }
                        }

                        // Also clean up any persistent mark rule for a different app that claimed this letter
                        for (int i = m_persistentMarks.size() - 1; i >= 0; --i) {
                            const auto &r = m_persistentMarks.at(i);
                            if (r.letter == pickedLetter && r.matchApp.compare(w->windowClass(), Qt::CaseInsensitive) != 0) {
                                m_persistentMarks.removeAt(i);
                            }
                        }

                        if (groupByApp) {
                            // Apply mark to all open windows of this application matching matchCaption
                            if (effects) {
                                const auto allWindows = effects->stackingOrder();
                                for (EffectWindow *other : allWindows) {
                                    if (other && other != w && isEligibleTargetWindow(other)) {
                                        if (other->windowClass().compare(w->windowClass(), Qt::CaseInsensitive) == 0) {
                                            if (matchCaption.isEmpty() || other->caption().contains(matchCaption, Qt::CaseInsensitive)) {
                                                setWindowMark(other, picked.isValid() ? picked : m_markDefaultColor, pickedLetter, labelStr);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        if (persist) {
                            LeapKMarks::PersistentMarkRule rule;
                            rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                            rule.letter = pickedLetter;
                            rule.color = picked.isValid() ? picked : m_markDefaultColor;
                            rule.label = labelStr;
                            rule.matchApp = w->windowClass();
                            rule.matchCaption = matchCaption;
                            rule.matchDesktop = matchDesktop;
                            rule.enabled = true;
                            rule.createdAt = QDateTime::currentMSecsSinceEpoch();

                            bool updated = false;
                            for (auto &r : m_persistentMarks) {
                                if (r.matchApp.compare(rule.matchApp, Qt::CaseInsensitive) == 0
                                    && r.matchCaption.compare(rule.matchCaption, Qt::CaseInsensitive) == 0
                                    && r.matchDesktop == rule.matchDesktop
                                    && r.letter == rule.letter) {
                                    r.color = rule.color;
                                    r.label = rule.label;
                                    updated = true;
                                    break;
                                }
                            }
                            if (!updated) {
                                m_persistentMarks.append(rule);
                            }
                            LeapKMarks::savePersistentMarks(m_persistentMarks);
                        }

                        if (effects) {
                            effects->addRepaintFull();
                        }
                    }
                }
            }
        });
        connect(process, &QProcess::errorOccurred, this, [process]() {
            process->deleteLater();
        });

        const auto desktops = target->desktops();
        const int desktopNum = (!desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
        const QString desktopName = (!desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->name() : QString();
        const bool isAlreadyMarked = m_markedWindows.contains(target);

        QJsonObject usedLettersObj;
        for (auto it = m_markedWindowLetters.constBegin(); it != m_markedWindowLetters.constEnd(); ++it) {
            EffectWindow *ew = it.key();
            const QChar ch = it.value().toUpper();
            if (ew && !ch.isNull()) {
                QString app = ew->windowClass();
                if (app.isEmpty()) {
                    app = ew->caption().left(25);
                }
                usedLettersObj[QString(ch)] = app;
            }
        }
        for (const auto &r : m_persistentMarks) {
            if (!r.letter.isNull() && !r.matchApp.isEmpty()) {
                const QString chStr = QString(r.letter).toUpper();
                if (!usedLettersObj.contains(chStr)) {
                    usedLettersObj[chStr] = r.matchApp;
                }
            }
        }

        QStringList args;
        args << initialColor.name(QColor::HexArgb) << QString(initialLetter);
        if (isAlreadyMarked) {
            args << (m_markedWindowLetters.contains(target) ? QStringLiteral("1") : QStringLiteral("0"));
            args << QStringLiteral("1");
            args << initialLabel;
        } else {
            args << QStringLiteral("1");
            args << QStringLiteral("1");
            args << initialLabel;
        }
        args << QStringLiteral("-1"); // Pass -1 to preserve saved persistence state from QSettings
        args << target->windowClass();
        args << target->caption();
        args << QString::number(desktopNum);
        args << desktopName;
        args << (isAlreadyMarked ? QStringLiteral("1") : QStringLiteral("0"));
        args << QString::fromUtf8(QJsonDocument(usedLettersObj).toJson(QJsonDocument::Compact));
        process->start(customPicker, args);
        return;
    }

    // 2. Zenity color picker with alpha support
    const QString zenityPath = QStandardPaths::findExecutable(QStringLiteral("zenity"));
    if (!zenityPath.isEmpty()) {
        QProcess *process = new QProcess(this);
        QStringList args;
        args << QStringLiteral("--color-selection")
             << QStringLiteral("--show-palette")
             << QStringLiteral("--color=%1").arg(initialColor.name(QColor::HexArgb))
             << QStringLiteral("--title") << tr("Select Window Mark Color");

        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [process, applyMark, initialLetter, initialLabel](int exitCode, QProcess::ExitStatus exitStatus) {
            process->deleteLater();
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    applyMark(QColor(out), initialLetter, initialLabel, true, true);
                }
            }
        });
        connect(process, &QProcess::errorOccurred, this, [process]() {
            process->deleteLater();
        });
        process->start(zenityPath, args);
        return;
    }

    // 3. KDialog fallback
    const QString kdialogPath = QStandardPaths::findExecutable(QStringLiteral("kdialog"));
    if (!kdialogPath.isEmpty()) {
        QProcess *process = new QProcess(this);
        QStringList args;
        args << QStringLiteral("--getcolor")
             << QStringLiteral("--default") << initialColor.name(QColor::HexArgb)
             << QStringLiteral("--title") << tr("Select Window Mark Color");

        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this, [process, applyMark, initialLetter, initialLabel](int exitCode, QProcess::ExitStatus exitStatus) {
            process->deleteLater();
            if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                const QString out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
                if (!out.isEmpty()) {
                    applyMark(QColor(out), initialLetter, initialLabel, true, true);
                }
            }
        });
        connect(process, &QProcess::errorOccurred, this, [process]() {
            process->deleteLater();
        });
        process->start(kdialogPath, args);
        return;
    }

    // 4. Non-modal QColorDialog with ShowAlphaChannel fallback
    QColorDialog *dialog = new QColorDialog(initialColor);
    dialog->setOption(QColorDialog::ShowAlphaChannel, true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Select Window Mark Color"));
    connect(dialog, &QColorDialog::colorSelected, this, [applyMark, initialLetter, initialLabel](const QColor &color) {
        applyMark(color, initialLetter, initialLabel, true, true);
    });
    dialog->show();
}

void ActiveWindowOutlineEffect::refreshWorkspaces()
{
    m_workspaces = LeapKWorkspace::loadWorkspaces();
    const int totalViews = qMax(1, 3 + m_workspaces.size());
    if (m_activeHudView >= totalViews || m_activeHudView < 0) {
        m_activeHudView = 0;
    }
}

void ActiveWindowOutlineEffect::slotOpenWorkspaceManager()
{
    EffectWindow *target = effects ? effects->activeWindow() : nullptr;
    if (!isEligibleOutlineWindow(target)) {
        target = m_activeWindow;
    }

    QStringList candidateDirs;
    candidateDirs << QDir::homePath() + QStringLiteral("/.local/bin");
#ifdef COLORPICKER_INSTALL_DIR
    candidateDirs << QStringLiteral(COLORPICKER_INSTALL_DIR);
#endif
    candidateDirs << QCoreApplication::applicationDirPath()
                  << QStringLiteral("/usr/libexec")
                  << QStringLiteral("/usr/lib/libexec")
                  << QStringLiteral("/usr/local/bin")
                  << QStringLiteral("/usr/bin");

    QString wsManagerExe = QStandardPaths::findExecutable(QStringLiteral("leapk-workspace-manager"), candidateDirs);
    if (wsManagerExe.isEmpty()) {
        wsManagerExe = QStandardPaths::findExecutable(QStringLiteral("kleap-workspace-manager"), candidateDirs);
    }
    if (wsManagerExe.isEmpty()) {
        wsManagerExe = QStandardPaths::findExecutable(QStringLiteral("active-window-outline-workspace-manager"), candidateDirs);
    }

    if (wsManagerExe.isEmpty()) {
        return;
    }

    refreshWorkspaces();

    const auto desktops = target ? target->desktops() : QList<VirtualDesktop *>();
    const int desktopNum = (target && !desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
    const QString desktopName = (target && !desktops.isEmpty() && !target->isOnAllDesktops()) ? desktops.first()->name() : QString();
    const QString appClass = target ? target->windowClass() : QString();
    const QString caption = target ? target->caption() : QString();

    QString assignedWsId;
    for (const auto &ws : m_workspaces) {
        if (LeapKWorkspace::matchesWorkspace(ws, appClass, caption, desktopNum)) {
            assignedWsId = ws.id;
            break;
        }
    }

    QStringList args;
    args << appClass;
    args << caption;
    args << QString::number(desktopNum);
    args << desktopName;
    args << assignedWsId;

    QProcess *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            refreshWorkspaces();
            if (effects) {
                effects->addRepaintFull();
            }
            if (m_jumpMode) {
                m_jumpLetterToWindow = assignJumpLetters();
                m_badgeTextureCache.clear();
            }
        }
    });
    connect(process, &QProcess::errorOccurred, this, [process]() {
        process->deleteLater();
    });

    process->start(wsManagerExe, args);
}

void ActiveWindowOutlineEffect::slotToggleHudGrouping()
{
    m_badgeConfig.hudGroupWindows = !m_badgeConfig.hudGroupWindows;
    KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
    primaryConfig.writeEntry("BadgeHudGroupWindows", m_badgeConfig.hudGroupWindows);
    primaryConfig.sync();

    if (m_jumpMode) {
        if (m_subLeapActive) {
            exitSubLeapMode();
        }
        m_jumpLetterToWindow = assignJumpLetters();
        m_badgeTextureCache.clear();
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::applyWorkspaceViewIsolation(int hudViewIndex)
{
    if (!effects) {
        return;
    }

    const auto windows = effects->stackingOrder();

    if (hudViewIndex >= 1 && hudViewIndex <= m_workspaces.size()) {
        const auto &ws = m_workspaces.at(hudViewIndex - 1);
        EffectWindow *topMatchingWindow = nullptr;

        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }

            // Stashed windows must always stay hidden in stash!
            if (m_stashedWindows.contains(w)) {
                continue;
            }

            const auto desktops = w->desktops();
            const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
            const bool matches = LeapKWorkspace::matchesWorkspace(ws, w->windowClass(), w->caption(), dNum);

            if (matches) {
                // Should appear: unminimize if minimized
                if (w->isMinimized()) {
                    m_workspaceMinimizedWindows.remove(w);
                    w->setMinimized(false);
                }
                topMatchingWindow = w;
            } else {
                // Should disappear: minimize if not minimized
                if (!w->isMinimized()) {
                    m_workspaceMinimizedWindows.insert(w);
                    w->setMinimized(true);
                }
            }
        }

        // If active window was minimized (or does not match workspace), focus top matching window
        if (topMatchingWindow && (!m_activeWindow || m_activeWindow->isMinimized())) {
            effects->activateWindow(topMatchingWindow);
            m_activeWindow = topMatchingWindow;
        }
    } else if (hudViewIndex == 0 || hudViewIndex == m_workspaces.size() + 1) {
        // Standard view (0) or All windows: restore windows minimized by workspace isolation
        for (EffectWindow *w : m_workspaceMinimizedWindows) {
            if (w && !w->isDeleted() && w->isMinimized() && !m_stashedWindows.contains(w)) {
                w->setMinimized(false);
            }
        }
        m_workspaceMinimizedWindows.clear();
    }
}

void ActiveWindowOutlineEffect::enterStashMode()
{
    m_stashModeActive = true;
    m_subLeapActive = false;
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::exitStashMode()
{
    m_stashModeActive = false;
    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::addWindowToWorkspace(EffectWindow *w, int workspaceIndex)
{
    if (!w || w->isDeleted()) {
        return;
    }

    m_stashedWindows.remove(w);
    m_workspaceMinimizedWindows.remove(w);

    if (workspaceIndex >= 1 && workspaceIndex <= m_workspaces.size()) {
        auto &ws = m_workspaces[workspaceIndex - 1];
        const QString appClass = w->windowClass().trimmed();
        const QString caption = w->caption().trimmed();

        const auto desktops = w->desktops();
        const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;

        if (!LeapKWorkspace::matchesWorkspace(ws, appClass, caption, dNum)) {
            LeapKWorkspace::WindowMatchRule newRule;
            if (!appClass.isEmpty()) {
                newRule.matchApp = appClass;
            } else if (!caption.isEmpty()) {
                newRule.matchCaption = caption;
            } else {
                return;
            }
            newRule.matchDesktop = 0;

            // Remove rule matching this app from other workspaces so it belongs exclusively to this workspace
            for (int i = 0; i < m_workspaces.size(); ++i) {
                if (i == workspaceIndex - 1) {
                    continue;
                }
                auto &otherWs = m_workspaces[i];
                for (int r = otherWs.rules.size() - 1; r >= 0; --r) {
                    const auto &rule = otherWs.rules.at(r);
                    if (!appClass.isEmpty() && rule.matchApp.compare(appClass, Qt::CaseInsensitive) == 0 && rule.matchCaption.isEmpty()) {
                        otherWs.rules.removeAt(r);
                    }
                }
            }

            ws.rules.append(newRule);
            LeapKWorkspace::saveWorkspaces(m_workspaces);
        }

        if (effects) {
            effects->windowToDesktops(w, {effects->currentDesktop()});
        }
    } else if (workspaceIndex == 0) {
        if (effects) {
            effects->windowToDesktops(w, {effects->currentDesktop()});
        }
    }

    if (w->isMinimized()) {
        w->setMinimized(false);
    }
    if (effects) {
        effects->activateWindow(w);
    }
    m_activeWindow = w;
}

void ActiveWindowOutlineEffect::createEmptyWorkspace(const QString &suggestedName)
{
    if (m_stashModeActive) {
        exitStashMode();
    }
    refreshWorkspaces();

    const bool isPolish = (QLocale().language() == QLocale::Polish);
    const int nextNum = m_workspaces.size() + 1;
    const QString name = !suggestedName.trimmed().isEmpty()
        ? suggestedName.trimmed()
        : (isPolish ? QStringLiteral("Obszar %1").arg(nextNum) : QStringLiteral("Workspace %1").arg(nextNum));

    LeapKWorkspace::createEmptyWorkspace(m_workspaces, name);

    // Switch active HUD view directly to the new empty workspace
    // Views: 0 = Standard, 1..N = Workspaces, N+1 = All
    m_activeHudView = m_workspaces.size();

    if (effects && effects->config()) {
        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
        primaryConfig.writeEntry("ActiveHudView", m_activeHudView);
        primaryConfig.sync();
    }

    if (m_subLeapActive) {
        exitSubLeapMode();
    }

    applyWorkspaceViewIsolation(m_activeHudView);

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    if (effects) {
        effects->addRepaintFull();
    }
}

void ActiveWindowOutlineEffect::slotCycleHudView(int delta)
{
    if (m_stashModeActive) {
        exitStashMode();
    }
    refreshWorkspaces();
    const int totalViews = qMax(1, 2 + m_workspaces.size());
    m_activeHudView = (m_activeHudView + delta) % totalViews;
    if (m_activeHudView < 0) {
        m_activeHudView += totalViews;
    }
    if (effects && effects->config()) {
        KConfigGroup primaryConfig = effects->config()->group(QStringLiteral("Effect-leapk"));
        primaryConfig.writeEntry("ActiveHudView", m_activeHudView);
        primaryConfig.sync();
    }
    if (m_subLeapActive) {
        exitSubLeapMode();
    }

    applyWorkspaceViewIsolation(m_activeHudView);

    m_jumpLetterToWindow = assignJumpLetters();
    m_badgeTextureCache.clear();
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    effects->addRepaintFull();
}

bool ActiveWindowOutlineEffect::isEligibleTargetWindow(EffectWindow *w) const
{
    if (effects && effects->isScreenLocked()) {
        return false;
    }
    if (isSessionLockOrLogoutActive()) {
        return false;
    }
    if (!w || w == m_activeWindow || !isEligibleOutlineWindow(w)) {
        return false;
    }
    if (w->isSkipSwitcher() || w->isLockScreen() || isSystemOverlay(w)) {
        return false;
    }
    if (!w->acceptsFocus()) {
        return false;
    }
    if (w->isPopupWindow() || w->isTooltip() || w->isNotification()
        || w->isCriticalNotification() || w->isOnScreenDisplay() || isBlockingPopup(w)) {
        return false;
    }
    if (w->isHiddenByShowDesktop()) {
        return false;
    }
    if (!w->isMinimized()) {
        const QRectF geo = w->frameGeometry();
        if (geo.width() < 30 || geo.height() < 30) {
            return false;
        }
    }
    return true;
}

QMap<QChar, EffectWindow *> ActiveWindowOutlineEffect::assignJumpLetters() const
{
    if (effects && effects->isScreenLocked()) {
        return {};
    }
    if (isSessionLockOrLogoutActive()) {
        return {};
    }
    QMap<QChar, EffectWindow *> map;
    m_letterWindowGroups.clear();
    const auto windows = effects->stackingOrder();
    QList<EffectWindow *> candidates;
    for (EffectWindow *w : windows) {
        if (!isEligibleTargetWindow(w)) {
            continue;
        }
        if (m_closingWindows.contains(w)) {
            continue;
        }

        const bool isMarked = m_markedWindows.contains(w);
        const bool onCurrent = w->isOnCurrentDesktop();

        const auto desktops = w->desktops();
        const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;

        if (m_stashModeActive) { // Stash view: ALL hidden / stashed / minimized windows
            if (!w->isMinimized() && !m_stashedWindows.contains(w)) {
                continue;
            }
        } else {
            // In regular workspace views, stashed windows MUST stay hidden!
            if (m_stashedWindows.contains(w)) {
                continue;
            }

            if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) { // Workspace view: only windows matching this workspace
                const auto &ws = m_workspaces.at(m_activeHudView - 1);
                if (!LeapKWorkspace::matchesWorkspace(ws, w->windowClass(), w->caption(), dNum)) {
                    continue;
                }
            } else if (m_activeHudView == m_workspaces.size() + 1) { // All windows across all virtual desktops
                // include all
            } else { // View 0: Standard configured scope
                if (m_jumpTargets == 3) { // Marked windows only (all desktops)
                    if (!isMarked) {
                        continue;
                    }
                } else if (m_jumpTargets == 0) { // Current desktop visible + marked all desktops
                    if (!onCurrent && !isMarked) {
                        continue;
                    }
                    if (w->isMinimized() && !isMarked) {
                        continue;
                    }
                } else if (m_jumpTargets == 1) { // Current desktop all windows (visible & minimized) + marked all desktops
                    if (!onCurrent && !isMarked) {
                        continue;
                    }
                } else { // 2: All windows across all virtual desktops (visible & minimized)
                    // include all
                }
            }
        }

        candidates.append(w);
    }

    if (candidates.isEmpty()) {
        return map;
    }

    QSet<QChar> assignedLetters;
    QList<EffectWindow *> unassignedCandidates;
    m_groupJumpLetters.clear();
    m_windowToGroupLetter.clear();
    m_windowToDirectLetter.clear();

    for (EffectWindow *w : candidates) {
        if (m_markedWindowLetters.contains(w)) {
            const QChar ch = m_markedWindowLetters.value(w).toUpper();
            m_letterWindowGroups[ch].append(w);
            continue;
        }
        unassignedCandidates.append(w);
    }

    const auto activeLayout = LeapKKeyboard::detectActiveLayout();
    const QString pool = LeapKKeyboard::preferredLetterPool(activeLayout);
    int poolIdx = 0;

    auto nextFreeLetter = [&]() -> QChar {
        while (poolIdx < pool.length() && assignedLetters.contains(pool.at(poolIdx))) {
            poolIdx++;
        }
        if (poolIdx < pool.length()) {
            const QChar ch = pool.at(poolIdx);
            assignedLetters.insert(ch);
            poolIdx++;
            return ch;
        }
        return QChar();
    };

    if (m_badgeConfig.hudGroupWindows) {
        // Mode A: Group multi-instance windows into single card with counter
        for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
            const QChar groupKey = it.key();
            const auto &winList = it.value();
            assignedLetters.insert(groupKey);
            m_groupJumpLetters[groupKey] = winList;
            if (!winList.isEmpty()) {
                map[groupKey] = winList.first();
                for (EffectWindow *w : winList) {
                    m_windowToGroupLetter[w] = groupKey;
                    m_windowToDirectLetter[w] = groupKey;
                }
            }
        }

        for (EffectWindow *w : unassignedCandidates) {
            const QChar ch = nextFreeLetter();
            if (!ch.isNull()) {
                m_letterWindowGroups[ch].append(w);
                map[ch] = w;
                m_windowToDirectLetter[w] = ch;
            }
        }
    } else {
        // Mode B: Show all windows directly with unique jump letters and group container frame
        for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
            const QChar groupKey = it.key();
            const auto &winList = it.value();
            if (winList.size() > 1) {
                assignedLetters.insert(groupKey);
                m_groupJumpLetters[groupKey] = winList;
            }
        }

        for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
            const QChar groupKey = it.key();
            const auto &winList = it.value();
            if (winList.size() > 1) {
                for (EffectWindow *w : winList) {
                    const QChar directKey = nextFreeLetter();
                    if (!directKey.isNull()) {
                        map[directKey] = w;
                        m_windowToGroupLetter[w] = groupKey;
                        m_windowToDirectLetter[w] = directKey;
                    }
                }
            } else if (winList.size() == 1) {
                EffectWindow *w = winList.first();
                assignedLetters.insert(groupKey);
                map[groupKey] = w;
                m_windowToGroupLetter[w] = groupKey;
                m_windowToDirectLetter[w] = groupKey;
            }
        }

        for (EffectWindow *w : unassignedCandidates) {
            const QChar directKey = nextFreeLetter();
            if (!directKey.isNull()) {
                m_letterWindowGroups[directKey].append(w);
                map[directKey] = w;
                m_windowToDirectLetter[w] = directKey;
            }
        }
    }

    return map;
}

void ActiveWindowOutlineEffect::slotTriggerJumpMode()
{
    if (effects && effects->isScreenLocked()) {
        return;
    }
    if (!m_outlineEnabled || m_badgeDisplayMode == 2) {
        return;
    }

    if (m_jumpMode) {
        slotJumpToRecentWindow();
        return;
    }

    m_subLeapActive = false;
    m_stashModeActive = false;
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();
    m_closingWindows.clear();

    refreshPersistentMarksForWindows();
    refreshWorkspaces();

    if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        applyWorkspaceViewIsolation(m_activeHudView);
    }

    m_jumpLetterToWindow = assignJumpLetters();

    if (!effects->grabKeyboard(this)) {
        m_jumpLetterToWindow.clear();
        return;
    }

    m_jumpMode = true;
    updateAnimationTimer();
    m_badgeTextureCache.clear();
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    if (m_jumpTimeoutTimer) {
        if (m_badgeTimeout > 0) {
            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
        } else {
            m_jumpTimeoutTimer->stop();
        }
    }
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::cancelJumpMode()
{
    if (!m_jumpMode) {
        return;
    }
    m_jumpMode = false;
    m_subLeapActive = false;
    m_stashModeActive = false;
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();
    m_closingWindows.clear();
    m_letterWindowGroups.clear();
    if (m_jumpTimeoutTimer && m_jumpTimeoutTimer->isActive()) {
        m_jumpTimeoutTimer->stop();
    }
    m_jumpLetterToWindow.clear();
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    effects->ungrabKeyboard();
    updateAnimationTimer();
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::exitSubLeapMode()
{
    m_subLeapActive = false;
    m_stashModeActive = false;
    m_subLeapBaseKey = QChar();
    m_subLeapWindows.clear();
    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();
    m_hudHintBarTexture.reset();
    m_hudHintBarCacheKey = -1;
    m_hudHeaderTexture.reset();
    m_hudHeaderCacheKey = -1;
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    m_badgeTextureCache.clear();
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::enterSubLeapForGroup(QChar keyChar, const QList<EffectWindow *> &group)
{
    m_subLeapActive = true;
    m_subLeapBaseKey = keyChar;
    m_subLeapWindows.clear();
    for (EffectWindow *w : group) {
        if (w && !w->isDeleted() && !m_subLeapWindows.contains(w)) {
            m_subLeapWindows.append(w);
        }
    }

    // Sort by recency so primary/most recent window is at index 0
    std::sort(m_subLeapWindows.begin(), m_subLeapWindows.end(), [this](EffectWindow *a, EffectWindow *b) {
        int idxA = m_windowHistory.indexOf(a);
        int idxB = m_windowHistory.indexOf(b);
        if (idxA != -1 && idxB != -1) return idxA < idxB;
        if (idxA != -1) return true;
        if (idxB != -1) return false;
        return false;
    });

    const auto layout = LeapKKeyboard::detectActiveLayout();
    const QList<QChar> neighborKeys = LeapKKeyboard::findSpatialNeighborKeys(keyChar, m_subLeapWindows.size(), layout);

    m_subLeapLetterToWindow.clear();
    m_subLeapTargets.clear();

    // If count == 3, neighborKeys is [Left, Base, Right] (e.g. [D, F, G])
    // Make sure baseKey maps to primary window (m_subLeapWindows[0])
    if (neighborKeys.contains(keyChar)) {
        const int baseIdx = neighborKeys.indexOf(keyChar);
        int otherWinIdx = 1;
        for (int i = 0; i < neighborKeys.size(); ++i) {
            const QChar k = neighborKeys.at(i);
            if (i == baseIdx) {
                m_subLeapLetterToWindow[k] = m_subLeapWindows[0];
                m_subLeapTargets.append(qMakePair(k, m_subLeapWindows[0]));
            } else if (otherWinIdx < m_subLeapWindows.size()) {
                m_subLeapLetterToWindow[k] = m_subLeapWindows[otherWinIdx];
                m_subLeapTargets.append(qMakePair(k, m_subLeapWindows[otherWinIdx]));
                otherWinIdx++;
            }
        }
    } else {
        for (int i = 0; i < neighborKeys.size() && i < m_subLeapWindows.size(); ++i) {
            m_subLeapLetterToWindow[neighborKeys[i]] = m_subLeapWindows[i];
            m_subLeapTargets.append(qMakePair(neighborKeys[i], m_subLeapWindows[i]));
        }
    }

    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }
    m_badgeTextureCache.clear();
    effects->addRepaintFull();
}

void ActiveWindowOutlineEffect::grabbedKeyboardEvent(QKeyEvent *e)
{
    if (!m_jumpMode) {
        return;
    }
    if (e->type() != QEvent::KeyPress) {
        return;
    }

    // Reset timeout countdown on EVERY keypress in HUD!
    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
    }

    // Ignore standalone modifier keys so holding Shift, Ctrl, Alt, Meta etc.
    // to perform a modal jump action does NOT dismiss the Jump HUD.
    switch (e->key()) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_Menu:
    case Qt::Key_Hyper_L:
    case Qt::Key_Hyper_R:
    case Qt::Key_Help:
    case Qt::Key_Direction_L:
    case Qt::Key_Direction_R:
    case Qt::Key_Mode_switch:
        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
        }
        return;
    default:
        break;
    }

    const Qt::KeyboardModifiers mods = e->modifiers();
    const bool isShift = (mods & Qt::ShiftModifier);
    const bool isAlt = (mods & Qt::AltModifier) && !(mods & Qt::ControlModifier);
    const bool isCtrl = (mods & Qt::ControlModifier) && !(mods & Qt::AltModifier);

    auto extractKeyChar = [e, isShift]() -> QString {
        if (e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z) {
            return QString(QChar('A' + (e->key() - Qt::Key_A)));
        }
        if (e->key() >= Qt::Key_0 && e->key() <= Qt::Key_9) {
            return QString(QChar('0' + (e->key() - Qt::Key_0)));
        }
        if (isShift) {
            switch (e->key()) {
            case Qt::Key_Exclam: return QStringLiteral("1");
            case Qt::Key_At: return QStringLiteral("2");
            case Qt::Key_NumberSign: return QStringLiteral("3");
            case Qt::Key_Dollar: return QStringLiteral("4");
            case Qt::Key_Percent: return QStringLiteral("5");
            case Qt::Key_AsciiCircum: return QStringLiteral("6");
            case Qt::Key_Ampersand: return QStringLiteral("7");
            case Qt::Key_Asterisk: return QStringLiteral("8");
            case Qt::Key_ParenLeft: return QStringLiteral("9");
            case Qt::Key_ParenRight: return QStringLiteral("0");
            default: break;
            }
        }
        return e->text().toUpper().trimmed();
    };

    // 1. If currently in Sub-Leap disambiguation:
    if (m_subLeapActive) {
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace) {
            exitSubLeapMode();
            return;
        }

        const QString text = extractKeyChar();

        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0);
            if (m_subLeapLetterToWindow.contains(keyChar)) {
                EffectWindow *target = m_subLeapLetterToWindow.value(keyChar);
                if (target && !target->isDeleted()) {
                    if (isShift) {
                        // Quick close selected window
                        target->closeWindow();
                        m_closingWindows.insert(target);
                        m_subLeapWindows.removeAll(target);
                        if (m_subLeapWindows.size() <= 1) {
                            exitSubLeapMode();
                            m_jumpLetterToWindow = assignJumpLetters();
                            m_badgeTextureCache.clear();
                            if (m_jumpLetterToWindow.isEmpty()) {
                                cancelJumpMode();
                            } else {
                                if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                                    m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                                }
                                effects->addRepaintFull();
                            }
                        } else {
                            enterSubLeapForGroup(m_subLeapBaseKey, m_subLeapWindows);
                        }
                        return;
                    }

                    if (isCtrl) {
                        // Toggle stash/minimized
                        if (!target->isMinimized()) {
                            m_stashedWindows.insert(target);
                            target->setMinimized(true);
                        } else {
                            m_stashedWindows.remove(target);
                            target->setMinimized(false);
                        }
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                        return;
                    }

                    if (isAlt) {
                        // Pull to current desktop and activate
                        cancelJumpMode();
                        effects->windowToDesktops(target, {effects->currentDesktop()});
                        if (target->isMinimized()) {
                            target->setMinimized(false);
                        }
                        effects->activateWindow(target);
                        return;
                    }

                    // Plain letter: normal jump & activate
                    cancelJumpMode();
                    m_expectedJumpTarget = target;
                    if (m_expectedJumpTargetTimer) {
                        m_expectedJumpTargetTimer->start(1000);
                    }
                    if (!target->isOnCurrentDesktop()) {
                        const auto desktops = target->desktops();
                        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
                            effects->setCurrentDesktop(desktops.first());
                        }
                    }
                    if (target->isMinimized()) {
                        target->setMinimized(false);
                    }
                    effects->activateWindow(target);
                    return;
                }
            }
        }

        cancelJumpMode();
        return;
    }

    // 2. Main Jump Mode:
    if (m_stashModeActive) {
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Backspace || e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
            exitStashMode();
            return;
        }

        const QString text = extractKeyChar();
        if (!text.isEmpty()) {
            const QChar keyChar = text.at(0);
            if (m_jumpLetterToWindow.contains(keyChar)) {
                EffectWindow *target = m_jumpLetterToWindow.value(keyChar);
                if (target && !target->isDeleted()) {
                    if (isShift) {
                        target->closeWindow();
                        m_stashedWindows.remove(target);
                        m_jumpLetterToWindow = assignJumpLetters();
                        m_badgeTextureCache.clear();
                        m_hudHeaderTexture.reset();
                        m_hudHeaderCacheKey = -1;
                        effects->addRepaintFull();
                        return;
                    }
                    addWindowToWorkspace(target, m_activeHudView);
                    exitStashMode();
                    cancelJumpMode();
                    return;
                }
            }
        }
        return;
    }

    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        cancelJumpMode();
        return;
    }

    if (e->key() == Qt::Key_Tab || e->key() == Qt::Key_Backtab) {
        enterStashMode();
        return;
    }

    if ((mods & (Qt::AltModifier | Qt::ControlModifier)) && e->key() == Qt::Key_S) {
        enterStashMode();
        return;
    }

    if (e->key() == Qt::Key_Comma || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char(','))) {
        slotCycleHudView(-1);
        return;
    }

    if (e->key() == Qt::Key_Period || (!e->text().isEmpty() && e->text().at(0) == QLatin1Char('.'))) {
        slotCycleHudView(1);
        return;
    }

    if (e->key() == Qt::Key_QuoteLeft || e->key() == Qt::Key_AsciiTilde || e->key() == Qt::Key_F1
        || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('`') || e->text().at(0) == QLatin1Char('~')))) {
        slotCycleHudView(1);
        return;
    }

    if ((isCtrl && e->key() == Qt::Key_N)
        || ((mods & Qt::AltModifier) && e->key() == Qt::Key_N)
        || e->key() == Qt::Key_Plus
        || (!e->text().isEmpty() && (e->text().at(0) == QLatin1Char('+') || (e->key() == Qt::Key_Equal && isShift)))) {
        createEmptyWorkspace();
        return;
    }

    bool isRecentKeyMatch = false;
    const QString rKey = m_jumpRecentKey.trimmed();
    if (rKey.compare(QLatin1String("Space"), Qt::CaseInsensitive) == 0 && e->key() == Qt::Key_Space) {
        isRecentKeyMatch = true;
    } else if (!rKey.isEmpty()) {
        QKeySequence seq(rKey);
        if (!seq.isEmpty()) {
            const QKeyCombination comb = seq[0];
            const int keyOnly = comb.key();
            const Qt::KeyboardModifiers keyMods = comb.keyboardModifiers();
            if (e->key() == keyOnly && (keyMods == Qt::NoModifier || e->modifiers() == keyMods)) {
                isRecentKeyMatch = true;
            }
        }
        if (!isRecentKeyMatch && !e->text().isEmpty() && e->text().compare(rKey, Qt::CaseInsensitive) == 0) {
            isRecentKeyMatch = true;
        }
    } else if (e->key() == Qt::Key_Space) {
        isRecentKeyMatch = true;
    }

    if (!isCtrl && !isAlt && isRecentKeyMatch) {
        slotJumpToRecentWindow();
        return;
    }

    const QString text = extractKeyChar();

    if (!text.isEmpty()) {
        const QChar keyChar = text.at(0);

        // Check if keyChar is a group trigger key with multiple windows
        if (m_groupJumpLetters.contains(keyChar) && m_groupJumpLetters.value(keyChar).size() > 1) {
            const auto group = m_groupJumpLetters.value(keyChar);
            if (isAlt) {
                // Smart Workspace summon: pull all windows in group to current desktop!
                cancelJumpMode();
                VirtualDesktop *currDesktop = effects->currentDesktop();
                EffectWindow *topWin = nullptr;
                for (EffectWindow *w : group) {
                    if (w && !w->isDeleted()) {
                        effects->windowToDesktops(w, {currDesktop});
                        if (w->isMinimized()) {
                            w->setMinimized(false);
                        }
                        if (!topWin) {
                            topWin = w;
                        }
                    }
                }
                if (topWin) {
                    effects->activateWindow(topWin);
                }
                return;
            }

            if (isCtrl) {
                // Toggle stash/minimized for whole group
                bool anyVisible = false;
                for (EffectWindow *w : group) {
                    if (w && !w->isDeleted() && !w->isMinimized()) {
                        anyVisible = true;
                        break;
                    }
                }
                for (EffectWindow *w : group) {
                    if (w && !w->isDeleted()) {
                        if (anyVisible) {
                            m_stashedWindows.insert(w);
                            w->setMinimized(true);
                        } else {
                            m_stashedWindows.remove(w);
                            w->setMinimized(false);
                        }
                    }
                }
                m_jumpLetterToWindow = assignJumpLetters();
                m_badgeTextureCache.clear();
                m_hudHeaderTexture.reset();
                m_hudHeaderCacheKey = -1;
                if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                    m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                }
                effects->addRepaintFull();
                return;
            }

            // Normal or Shift: enter Sub-Leap to disambiguate or pick individual window to close
            enterSubLeapForGroup(keyChar, group);
            return;
        }

        // Check direct jump letter (either single marked window or unique direct letter)
        if (m_jumpLetterToWindow.contains(keyChar)) {
            EffectWindow *target = m_jumpLetterToWindow.value(keyChar);
            if (target && !target->isDeleted()) {
                if (isShift) {
                    // Quick close single window
                    target->closeWindow();
                    m_closingWindows.insert(target);
                    m_jumpLetterToWindow = assignJumpLetters();
                    m_badgeTextureCache.clear();
                    if (m_jumpLetterToWindow.isEmpty()) {
                        cancelJumpMode();
                    } else {
                        if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                            m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                        }
                        effects->addRepaintFull();
                    }
                    return;
                }

                if (isCtrl) {
                    // Quick stash toggle
                    if (!target->isMinimized()) {
                        m_stashedWindows.insert(target);
                        target->setMinimized(true);
                    } else {
                        m_stashedWindows.remove(target);
                        target->setMinimized(false);
                    }
                    m_jumpLetterToWindow = assignJumpLetters();
                    m_badgeTextureCache.clear();
                    m_hudHeaderTexture.reset();
                    m_hudHeaderCacheKey = -1;
                    if (m_jumpTimeoutTimer && m_badgeTimeout > 0) {
                        m_jumpTimeoutTimer->start(m_badgeTimeout * 1000);
                    }
                    effects->addRepaintFull();
                    return;
                }

                if (isAlt) {
                    // Pull to current desktop and activate
                    cancelJumpMode();
                    effects->windowToDesktops(target, {effects->currentDesktop()});
                    if (target->isMinimized()) {
                        target->setMinimized(false);
                    }
                    effects->activateWindow(target);
                    return;
                }

                // Normal jump & activate
                cancelJumpMode();
                m_expectedJumpTarget = target;
                if (m_expectedJumpTargetTimer) {
                    m_expectedJumpTargetTimer->start(1000);
                }
                if (!target->isOnCurrentDesktop()) {
                    const auto desktops = target->desktops();
                    if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
                        effects->setCurrentDesktop(desktops.first());
                    }
                }
                if (target->isMinimized()) {
                    target->setMinimized(false);
                }
                effects->activateWindow(target);
                return;
            }
        }
    }

    cancelJumpMode();
}

EffectWindow *ActiveWindowOutlineEffect::findRecentWindow() const
{
    EffectWindow *recent = OutlineHistory::findRecentWindow(
        m_windowHistory,
        m_activeWindow,
        [this](EffectWindow *win) {
            return isEligibleOutlineWindow(win);
        });
    if (recent) {
        return recent;
    }
    const auto windows = effects->stackingOrder();
    for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
        EffectWindow *candidate = *it;
        if (candidate && !candidate->isDeleted() && candidate != m_activeWindow && isEligibleOutlineWindow(candidate)) {
            return candidate;
        }
    }
    return nullptr;
}

void ActiveWindowOutlineEffect::slotJumpToRecentWindow()
{
    if (m_jumpMode) {
        cancelJumpMode();
    }

    m_isHistoryNavigating = false;
    if (m_historyCommitTimer && m_historyCommitTimer->isActive()) {
        m_historyCommitTimer->stop();
    }

    EffectWindow *target = findRecentWindow();
    if (!target || target->isDeleted()) {
        return;
    }

    m_expectedJumpTarget = target;
    if (m_expectedJumpTargetTimer) {
        m_expectedJumpTargetTimer->start(1000);
    }

    if (!target->isOnCurrentDesktop()) {
        const auto desktops = target->desktops();
        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
            effects->setCurrentDesktop(desktops.first());
        }
    }
    if (target->isMinimized()) {
        target->setMinimized(false);
    }
    effects->activateWindow(target);
}

void ActiveWindowOutlineEffect::updateRecentWindowsFromHistory()
{
    const QList<EffectWindow *> oldRecents = m_recentWindows;
    m_recentWindows = OutlineHistory::computeRecentWindows(
        m_windowHistory,
        m_activeWindow,
        m_recentWindowsCount,
        m_highlightRecentWindow,
        [this](EffectWindow *win) {
            return isEligibleOutlineWindow(win);
        });

    for (EffectWindow *oldWin : oldRecents) {
        if (oldWin && !m_recentWindows.contains(oldWin) && oldWin != m_activeWindow && !m_markedWindows.contains(oldWin)) {
            effects->addRepaint(expandedGeometryFor(oldWin));
            m_outlineCache.remove(oldWin);
            m_lastGeometry.remove(oldWin);
            m_recentAmbilightColors.remove(oldWin);
            m_recentCycleColors.remove(oldWin);
        }
    }
}

bool ActiveWindowOutlineEffect::isBadgeOccluded(EffectWindow *target, const QRectF &badgeRect, const QList<QRectF> &hudRects) const
{
    if (!target) {
        return true;
    }
    const QRect badgeAligned = badgeRect.toAlignedRect();
    const QRect virtualScreen = effects->virtualScreenGeometry();
    if (!virtualScreen.intersects(badgeAligned)) {
        return true;
    }
    for (const QRectF &hudRect : hudRects) {
        if (hudRect.intersects(badgeRect)) {
            return true;
        }
    }
    bool aboveTarget = false;
    const auto windows = effects->stackingOrder();
    for (EffectWindow *candidate : windows) {
        if (candidate == target) {
            aboveTarget = true;
            continue;
        }
        if (!aboveTarget) {
            continue;
        }
        if (!candidate || candidate->isDeleted() || !candidate->isVisible()
            || candidate->isMinimized() || !candidate->isOnCurrentDesktop()
            || candidate->isHiddenByShowDesktop() || candidate->isOutline()
            || candidate->isDesktop()
            || candidate->opacity() <= 0.05
            || candidate->frameGeometry().width() < 10
            || candidate->frameGeometry().height() < 10) {
            continue;
        }
        if (candidate->frameGeometry().intersects(badgeAligned)) {
            return true;
        }
    }
    return false;
}

void ActiveWindowOutlineEffect::slotPrevRecentWindow()
{
    for (auto it = m_windowHistory.begin(); it != m_windowHistory.end(); ) {
        if (!(*it) || (*it)->isDeleted()) {
            it = m_windowHistory.erase(it);
        } else {
            ++it;
        }
    }

    if (!m_isHistoryNavigating && m_activeWindow && isEligibleOutlineWindow(m_activeWindow)) {
        m_windowHistory.removeAll(m_activeWindow);
        m_windowHistory.prepend(m_activeWindow);
        m_historyNavIndex = 0;
    }

    if (m_windowHistory.size() < 2) {
        return;
    }

    int nextIdx = m_historyNavIndex + 1;
    while (nextIdx < m_windowHistory.size()) {
        EffectWindow *candidate = m_windowHistory.at(nextIdx);
        if (candidate && isEligibleOutlineWindow(candidate)) {
            m_historyNavIndex = nextIdx;
            m_isHistoryNavigating = true;
            m_expectedJumpTarget = candidate;
            if (m_expectedJumpTargetTimer) {
                m_expectedJumpTargetTimer->start(1000);
            }

            if (!candidate->isOnCurrentDesktop()) {
                const auto desktops = candidate->desktops();
                if (!desktops.isEmpty() && !candidate->isOnAllDesktops()) {
                    effects->setCurrentDesktop(desktops.first());
                }
            }
            if (candidate->isMinimized()) {
                candidate->setMinimized(false);
            }
            effects->activateWindow(candidate);
            break;
        } else {
            nextIdx++;
        }
    }

    if (m_historyCommitTimer) {
        m_historyCommitTimer->start(2000);
    }
}

void ActiveWindowOutlineEffect::slotNextRecentWindow()
{
    for (auto it = m_windowHistory.begin(); it != m_windowHistory.end(); ) {
        if (!(*it) || (*it)->isDeleted()) {
            it = m_windowHistory.erase(it);
        } else {
            ++it;
        }
    }

    if (m_windowHistory.size() < 2 || m_historyNavIndex <= 0) {
        return;
    }

    int prevIdx = m_historyNavIndex - 1;
    while (prevIdx >= 0) {
        EffectWindow *candidate = m_windowHistory.at(prevIdx);
        if (candidate && isEligibleOutlineWindow(candidate)) {
            m_historyNavIndex = prevIdx;
            m_isHistoryNavigating = true;
            m_expectedJumpTarget = candidate;
            if (m_expectedJumpTargetTimer) {
                m_expectedJumpTargetTimer->start(1000);
            }

            if (!candidate->isOnCurrentDesktop()) {
                const auto desktops = candidate->desktops();
                if (!desktops.isEmpty() && !candidate->isOnAllDesktops()) {
                    effects->setCurrentDesktop(desktops.first());
                }
            }
            if (candidate->isMinimized()) {
                candidate->setMinimized(false);
            }
            effects->activateWindow(candidate);
            break;
        } else {
            prevIdx--;
        }
    }

    if (m_historyCommitTimer) {
        m_historyCommitTimer->start(2000);
    }
}

void ActiveWindowOutlineEffect::slotCommitHistoryNavigation()
{
    if (m_historyCommitTimer && m_historyCommitTimer->isActive()) {
        m_historyCommitTimer->stop();
    }
    m_isHistoryNavigating = false;
    if (m_historyNavIndex >= 0 && m_historyNavIndex < m_windowHistory.size()) {
        EffectWindow *committedWindow = m_windowHistory.at(m_historyNavIndex);
        if (committedWindow && !committedWindow->isDeleted()) {
            m_windowHistory.removeAll(committedWindow);
            m_windowHistory.prepend(committedWindow);
            m_activeWindow = committedWindow;
        }
    }
    m_historyNavIndex = 0;
    updateRecentWindowsFromHistory();
    repaintAllOutlines();
}

bool ActiveWindowOutlineEffect::isWindowObscured(EffectWindow *target) const
{
    if (!target || !effects) {
        return false;
    }
    const auto stacking = effects->stackingOrder();
    const int idx = stacking.indexOf(target);
    if (idx < 0 || idx == stacking.size() - 1) {
        return false;
    }
    const QRectF targetGeo = target->frameGeometry();
    for (int i = idx + 1; i < stacking.size(); ++i) {
        EffectWindow *above = stacking.at(i);
        if (above && !above->isDeleted() && above->isVisible() && !above->isMinimized()
            && above->isOnCurrentDesktop() && isEligibleOutlineWindow(above)) {
            if (above->frameGeometry().intersects(targetGeo)) {
                return true;
            }
        }
    }
    return false;
}

#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
bool ActiveWindowOutlineEffect::paintWindow(const RenderTarget &renderTarget,
#else
void ActiveWindowOutlineEffect::paintWindow(const RenderTarget &renderTarget,
#endif
                                            const RenderViewport &viewport,
                                            EffectWindow *w,
                                            int mask,
                                            const OutlineRegion &deviceRegion,
                                            WindowPaintData &data)
{
#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
    static_cast<void>(effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data));
#else
    effects->paintWindow(renderTarget, viewport, w, mask, deviceRegion, data);
#endif

    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const bool wantThumbnail = (cfg.windowShowThumbnail || cfg.hudShowThumbnail)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (wantThumbnail && w && !w->isDeleted() && isEligibleOutlineWindow(w)
        && !w->isMinimized() && w->isVisible() && !isWindowObscured(w)
        && !(effects && effects->isScreenLocked()) && !w->isLockScreen()
        && !m_inThumbnailCapture) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastCapture = m_lastCaptureTimestamp.value(w, 0);
        const qint64 throttleInterval = m_badgeLivePreviews ? 33 : 1000;
        if (!m_thumbnailCache.contains(w) || (now - lastCapture >= throttleInterval)) {
            const QSize targetSize(qMax(cfg.windowThumbnailWidth, cfg.hudThumbnailWidth) * 2,
                                   qMax(cfg.windowThumbnailHeight, cfg.hudThumbnailHeight) * 2);
            captureWindowThumbnail(w, renderTarget, viewport, targetSize, /*forceBlit=*/false);
            m_lastCaptureTimestamp[w] = now;
        }
    }

#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
    return true;
#endif
}

#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
bool ActiveWindowOutlineEffect::drawWindow(const RenderTarget &renderTarget,
#else
void ActiveWindowOutlineEffect::drawWindow(const RenderTarget &renderTarget,
#endif
                                          const RenderViewport &viewport,
                                          EffectWindow *w,
                                          int mask,
                                          const OutlineRegion &deviceRegion,
                                          WindowPaintData &data)
{
#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
    static_cast<void>(effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data));
#else
    effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
#endif

    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const bool wantThumbnail = (cfg.windowShowThumbnail || cfg.hudShowThumbnail)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (wantThumbnail && w && !w->isDeleted() && isEligibleOutlineWindow(w)
        && !w->isMinimized() && w->isVisible() && !isWindowObscured(w)
        && !(effects && effects->isScreenLocked()) && !w->isLockScreen()
        && !m_inThumbnailCapture) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastCapture = m_lastCaptureTimestamp.value(w, 0);
        const qint64 throttleInterval = m_badgeLivePreviews ? 33 : 1000;
        if (!m_thumbnailCache.contains(w) || (now - lastCapture >= throttleInterval)) {
            const QSize targetSize(qMax(cfg.windowThumbnailWidth, cfg.hudThumbnailWidth) * 2,
                                   qMax(cfg.windowThumbnailHeight, cfg.hudThumbnailHeight) * 2);
            captureWindowThumbnail(w, renderTarget, viewport, targetSize, /*forceBlit=*/false);
            m_lastCaptureTimestamp[w] = now;
        }
    }

#if defined(ACTIVE_WINDOW_OUTLINE_BOOL_DRAW_WINDOW) || defined(ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN)
    return true;
#endif
}

QImage ActiveWindowOutlineEffect::captureWindowThumbnail(EffectWindow *target,
                                                         const RenderTarget &renderTarget,
                                                         const RenderViewport &viewport,
                                                         const QSize &targetSize,
                                                         bool forceBlit)
{
    if (!target || target->isDeleted()) {
        return QImage();
    }

    if (m_inThumbnailCapture) {
        auto itEarly = m_thumbnailCache.find(target);
        return itEarly != m_thumbnailCache.end() ? itEarly.value() : QImage();
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_thumbnailCache.find(target);
    const qint64 lastCapture = m_lastCaptureTimestamp.value(target, 0);
    const qint64 cacheInterval = m_badgeLivePreviews ? 33 : 750;

    // Fast-path: return cached thumbnail if captured recently (avoids GPU-to-CPU stalls during animations)
    if (!forceBlit && it != m_thumbnailCache.end() && !it.value().isNull() && (now - lastCapture < cacheInterval)) {
        return it.value();
    }

#if !defined(ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES) && defined(ACTIVE_WINDOW_OUTLINE_HAS_OPENGL_SURFACE_TEXTURE)
    // Priority 1: Window's direct surface texture from KWin's scene graph.
    // This is the window's actual GPU surface buffer. It completely ignores Z-order
    // (windows on top do NOT obscure it), and works even when the window is covered
    // or located on another virtual desktop!
    if (auto wi = target->windowItem()) {
        if (auto si = wi->surfaceItem()) {
            if (auto st = dynamic_cast<OpenGLSurfaceTexture*>(si->texture())) {
                if (st->isValid()) {
                    auto contents = st->texture();
                    if (contents.isValid() && !contents.planes.isEmpty() && contents.planes.first()) {
                        QImage rawImg = contents.planes.first()->toImage();
                        if (!rawImg.isNull() && rawImg.width() >= 8 && rawImg.height() >= 8) {
                            QImage thumb = rawImg.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                            if (thumb.format() != QImage::Format_RGBA8888 && thumb.format() != QImage::Format_ARGB32_Premultiplied) {
                                thumb = thumb.convertToFormat(QImage::Format_RGBA8888);
                            }
                            // Wayland clients rendering via DMABUF / EGL (e.g. Firefox) store textures
                            // with coordinate (0,0) at bottom-left in GL, making direct toImage() inverted.
                            // SHM raster surfaces (Kate, Dolphin) upload top-to-bottom so they are not inverted.
                            bool needsFlip = false;
                            if (auto buf = si->buffer()) {
                                if (buf->dmabufAttributes()) {
                                    needsFlip = true;
                                }
                            } else if (target->isWaylandClient()) {
                                needsFlip = true;
                            }
                            if (needsFlip) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                                thumb = thumb.flipped(Qt::Vertical);
#else
                                thumb = thumb.mirrored(false, true);
#endif
                            }
                            for (qsizetype i = 3; i < thumb.sizeInBytes(); i += 4) {
                                thumb.bits()[i] = 255;
                            }
                            m_thumbnailCache[target] = thumb;
                            m_lastCaptureTimestamp[target] = now;
                            return thumb;
                        }
                    }
                }
            }
        }
    }
#endif

    const QRectF frameGeo = target->frameGeometry();
    const QSize winSize = frameGeo.size().toSize();
    QSize actualThumbSize = targetSize;
    if (winSize.width() > 0 && winSize.height() > 0) {
        actualThumbSize = winSize.scaled(targetSize, Qt::KeepAspectRatio);
        actualThumbSize.setWidth(qMax(8, actualThumbSize.width()));
        actualThumbSize.setHeight(qMax(8, actualThumbSize.height()));
    }

    // Priority 2: Blit directly from renderTarget if the window is currently visible and unobscured on this desktop
    if (forceBlit || (!isWindowObscured(target) && target->isOnCurrentDesktop() && !target->isMinimized() && target->isVisible())) {
        const QRect viewRect = QRectF(viewport.renderRect()).toRect();
        const QRect windowRect = frameGeo.toRect().intersected(viewRect);

        if (windowRect.width() >= 16 && windowRect.height() >= 16) {
            if (renderTarget.image()) {
                const QImage *img = renderTarget.image();
                const QRect src = windowRect.intersected(img->rect());
                if (!src.isEmpty()) {
                    QImage thumb = img->copy(src).scaled(actualThumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    if (thumb.format() != QImage::Format_RGBA8888 && thumb.format() != QImage::Format_ARGB32_Premultiplied) {
                        thumb = thumb.convertToFormat(QImage::Format_RGBA8888);
                    }
                    for (qsizetype i = 3; i < thumb.sizeInBytes(); i += 4) {
                        thumb.bits()[i] = 255;
                    }
                    m_thumbnailCache[target] = thumb;
                    return thumb;
                }
            }

            if (!m_thumbTexture || m_thumbTexture->size() != actualThumbSize) {
                m_thumbTexture = GLTexture::allocate(GL_RGBA8, actualThumbSize);
                if (m_thumbTexture) {
                    m_thumbFbo = std::make_unique<GLFramebuffer>(m_thumbTexture.get());
                }
            }

            if (m_thumbFbo && m_thumbFbo->valid()) {
                GLFramebuffer *savedFbo = GLFramebuffer::currentFramebuffer();
                const OutlineRect srcRect(windowRect);
                const OutlineRect dstRect(0, 0, actualThumbSize.width(), actualThumbSize.height());
                const bool blitOk = m_thumbFbo->blitFromRenderTarget(renderTarget, viewport, srcRect, dstRect);
                if (blitOk) {
                    QImage thumb = m_thumbTexture->toImage();
                    if (thumb.isNull()) {
                        thumb = QImage(actualThumbSize, QImage::Format_RGBA8888);
                        GLFramebuffer::pushFramebuffer(m_thumbFbo.get());
                        glReadPixels(0, 0, actualThumbSize.width(), actualThumbSize.height(), GL_RGBA, GL_UNSIGNED_BYTE, thumb.bits());
                        GLFramebuffer::popFramebuffer();
                    }
                    if (!thumb.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                        thumb = thumb.flipped(Qt::Vertical);
#else
                        thumb = thumb.mirrored(false, true);
#endif
                        if (thumb.format() != QImage::Format_RGBA8888 && thumb.format() != QImage::Format_ARGB32_Premultiplied) {
                            thumb = thumb.convertToFormat(QImage::Format_RGBA8888);
                        }
                        for (qsizetype i = 3; i < thumb.sizeInBytes(); i += 4) {
                            thumb.bits()[i] = 255;
                        }
                        m_thumbnailCache[target] = thumb;
                        m_lastCaptureTimestamp[target] = now;
                        if (savedFbo && GLFramebuffer::currentFramebuffer() != savedFbo) {
                            GLFramebuffer::pushFramebuffer(savedFbo);
                        }
                        return thumb;
                    }
                }
                if (savedFbo && GLFramebuffer::currentFramebuffer() != savedFbo) {
                    GLFramebuffer::pushFramebuffer(savedFbo);
                }
            }
        }
    }

    // 2. Offscreen rendering for covered/obscured windows, or windows on other desktops/minimized
    const QRectF offscreenFrameGeo = target->frameGeometry();
    if (offscreenFrameGeo.width() >= 16 && offscreenFrameGeo.height() >= 16) {
        if (!m_thumbTexture || m_thumbTexture->size() != actualThumbSize) {
            m_thumbTexture = GLTexture::allocate(GL_RGBA8, actualThumbSize);
            if (m_thumbTexture) {
                m_thumbFbo = std::make_unique<GLFramebuffer>(m_thumbTexture.get());
            }
        }

        if (m_thumbFbo && m_thumbFbo->valid()) {
            GLFramebuffer *savedFbo = GLFramebuffer::currentFramebuffer();

            EffectWindowVisibleRef visDesk(target, EffectWindow::PAINT_DISABLED_BY_DESKTOP);
            EffectWindowVisibleRef visMin(target, EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
            EffectWindowVisibleRef visAct(target, EffectWindow::PAINT_DISABLED_BY_ACTIVITY);

            RenderTarget thumbTarget(m_thumbFbo.get());
            RenderViewport thumbViewport(RectF(0, 0, actualThumbSize.width(), actualThumbSize.height()), 1.0, thumbTarget, QPoint(0, 0));

            GLFramebuffer::pushFramebuffer(m_thumbFbo.get());
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            WindowPaintData data;
            data += -offscreenFrameGeo.toRect().topLeft();
            const qreal scale = qreal(actualThumbSize.width()) / qMax(1.0, offscreenFrameGeo.width());
            data *= QVector2D(scale, scale);

            OutlineRegion devReg(QRegion(0, 0, actualThumbSize.width(), actualThumbSize.height()));
            m_inThumbnailCapture = true;
            static_cast<void>(effects->drawWindow(thumbTarget, thumbViewport, target, PAINT_WINDOW_TRANSFORMED, devReg, data));
            m_inThumbnailCapture = false;

            QImage thumb = m_thumbTexture->toImage();
            if (thumb.isNull()) {
                thumb = QImage(actualThumbSize, QImage::Format_RGBA8888);
                glReadPixels(0, 0, actualThumbSize.width(), actualThumbSize.height(), GL_RGBA, GL_UNSIGNED_BYTE, thumb.bits());
            }
            GLFramebuffer::popFramebuffer();
            if (savedFbo && GLFramebuffer::currentFramebuffer() != savedFbo) {
                GLFramebuffer::pushFramebuffer(savedFbo);
            }

            if (!thumb.isNull()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                thumb = thumb.flipped(Qt::Vertical);
#else
                thumb = thumb.mirrored(false, true);
#endif
                if (thumb.format() != QImage::Format_RGBA8888 && thumb.format() != QImage::Format_ARGB32_Premultiplied) {
                    thumb = thumb.convertToFormat(QImage::Format_RGBA8888);
                }
                bool hasContent = false;
                const quint32 *pixels = reinterpret_cast<const quint32 *>(thumb.constBits());
                const qsizetype count = thumb.sizeInBytes() / 4;
                for (qsizetype i = 0; i < count; ++i) {
                    if ((pixels[i] & 0x00FFFFFF) != 0) {
                        hasContent = true;
                        break;
                    }
                }
                if (hasContent) {
                    for (qsizetype i = 3; i < thumb.sizeInBytes(); i += 4) {
                        thumb.bits()[i] = 255;
                    }
                    m_thumbnailCache[target] = thumb;
                    m_lastCaptureTimestamp[target] = now;
                    return thumb;
                }
            }
        }
    }

    // If we have any previously cached capture for this window (e.g. from background sampling
    // when the window was visible on screen), return it rather than the fallback
    if (it != m_thumbnailCache.end() && !it.value().isNull()) {
        return it.value();
    }

    // 2. Fallback for windows that cannot be captured: stylish preview card with app icon and title
    QImage fallback(actualThumbSize, QImage::Format_RGBA8888_Premultiplied);
    fallback.fill(QColor(26, 29, 33, 245));
    QPainter p(&fallback);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const QIcon icon = target->icon();
    if (!icon.isNull()) {
        const int iconDim = qMin(actualThumbSize.width(), actualThumbSize.height()) * 5 / 10;
        const QRect iconRect((actualThumbSize.width() - iconDim) / 2, (actualThumbSize.height() - iconDim) / 2 - 4, iconDim, iconDim);
        icon.paint(&p, iconRect);
    }
    const QString cap = target->caption().trimmed();
    if (!cap.isEmpty()) {
        QFont f(QStringLiteral("sans-serif"));
        f.setPixelSize(qMax(8, actualThumbSize.height() / 7));
        QFontMetrics fm(f);
        const QString elided = fm.elidedText(cap, Qt::ElideRight, actualThumbSize.width() - 8);
        p.setFont(f);
        p.setPen(QColor(200, 205, 215));
        p.drawText(QRect(4, actualThumbSize.height() - fm.height() - 4, actualThumbSize.width() - 8, fm.height()), Qt::AlignCenter, elided);
    }
    p.end();
    m_thumbnailCache[target] = fallback;
    return fallback;
}

void ActiveWindowOutlineEffect::renderBadge(const RenderTarget &renderTarget,
                                           const RenderViewport &viewport,
                                           const QRectF &badgeRect,
                                           const QChar &letter,
                                           const QColor &bgColor,
                                           const QColor &textColor,
                                           bool isJumpMode,
                                           int badgeSize,
                                           const QString &customLabel,
                                           EffectWindow *target,
                                           const QString &recentKey,
                                           bool isRecent)
{
    const qreal scale = viewport.scale();
    const quint64 scaleKey = static_cast<quint64>(qRound(scale * 100));
    const quint64 sizeKey = static_cast<quint64>(badgeSize);
    const quint64 labelKey = static_cast<quint64>(qHash(customLabel));
    const quint64 rKeyHash = static_cast<quint64>(qHash(recentKey));
    const quint64 windowKey = target ? static_cast<quint64>(qHash(target->internalId())) : 0;
    const quint64 previewKey = m_badgePreviewActive ? 0x8000 : 0;
    const quint64 flagsKey = (m_badgeShowIcon ? 0x1000 : 0) | (m_badgeShowThumbnail ? 0x2000 : 0) | (isRecent ? 0x4000 : 0) | (isJumpMode ? 1 : 0) | previewKey;
    const quint64 key = (static_cast<quint64>(letter.unicode()) << 48)
        ^ (static_cast<quint64>(bgColor.rgba()) << 16)
        ^ (static_cast<quint64>(textColor.rgba()))
        ^ (scaleKey << 8)
        ^ (sizeKey << 32)
        ^ (labelKey << 24)
        ^ (rKeyHash << 20)
        ^ (windowKey << 12)
        ^ flagsKey;

    std::shared_ptr<GLTexture> texture;
    auto it = m_badgeTextureCache.find(key);
    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const bool showThumbOnWindow = cfg.windowShowThumbnail;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 lastCapture = m_lastCaptureTimestamp.value(target, 0);
    const qint64 cacheInterval = m_badgeLivePreviews ? 33 : 750;
    const bool cacheExpired = showThumbOnWindow && (now - lastCapture >= cacheInterval);

    if (!cacheExpired && it != m_badgeTextureCache.end()) {
        texture = it.value();
    }
    if (!texture || texture->isNull()) {
        const bool showIconOnWindow = cfg.windowShowIcon;
        const QIcon icon = (showIconOnWindow && target) ? target->icon() : QIcon();
        const QImage thumbnail = (showThumbOnWindow && target)
            ? captureWindowThumbnail(target, renderTarget, viewport, QSize(cfg.windowThumbnailWidth * 2, cfg.windowThumbnailHeight * 2))
            : QImage();

        const QImage img = OutlineBadge::renderBadgeImage(cfg, letter, bgColor, textColor, isJumpMode, badgeSize, scale, customLabel,
                                                          icon, thumbnail, recentKey, isRecent);
        auto uploaded = GLTexture::upload(img);
        if (!uploaded) {
            return;
        }
        uploaded->setFilter(GL_LINEAR);
        uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
        texture = std::shared_ptr<GLTexture>(uploaded.release());
        m_badgeTextureCache[key] = texture;
    }

    const float x0 = badgeRect.left() * scale;
    const float y0 = badgeRect.top() * scale;
    const float x1 = badgeRect.right() * scale;
    const float y1 = badgeRect.bottom() * scale;

    const std::array<GLVertex2D, 6> verts = {{
        {QVector2D(x0, y0), QVector2D(0.0f, 0.0f)},
        {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
        {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
        {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
        {QVector2D(x1, y1), QVector2D(1.0f, 1.0f)},
        {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
    }};

    auto vbo = GLVertexBuffer::streamingBuffer();
    vbo->setVertices(verts);

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, texture->matrix(NormalizedCoordinates));
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

    texture->bind();
    vbo->render(GL_TRIANGLES);
    texture->unbind();

    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }

    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

QString ActiveWindowOutlineEffect::getWindowLocationString(EffectWindow *target) const
{
    if (!target) {
        return QString();
    }
    QString loc;
    const bool isPolish = (QLocale().language() == QLocale::Polish);
    if (!target->isOnCurrentDesktop()) {
        const auto desktops = target->desktops();
        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
            loc = desktops.first()->name();
        }
        if (loc.isEmpty()) {
            const uint num = desktops.isEmpty() ? 0 : desktops.first()->x11DesktopNumber();
            loc = isPolish ? QStringLiteral("Pulpit %1").arg(num) : tr("Desktop %1").arg(num);
        }
        if (target->isMinimized()) {
            loc += isPolish ? QStringLiteral(" [📦 Schowek]") : QStringLiteral(" [📦 Stash]");
        }
    } else if (target->isMinimized()) {
        loc = isPolish ? QStringLiteral("📦 Schowek (Ukryte)") : QStringLiteral("📦 Stash (Hidden)");
    } else if (m_badgePreviewActive || target->isVisible()) {
        loc = isPolish ? QStringLiteral("Bieżący pulpit") : QStringLiteral("Current Desktop");
    } else {
        loc = isPolish ? QStringLiteral("Schowane") : tr("Covered");
    }
    return loc;
}

QList<QRectF> ActiveWindowOutlineEffect::computeHudPillRects(
    const QList<QPair<QChar, EffectWindow *>> &targets,
    const RenderViewport &viewport,
    int badgeSize,
    const OutlineBadge::BadgeConfig &cfg) const
{
    if (targets.isEmpty()) {
        return {};
    }

    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();
    QList<QSizeF> pillSizes;
    pillSizes.reserve(targets.size());

    EffectWindow *recentWin = findRecentWindow();

    for (const auto &pair : targets) {
        EffectWindow *target = pair.second;
        if (!target || target->isDeleted()) {
            continue;
        }

        const bool isRecent = (target == recentWin) || m_recentWindows.contains(target);
        const bool isMostRecent = (target == recentWin);
        const QString recentKey = isMostRecent ? (m_badgePreviewActive ? cfg.jumpRecentKey : m_jumpRecentKey) : QString();
        const QString loc = getWindowLocationString(target);
        const QString caption = target->caption();
        const QString customLabel = m_markedWindowLabels.value(target);
        const bool hasIcon = cfg.hudShowIcon && !target->icon().isNull();
        const bool hasThumb = cfg.hudShowThumbnail && m_thumbnailCache.contains(target);
        const QSize thumbSize = hasThumb ? m_thumbnailCache.value(target).size() : QSize();

        const QSizeF sz = OutlineBadge::computeHudPillSize(cfg, loc, caption, badgeSize, scale,
                                                           customLabel, hasIcon, hasThumb, thumbSize,
                                                           recentKey, isRecent);
        pillSizes.append(sz);
    }

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    const qreal topMargin = (hudPos == 1) ? qRound(58.0 * scale) : 24.0;
    return OutlineBadge::computeHudPillPositions(pillSizes, screenRect, hudPos, topMargin);
}

void ActiveWindowOutlineEffect::renderOffscreenJumpTargets(const RenderTarget &renderTarget,
                                                           const RenderViewport &viewport,
                                                           const QList<QPair<QChar, EffectWindow *>> &targets,
                                                           int badgeSize)
{
    if (targets.isEmpty()) {
        return;
    }

    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();

    struct PillItem {
        std::shared_ptr<GLTexture> texture;
        qreal width;
        qreal height;
    };
    QList<PillItem> pills;

    for (const auto &pair : targets) {
        const QChar letter = pair.first;
        EffectWindow *target = pair.second;
        if (!target || target->isDeleted()) {
            continue;
        }

        const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;

        int groupCount = 0;
        if (cfg.hudGroupWindows && m_groupJumpLetters.contains(letter)) {
            groupCount = m_groupJumpLetters.value(letter).size();
        }

        bool isMarked = m_markedWindows.contains(target);
        // If grouping is unchecked and this window is part of a multi-window group,
        // the enclosing group frame visually groups them and carries the mark styling
        if (!cfg.hudGroupWindows && !m_subLeapActive && m_windowToGroupLetter.contains(target)) {
            const QChar gKey = m_windowToGroupLetter.value(target);
            if (m_groupJumpLetters.contains(gKey) && m_groupJumpLetters.value(gKey).size() > 1) {
                isMarked = false;
            }
        }

        const bool isRecent = (target == findRecentWindow()) || m_recentWindows.contains(target);
        const bool isMostRecent = (target == findRecentWindow());
        const QString recentKey = isMostRecent ? (m_badgePreviewActive ? m_previewBadgeConfig.jumpRecentKey : m_jumpRecentKey) : QString();

        QColor bgCol;
        if (isMarked) {
            bgCol = m_markedWindows.value(target);
        } else if (isRecent) {
            bgCol = resolveColor(1.0, target);
            if (!bgCol.isValid() || bgCol.alpha() == 0) {
                bgCol = m_recentWindowCustomColor.isValid() ? m_recentWindowCustomColor : QColor(52, 152, 219);
            }
        } else {
            bgCol = cfg.defaultBgColor;
        }
        const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);

        const QString loc = getWindowLocationString(target);

        const QString caption = target->caption();
        const QString customLabel = m_markedWindowLabels.value(target);

        const bool showIconOnHud = cfg.hudShowIcon;
        const bool showThumbOnHud = cfg.hudShowThumbnail;
        const QIcon icon = (showIconOnHud && target) ? target->icon() : QIcon();
        const QImage thumbnail = (showThumbOnHud && target)
            ? captureWindowThumbnail(target, renderTarget, viewport, QSize(cfg.hudThumbnailWidth * 2, cfg.hudThumbnailHeight * 2))
            : QImage();

        const QImage img = OutlineBadge::renderOffscreenTargetPill(cfg, letter, bgCol, textCol, loc, caption, badgeSize, scale, customLabel,
                                                                   icon, thumbnail, isMarked, recentKey, isRecent, groupCount);
        auto uploaded = GLTexture::upload(img);
        if (!uploaded) {
            continue;
        }
        uploaded->setFilter(GL_LINEAR);
        uploaded->setWrapMode(GL_CLAMP_TO_EDGE);

        const qreal w = img.width() / scale;
        const qreal h = img.height() / scale;
        pills.append({std::shared_ptr<GLTexture>(uploaded.release()), w, h});
    }

    if (pills.isEmpty()) {
        return;
    }

    QList<QSizeF> pillSizes;
    pillSizes.reserve(pills.size());
    for (const PillItem &item : pills) {
        pillSizes.append(QSizeF(item.width, item.height));
    }

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    const OutlineBadge::BadgeConfig &hudCfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const qreal hudRowGap = (!hudCfg.hudGroupWindows && !m_subLeapActive) ? 40.0 : 12.0;
    const qreal topMargin = (hudPos == 1) ? qRound(58.0 * scale) : 24.0;
    const QList<QRectF> rects = OutlineBadge::computeHudPillPositions(pillSizes, screenRect, hudPos, topMargin, 12.0, hudRowGap);

    struct GroupFrameItem {
        std::shared_ptr<GLTexture> texture;
        QRectF rect;
    };
    QList<GroupFrameItem> groupFrames;

    if (!hudCfg.hudGroupWindows && !m_subLeapActive) {
        // Collect multi-window groups
        QMap<QChar, QList<int>> groupIndices;
        for (int i = 0; i < targets.size(); ++i) {
            EffectWindow *win = targets.at(i).second;
            if (win && m_windowToGroupLetter.contains(win)) {
                const QChar gKey = m_windowToGroupLetter.value(win);
                if (m_groupJumpLetters.contains(gKey) && m_groupJumpLetters.value(gKey).size() > 1) {
                    groupIndices[gKey].append(i);
                }
            }
        }

        for (auto it = groupIndices.constBegin(); it != groupIndices.constEnd(); ++it) {
            const QChar gKey = it.key();
            const auto &indices = it.value();
            if (indices.isEmpty()) {
                continue;
            }

            // Partition group indices by row / column so wrapping does not create cross-row bounding box collision
            QMap<int, QList<int>> segmentToIndices;
            for (int idx : indices) {
                if (idx >= 0 && idx < rects.size()) {
                    const int bucket = (hudPos == 2 || hudPos == 3)
                        ? qRound(rects.at(idx).center().x() / 20.0)
                        : qRound(rects.at(idx).center().y() / 20.0);
                    segmentToIndices[bucket].append(idx);
                }
            }

            EffectWindow *firstWin = m_groupJumpLetters.value(gKey).first();
            const QColor groupColor = m_markedWindows.value(firstWin, QColor(52, 152, 219));
            QString groupTitle = m_markedWindowLabels.value(firstWin);
            if (groupTitle.isEmpty() && firstWin) {
                groupTitle = firstWin->windowClass();
            }
            const int count = m_groupJumpLetters.value(gKey).size();
            const int effGroupFontSize = (hudCfg.hudGroupFontSize > 0) ? hudCfg.hudGroupFontSize : 11;
            const int tabH = qMax(22, effGroupFontSize + 10);
            const bool tabAtBottom = (hudPos == 1);
            const qreal padX = 8.0;

            bool isFirstSegment = true;
            for (auto segIt = segmentToIndices.constBegin(); segIt != segmentToIndices.constEnd(); ++segIt) {
                const auto &segIdxs = segIt.value();
                QRectF groupBoundingRect;
                for (int idx : segIdxs) {
                    if (groupBoundingRect.isEmpty()) {
                        groupBoundingRect = rects.at(idx);
                    } else {
                        groupBoundingRect = groupBoundingRect.united(rects.at(idx));
                    }
                }
                if (groupBoundingRect.isEmpty()) {
                    continue;
                }

                const qreal padBottom = tabAtBottom ? (tabH + 8.0) : 6.0;
                const qreal padTop = tabAtBottom ? 6.0 : (tabH + 8.0);
                QRectF frameRect = groupBoundingRect.adjusted(-padX, -padTop, padX, padBottom);

                if (frameRect.left() < screenRect.left() + 4.0) frameRect.setLeft(screenRect.left() + 4.0);
                if (frameRect.right() > screenRect.right() - 4.0) frameRect.setRight(screenRect.right() - 4.0);
                if (frameRect.top() < screenRect.top() + 4.0) frameRect.setTop(screenRect.top() + 4.0);
                if (frameRect.bottom() > screenRect.bottom() - 4.0) frameRect.setBottom(screenRect.bottom() - 4.0);

                const QImage frameImg = OutlineBadge::renderGroupContainerFrame(
                    frameRect.size(), isFirstSegment ? gKey : QChar(), groupColor,
                    isFirstSegment ? groupTitle : QString(), count, scale,
                    hudCfg.hudGroupCornerRadius, hudCfg.hudGroupBorderWidth,
                    hudCfg.hudGroupFontSize, tabAtBottom);
                auto uploaded = GLTexture::upload(frameImg);
                if (uploaded) {
                    uploaded->setFilter(GL_LINEAR);
                    uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
                    groupFrames.append({std::shared_ptr<GLTexture>(uploaded.release()), frameRect});
                }
                isFirstSegment = false;
            }
        }
    }

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);

    float groupAlpha = 1.0f;
    if (hudCfg.hudGroupShaderEnabled) {
        const qint64 elapsedMs = m_animationClock.isValid() ? m_animationClock.elapsed() : 0;
        const qreal elapsedSec = elapsedMs / 1000.0;
        if (m_breatheEnabled) {
            groupAlpha *= float(computeBreatheFactor(elapsedSec));
        }
        if (m_neonFlickerEnabled) {
            groupAlpha *= float(computeNeonFactor(elapsedSec));
        }
    }

    // 1. Render group container frames behind HUD pills
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(groupAlpha, groupAlpha, groupAlpha, groupAlpha));
    for (const GroupFrameItem &gf : groupFrames) {
        const float x0 = gf.rect.left() * scale;
        const float y0 = gf.rect.top() * scale;
        const float x1 = gf.rect.right() * scale;
        const float y1 = gf.rect.bottom() * scale;

        const std::array<GLVertex2D, 6> verts = {{
            {QVector2D(x0, y0), QVector2D(0.0f, 0.0f)},
            {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
            {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
            {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
            {QVector2D(x1, y1), QVector2D(1.0f, 1.0f)},
            {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
        }};

        auto vbo = GLVertexBuffer::streamingBuffer();
        vbo->setVertices(verts);

        shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, gf.texture->matrix(NormalizedCoordinates));
        gf.texture->bind();
        vbo->render(GL_TRIANGLES);
        gf.texture->unbind();
    }

    // 2. Render HUD pills on top with full modulation
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    for (int i = 0; i < pills.size(); ++i) {
        const PillItem &item = pills.at(i);
        const QRectF &rect = rects.at(i);

        const float x0 = rect.left() * scale;
        const float y0 = rect.top() * scale;
        const float x1 = rect.right() * scale;
        const float y1 = rect.bottom() * scale;

        const std::array<GLVertex2D, 6> verts = {{
            {QVector2D(x0, y0), QVector2D(0.0f, 0.0f)},
            {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
            {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
            {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
            {QVector2D(x1, y1), QVector2D(1.0f, 1.0f)},
            {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
        }};

        auto vbo = GLVertexBuffer::streamingBuffer();
        vbo->setVertices(verts);

        shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, item.texture->matrix(NormalizedCoordinates));
        item.texture->bind();
        vbo->render(GL_TRIANGLES);
        item.texture->unbind();
    }

    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }

    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }

    QRectF allPillsBounds;
    for (const QRectF &r : rects) {
        if (allPillsBounds.isEmpty()) {
            allPillsBounds = r;
        } else {
            allPillsBounds = allPillsBounds.united(r);
        }
    }
    for (const GroupFrameItem &gf : groupFrames) {
        allPillsBounds = allPillsBounds.united(gf.rect);
    }
    const QRectF hintRect = renderHudHintBar(renderTarget, viewport, allPillsBounds);
    renderHudModeHeader(renderTarget, viewport, allPillsBounds, hintRect);
}

QRectF ActiveWindowOutlineEffect::renderHudHintBar(const RenderTarget &renderTarget,
                                                  const RenderViewport &viewport,
                                                  const QRectF &pillsBounds)
{
    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();
    const bool isPolish = (QLocale().language() == QLocale::Polish);

    QString customViewName;
    QColor customViewColor;
    if (m_stashModeActive) {
        customViewName = isPolish ? QStringLiteral("Widok: 📦 Schowek") : QStringLiteral("View: 📦 Stash");
        customViewColor = QColor(46, 204, 113); // Emerald Green
    } else if (m_activeHudView == 0) {
        customViewName = isPolish ? QStringLiteral("Widok: 💻 Standard") : QStringLiteral("View: 💻 Standard");
        customViewColor = QColor(230, 126, 34); // Orange
    } else if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        const auto &ws = m_workspaces.at(m_activeHudView - 1);
        customViewName = (isPolish ? QStringLiteral("Widok: 📁 ") : QStringLiteral("View: 📁 ")) + ws.name;
        customViewColor = ws.color.isValid() ? ws.color : QColor(52, 152, 219);
    } else {
        customViewName = isPolish ? QStringLiteral("Widok: 🌐 Wszystkie") : QStringLiteral("View: 🌐 All");
        customViewColor = QColor(155, 89, 182); // Purple
    }

    const int nameHash = (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) ? (qHash(m_workspaces.at(m_activeHudView - 1).name) % 10000) : 0;
    const int hintBarKey = (m_subLeapActive ? 100000 : 0) + (m_stashModeActive ? 500000 : 0) + m_activeHudView * 1000 + (isPolish ? 100 : 0) + nameHash + qRound(scale * 10);
    if (!m_hudHintBarTexture || m_hudHintBarCacheKey != hintBarKey) {
        const QImage hintImg = OutlineBadge::renderHudActionHintBar(isPolish, scale, m_subLeapActive, m_activeHudView, customViewName, customViewColor, m_stashModeActive);
        auto uploaded = GLTexture::upload(hintImg);
        if (uploaded) {
            uploaded->setFilter(GL_LINEAR);
            uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
            m_hudHintBarTexture = std::shared_ptr<GLTexture>(uploaded.release());
            m_hudHintBarCacheKey = hintBarKey;
        }
    }

    if (!m_hudHintBarTexture) {
        return QRectF();
    }

    const qreal hintW = m_hudHintBarTexture->width() / scale;
    const qreal hintH = m_hudHintBarTexture->height() / scale;

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    qreal hintY = 0.0;
    if (pillsBounds.isValid() && !pillsBounds.isEmpty()) {
        if (hudPos == 1) { // Top HUD: place below pills
            hintY = pillsBounds.bottom() + 12.0;
            if (hintY + hintH > screenRect.bottom() - 10.0) {
                hintY = screenRect.bottom() - hintH - 10.0;
            }
        } else { // Bottom (0), Left (2), Right (3): place above pills
            hintY = pillsBounds.top() - hintH - 12.0;
            if (hintY < screenRect.top() + 10.0) {
                hintY = screenRect.top() + 10.0;
            }
        }
    } else {
        if (hudPos == 1) { // Top HUD without pills: place below header
            hintY = screenRect.top() + qRound(58.0 * scale);
        } else { // Default bottom
            hintY = screenRect.bottom() - hintH - 24.0;
        }
    }

    const qreal hintX = screenRect.center().x() - (hintW / 2.0);
    const QRectF hintRect(hintX, hintY, hintW, hintH);

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    const float x0 = hintRect.left() * scale;
    const float y0 = hintRect.top() * scale;
    const float x1 = hintRect.right() * scale;
    const float y1 = hintRect.bottom() * scale;

    const std::array<GLVertex2D, 6> verts = {{
        {QVector2D(x0, y0), QVector2D(0.0f, 0.0f)},
        {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
        {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
        {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
        {QVector2D(x1, y1), QVector2D(1.0f, 1.0f)},
        {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
    }};

    auto vbo = GLVertexBuffer::streamingBuffer();
    vbo->setVertices(verts);

    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, m_hudHintBarTexture->matrix(NormalizedCoordinates));
    m_hudHintBarTexture->bind();
    vbo->render(GL_TRIANGLES);
    m_hudHintBarTexture->unbind();

    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }
    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
    return hintRect;
}

void ActiveWindowOutlineEffect::renderHudModeHeader(const RenderTarget &renderTarget,
                                                   const RenderViewport &viewport,
                                                   const QRectF &pillsBounds,
                                                   const QRectF &hintBarBounds)
{
    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();
    const bool isPolish = (QLocale().language() == QLocale::Polish);

    QString subLeapTitle;
    QColor subLeapColor;
    int subLeapCount = 0;

    QList<OutlineBadge::HudModeHeaderView> viewTabs;

    int stashCount = 0;
    QString targetWorkspaceName;

    if (m_subLeapActive) {
        subLeapCount = m_subLeapWindows.size();
        if (!m_subLeapWindows.isEmpty() && m_subLeapWindows.first()) {
            EffectWindow *firstWin = m_subLeapWindows.first();
            subLeapTitle = !m_markedWindowLabels.value(firstWin).isEmpty()
                ? m_markedWindowLabels.value(firstWin)
                : (!firstWin->windowClass().isEmpty() ? firstWin->windowClass() : firstWin->caption());
            if (m_markedWindows.contains(firstWin)) {
                subLeapColor = m_markedWindows.value(firstWin);
            }
        }
        if (subLeapTitle.isEmpty()) {
            subLeapTitle = QString(m_subLeapBaseKey);
        }
    } else {
        const auto windows = effects ? effects->stackingOrder() : QList<EffectWindow *>();

        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }
            if (w->isMinimized() || m_stashedWindows.contains(w)) {
                stashCount++;
            }
        }

        if (m_activeHudView == 0) {
            targetWorkspaceName = isPolish ? QStringLiteral("💻 Standard") : QStringLiteral("💻 Standard");
        } else if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
            targetWorkspaceName = QStringLiteral("📁 ") + m_workspaces.at(m_activeHudView - 1).name;
        } else {
            targetWorkspaceName = isPolish ? QStringLiteral("🌐 Wszystkie") : QStringLiteral("🌐 All");
        }

        // 1. Standard count: windows eligible on current desktop (visible or unminimized, not in stash)
        int standardCount = 0;
        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }
            if (w->isOnCurrentDesktop() && !w->isMinimized() && !m_stashedWindows.contains(w)) {
                standardCount++;
            }
        }
        viewTabs.append({
            isPolish ? QStringLiteral("💻 Standard") : QStringLiteral("💻 Standard"),
            QColor(230, 126, 34),
            standardCount,
            m_activeHudView == 0
        });

        // 2. Custom Workspaces (not in stash)
        for (int i = 0; i < m_workspaces.size(); ++i) {
            const auto &ws = m_workspaces.at(i);
            int wsCount = 0;
            for (EffectWindow *w : windows) {
                if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                    continue;
                }
                const auto desktops = w->desktops();
                const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
                if (LeapKWorkspace::matchesWorkspace(ws, w->windowClass(), w->caption(), dNum)) {
                    wsCount++;
                }
            }
            viewTabs.append({
                QStringLiteral("📁 ") + ws.name,
                ws.color.isValid() ? ws.color : QColor(52, 152, 219),
                wsCount,
                m_activeHudView == (i + 1)
            });
        }

        // 3. All windows (excluding stashed)
        int allCount = 0;
        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }
            if (!m_stashedWindows.contains(w)) {
                allCount++;
            }
        }
        viewTabs.append({
            isPolish ? QStringLiteral("🌐 Wszystkie") : QStringLiteral("🌐 All"),
            QColor(155, 89, 182),
            allCount,
            m_activeHudView == (m_workspaces.size() + 1)
        });
    }

    int countsHash = 0;
    for (const auto &vt : viewTabs) {
        countsHash = (countsHash * 31 + vt.windowCount + (vt.isActive ? 1000 : 0)) % 100000;
    }
    const int headerKey = (m_subLeapActive ? 2000000 : 0) + (m_stashModeActive ? 4000000 : 0) + m_activeHudView * 100000
        + (isPolish ? 50000 : 0) + countsHash + stashCount * 13 + qRound(scale * 10);

    if (!m_hudHeaderTexture || m_hudHeaderCacheKey != headerKey) {
        const QImage headerImg = OutlineBadge::renderHudModeHeader(
            isPolish, scale, m_subLeapActive, subLeapTitle, subLeapColor, subLeapCount, viewTabs, stashCount, m_stashModeActive, targetWorkspaceName);
        if (!headerImg.isNull()) {
            auto uploaded = GLTexture::upload(headerImg);
            if (uploaded) {
                uploaded->setFilter(GL_LINEAR);
                uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
                m_hudHeaderTexture = std::shared_ptr<GLTexture>(uploaded.release());
                m_hudHeaderCacheKey = headerKey;
            }
        }
    }

    if (!m_hudHeaderTexture) {
        return;
    }

    const qreal headerW = m_hudHeaderTexture->width() / scale;
    const qreal headerH = m_hudHeaderTexture->height() / scale;

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    qreal headerY = 0.0;
    if (hudPos == 1) { // Top HUD
        headerY = screenRect.top() + 12.0;
    } else { // Bottom (0), Left (2), Right (3): place above hint bar
        if (hintBarBounds.isValid() && !hintBarBounds.isEmpty()) {
            headerY = hintBarBounds.top() - headerH - 8.0;
        } else if (pillsBounds.isValid() && !pillsBounds.isEmpty()) {
            headerY = pillsBounds.top() - headerH - 8.0;
        } else {
            headerY = screenRect.bottom() - headerH - 64.0;
        }
        if (headerY < screenRect.top() + 10.0) {
            headerY = screenRect.top() + 10.0;
        }
    }

    const qreal headerX = screenRect.center().x() - (headerW / 2.0);
    const QRectF headerRect(headerX, headerY, headerW, headerH);

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLint previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &previousSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &previousDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &previousDstAlpha);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_SCISSOR_TEST);

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    const float x0 = headerRect.left() * scale;
    const float y0 = headerRect.top() * scale;
    const float x1 = headerRect.right() * scale;
    const float y1 = headerRect.bottom() * scale;

    const std::array<GLVertex2D, 6> verts = {{
        {QVector2D(x0, y0), QVector2D(0.0f, 0.0f)},
        {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
        {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
        {QVector2D(x1, y0), QVector2D(1.0f, 0.0f)},
        {QVector2D(x1, y1), QVector2D(1.0f, 1.0f)},
        {QVector2D(x0, y1), QVector2D(0.0f, 1.0f)},
    }};

    auto vbo = GLVertexBuffer::streamingBuffer();
    vbo->setVertices(verts);

    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, m_hudHeaderTexture->matrix(NormalizedCoordinates));
    m_hudHeaderTexture->bind();
    vbo->render(GL_TRIANGLES);
    m_hudHeaderTexture->unbind();

    if (scissorWasEnabled) {
        glEnable(GL_SCISSOR_TEST);
    }
    glBlendFuncSeparate(previousSrcRgb, previousDstRgb, previousSrcAlpha, previousDstAlpha);
    if (blendWasEnabled) {
        glEnable(GL_BLEND);
    } else {
        glDisable(GL_BLEND);
    }
}

void ActiveWindowOutlineEffect::slotWindowMinimizedChanged(EffectWindow *w)
{
    if (w && !w->isMinimized()) {
        m_workspaceMinimizedWindows.remove(w);
    }
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
    if (m_markedWindows.contains(w)) {
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
    if (m_markedWindows.contains(w)) {
        effects->addRepaint(expandedGeometryFor(w));
    }
    if (w && w == m_activeWindow) {
        effects->addRepaint(expandedGeometryFor(w));
    }
}

void ActiveWindowOutlineEffect::slotWindowDamaged(EffectWindow *w)
{
    if (!w || w->isDeleted()) {
        return;
    }
    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const bool wantThumbnail = (cfg.windowShowThumbnail || cfg.hudShowThumbnail)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (!wantThumbnail) {
        return;
    }
    m_thumbnailCache.remove(w);
    m_lastCaptureTimestamp.remove(w);
    const bool showContinuousBadges = m_outlineEnabled && (m_badgeDisplayMode == 1) && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked;
    const bool hasBadgesToDraw = (showContinuousBadges || (m_outlineEnabled && m_jumpMode) || m_badgePreviewActive);
    if (hasBadgesToDraw) {
        m_badgeTextureCache.clear();
        effects->addRepaintFull();
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

void ActiveWindowOutlineEffect::slotActiveFullScreenEffectChanged()
{
    // Fullscreen interactive effects (Overview, Present Windows, Desktop Grid)
    // arrange window thumbnails in a transformed QML canvas. Damage old bands
    // so outlines disappear cleanly upon entering and reappear when exiting.
    m_suppressedWindows.clear();
    repaintAllOutlines();
}

QRectF ActiveWindowOutlineEffect::expandedGeometryFor(EffectWindow *w) const
{
    if (!w) {
        return QRectF();
    }
    const bool hasGlow = (w == m_activeWindow) ? (m_glowEnabled || m_glitchEnabled) : m_glowEnabled;
    const qreal glowExtent = hasGlow ? m_glowRadius : 0;
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
    if (!m_outlineEnabled) {
        return false;
    }
    if (m_jumpMode) {
        return true;
    }
    if (effects->hasActiveFullScreenEffect()) {
        return false;
    }
    if (m_badgeDisplayMode == 1) {
        return true;
    }
    if (m_badgeConfig.windowShowThumbnail || m_badgeConfig.hudShowThumbnail || m_badgeShowThumbnail) {
        return true;
    }
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
    bool markedEligible = false;
    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        EffectWindow *marked = it.key();
        if (marked && !marked->isDeleted() && !marked->isMinimized()
            && !marked->isHiddenByShowDesktop() && marked->isOnCurrentDesktop()) {
            markedEligible = true;
            break;
        }
    }
    return m_outlineEnabled && (markedEligible || recentEligible || OutlineVisibility::StateMachine::isEffectActive(m_outlineEnabled, showPending, activeEligible, delayedEligible));
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
    result += QStringLiteral("\nblockingPopup=%1 popupBlocked=%2 visibilityOpacity=%3 targetOpacity=%4 delayedHideWindow=%5 delayedHideOpacity=%6 showTimerActive=%7 recentCount=%8 recentOpacity=%9 markedCount=%10 effectActive=%11")
        .arg(hasVisibleBlockingPopup())
        .arg(m_visibilityState.popupBlocked)
        .arg(m_visibilityState.visibilityOpacity)
        .arg(m_visibilityState.targetOpacity)
        .arg(m_delayedHideWindow != nullptr)
        .arg(m_delayedHideState.opacity)
        .arg(m_windowShowTimer && m_windowShowTimer->isActive())
        .arg(m_recentWindows.size())
        .arg(m_recentWindowOpacity)
        .arg(m_markedWindows.size())
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
    if (!m_outlineEnabled) {
        return;
    }
    if (m_visibilityState.needsSelfHeal(hasVisibleBlockingPopup())) {
        updatePopupBlockingState();
    }

    EffectWindow *w = m_activeWindow;
    if (effects->hasActiveFullScreenEffect()) {
        if (w && !m_suppressedWindows.contains(w)) {
            m_suppressedWindows.insert(w);
            data.paint += expandedGeometryFor(w).toAlignedRect();
        }
        if (m_delayedHideWindow && !m_suppressedWindows.contains(m_delayedHideWindow)) {
            m_suppressedWindows.insert(m_delayedHideWindow);
            data.paint += expandedGeometryFor(m_delayedHideWindow).toAlignedRect();
        }
        for (EffectWindow *recent : m_recentWindows) {
            if (recent && !m_suppressedWindows.contains(recent)) {
                m_suppressedWindows.insert(recent);
                data.paint += expandedGeometryFor(recent).toAlignedRect();
            }
        }
        for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
            EffectWindow *marked = it.key();
            if (marked && !m_suppressedWindows.contains(marked)) {
                m_suppressedWindows.insert(marked);
                data.paint += expandedGeometryFor(marked).toAlignedRect();
            }
        }
        return;
    }

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

    int rankMarked = m_stylePrecedence.indexOf(QStringLiteral("Marked"));
    int rankRecent = m_stylePrecedence.indexOf(QStringLiteral("Recent"));
    if (rankMarked < 0) rankMarked = 1;
    if (rankRecent < 0) rankRecent = 2;
    const bool recentOverridesMark = (rankRecent < rankMarked);

    if (m_highlightRecentWindow) {
        const int count = qMin(m_recentWindows.size(), m_recentWindowsCount);
        for (int i = 0; i < count; ++i) {
            EffectWindow *recent = m_recentWindows.at(i);
            const bool markedRecentSkipped = m_markedWindows.contains(recent) && !recentOverridesMark;
            if (recent && recent != w && !markedRecentSkipped && !recent->isDeleted()
                && recent->isOnCurrentDesktop() && !recent->isMinimized()
                && !recent->isHiddenByShowDesktop()
                && !(m_hideOnFullScreen && recent->isFullScreen())
                && !isBlockingPopup(recent)) {
                data.paint += expandedGeometryFor(recent).toAlignedRect();
            }
        }
    }

    for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
        EffectWindow *marked = it.key();
        const bool recentMarkedSkipped = m_highlightRecentWindow && m_recentWindows.contains(marked) && recentOverridesMark;
        if (!marked || marked == w || recentMarkedSkipped || marked->isDeleted() || !marked->isOnCurrentDesktop()) {
            continue;
        }
        if (marked->isMinimized() || marked->isHiddenByShowDesktop()) {
            if (!m_suppressedWindows.contains(marked)) {
                m_suppressedWindows.insert(marked);
                data.paint += expandedGeometryFor(marked).toAlignedRect();
            }
            continue;
        }
        m_suppressedWindows.remove(marked);
        if (!(m_hideOnFullScreen && marked->isFullScreen())) {
            data.paint += expandedGeometryFor(marked).toAlignedRect();
        }
    }

    if (m_jumpMode) {
        for (auto it = m_jumpLetterToWindow.constBegin(); it != m_jumpLetterToWindow.constEnd(); ++it) {
            EffectWindow *jw = it.value();
            if (jw && !jw->isDeleted() && jw->isOnCurrentDesktop()) {
                data.paint += jw->frameGeometry().toAlignedRect();
            }
        }
        data.paint += effects->virtualScreenGeometry();
    } else if (m_badgeDisplayMode == 1) {
        const QMap<QChar, EffectWindow *> backgroundBadges = assignJumpLetters();
        for (auto it = backgroundBadges.constBegin(); it != backgroundBadges.constEnd(); ++it) {
            EffectWindow *jw = it.value();
            if (jw && !jw->isDeleted() && jw->isOnCurrentDesktop()) {
                data.paint += jw->frameGeometry().toAlignedRect();
            }
        }
    }
    if (m_badgePreviewActive) {
        data.paint += effects->virtualScreenGeometry();
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
    const bool isScreenLocked = (effects && effects->isScreenLocked()) || (w && w->isLockScreen());
    if (isScreenLocked) {
        if (m_jumpMode) {
            cancelJumpMode();
        }
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        return effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
#else
        effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
        return;
#endif
    }

    const bool isFullScreenActive = effects->hasActiveFullScreenEffect();
    const bool isScreenTransformed = (mask & (PAINT_SCREEN_TRANSFORMED | PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS));
    const bool inhibited = isFullScreenActive || isScreenTransformed;

    const bool showPending = (m_windowShowTimer && m_windowShowTimer->isActive());
    const bool drawOutline = !inhibited && !showPending && shouldOutlineWindow(w) && m_visibilityState.isEffectivelyVisible()
        && !w->isMinimized() && !w->isHiddenByShowDesktop() && w->isOnCurrentDesktop();
    const bool drawDelayed = !inhibited && (m_delayedHideWindow && m_delayedHideWindow != w
        && isEligibleOutlineWindow(m_delayedHideWindow)
        && !m_delayedHideWindow->isDeleted() && !m_delayedHideWindow->isMinimized()
        && !m_delayedHideWindow->isHiddenByShowDesktop() && m_delayedHideWindow->isOnCurrentDesktop()
        && m_delayedHideState.isEffectivelyVisible());
    int rankMarked = m_stylePrecedence.indexOf(QStringLiteral("Marked"));
    int rankRecent = m_stylePrecedence.indexOf(QStringLiteral("Recent"));
    if (rankMarked < 0) rankMarked = 1;
    if (rankRecent < 0) rankRecent = 2;
    const bool recentOverridesMark = (rankRecent < rankMarked);

    bool hasRecentToDraw = false;
    if (m_outlineEnabled && !inhibited && m_highlightRecentWindow && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked) {
        const int count = qMin(m_recentWindows.size(), m_recentWindowsCount);
        for (int i = 0; i < count; ++i) {
            EffectWindow *recent = m_recentWindows.at(i);
            const bool markedRecentSkipped = m_markedWindows.contains(recent) && !recentOverridesMark;
            if (recent && recent != w && !markedRecentSkipped && isEligibleOutlineWindow(recent) && !recent->isDeleted() && !recent->isMinimized()
                && !recent->isHiddenByShowDesktop() && recent->isOnCurrentDesktop()
                && !(m_hideOnFullScreen && recent->isFullScreen())
                && !isBlockingPopup(recent)) {
                hasRecentToDraw = true;
                break;
            }
        }
    }
    bool hasMarkedToDraw = false;
    if (m_outlineEnabled && !inhibited && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked) {
        for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
            EffectWindow *marked = it.key();
            const bool recentMarkedSkipped = m_highlightRecentWindow && m_recentWindows.contains(marked) && recentOverridesMark;
            if (marked && marked != w && !recentMarkedSkipped
                && isEligibleOutlineWindow(marked)
                && !marked->isDeleted() && !marked->isMinimized()
                && !marked->isHiddenByShowDesktop() && marked->isOnCurrentDesktop()
                && !(m_hideOnFullScreen && marked->isFullScreen())
                && !isBlockingPopup(marked)) {
                hasMarkedToDraw = true;
                break;
            }
        }
    }
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    std::unique_ptr<OutlineCursorOverlay> cursorOverlay;
    // Only defer the cursor for the ordinary output pass. Transformed/offscreen
    // effects own their own cursor handling and must retain their scene filters.
    if ((drawOutline || drawDelayed || hasRecentToDraw || hasMarkedToDraw || m_jumpMode) && m_sceneView && m_sceneView->scene() == effects->scene()
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

    if (isSessionLockOrLogoutActive()) {
        if (m_jumpMode) {
            cancelJumpMode();
        }
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        return true;
#else
        return;
#endif
    }

    const qreal elapsedSec = m_animationClock.isValid() ? (m_animationClock.elapsed() / 1000.0) : 0.0;
    const qreal sampleInterval = 1.0 / qMax<qreal>(0.1, m_ambilightFrequency);
    const bool timeToSample = m_ambilightSampleNeeded || ((elapsedSec - m_lastAmbilightSampleTime) >= sampleInterval);

    if (!inhibited && (m_ambilightEnabled || m_ensurePerimeterContrast || m_markSmartAdaptive) && m_activeWindow && timeToSample) {
        const QPoint windowCenter = m_activeWindow->frameGeometry().center().toPoint();
        const bool isScreenForWindow = !screen || QRect(screen->geometry()).contains(windowCenter);
        if (isScreenForWindow) {
            if (sampleAmbilightColor(renderTarget, viewport, m_activeWindow)) {
                m_ambilightSampleNeeded = false;
                m_lastAmbilightSampleTime = elapsedSec;
            }
        }
    }

    const OutlineBadge::BadgeConfig &activeBadgeCfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const bool wantThumbnail = (activeBadgeCfg.windowShowThumbnail || activeBadgeCfg.hudShowThumbnail || m_badgeShowThumbnail)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (wantThumbnail && !inhibited) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastCapture = m_lastCaptureTimestamp.value(m_activeWindow, 0);
        const qint64 throttleInterval = m_badgeLivePreviews ? 33 : 1000;
        if (m_activeWindow && m_activeWindow->isOnCurrentDesktop()
            && !m_activeWindow->isMinimized() && m_activeWindow->isVisible() && !m_activeWindow->isDeleted()
            && (!m_thumbnailCache.contains(m_activeWindow) || (nowMs - lastCapture >= throttleInterval))) {
            captureWindowThumbnail(m_activeWindow, renderTarget, viewport,
                                   QSize(activeBadgeCfg.hudThumbnailWidth * 2, activeBadgeCfg.hudThumbnailHeight * 2));
        }
    }

    const bool showContinuousBadges = m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && (m_badgeDisplayMode == 1) && !inhibited && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked;
    const bool hasBadgesToDraw = (showContinuousBadges || (m_outlineEnabled && m_jumpMode) || m_badgePreviewActive);

    if ((!m_outlineEnabled && !m_badgePreviewActive) || (!drawOutline && !drawDelayed && !hasRecentToDraw && !hasMarkedToDraw && !hasBadgesToDraw)) {
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        return true;
#else
        return;
#endif
    }

    if (drawDelayed) {
        const QRectF delayedGeo = m_delayedHideWindow->frameGeometry();
        if (!m_outlineCache.contains(m_delayedHideWindow) || m_lastGeometry.value(m_delayedHideWindow) != delayedGeo || m_outlineCache.value(m_delayedHideWindow).borderWidth != m_width) {
            cacheWindowOutline(m_delayedHideWindow, delayedGeo, m_width);
            m_lastGeometry[m_delayedHideWindow] = delayedGeo;
        }
        renderOutline(renderTarget, viewport, m_outlineCache[m_delayedHideWindow], deviceRegion, m_delayedHideWindow, m_delayedHideState.opacity);
    }

    if (hasMarkedToDraw) {
        for (auto it = m_markedWindows.constBegin(); it != m_markedWindows.constEnd(); ++it) {
            EffectWindow *marked = it.key();
            const bool recentMarkedSkipped = m_highlightRecentWindow && m_recentWindows.contains(marked) && recentOverridesMark;
            if (!marked || marked == w || recentMarkedSkipped
                || !isEligibleOutlineWindow(marked)
                || marked->isDeleted() || marked->isMinimized()
                || marked->isHiddenByShowDesktop() || !marked->isOnCurrentDesktop()
                || (m_hideOnFullScreen && marked->isFullScreen())
                || isBlockingPopup(marked)) {
                continue;
            }

            const QRectF markedGeo = marked->frameGeometry();
            const qreal markedWidth = std::max<qreal>(1.0, m_width - m_markInactiveWidthReduction);
            if (!m_outlineCache.contains(marked) || m_lastGeometry.value(marked) != markedGeo || m_outlineCache.value(marked).borderWidth != markedWidth) {
                cacheWindowOutline(marked, markedGeo, markedWidth);
                m_lastGeometry[marked] = markedGeo;
            }

            const qreal inactiveOpacity = m_visibilityState.visibilityOpacity;
            renderOutline(renderTarget, viewport, m_outlineCache[marked], deviceRegion, marked, inactiveOpacity);
        }
    }

    if (hasRecentToDraw) {
        const int count = qMin(m_recentWindows.size(), m_recentWindowsCount);
        for (int i = count - 1; i >= 0; --i) {
            EffectWindow *recent = m_recentWindows.at(i);
            const bool markedRecentSkipped = m_markedWindows.contains(recent) && !recentOverridesMark;
            if (!recent || recent == w || markedRecentSkipped || !isEligibleOutlineWindow(recent) || recent->isDeleted() || recent->isMinimized()
                || recent->isHiddenByShowDesktop() || !recent->isOnCurrentDesktop()
                || (m_hideOnFullScreen && recent->isFullScreen())
                || isBlockingPopup(recent)) {
                continue;
            }

            const QRectF recentGeo = recent->frameGeometry();
            const qreal recentWidth = OutlineGeometry::computeRecentWindowWidth(i, m_width, m_recentWindowWidthReduction);
            if (!m_outlineCache.contains(recent) || m_lastGeometry.value(recent) != recentGeo || m_outlineCache.value(recent).borderWidth != recentWidth) {
                cacheWindowOutline(recent, recentGeo, recentWidth);
                m_lastGeometry[recent] = recentGeo;
            }

            const qreal interpolatedOpacity = OutlineColor::computeRecentWindowOpacity(i, m_recentWindowsCount, m_recentWindowOpacity);
            const qreal recentOpacity = interpolatedOpacity * m_visibilityState.visibilityOpacity;
            renderOutline(renderTarget, viewport, m_outlineCache[recent], deviceRegion, recent, recentOpacity);
        }
    }

    if (drawOutline) {
        const QRectF currentGeo = w->frameGeometry();
        if (!m_outlineCache.contains(w) || m_lastGeometry.value(w) != currentGeo || m_outlineCache.value(w).borderWidth != m_width) {
            cacheWindowOutline(w, currentGeo, m_width);
            m_lastGeometry[w] = currentGeo;
        }
        renderOutline(renderTarget, viewport, m_outlineCache[w], deviceRegion, w, m_visibilityState.visibilityOpacity);
    }

    if (!m_jumpMode && showContinuousBadges) {
        const int badgeH = m_badgeConfig.windowBadgeSize;
        const QMap<QChar, EffectWindow *> backgroundBadges = assignJumpLetters();
        const QRectF screenRect = viewport.renderRect();
        for (auto it = backgroundBadges.constBegin(); it != backgroundBadges.constEnd(); ++it) {
            const QChar letter = it.key();
            EffectWindow *target = it.value();
            if (!target || target == w || target->isDeleted() || !isEligibleTargetWindow(target)
                || target->isMinimized() || !target->isOnCurrentDesktop()
                || !target->isVisible()
                || (m_hideOnFullScreen && target->isFullScreen())) {
                continue;
            }
            const QRectF geo = target->frameGeometry();
            const QRectF visibleWindowGeo = geo.intersected(screenRect);
            if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50) {
                continue;
            }

            const bool isMarked = m_markedWindows.contains(target);
            const bool isRecent = (target == findRecentWindow()) || m_recentWindows.contains(target);
            QColor bgCol;
            if (isMarked) {
                bgCol = m_markedWindows.value(target);
            } else if (isRecent) {
                bgCol = resolveColor(1.0, target);
                if (!bgCol.isValid() || bgCol.alpha() == 0) {
                    bgCol = m_recentWindowCustomColor.isValid() ? m_recentWindowCustomColor : QColor(52, 152, 219);
                }
            } else {
                bgCol = m_badgeConfig.defaultBgColor;
            }
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);
            const QString customLabel = m_markedWindowLabels.value(target);
            const qreal badgeW = OutlineBadge::computeBadgeWidth(m_badgeConfig, badgeH, customLabel);
            QRectF badgeRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, m_badgePosition, badgeW, badgeH, m_badgeMargin);
            if (badgeRect.left() < screenRect.left() + 4.0) badgeRect.moveLeft(screenRect.left() + 4.0);
            if (badgeRect.right() > screenRect.right() - 4.0) badgeRect.moveRight(screenRect.right() - 4.0);
            if (badgeRect.top() < screenRect.top() + 4.0) badgeRect.moveTop(screenRect.top() + 4.0);
            if (badgeRect.bottom() > screenRect.bottom() - 4.0) badgeRect.moveBottom(screenRect.bottom() - 4.0);

            if (isBadgeOccluded(target, badgeRect)) {
                const int altPositions[] = { 4 /* Center */, 1 /* Top */, 0 /* TopLeft */, 2 /* TopRight */, 7 /* Bottom */, 6 /* BottomLeft */, 8 /* BottomRight */, 3 /* Left */, 5 /* Right */ };
                bool foundVisiblePosition = false;
                for (int altPos : altPositions) {
                    if (altPos == m_badgePosition) continue;
                    QRectF altRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, altPos, badgeW, badgeH, m_badgeMargin);
                    if (altRect.left() < screenRect.left() + 4.0) altRect.moveLeft(screenRect.left() + 4.0);
                    if (altRect.right() > screenRect.right() - 4.0) altRect.moveRight(screenRect.right() - 4.0);
                    if (altRect.top() < screenRect.top() + 4.0) altRect.moveTop(screenRect.top() + 4.0);
                    if (altRect.bottom() > screenRect.bottom() - 4.0) altRect.moveBottom(screenRect.bottom() - 4.0);

                    if (!isBadgeOccluded(target, altRect)) {
                        badgeRect = altRect;
                        foundVisiblePosition = true;
                        break;
                    }
                }
                if (!foundVisiblePosition) {
                    continue;
                }
            }
            renderBadge(renderTarget, viewport, badgeRect, letter, bgCol, textCol, false, m_badgeConfig.windowBadgeSize, customLabel, target, QString(), isRecent);
        }
    }

    if (m_jumpMode && !inhibited) {
        const int jumpH = m_badgeConfig.jumpBadgeSize;
        const QRectF screenRect = viewport.renderRect();
        QList<QPair<QChar, EffectWindow *>> offscreenTargets;
        QList<QPair<QChar, EffectWindow *>> onscreenTargets;

        if (m_subLeapActive) {
            // In Sub-Leap disambiguation mode, distribute targets mutually exclusively:
            // Visible unobscured windows on current desktop get badges on the windows;
            // covered, minimized, or other-desktop windows appear in the edge HUD.
            const bool forceHud = m_badgeConfig.hudIncludeAll || !m_badgeConfig.windowBadgesEnabled;
            for (const auto &pair : m_subLeapTargets) {
                EffectWindow *target = pair.second;
                if (!target || target->isDeleted() || !isEligibleTargetWindow(target)) {
                    continue;
                }

                if (forceHud) {
                    offscreenTargets.append(pair);
                }

                const bool onCurrent = target->isOnCurrentDesktop();
                const bool minimized = target->isMinimized();

                if (!onCurrent || minimized) {
                    if (!forceHud) {
                        offscreenTargets.append(pair);
                    }
                    continue;
                }

                const QRectF geo = target->frameGeometry();
                const QRectF visibleWindowGeo = geo.intersected(screenRect);
                if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50 || isWindowObscured(target)) {
                    if (!forceHud) {
                        offscreenTargets.append(pair);
                    }
                    continue;
                }

                if (m_badgeConfig.windowBadgesEnabled) {
                    onscreenTargets.append(pair);
                }
            }
        } else {
            if (m_badgeConfig.hudGroupWindows) {
                // Mode A: Group multi-instance windows into single card with counter
                for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
                    const QChar letter = it.key();
                    const auto &winList = it.value();
                    if (winList.isEmpty()) {
                        continue;
                    }
                    EffectWindow *firstWin = winList.first();
                    if (!firstWin || firstWin == w || firstWin->isDeleted() || !isEligibleTargetWindow(firstWin)) {
                        continue;
                    }

                    const bool forceHud = m_badgeConfig.hudIncludeAll || !m_badgeConfig.windowBadgesEnabled;
                    if (forceHud) {
                        offscreenTargets.append(qMakePair(letter, firstWin));
                    } else {
                        bool hasOffscreenMember = false;
                        for (EffectWindow *target : winList) {
                            if (!target || target == w || target->isDeleted()) continue;
                            if (!target->isOnCurrentDesktop() || target->isMinimized()) {
                                hasOffscreenMember = true;
                                break;
                            }
                            const QRectF geo = target->frameGeometry();
                            const QRectF visibleWindowGeo = geo.intersected(screenRect);
                            if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50 || isWindowObscured(target)) {
                                hasOffscreenMember = true;
                                break;
                            }
                        }
                        if (hasOffscreenMember) {
                            offscreenTargets.append(qMakePair(letter, firstWin));
                        }
                    }

                    if (m_badgeConfig.windowBadgesEnabled) {
                        for (EffectWindow *target : winList) {
                            if (!target || target == w || target->isDeleted() || !isEligibleTargetWindow(target)) {
                                continue;
                            }
                            if (!target->isOnCurrentDesktop() || target->isMinimized()) {
                                continue;
                            }
                            const QRectF geo = target->frameGeometry();
                            const QRectF visibleWindowGeo = geo.intersected(screenRect);
                            if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50) {
                                continue;
                            }
                            onscreenTargets.append(qMakePair(letter, target));
                        }
                    }
                }
            } else {
                // Mode B: Show all windows directly with unique jump letters
                for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
                    for (EffectWindow *target : it.value()) {
                        if (!target || target == w || target->isDeleted() || !isEligibleTargetWindow(target)) {
                            continue;
                        }
                        const QChar directKey = m_windowToDirectLetter.value(target);
                        if (directKey.isNull()) {
                            continue;
                        }

                        const bool forceHud = m_badgeConfig.hudIncludeAll || !m_badgeConfig.windowBadgesEnabled;
                        if (forceHud) {
                            offscreenTargets.append(qMakePair(directKey, target));
                        }

                        const bool onCurrent = target->isOnCurrentDesktop();
                        const bool minimized = target->isMinimized();

                        if (!onCurrent || minimized) {
                            if (!forceHud) {
                                offscreenTargets.append(qMakePair(directKey, target));
                            }
                            continue;
                        }

                        const QRectF geo = target->frameGeometry();
                        const QRectF visibleWindowGeo = geo.intersected(screenRect);
                        if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50) {
                            if (!forceHud) {
                                offscreenTargets.append(qMakePair(directKey, target));
                            }
                            continue;
                        }

                        if (m_badgeConfig.windowBadgesEnabled) {
                            onscreenTargets.append(qMakePair(directKey, target));
                        }
                    }
                }
            }
        }

        QList<QRectF> hudRects = computeHudPillRects(offscreenTargets, viewport, m_badgeConfig.hudBadgeSize, m_badgeConfig);

        for (const auto &pair : onscreenTargets) {
            const QChar letter = pair.first;
            EffectWindow *target = pair.second;
            const QRectF geo = target->frameGeometry();
            const QRectF visibleWindowGeo = geo.intersected(screenRect);

            const bool isMarked = m_markedWindows.contains(target);
            const bool isRecent = (target == findRecentWindow()) || m_recentWindows.contains(target);
            const bool isMostRecent = (target == findRecentWindow());
            const QString recentKey = isMostRecent ? m_jumpRecentKey : QString();

            QColor bgCol;
            if (isMarked) {
                bgCol = m_markedWindows.value(target);
            } else if (isRecent) {
                bgCol = resolveColor(1.0, target);
                if (!bgCol.isValid() || bgCol.alpha() == 0) {
                    bgCol = m_recentWindowCustomColor.isValid() ? m_recentWindowCustomColor : QColor(52, 152, 219);
                }
            } else {
                bgCol = m_badgeConfig.defaultBgColor;
            }
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);
            const QString customLabel = m_markedWindowLabels.value(target);
            const qreal jumpW = OutlineBadge::computeBadgeWidth(m_badgeConfig, jumpH, customLabel, recentKey);
            QRectF badgeRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, m_badgePosition, jumpW, jumpH, m_badgeMargin);
            if (badgeRect.left() < screenRect.left() + 4.0) badgeRect.moveLeft(screenRect.left() + 4.0);
            if (badgeRect.right() > screenRect.right() - 4.0) badgeRect.moveRight(screenRect.right() - 4.0);
            if (badgeRect.top() < screenRect.top() + 4.0) badgeRect.moveTop(screenRect.top() + 4.0);
            if (badgeRect.bottom() > screenRect.bottom() - 4.0) badgeRect.moveBottom(screenRect.bottom() - 4.0);

            if (isBadgeOccluded(target, badgeRect, hudRects)) {
                const int altPositions[] = { 4 /* Center */, 1 /* Top */, 0 /* TopLeft */, 2 /* TopRight */, 7 /* Bottom */, 6 /* BottomLeft */, 8 /* BottomRight */, 3 /* Left */, 5 /* Right */ };
                bool foundVisiblePosition = false;
                for (int altPos : altPositions) {
                    if (altPos == m_badgePosition) continue;
                    QRectF altRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, altPos, jumpW, jumpH, m_badgeMargin);
                    if (altRect.left() < screenRect.left() + 4.0) altRect.moveLeft(screenRect.left() + 4.0);
                    if (altRect.right() > screenRect.right() - 4.0) altRect.moveRight(screenRect.right() - 4.0);
                    if (altRect.top() < screenRect.top() + 4.0) altRect.moveTop(screenRect.top() + 4.0);
                    if (altRect.bottom() > screenRect.bottom() - 4.0) altRect.moveBottom(screenRect.bottom() - 4.0);

                    if (!isBadgeOccluded(target, altRect, hudRects)) {
                        badgeRect = altRect;
                        foundVisiblePosition = true;
                        break;
                    }
                }
                if (!foundVisiblePosition) {
                    if (!m_badgeConfig.hudIncludeAll) {
                        offscreenTargets.append(qMakePair(letter, target));
                        hudRects = computeHudPillRects(offscreenTargets, viewport, m_badgeConfig.hudBadgeSize, m_badgeConfig);
                    }
                    continue;
                }
            }
            renderBadge(renderTarget, viewport, badgeRect, letter, bgCol, textCol, true, m_badgeConfig.jumpBadgeSize, customLabel, target, recentKey, isRecent);
        }

        if (!offscreenTargets.isEmpty()) {
            renderOffscreenJumpTargets(renderTarget, viewport, offscreenTargets, m_badgeConfig.hudBadgeSize);
        } else {
            const QRectF hintRect = renderHudHintBar(renderTarget, viewport, QRectF());
            renderHudModeHeader(renderTarget, viewport, QRectF(), hintRect);
        }
    }

    if (m_badgePreviewActive && !inhibited) {
        renderBadgePreview(renderTarget, viewport);
    }

    if (m_badgeLivePreviews && hasBadgesToDraw && wantThumbnail && !inhibited) {
        effects->addRepaintFull();
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

        OutlineAmbilight::Settings smartSettings = settings;
        smartSettings.mode = OutlineAmbilight::Mode::Smart;
        const QColor smartColor = OutlineAmbilight::processRgbaSamples(scaled.constBits(), contentSize.width() * contentSize.height(), smartSettings);
        if (w && smartColor.isValid()) {
            m_smartAccentCache[w] = smartColor;
        }

        if (m_ambilightEnabled || m_ensurePerimeterContrast) {
            QColor newTargetColor;
            if (m_ambilightEnabled) {
                newTargetColor = OutlineAmbilight::processRgbaSamples(scaled.constBits(), contentSize.width() * contentSize.height(), settings);
            } else {
                const QColor baseColor = m_useAccentColor ? qApp->palette().color(QPalette::Highlight) : m_customColor;
                newTargetColor = OutlineAmbilight::enforcePerimeterContrast(baseColor, scaled.constBits(), contentSize.width(), contentSize.height(), settings);
            }
            applyNewAmbilightColor(newTargetColor);
        }
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

    const OutlineRect windowRect(sampleGeo.toRect());
    const OutlineRect destContentRect(0, 0, kSampleDim, kSampleDim);
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
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, OutlineRect(topSrc), OutlineRect(0, 0, 32, 2));
            }
            if (botSrc.width() >= 4 && botSrc.height() >= 1) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, OutlineRect(botSrc), OutlineRect(0, 2, 32, 2));
            }
            if (leftSrc.width() >= 1 && leftSrc.height() >= 4) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, OutlineRect(leftSrc), OutlineRect(0, 4, 32, 2));
            }
            if (rightSrc.width() >= 1 && rightSrc.height() >= 4) {
                m_outsideFbo->blitFromRenderTarget(renderTarget, viewport, OutlineRect(rightSrc), OutlineRect(0, 6, 32, 2));
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

    OutlineAmbilight::Settings smartSettings = settings;
    smartSettings.mode = OutlineAmbilight::Mode::Smart;
    const QColor smartColor = OutlineAmbilight::processRgbaSamples(windowPixels, kSampleDim * kSampleDim, smartSettings);
    if (w && smartColor.isValid()) {
        m_smartAccentCache[w] = smartColor;
    }

    if (m_ambilightEnabled || m_ensurePerimeterContrast) {
        QColor newTargetColor;
        if (m_ambilightEnabled) {
            newTargetColor = OutlineAmbilight::processRgbaSamples(windowPixels, kSampleDim * kSampleDim, settings);
        } else {
            const QColor baseColor = m_useAccentColor ? qApp->palette().color(QPalette::Highlight) : m_customColor;
            newTargetColor = OutlineAmbilight::enforcePerimeterContrast(baseColor, windowPixels, kSampleDim, kSampleDim, settings);
        }
        applyNewAmbilightColor(newTargetColor);
    }
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

#include "leapk.moc"
