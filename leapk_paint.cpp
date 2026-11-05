/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{

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
    const bool wantThumbnail = (cfg.windowShowThumbnail || cfg.hudShowThumbnail || m_hudThumbnailZoomActive)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (wantThumbnail && w && !w->isDeleted() && isEligibleOutlineWindow(w)
        && !w->isMinimized() && w->isVisible() && !isWindowObscured(w)
        && !(effects && effects->isScreenLocked()) && !w->isLockScreen()
        && !m_inThumbnailCapture) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastCapture = m_lastCaptureTimestamp.value(w, 0);
        const qint64 throttleInterval = m_badgeLivePreviews ? 33 : 1000;
        if (!m_thumbnailCache.contains(w) || (now - lastCapture >= throttleInterval)) {
            const int effHudW = m_hudThumbnailZoomActive ? qMax(180, cfg.hudThumbnailWidth * 2) : cfg.hudThumbnailWidth;
            const int effHudH = m_hudThumbnailZoomActive ? qMax(110, cfg.hudThumbnailHeight * 2) : cfg.hudThumbnailHeight;
            const QSize targetSize(qMax(cfg.windowThumbnailWidth, effHudW) * 2,
                                   qMax(cfg.windowThumbnailHeight, effHudH) * 2);
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
    const bool wantThumbnail = (cfg.windowShowThumbnail || cfg.hudShowThumbnail || m_hudThumbnailZoomActive)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (wantThumbnail && w && !w->isDeleted() && isEligibleOutlineWindow(w)
        && !w->isMinimized() && w->isVisible() && !isWindowObscured(w)
        && !(effects && effects->isScreenLocked()) && !w->isLockScreen()
        && !m_inThumbnailCapture) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastCapture = m_lastCaptureTimestamp.value(w, 0);
        const qint64 throttleInterval = m_badgeLivePreviews ? 33 : 1000;
        if (!m_thumbnailCache.contains(w) || (now - lastCapture >= throttleInterval)) {
            const int effHudW = m_hudThumbnailZoomActive ? qMax(180, cfg.hudThumbnailWidth * 2) : cfg.hudThumbnailWidth;
            const int effHudH = m_hudThumbnailZoomActive ? qMax(110, cfg.hudThumbnailHeight * 2) : cfg.hudThumbnailHeight;
            const QSize targetSize(qMax(cfg.windowThumbnailWidth, effHudW) * 2,
                                   qMax(cfg.windowThumbnailHeight, effHudH) * 2);
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
            if (jw && jw != m_activeWindow && (!effects || jw != effects->activeWindow()) && !jw->isDeleted() && jw->isOnCurrentDesktop()) {
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
    const bool canDrawAudioInactive = m_outlineEnabled && !inhibited && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked
        && m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty()
        && (m_audioWindowFrameShaderMode == QLatin1String("always") || m_audioWindowFrameShaderMode == QLatin1String("inactive"))
        && m_audioTracker.hasPlayingAudio();
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
    std::unique_ptr<OutlineCursorOverlay> cursorOverlay;
    // Only defer the cursor for the ordinary output pass. Transformed/offscreen
    // effects own their own cursor handling and must retain their scene filters.
    if ((drawOutline || drawDelayed || hasRecentToDraw || hasMarkedToDraw || canDrawAudioInactive || m_jumpMode) && m_sceneView && m_sceneView->scene() == effects->scene()
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
    const bool wantThumbnail = (activeBadgeCfg.windowShowThumbnail || activeBadgeCfg.hudShowThumbnail || m_badgeShowThumbnail || m_hudThumbnailZoomActive)
        || (m_badgePreviewActive && (m_previewBadgeConfig.windowShowThumbnail || m_previewBadgeConfig.hudShowThumbnail));
    if (wantThumbnail && !inhibited) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const qint64 lastCapture = m_lastCaptureTimestamp.value(m_activeWindow, 0);
        const qint64 throttleInterval = m_badgeLivePreviews ? 33 : 1000;
        if (m_activeWindow && m_activeWindow->isOnCurrentDesktop()
            && !m_activeWindow->isMinimized() && m_activeWindow->isVisible() && !m_activeWindow->isDeleted()
            && (!m_thumbnailCache.contains(m_activeWindow) || (nowMs - lastCapture >= throttleInterval))) {
            const int effHudW = m_hudThumbnailZoomActive ? qMax(180, activeBadgeCfg.hudThumbnailWidth * 2) : activeBadgeCfg.hudThumbnailWidth;
            const int effHudH = m_hudThumbnailZoomActive ? qMax(110, activeBadgeCfg.hudThumbnailHeight * 2) : activeBadgeCfg.hudThumbnailHeight;
            captureWindowThumbnail(m_activeWindow, renderTarget, viewport,
                                   QSize(effHudW * 2, effHudH * 2));
        }
    }

    const bool showContinuousBadges = m_outlineEnabled && m_badgeConfig.windowBadgesEnabled && (m_badgeDisplayMode == 1) && !inhibited && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked;
    const bool hasPinnedToDraw = !m_pinnedWindows.isEmpty() && m_outlineEnabled && !inhibited && !hasVisibleBlockingPopup() && !m_visibilityState.popupBlocked;
    const bool hasBadgesToDraw = (showContinuousBadges || (m_outlineEnabled && m_jumpMode) || m_badgePreviewActive || hasPinnedToDraw);

    if ((!m_outlineEnabled && !m_badgePreviewActive) || (!drawOutline && !drawDelayed && !hasRecentToDraw && !hasMarkedToDraw && !hasPinnedToDraw && !canDrawAudioInactive && !hasBadgesToDraw)) {
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

    if (hasPinnedToDraw) {
        for (EffectWindow *pinned : m_pinnedWindows) {
            if (!pinned || pinned == w || m_markedWindows.contains(pinned)
                || !isEligibleOutlineWindow(pinned)
                || pinned->isDeleted() || pinned->isMinimized()
                || pinned->isHiddenByShowDesktop() || !pinned->isOnCurrentDesktop()
                || (m_hideOnFullScreen && pinned->isFullScreen())
                || isBlockingPopup(pinned)) {
                continue;
            }

            const QRectF pinnedGeo = pinned->frameGeometry();
            const qreal pinnedWidth = std::max<qreal>(1.0, m_width - m_markInactiveWidthReduction);
            if (!m_outlineCache.contains(pinned) || m_lastGeometry.value(pinned) != pinnedGeo || m_outlineCache.value(pinned).borderWidth != pinnedWidth) {
                cacheWindowOutline(pinned, pinnedGeo, pinnedWidth);
                m_lastGeometry[pinned] = pinnedGeo;
            }

            const qreal inactiveOpacity = m_visibilityState.visibilityOpacity;
            renderOutline(renderTarget, viewport, m_outlineCache[pinned], deviceRegion, pinned, inactiveOpacity);
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

    if (canDrawAudioInactive) {
        const auto allWindows = effects->stackingOrder();
        for (EffectWindow *aw : allWindows) {
            if (!aw || aw == w || aw->isDeleted() || aw->isMinimized()
                || aw->isHiddenByShowDesktop() || !aw->isOnCurrentDesktop()
                || (m_hideOnFullScreen && aw->isFullScreen())
                || isBlockingPopup(aw) || !isEligibleOutlineWindow(aw)) {
                continue;
            }
            if (m_markedWindows.contains(aw) || m_pinnedWindows.contains(aw) || (m_highlightRecentWindow && m_recentWindows.contains(aw))) {
                continue;
            }
            if (!m_audioTracker.isWindowPlaying(aw->pid(), aw->caption())) {
                continue;
            }

            const QRectF awGeo = aw->frameGeometry();
            const qreal awWidth = m_width;
            if (!m_outlineCache.contains(aw) || m_lastGeometry.value(aw) != awGeo || m_outlineCache.value(aw).borderWidth != awWidth) {
                cacheWindowOutline(aw, awGeo, awWidth);
                m_lastGeometry[aw] = awGeo;
            }

            const qreal inactiveOpacity = m_visibilityState.visibilityOpacity;
            renderOutline(renderTarget, viewport, m_outlineCache[aw], deviceRegion, aw, inactiveOpacity);
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
            if (!target || target == w || target == m_activeWindow
                || (effects && target == effects->activeWindow())
                || target->isDeleted() || !isEligibleTargetWindow(target)
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
            QString customLabel = m_markedWindowLabels.value(target);
            if (isWindowPinned(target)) {
                customLabel = customLabel.isEmpty() ? QStringLiteral("●") : QStringLiteral("● %1").arg(customLabel);
            }
            const bool isPlayingAudio = target ? m_audioTracker.isWindowPlaying(target->pid(), target->caption()) : false;
            const qreal badgeW = OutlineBadge::computeBadgeWidth(m_badgeConfig, badgeH, customLabel, QString(), true, isPlayingAudio);
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

    if (!m_jumpMode && !showContinuousBadges && hasPinnedToDraw) {
        const int badgeH = 16;
        const QRectF screenRect = viewport.renderRect();
        for (EffectWindow *target : m_pinnedWindows) {
            if (!target || target->isDeleted() || !isEligibleTargetWindow(target)
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
            QColor bgCol = isMarked ? m_markedWindows.value(target) : QColor();
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol.isValid() ? bgCol : QColor(255, 185, 0));
            QString customLabel = m_markedWindowLabels.value(target);
            customLabel = customLabel.isEmpty() ? QStringLiteral("●") : QStringLiteral("● %1").arg(customLabel);

            const qreal badgeW = OutlineBadge::computeBadgeWidth(m_badgeConfig, badgeH, customLabel, QString(), false);
            QRectF badgeRect = OutlineBadge::computeTopBarPinnedBadgeRect(geo, target->clientGeometry(), target->hasDecoration(), badgeW, badgeH, 0);
            if (badgeRect.left() < screenRect.left() + 4.0) badgeRect.moveLeft(screenRect.left() + 4.0);
            if (badgeRect.right() > screenRect.right() - 4.0) badgeRect.moveRight(screenRect.right() - 4.0);
            if (badgeRect.top() < screenRect.top() + 4.0) badgeRect.moveTop(screenRect.top() + 4.0);
            if (badgeRect.bottom() > screenRect.bottom() - 4.0) badgeRect.moveBottom(screenRect.bottom() - 4.0);

            if (isBadgeOccluded(target, badgeRect)) {
                bool foundVisiblePosition = false;
                for (int alt = 1; alt <= 3; ++alt) {
                    QRectF altRect = OutlineBadge::computeTopBarPinnedBadgeRect(geo, target->clientGeometry(), target->hasDecoration(), badgeW, badgeH, alt);
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
            renderBadge(renderTarget, viewport, badgeRect, QChar(), bgCol, textCol, false, badgeH, customLabel, target);
        }
    }

    if (m_tilingEditModeActive && !inhibited) {
        renderTilingEditOverlay(renderTarget, viewport);
    } else if (m_jumpMode && !inhibited) {
        if (m_tilingPresetsModeActive) {
            renderTilingPresetPills(renderTarget, viewport, m_tilingPresets);
        } else {
            const int jumpH = m_badgeConfig.jumpBadgeSize;
        const QRectF screenRect = viewport.renderRect();
        EffectWindow *hudRefWindow = (m_jumpMode && m_hudOriginWindow) ? m_hudOriginWindow.data() : w;
        const qint64 hudAnimElapsed = (m_hudSelectAnimActive && m_hudSelectAnimTimer.isValid())
            ? m_hudSelectAnimTimer.elapsed()
            : 999;
        const bool isSelectAnim = m_jumpMode && m_hudSelectAnimActive && (hudAnimElapsed < 250);
        const qreal animProgress = isSelectAnim ? qBound(0.0, hudAnimElapsed / 250.0, 1.0) : 1.0;
        const qreal easeOut = 1.0 - std::pow(1.0 - animProgress, 3.0);

        QList<QPair<QChar, EffectWindow *>> offscreenTargets;
        QList<QPair<QChar, EffectWindow *>> onscreenTargets;

        if (m_searchModeActive) {
            for (const auto &cand : m_searchMatches) {
                EffectWindow *target = reinterpret_cast<EffectWindow *>(cand.windowPtr);
                if (!target || target->isDeleted() || !isEligibleTargetWindow(target)) {
                    continue;
                }
                const QChar letter = m_jumpLetterToWindow.key(target);
                offscreenTargets.append(qMakePair(letter, target));
                if (target->isOnCurrentDesktop() && !target->isMinimized()) {
                    onscreenTargets.append(qMakePair(letter, target));
                }
            }
        } else if (m_subLeapActive) {
            // In Sub-Leap disambiguation mode:
            // When hudIncludeAll, hudGroupWindows, or window badges are disabled, all disambiguation targets
            // appear as cards in the HUD dock.
            // Any window that is obscured, minimized, off-desktop, pinned, or stashed also appears in the HUD.
            // Additionally, any window visible unobscured on the current desktop receives an on-screen badge.
            for (const auto &pair : m_subLeapTargets) {
                EffectWindow *target = pair.second;
                if (!target || target->isDeleted() || !isEligibleTargetWindow(target)) {
                    continue;
                }

                OutlineBadge::SubLeapCandidate cand;
                cand.letter = pair.first;
                cand.isOnCurrentDesktop = target->isOnCurrentDesktop();
                cand.isMinimized = target->isMinimized();
                const QRectF geo = target->frameGeometry();
                const QRectF visibleWindowGeo = geo.intersected(screenRect);
                cand.isTinyOrOffscreen = (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50);
                cand.isObscured = isWindowObscured(target);
                cand.isPinned = isWindowPinned(target);
                cand.isActiveWindow = (target == hudRefWindow || target == w || target == m_activeWindow);

                const auto dist = OutlineBadge::classifySubLeapTarget(cand, m_badgeConfig, m_stashModeActive);
                if (dist.addToHud) {
                    offscreenTargets.append(pair);
                }
                if (dist.addToOnscreen) {
                    onscreenTargets.append(pair);
                }
            }
        } else {
            if (m_badgeConfig.hudGroupWindows && !m_keyboardOverlayEnabled) {
                // Mode A: Group multi-instance windows into single card with counter
                for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
                    const QChar letter = it.key();
                    const auto &winList = it.value();
                    if (winList.isEmpty()) {
                        continue;
                    }
                    EffectWindow *firstWin = winList.first();
                    if (!firstWin || firstWin->isDeleted() || !isEligibleTargetWindow(firstWin)) {
                        continue;
                    }

                    EffectWindow *repWin = firstWin;
                    const bool forceHud = m_badgeConfig.hudIncludeAll || !m_badgeConfig.windowBadgesEnabled || isWindowPinned(repWin) || m_stashModeActive || m_closeWindowModeActive || m_pinModeActive;
                    if (forceHud) {
                        for (EffectWindow *target : winList) {
                            if (target && !target->isDeleted() && (target == w || target == m_activeWindow || target == hudRefWindow)) {
                                repWin = target;
                                break;
                            }
                        }
                        offscreenTargets.append(qMakePair(letter, repWin));
                    } else {
                        bool hasOffscreenMember = false;
                        EffectWindow *offscreenRep = nullptr;
                        for (EffectWindow *target : winList) {
                            if (!target || target->isDeleted()) continue;
                            const bool onCurrent = target->isOnCurrentDesktop();
                            const bool min = target->isMinimized();
                            if (!onCurrent || min) {
                                hasOffscreenMember = true;
                                offscreenRep = target;
                                break;
                            }
                            const QRectF geo = target->frameGeometry();
                            const QRectF visibleWindowGeo = geo.intersected(screenRect);
                            if (visibleWindowGeo.width() < 50 || visibleWindowGeo.height() < 50 || isWindowObscured(target)) {
                                hasOffscreenMember = true;
                                offscreenRep = target;
                                break;
                            }
                        }
                        if (hasOffscreenMember && offscreenRep) {
                            offscreenTargets.append(qMakePair(letter, offscreenRep));
                        }
                    }

                    for (EffectWindow *target : winList) {
                        if (!target || target->isDeleted() || !isEligibleTargetWindow(target)) {
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
                        // Force window badge on active window hudRefWindow / w or fading previous active window
                        if (target == hudRefWindow || target == w || (isSelectAnim && target == m_hudPrevActiveWindow.data())) {
                            onscreenTargets.append(qMakePair(letter, target));
                        } else if (m_badgeConfig.windowBadgesEnabled && !isWindowObscured(target)) {
                            onscreenTargets.append(qMakePair(letter, target));
                        }
                    }
                }
            } else {
                // Mode B: Show all windows directly with unique jump letters
                for (auto it = m_letterWindowGroups.constBegin(); it != m_letterWindowGroups.constEnd(); ++it) {
                    for (EffectWindow *target : it.value()) {
                        if (!target || target->isDeleted() || !isEligibleTargetWindow(target)) {
                            continue;
                        }
                        const QChar directKey = m_windowToDirectLetter.value(target);
                        if (directKey.isNull()) {
                            continue;
                        }

                        // Force window badge on active origin window hudRefWindow / w or fading previous active window
                        if (target == hudRefWindow || target == w || target == m_activeWindow || (isSelectAnim && target == m_hudPrevActiveWindow.data())) {
                            if (target->isOnCurrentDesktop() && !target->isMinimized()) {
                                onscreenTargets.append(qMakePair(directKey, target));
                            }
                            if (m_badgeConfig.hudIncludeAll || !m_badgeConfig.windowBadgesEnabled || isWindowPinned(target) || m_stashModeActive || m_closeWindowModeActive || m_pinModeActive) {
                                offscreenTargets.append(qMakePair(directKey, target));
                            }
                            continue;
                        }

                        const bool forceHud = m_badgeConfig.hudIncludeAll || !m_badgeConfig.windowBadgesEnabled || isWindowPinned(target) || m_stashModeActive || m_closeWindowModeActive || m_pinModeActive;
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

        // Partition offscreenTargets into left and right groups with dividing line
        int dividerAfterIndex = -1;
        if (!offscreenTargets.isEmpty()) {
            QList<QPair<QChar, EffectWindow *>> leftTargets;
            QList<QPair<QChar, EffectWindow *>> rightTargets;

            if (m_stashModeActive) {
                // In Stash mode: workspace windows on the left, stashed windows on the right
                for (const auto &pair : offscreenTargets) {
                    if (isWindowInStash(pair.second)) {
                        rightTargets.append(pair);
                    } else {
                        leftTargets.append(pair);
                    }
                }
            } else {
                // In Normal and Super Jump mode: pinned windows on the left, regular windows on the right
                for (const auto &pair : offscreenTargets) {
                    if (isWindowPinned(pair.second)) {
                        leftTargets.append(pair);
                    } else {
                        rightTargets.append(pair);
                    }
                }
            }

            if (!m_searchModeActive) {
                const auto &history = !m_hudSnapshotHistory.isEmpty() ? m_hudSnapshotHistory : m_windowHistory;
                auto recencyRank = [&history](EffectWindow *w) -> int {
                    if (!w) return 999999;
                    int idx = history.indexOf(w);
                    return idx >= 0 ? idx : 999999;
                };
                auto sortByRecencyOldestToNewest = [&recencyRank](QList<QPair<QChar, EffectWindow *>> &list) {
                    std::stable_sort(list.begin(), list.end(), [&recencyRank](const QPair<QChar, EffectWindow *> &a, const QPair<QChar, EffectWindow *> &b) {
                        int rankA = recencyRank(a.second);
                        int rankB = recencyRank(b.second);
                        if (rankA != rankB) {
                            return rankA > rankB; // Older window (higher index) on the left (first)
                        }
                        return false;
                    });
                };
                sortByRecencyOldestToNewest(leftTargets);
                sortByRecencyOldestToNewest(rightTargets);
            }

            if (!leftTargets.isEmpty() && !rightTargets.isEmpty()) {
                dividerAfterIndex = leftTargets.size() - 1;
            }
            offscreenTargets = leftTargets + rightTargets;
        }

        QList<QRectF> hudRects = computeAllHudCollisionRects(offscreenTargets, viewport, m_badgeConfig.hudBadgeSize, m_badgeConfig, dividerAfterIndex);

        for (const auto &pair : onscreenTargets) {
            const QChar letter = pair.first;
            EffectWindow *target = pair.second;
            const QRectF geo = target->frameGeometry();
            const QRectF visibleWindowGeo = geo.intersected(screenRect);

            const bool isMarked = m_markedWindows.contains(target);
            const bool isRecent = (target == findRecentWindow()) || m_recentWindows.contains(target);
            const bool isMostRecent = (target == findRecentWindow());
            const QString recentKey = (isMostRecent && m_jumpRecentKey.compare(QLatin1String("Space"), Qt::CaseInsensitive) != 0)
                ? m_jumpRecentKey
                : QString();

            QColor bgCol;
            if (m_closeWindowModeActive) {
                bgCol = QColor(230, 57, 70);
            } else if (m_pinModeActive) {
                bgCol = QColor(245, 166, 35);
            } else if (isMarked) {
                bgCol = m_markedWindows.value(target);
            } else if (isRecent) {
                bgCol = resolveColor(1.0, target);
                if (!bgCol.isValid() || bgCol.alpha() == 0) {
                    bgCol = m_recentWindowCustomColor.isValid() ? m_recentWindowCustomColor : QColor(52, 152, 219);
                }
            } else if (target == w) {
                bgCol = resolveColor(1.0, target);
                if (!bgCol.isValid() || bgCol.alpha() == 0) {
                    bgCol = m_customColor.isValid() ? m_customColor : m_badgeConfig.defaultBgColor;
                }
            } else {
                bgCol = m_badgeConfig.defaultBgColor;
            }

            // Animate smooth transition of badge highlight between windows
            qreal badgeOpacity = 1.0;
            if (isSelectAnim) {
                if (target == m_hudPrevActiveWindow.data() && target != w) {
                    QColor activeCol = resolveColor(1.0, target);
                    if (!activeCol.isValid() || activeCol.alpha() == 0) {
                        activeCol = m_customColor.isValid() ? m_customColor : m_badgeConfig.defaultBgColor;
                    }
                    const qreal fade = 1.0 - easeOut; // 1.0 down to 0.0
                    int r = qBound(0, qRound(bgCol.red() + (activeCol.red() - bgCol.red()) * fade), 255);
                    int g = qBound(0, qRound(bgCol.green() + (activeCol.green() - bgCol.green()) * fade), 255);
                    int b = qBound(0, qRound(bgCol.blue() + (activeCol.blue() - bgCol.blue()) * fade), 255);
                    int a = qBound(0, qRound(bgCol.alpha() + (activeCol.alpha() - bgCol.alpha()) * fade), 255);
                    bgCol = QColor(r, g, b, a);

                    if (!m_badgeConfig.windowBadgesEnabled) {
                        badgeOpacity = fade;
                    }
                } else if (target == w && m_hudPrevActiveWindow) {
                    QColor baseCol = m_badgeConfig.defaultBgColor;
                    const qreal rise = easeOut; // 0.0 up to 1.0
                    int r = qBound(0, qRound(baseCol.red() + (bgCol.red() - baseCol.red()) * rise), 255);
                    int g = qBound(0, qRound(baseCol.green() + (bgCol.green() - baseCol.green()) * rise), 255);
                    int b = qBound(0, qRound(baseCol.blue() + (bgCol.blue() - baseCol.blue()) * rise), 255);
                    int a = qBound(0, qRound(baseCol.alpha() + (bgCol.alpha() - baseCol.alpha()) * rise), 255);
                    bgCol = QColor(r, g, b, a);

                    if (!m_badgeConfig.windowBadgesEnabled) {
                        badgeOpacity = rise;
                    }
                }
            }
            const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);
            QString customLabel = m_markedWindowLabels.value(target);
            if (m_closeWindowModeActive) {
                customLabel = customLabel.isEmpty() ? QStringLiteral("✕") : QStringLiteral("✕ %1").arg(customLabel);
            } else if (m_pinModeActive) {
                customLabel = isWindowPinned(target) ? QStringLiteral("📌") : QStringLiteral("+📌");
            } else if (isWindowPinned(target)) {
                customLabel = customLabel.isEmpty() ? QStringLiteral("●") : QStringLiteral("● %1").arg(customLabel);
            } else if (isWindowInStash(target)) {
                customLabel = customLabel.isEmpty() ? QStringLiteral("📦") : QStringLiteral("📦 %1").arg(customLabel);
            }
            const bool isPlayingAudio = target ? m_audioTracker.isWindowPlaying(target->pid(), target->caption()) : false;
            const qreal jumpW = OutlineBadge::computeBadgeWidth(m_badgeConfig, jumpH, customLabel, recentKey, true, isPlayingAudio);

            bool foundVisiblePosition = false;
            QRectF badgeRect = findBestBadgePositionForWindow(target, visibleWindowGeo, m_badgePosition, jumpW, jumpH, m_badgeMargin, screenRect, hudRects, &foundVisiblePosition);

            if (!foundVisiblePosition) {
                if (target == w || target == hudRefWindow || (isSelectAnim && target == m_hudPrevActiveWindow.data())) {
                    badgeRect = OutlineBadge::computeBadgeRect(visibleWindowGeo, m_badgePosition, jumpW, jumpH, m_badgeMargin);
                    if (badgeRect.left() < screenRect.left() + 4.0) badgeRect.moveLeft(screenRect.left() + 4.0);
                    if (badgeRect.right() > screenRect.right() - 4.0) badgeRect.moveRight(screenRect.right() - 4.0);
                    if (badgeRect.top() < screenRect.top() + 4.0) badgeRect.moveTop(screenRect.top() + 4.0);
                    if (badgeRect.bottom() > screenRect.bottom() - 4.0) badgeRect.moveBottom(screenRect.bottom() - 4.0);
                } else {
                    if (!m_badgeConfig.hudIncludeAll) {
                        const auto offPair = qMakePair(letter, target);
                        if (!offscreenTargets.contains(offPair)) {
                            offscreenTargets.append(offPair);
                            hudRects = computeAllHudCollisionRects(offscreenTargets, viewport, m_badgeConfig.hudBadgeSize, m_badgeConfig, dividerAfterIndex);
                        }
                    }
                    continue;
                }
            }
            renderBadge(renderTarget, viewport, badgeRect, letter, bgCol, textCol, true, m_badgeConfig.jumpBadgeSize, customLabel, target, recentKey, isRecent, badgeOpacity);
        }

        if (!offscreenTargets.isEmpty()) {
            renderOffscreenJumpTargets(renderTarget, viewport, offscreenTargets, m_badgeConfig.hudBadgeSize, dividerAfterIndex);
        } else {
            const QRectF hintRect = renderHudHintBar(renderTarget, viewport, QRectF());
            renderHudModeHeader(renderTarget, viewport, QRectF(), hintRect);
        }

        if (m_hudMetaHeld && !m_metaSpaceTraverseActive) {
            renderHudMetaOverlay(renderTarget, viewport);
        }

        if (m_crossWsActionActive) {
            renderHudCrossWsActionOverlay(renderTarget, viewport);
        }

        if (m_pinWsSelectionActive) {
            renderHudPinWsSelectionOverlay(renderTarget, viewport);
        }
        }

        if (m_keyboardOverlayEnabled && !m_searchModeActive && !m_tilingPresetsModeActive && !m_crossWsActionActive && !m_pinWsSelectionActive) {
            renderKeyboardArrowsOverlay(renderTarget, viewport);
        }
    }

    if (m_badgePreviewActive && !inhibited) {
        renderBadgePreview(renderTarget, viewport);
    }

    if (m_tilingToastTimer.isValid() && m_tilingToastTimer.elapsed() < 4500 && !m_tilingToastMessage.isEmpty() && !inhibited) {
        renderTilingToast(renderTarget, viewport);
    }

    if (m_jumpMode && m_invalidJumpKeyTimer.isValid() && m_invalidJumpKeyTimer.elapsed() < 1200 && !inhibited) {
        const QRectF screenRect = viewport.renderRect();
        const QPointF toastCenter(screenRect.center().x(), screenRect.top() + 70.0);
        const QString msg = QStringLiteral("⚠ No window assigned to '%1' — Press a valid letter or Esc").arg(m_invalidJumpKey);
        renderFloatingPill(renderTarget, viewport, msg, toastCenter, QColor(190, 30, 45, 235), QColor(255, 255, 255), 11, screenRect.width() - 60.0);
        if (m_invalidJumpKeyTimer.elapsed() < 1200 && effects) {
            effects->addRepaintFull();
        }
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


} // namespace KWin
