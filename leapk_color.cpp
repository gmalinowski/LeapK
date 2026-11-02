/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{

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
    const bool isAudioShaderActive = m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty() && m_audioTracker.hasPlayingAudio();
    if (m_visibilityState.isEffectivelyVisible() || hasDelayed || (isAudioShaderActive && (m_audioWindowFrameShaderMode == QLatin1String("always") || m_audioWindowFrameShaderMode == QLatin1String("inactive")))) {
        return m_colorCycleEnabled || m_breatheEnabled
            || m_glitchEnabled || m_neonFlickerEnabled || m_flashEnabled
            || m_ambilightInTransition
            || (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty())
            || isAudioShaderActive;
    }
    if (m_jumpMode && m_audioHudShaderEnabled && isAudioShaderActive) {
        return true;
    }
    if (m_jumpMode && m_badgeHudGroupShaderEnabled && (m_breatheEnabled || m_neonFlickerEnabled || (m_customShaderEnabled && !m_activeCustomShaderId.isEmpty()))) {
        return true;
    }
    if (m_jumpMode && m_hudSelectAnimActive && m_hudSelectAnimTimer.isValid() && m_hudSelectAnimTimer.elapsed() < 250) {
        return true;
    }
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const bool badgePulse = theme.resolveBool(QStringLiteral("$audioPulseBadge"), theme.windowBadge.audioPulseBadge)
        && theme.resolveDouble(QStringLiteral("$audioPulseIntensity"), theme.windowBadge.audioPulseIntensity) > 0.001;
    const bool cardPulse = theme.resolveBool(QStringLiteral("$audioPulseBadge"), theme.hudCard.audioPulseBadge)
        && theme.resolveDouble(QStringLiteral("$audioPulseIntensity"), theme.hudCard.audioPulseIntensity) > 0.001;
    const bool audioPulsingEnabled = badgePulse || cardPulse;
    if (audioPulsingEnabled && m_audioTracker.hasPlayingAudio() && (m_jumpMode || (m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && m_badgeDisplayMode == 1))) {
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

    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const bool badgePulse = theme.resolveBool(QStringLiteral("$audioPulseBadge"), theme.windowBadge.audioPulseBadge)
        && theme.resolveDouble(QStringLiteral("$audioPulseIntensity"), theme.windowBadge.audioPulseIntensity) > 0.001;
    const bool cardPulse = theme.resolveBool(QStringLiteral("$audioPulseBadge"), theme.hudCard.audioPulseBadge)
        && theme.resolveDouble(QStringLiteral("$audioPulseIntensity"), theme.hudCard.audioPulseIntensity) > 0.001;
    const bool audioPulsingEnabled = badgePulse || cardPulse;
    if (audioPulsingEnabled && m_audioTracker.hasPlayingAudio() && (m_jumpMode || (m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && m_badgeDisplayMode == 1))) {
        effects->addRepaintFull();
    }

    if (m_jumpMode && m_hudSelectAnimActive) {
        if (m_hudSelectAnimTimer.isValid() && m_hudSelectAnimTimer.elapsed() < 250) {
            effects->addRepaintFull();
        } else {
            m_hudSelectAnimActive = false;
            effects->addRepaintFull();
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
    const bool isPinned = (w != nullptr && m_pinnedWindows.contains(w));

    if (isPinned && !isFocus && !isMarked) {
        color = QColor(241, 196, 15);
        color.setAlphaF(qBound(0.1, m_visibilityState.visibilityOpacity, 1.0));
        return color;
    }

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

    const bool isPlayingAudio = w ? m_audioTracker.isWindowPlaying(w->pid(), w->caption()) : false;
    if (!treatAsMarked && !isPinned && !treatAsRecent && isPlayingAudio && m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty()) {
        color = m_badgeConfig.activeTheme.resolveColor(QStringLiteral("$audioColor"), QColor(241, 196, 15));
        color.setAlphaF(qBound(0.1, m_visibilityState.visibilityOpacity, 1.0));
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

} // namespace KWin
