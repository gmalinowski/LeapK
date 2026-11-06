/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

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

    const bool isPlayingAudio = outlinedWindow ? m_audioTracker.isWindowPlaying(outlinedWindow->pid(), outlinedWindow->caption(), outlinedWindow->windowClass()) : false;
    bool matchFrameMode = true;
    if (m_audioWindowFrameShaderMode == QLatin1String("active")) {
        matchFrameMode = isFocus;
    } else if (m_audioWindowFrameShaderMode == QLatin1String("inactive")) {
        matchFrameMode = !isFocus;
    }
    const bool isAudioShader = isPlayingAudio && m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty() && matchFrameMode;
    const LeapKShader::CustomShaderEffect &effectiveShader = isAudioShader ? m_audioShaderEffect : m_activeCustomShader;
    const bool customGlow = isAudioShader
        ? (effectiveShader.target == QStringLiteral("glow") || effectiveShader.target == QStringLiteral("both"))
        : (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty() && (effectiveShader.target == QStringLiteral("glow") || effectiveShader.target == QStringLiteral("both")));
    const bool enableGlow = isFocus ? (m_glowEnabled || m_glitchEnabled || customGlow) : (m_glowEnabled || customGlow);

    std::unique_ptr<GLShader> &glowShaderPtr = isAudioShader ? m_audioGlowShader : m_glowShader;

    if (enableGlow && dynamicColor.alpha() > 0) {
        if (!glowShaderPtr) {
            const EglContext *context = EglContext::currentContext();
            const bool isOpenGLES = !epoxy_is_desktop_gl();
            const bool modern = isOpenGLES
                ? (context ? context->glslVersion() >= Version(3, 0) : true)
                : (context ? context->glslVersion() >= Version(1, 40) : true);

            if (customGlow) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                glowShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), LeapKShader::buildGlowFragmentSource(effectiveShader, modern, isOpenGLES));
#else
                glowShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), LeapKShader::buildGlowFragmentSource(effectiveShader, modern, isOpenGLES));
#endif
                if (!glowShaderPtr
#ifndef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
                    || !glowShaderPtr->isValid()
#endif
                ) {
                    qWarning() << "ActiveWindowOutline: Custom glow shader failed to compile, falling back.";
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                    glowShaderPtr = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::UniformColor, glowVertexSource(), glowFragmentSource());
#else
                    glowShaderPtr = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::Border, QByteArray(), glowFragmentSource());
#endif
                }
            } else {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                glowShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), glowFragmentSource());
#else
                glowShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), glowFragmentSource());
#endif
            }
        }
#ifdef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
        if (glowShaderPtr) {
#else
        if (glowShaderPtr && glowShaderPtr->isValid()) {
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

                ShaderBinder glowBinder(glowShaderPtr.get());
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
                    for (const auto &param : effectiveShader.parameters) {
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

    const bool customOutline = isAudioShader
        ? (effectiveShader.target == QStringLiteral("outline") || effectiveShader.target == QStringLiteral("both"))
        : (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty() && (effectiveShader.target == QStringLiteral("outline") || effectiveShader.target == QStringLiteral("both")));
    const bool useCustomShader = m_fadeCorners || isDashed || customOutline;
    std::unique_ptr<GLShader> &outlineShaderPtr = isAudioShader ? m_audioOutlineShader : m_fadeCornersShader;
    if (useCustomShader) {
        if (!outlineShaderPtr) {
            const EglContext *context = EglContext::currentContext();
            const bool isOpenGLES = !epoxy_is_desktop_gl();
            const bool modern = isOpenGLES
                ? (context ? context->glslVersion() >= Version(3, 0) : true)
                : (context ? context->glslVersion() >= Version(1, 40) : true);

            if (customOutline) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                outlineShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), LeapKShader::buildOutlineFragmentSource(effectiveShader, modern, isOpenGLES));
#else
                outlineShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), LeapKShader::buildOutlineFragmentSource(effectiveShader, modern, isOpenGLES));
#endif
                if (!outlineShaderPtr
#ifndef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
                    || !outlineShaderPtr->isValid()
#endif
                ) {
                    qWarning() << "ActiveWindowOutline: Custom outline shader failed to compile, falling back.";
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                    outlineShaderPtr = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::UniformColor, glowVertexSource(), outlineFragmentSource());
#else
                    outlineShaderPtr = ShaderManager::instance()->generateCustomShader(
                        ShaderTrait::Border, QByteArray(), outlineFragmentSource());
#endif
                }
            } else {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
                outlineShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::UniformColor, glowVertexSource(), outlineFragmentSource());
#else
                outlineShaderPtr = ShaderManager::instance()->generateCustomShader(
                    ShaderTrait::Border, QByteArray(), outlineFragmentSource());
#endif
            }
        }
    }

    const bool useCustomFade = useCustomShader &&
#ifdef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
        outlineShaderPtr;
#else
        outlineShaderPtr && outlineShaderPtr->isValid();
#endif

    std::unique_ptr<ShaderBinder> customBinder;
    std::unique_ptr<ShaderBinder> defaultBinder;
    GLShader *activeShader = nullptr;

    if (useCustomFade) {
        customBinder = std::make_unique<ShaderBinder>(outlineShaderPtr.get());
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
            for (const auto &param : effectiveShader.parameters) {
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

void ActiveWindowOutlineEffect::generateRoundedRectStrip(
    const QRectF &outer, qreal outerRadius,
    const QRectF &inner, qreal innerRadius,
    qreal scale,
    QList<QVector2D> &verts)
{
    OutlineGeometry::buildRoundedRectStrip(outer, outerRadius, inner, innerRadius, scale, verts);
}

void ActiveWindowOutlineEffect::ensureAudioShadersCompiled()
{
    if (!m_audioShaderRuleEnabled || m_audioShaderId.isEmpty()) {
        return;
    }
    const EglContext *context = EglContext::currentContext();
    const bool isOpenGLES = !epoxy_is_desktop_gl();
    const bool modern = isOpenGLES
        ? (context ? context->glslVersion() >= Version(3, 0) : true)
        : (context ? context->glslVersion() >= Version(1, 40) : true);

    if (!m_audioGlowShader && (m_audioShaderEffect.target == QStringLiteral("glow") || m_audioShaderEffect.target == QStringLiteral("both"))) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
        m_audioGlowShader = ShaderManager::instance()->generateCustomShader(
            ShaderTrait::UniformColor, glowVertexSource(), LeapKShader::buildGlowFragmentSource(m_audioShaderEffect, modern, isOpenGLES));
#else
        m_audioGlowShader = ShaderManager::instance()->generateCustomShader(
            ShaderTrait::Border, QByteArray(), LeapKShader::buildGlowFragmentSource(m_audioShaderEffect, modern, isOpenGLES));
#endif
    }
    if (!m_audioOutlineShader && (m_audioShaderEffect.target == QStringLiteral("outline") || m_audioShaderEffect.target == QStringLiteral("both"))) {
#ifdef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
        m_audioOutlineShader = ShaderManager::instance()->generateCustomShader(
            ShaderTrait::UniformColor, glowVertexSource(), LeapKShader::buildOutlineFragmentSource(m_audioShaderEffect, modern, isOpenGLES));
#else
        m_audioOutlineShader = ShaderManager::instance()->generateCustomShader(
            ShaderTrait::Border, QByteArray(), LeapKShader::buildOutlineFragmentSource(m_audioShaderEffect, modern, isOpenGLES));
#endif
    }
}

void ActiveWindowOutlineEffect::renderHudPillAudioShader(const RenderTarget &renderTarget,
                                                         const RenderViewport &viewport,
                                                         const QRectF &rect,
                                                         qreal scale,
                                                         const OutlineBadge::BadgeConfig &hudCfg,
                                                         bool isOutlinePass)
{
    Q_UNUSED(renderTarget);
    ensureAudioShadersCompiled();

    const bool needGlow = !isOutlinePass && (m_audioShaderEffect.target == QStringLiteral("glow") || m_audioShaderEffect.target == QStringLiteral("both"));
    const bool needOutline = isOutlinePass && (m_audioShaderEffect.target == QStringLiteral("outline") || m_audioShaderEffect.target == QStringLiteral("both"));

    if (!needGlow && !needOutline) {
        return;
    }

    std::unique_ptr<GLShader> &activeShaderPtr = isOutlinePass ? m_audioOutlineShader : m_audioGlowShader;
#ifdef ACTIVE_WINDOW_OUTLINE_SHADER_POINTER_VALIDITY
    if (!activeShaderPtr) {
        return;
    }
#else
    if (!activeShaderPtr || !activeShaderPtr->isValid()) {
        return;
    }
#endif

    const auto box = [scale](const QRectF &r) {
        return QVector4D(r.center().x() * scale,
                         r.center().y() * scale,
                         r.width() * scale / 2.0,
                         r.height() * scale / 2.0);
    };

    const qreal reach = isOutlinePass ? 2.5 : qBound(4.0, m_glowRadius * 0.75, 20.0);
    const qreal cardRadius = (hudCfg.activeTheme.hudCard.cornerRadius > 0)
        ? hudCfg.activeTheme.hudCard.cornerRadius
        : ((hudCfg.cornerRadius > 0) ? hudCfg.cornerRadius : 8.0);

    const QRectF outerRect = rect.adjusted(-reach, -reach, reach, reach);
    const QRectF innerRect = isOutlinePass ? rect : rect.adjusted(1.0, 1.0, -1.0, -1.0);

    QList<QVector2D> vertices;
    generateRoundedRectStrip(outerRect, cardRadius + reach,
                             innerRect, qMax<qreal>(0.0, cardRadius),
                             1.0, vertices);
    if (vertices.isEmpty()) {
        return;
    }

    QList<QVector2D> scaledVerts;
    scaledVerts.reserve(vertices.size());
    for (const QVector2D &v : std::as_const(vertices)) {
        scaledVerts.append(v * scale);
    }

    static constexpr GLVertexAttrib layout{
        .attributeIndex = VA_Position,
        .componentCount = 2,
        .type = GL_FLOAT,
        .relativeOffset = 0,
    };

    auto vbo = GLVertexBuffer::streamingBuffer();
    auto mapped = vbo->map<QVector2D>(scaledVerts.size());
    if (!mapped) {
        return;
    }
    memcpy(mapped->data(), scaledVerts.data(), scaledVerts.size() * sizeof(QVector2D));
    vbo->unmap();
    vbo->setVertexCount(scaledVerts.size());
    vbo->setAttribLayout(std::span(&layout, 1), sizeof(QVector2D));

    ShaderBinder binder(activeShaderPtr.get());
    GLShader *sh = binder.shader();
    sh->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    const QColor themeAudio = hudCfg.activeTheme.resolveColor(hudCfg.activeTheme.hudCard.audioIndicatorColor, QColor(241, 196, 15));
    sh->setUniform(GLShader::ColorUniform::Color, themeAudio);
    sh->setUniform("outerBox", box(outerRect));
    sh->setUniform("innerBox", box(innerRect));
    sh->setUniform("outerRadius", float((cardRadius + reach) * scale));
    sh->setUniform("innerRadius", float(cardRadius * scale));
    sh->setUniform("glowRadius", float(reach * scale));
    sh->setUniform("fadeCorners", 0.0f);
    sh->setUniform("dashActive", 0.0f);
    const float elapsedSec = m_animationClock.isValid() ? float(m_animationClock.elapsed() / 1000.0) : 0.0f;
    sh->setUniform("time", elapsedSec);
    sh->setUniform("glitchActive", 0.0f);

    for (const auto &param : m_audioShaderEffect.parameters) {
        const QByteArray uniformName = param.id.toUtf8();
        if (param.type == QStringLiteral("float")) {
            sh->setUniform(uniformName.constData(), float(param.currentValue));
        } else if (param.type == QStringLiteral("int")) {
            sh->setUniform(uniformName.constData(), int(param.currentValue));
        } else if (param.type == QStringLiteral("bool")) {
            sh->setUniform(uniformName.constData(), param.currentValue > 0.5 ? 1.0f : 0.0f);
        } else if (param.type == QStringLiteral("color")) {
            const QColor c(param.colorCurrent);
            sh->setUniform(uniformName.constData(), QVector4D(c.redF(), c.greenF(), c.blueF(), c.alphaF()));
        }
    }

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    vbo->render(GL_TRIANGLES);
}


} // namespace KWin
