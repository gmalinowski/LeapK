/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "leapk_internal.h"

namespace KWin
{

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
        cfg.hudBadgeSize = qBound(24, obj.value(QStringLiteral("BadgeHudSize")).toInt(44), 500);
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
    if (obj.contains(QStringLiteral("BadgeHudNavFontSize"))) {
        cfg.hudNavFontSize = qBound(0, obj.value(QStringLiteral("BadgeHudNavFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeHudHintsFontSize"))) {
        cfg.hudHintsFontSize = qBound(0, obj.value(QStringLiteral("BadgeHudHintsFontSize")).toInt(0), 200);
    }
    if (obj.contains(QStringLiteral("BadgeHudShowHints"))) {
        cfg.hudShowHints = obj.value(QStringLiteral("BadgeHudShowHints")).toBool(true);
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
    if (obj.contains(QStringLiteral("ActiveThemeId"))) {
        cfg.activeThemeId = obj.value(QStringLiteral("ActiveThemeId")).toString(QStringLiteral("default-breeze"));
        QJsonObject previewThemeOverrides;
        const QString pJson = obj.value(QStringLiteral("ActiveThemeParams")).toString();
        if (!pJson.isEmpty()) {
            previewThemeOverrides = QJsonDocument::fromJson(pJson.toUtf8()).object();
        }
        cfg.activeTheme = LeapKTheme::findThemeById(cfg.activeThemeId, previewThemeOverrides);
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

    if (m_jumpMode) {
        cancelJumpMode();
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
    Q_EMIT badgePreviewChanged(true);
}

void ActiveWindowOutlineEffect::clearBadgePreview()
{
    if (!m_badgePreviewActive) {
        return;
    }
    m_badgePreviewActive = false;
    m_previewShowWindowBadges = false;
    m_previewShowHud = false;
    m_previewWindowBadgeRects.clear();
    m_previewHudBounds = QRectF();
    if (!m_jumpMode) {
        m_hudTargetPillRects.clear();
        m_hudCloseButtonScreenRect = QRectF();
        m_hudHeaderScreenRect = QRectF();
        m_hudHintBarScreenRect = QRectF();
    }
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_WINDOW_INPUT_MOUSE_EVENT
    if (m_previewMouseIntercepting) {
        effects->stopMouseInterception(this);
        m_previewMouseIntercepting = false;
    }
#endif
    if (m_previewSafetyTimer && m_previewSafetyTimer->isActive()) {
        m_previewSafetyTimer->stop();
    }
    m_badgeTextureCache.clear();
    m_thumbnailCache.clear();
    m_lastCaptureTimestamp.clear();
    if (effects) {
        effects->defineCursor(Qt::ArrowCursor);
        effects->addRepaintFull();
    }
    Q_EMIT badgePreviewChanged(false);
}

bool ActiveWindowOutlineEffect::isPosInsideBadgePreview(const QPointF &pos) const
{
    if (!m_badgePreviewActive) {
        return false;
    }
    if (m_previewShowWindowBadges) {
        for (const QRectF &r : m_previewWindowBadgeRects) {
            if (r.isValid() && r.contains(pos)) {
                return true;
            }
        }
    }
    if (m_previewShowHud) {
        if (m_hudCloseButtonScreenRect.isValid() && m_hudCloseButtonScreenRect.contains(pos)) {
            return true;
        }
        if (m_hudHeaderScreenRect.isValid() && m_hudHeaderScreenRect.contains(pos)) {
            return true;
        }
        if (m_hudHintBarScreenRect.isValid() && m_hudHintBarScreenRect.contains(pos)) {
            return true;
        }
        if (m_previewHudBounds.isValid() && m_previewHudBounds.contains(pos)) {
            return true;
        }
        for (const auto &pair : m_hudTargetPillRects) {
            if (pair.first.isValid() && pair.first.contains(pos)) {
                return true;
            }
        }
    }
    return false;
}

void ActiveWindowOutlineEffect::slotMouseChanged(const QPointF &pos, const QPointF &oldpos,
                                                Qt::MouseButtons buttons, Qt::MouseButtons oldbuttons,
                                                Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers oldmodifiers)
{
    Q_UNUSED(oldpos);
    Q_UNUSED(modifiers);
    Q_UNUSED(oldmodifiers);

    if (!m_badgePreviewActive) {
        return;
    }

    const bool isInside = isPosInsideBadgePreview(pos);
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_WINDOW_INPUT_MOUSE_EVENT
    if (isInside && !m_previewMouseIntercepting && !m_jumpMode) {
        effects->startMouseInterception(this, Qt::PointingHandCursor);
        m_previewMouseIntercepting = true;
    } else if (!isInside && m_previewMouseIntercepting && !m_jumpMode) {
        effects->stopMouseInterception(this);
        m_previewMouseIntercepting = false;
        effects->defineCursor(Qt::ArrowCursor);
    }
#else
    if (isInside) {
        effects->defineCursor(Qt::PointingHandCursor);
    } else {
        effects->defineCursor(Qt::ArrowCursor);
    }
#endif

    const bool buttonPressed = ((buttons & ~oldbuttons) != Qt::NoButton);
    if (buttonPressed && isInside) {
        playThemedSound(OutlineSound::SoundEffect::Dismiss);
        clearBadgePreview();
    }
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
            hudRects = computeAllHudCollisionRects(hudTargets, viewport, m_previewBadgeConfig.hudBadgeSize, m_previewBadgeConfig);
        }
    }

    // 1. Render Window Badges if enabled
    m_previewWindowBadgeRects.clear();
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
            QString customLabel = m_markedWindowLabels.value(target);
            if (isWindowPinned(target)) {
                customLabel = customLabel.isEmpty() ? QStringLiteral("●") : QStringLiteral("● %1").arg(customLabel);
            }
            const qreal badgeW = OutlineBadge::computeBadgeWidth(m_previewBadgeConfig, badgeH, customLabel, recentKey, true);

            bool foundVisiblePosition = false;
            QRectF badgeRect = findBestBadgePositionForWindow(target, visibleWindowGeo, m_previewBadgePosition, badgeW, badgeH, m_previewBadgeConfig.margin, screenRect, hudRects, &foundVisiblePosition);
            if (!foundVisiblePosition) {
                continue;
            }

            m_previewWindowBadgeRects.append(badgeRect);
            renderBadge(renderTarget, viewport, badgeRect, winLetters.value(target, 'A'), bgCol, textCol, false, badgeH, customLabel, target, recentKey, isRecent);
        }
    }

    // 2. Render HUD Badges if enabled
    if (m_previewShowHud && !hudTargets.isEmpty()) {
        renderOffscreenJumpTargets(renderTarget, viewport, hudTargets, m_previewBadgeConfig.hudBadgeSize);
    } else if (m_previewShowHud) {
        const QRectF hintRect = renderHudHintBar(renderTarget, viewport, QRectF());
        const QRectF headerRect = renderHudModeHeader(renderTarget, viewport, QRectF(), hintRect);
        m_previewHudBounds = hintRect.isValid() ? hintRect : QRectF();
        if (headerRect.isValid()) {
            m_previewHudBounds = m_previewHudBounds.united(headerRect);
        }
    }
}

void ActiveWindowOutlineEffect::renderHudMetaOverlay(const RenderTarget &renderTarget,
                                                     const RenderViewport &viewport)
{
    if (m_metaSpaceTraverseActive) {
        return;
    }
    const qreal scale = viewport.scale();
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;

    const qint64 overlayKey = (m_superJumpActive ? 1 : 0)
        | (m_closeWindowModeActive ? 2 : 0)
        | (m_stashModeActive ? 4 : 0)
        | (m_pinModeActive ? 8 : 0)
        | (m_audioFilterModeActive ? 16 : 0)
        | (m_tilingPresetsModeActive ? 32 : 0)
        | (m_metaSpaceTraverseActive ? 64 : 0)
        | (static_cast<qint64>(qRound(scale * 10)) << 8)
        | (static_cast<qint64>(qHash(theme.id) % 1000) << 16);

    if (!m_hudMetaOverlayTexture || m_hudMetaOverlayCacheKey != overlayKey) {
        const QImage img = OutlineBadge::renderHudMetaOverlayImage(
            scale, theme, m_superJumpActive, m_closeWindowModeActive,
            m_stashModeActive, m_pinModeActive, m_audioFilterModeActive,
            m_tilingPresetsModeActive, m_metaSpaceTraverseActive);
        if (!img.isNull()) {
            auto uploaded = GLTexture::upload(img);
            if (uploaded) {
                uploaded->setFilter(GL_LINEAR);
                uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
                m_hudMetaOverlayTexture = std::shared_ptr<GLTexture>(uploaded.release());
                m_hudMetaOverlayCacheKey = overlayKey;
            }
        }
    }

    if (!m_hudMetaOverlayTexture) {
        return;
    }

    const QRectF screenRect = viewport.renderRect();
    const qreal w = m_hudMetaOverlayTexture->width() / scale;
    const qreal h = m_hudMetaOverlayTexture->height() / scale;
    const QRectF overlayRect(screenRect.center().x() - w / 2.0,
                             screenRect.center().y() - h / 2.0,
                             w, h);

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

    const float x0 = overlayRect.left() * scale;
    const float y0 = overlayRect.top() * scale;
    const float x1 = overlayRect.right() * scale;
    const float y1 = overlayRect.bottom() * scale;

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

    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, m_hudMetaOverlayTexture->matrix(NormalizedCoordinates));
    m_hudMetaOverlayTexture->bind();
    vbo->render(GL_TRIANGLES);
    m_hudMetaOverlayTexture->unbind();

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

void ActiveWindowOutlineEffect::renderHudCrossWsActionOverlay(const RenderTarget &renderTarget,
                                                             const RenderViewport &viewport)
{
    if (!m_crossWsActionActive || !m_crossWsTargetWindow) {
        return;
    }
    const qreal scale = viewport.scale();
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;

    EffectWindow *target = m_crossWsTargetWindow.data();
    if (!target || target->isDeleted()) {
        return;
    }

    const QString winTitle = target->caption().trimmed();
    const QString winClass = target->windowClass().trimmed();

    const qint64 overlayKey = static_cast<qint64>(qHash(winTitle) % 100000)
        | (static_cast<qint64>(qHash(m_crossWsTargetWsName) % 10000) << 17)
        | (static_cast<qint64>(qHash(m_crossWsCurrentWsName) % 10000) << 31)
        | (static_cast<qint64>(qRound(scale * 10)) << 45)
        | (static_cast<qint64>(qHash(theme.id) % 1000) << 52);

    if (!m_crossWsActionOverlayTexture || m_crossWsActionOverlayCacheKey != overlayKey) {
        const QImage img = OutlineBadge::renderHudCrossWorkspaceActionDialog(
            scale, theme, winTitle, winClass, m_crossWsTargetWsName, m_crossWsCurrentWsName,
            m_crossWsRelJumpRect, m_crossWsRelMoveRect, m_crossWsRelPinRect, m_crossWsRelCancelRect);
        if (!img.isNull()) {
            auto uploaded = GLTexture::upload(img);
            if (uploaded) {
                uploaded->setFilter(GL_LINEAR);
                uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
                m_crossWsActionOverlayTexture = std::shared_ptr<GLTexture>(uploaded.release());
                m_crossWsActionOverlayCacheKey = overlayKey;
            }
        }
    }

    if (!m_crossWsActionOverlayTexture) {
        return;
    }

    const QRectF screenRect = viewport.renderRect();
    const qreal w = m_crossWsActionOverlayTexture->width() / scale;
    const qreal h = m_crossWsActionOverlayTexture->height() / scale;
    const QRectF overlayRect(screenRect.center().x() - w / 2.0,
                             screenRect.center().y() - h / 2.0,
                             w, h);

    // Update screen rects for mouse hit detection
    m_crossWsDialogScreenRect = overlayRect;
    m_crossWsJumpScreenRect = QRectF(overlayRect.left() + m_crossWsRelJumpRect.x(), overlayRect.top() + m_crossWsRelJumpRect.y(), m_crossWsRelJumpRect.width(), m_crossWsRelJumpRect.height());
    m_crossWsMoveScreenRect = QRectF(overlayRect.left() + m_crossWsRelMoveRect.x(), overlayRect.top() + m_crossWsRelMoveRect.y(), m_crossWsRelMoveRect.width(), m_crossWsRelMoveRect.height());
    m_crossWsPinScreenRect = QRectF(overlayRect.left() + m_crossWsRelPinRect.x(), overlayRect.top() + m_crossWsRelPinRect.y(), m_crossWsRelPinRect.width(), m_crossWsRelPinRect.height());
    m_crossWsCancelScreenRect = QRectF(overlayRect.left() + m_crossWsRelCancelRect.x(), overlayRect.top() + m_crossWsRelCancelRect.y(), m_crossWsRelCancelRect.width(), m_crossWsRelCancelRect.height());

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

    const float x0 = overlayRect.left() * scale;
    const float y0 = overlayRect.top() * scale;
    const float x1 = overlayRect.right() * scale;
    const float y1 = overlayRect.bottom() * scale;

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

    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, m_crossWsActionOverlayTexture->matrix(NormalizedCoordinates));
    m_crossWsActionOverlayTexture->bind();
    vbo->render(GL_TRIANGLES);
    m_crossWsActionOverlayTexture->unbind();

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

bool ActiveWindowOutlineEffect::isEligibleTargetWindow(EffectWindow *w) const
{
    if (effects && effects->isScreenLocked()) {
        return false;
    }
    if (isSessionLockOrLogoutActive()) {
        return false;
    }
    if (!w || !isEligibleOutlineWindow(w)) {
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
        if (geo.width() < 10 || geo.height() < 10) {
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

        if (m_searchModeActive) {
            // Search Mode: only windows matched in m_searchMatches!
            bool foundInSearch = false;
            for (const auto &cand : m_searchMatches) {
                if (cand.windowPtr == w) {
                    foundInSearch = true;
                    break;
                }
            }
            if (!foundInSearch) {
                continue;
            }
        } else if (m_audioFilterModeActive) {
            // Audio Filter View: ONLY windows actively playing audio across all workspaces!
            if (!m_audioTracker.isWindowPlaying(w->pid(), w->caption())) {
                continue;
            }
        } else if (m_superJumpActive) {
            // Super Jump: all eligible windows across all workspaces, desktops, and stash!
        } else if (m_stashModeActive) { // Stash view: include both stashed windows and visible workspace windows
            const bool isStashed = isWindowInStash(w);

            bool isCurrentWorkspaceVisible = false;
            if (!isStashed) {
                if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
                    const auto &ws = m_workspaces.at(m_activeHudView - 1);
                    if (LeapKWorkspace::matchesWorkspace(ws, w->windowClass(), w->caption(), dNum)) {
                        isCurrentWorkspaceVisible = true;
                    }
                } else if (m_activeHudView == m_workspaces.size() + 1) {
                    isCurrentWorkspaceVisible = true;
                } else {
                    if (onCurrent || isMarked) {
                        isCurrentWorkspaceVisible = true;
                    }
                }
            }

            if (!isStashed && !isCurrentWorkspaceVisible) {
                continue;
            }
        } else {
            // In regular workspace views, stashed windows MUST stay hidden!
            if (isWindowInStash(w)) {
                continue;
            }

            if (m_pinnedWindows.contains(w)) {
                // Pinned window: always visible across all workspaces!
            } else if (m_tilingActive && isWindowInTileSlot(w)) {
                // Tiled window currently occupying a slot: always visible and jumpable!
            } else if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) { // Workspace view: only windows matching this workspace
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
                } else if (m_jumpTargets == 0 || m_jumpTargets == 1) { // Current desktop all windows (visible & minimized) + marked all desktops
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

    if (m_keyboardOverlayEnabled) {
        // Spatial Keyboard mode: Window groups are completely bypassed!
        // Every window is an individual target and gets a unique 1-to-1 spatial key mapping
        // based on its actual position on the screen. No duplicate shortcuts, no group shortcuts.
        QRectF screenGeo(0, 0, 1920, 1080);
        if (effects && effects->activeScreen()) {
            screenGeo = QRectF(QRect(effects->activeScreen()->geometry()));
        }
        const auto stacking = effects ? effects->stackingOrder() : QList<EffectWindow *>();
        QList<LeapKKeyboardOverlay::WindowTargetItem> items;
        for (EffectWindow *w : candidates) {
            LeapKKeyboardOverlay::WindowTargetItem item;
            item.windowId = w;
            item.geometry = w->frameGeometry();
            item.title = w->caption();
            item.appClass = w->windowClass().split(QLatin1Char(' ')).first().toLower();
            if (m_markedWindowLetters.contains(w)) {
                item.preferredKey = m_markedWindowLetters.value(w).toUpper();
            }
            if (m_markedWindowLabels.contains(w)) {
                item.customLabel = m_markedWindowLabels.value(w);
            }
            item.zOrder = stacking.indexOf(w);
            item.isOccluded = isWindowObscured(w);
            item.isVisibleOnScreen = w->isOnCurrentDesktop() && !w->isMinimized() && item.geometry.intersects(screenGeo);
            for (const auto &pair : m_hudTargetPillRects) {
                if (pair.second == w && pair.first.isValid()) {
                    item.hudCardRect = pair.first;
                    break;
                }
            }
            items.append(item);
        }

        const auto spatialMap = LeapKKeyboardOverlay::assignSpatialKeys(items, screenGeo);
        for (auto sit = spatialMap.constBegin(); sit != spatialMap.constEnd(); ++sit) {
            const QChar key = sit.key();
            EffectWindow *targetWin = reinterpret_cast<EffectWindow *>(sit.value().windowId);
            if (targetWin && !assignedLetters.contains(key)) {
                assignedLetters.insert(key);
                m_letterWindowGroups[key].append(targetWin);
                map[key] = targetWin;
                m_windowToDirectLetter[targetWin] = key;
            }
        }

        // Assign any remaining candidates (e.g. if windows exceed 26) using layout pool
        for (EffectWindow *w : candidates) {
            if (!m_windowToDirectLetter.contains(w)) {
                const QChar ch = nextFreeLetter();
                if (!ch.isNull()) {
                    m_letterWindowGroups[ch].append(w);
                    map[ch] = w;
                    m_windowToDirectLetter[w] = ch;
                }
            }
        }

        return map;
    }

    // Traditional HUD Jump Mode: group windows according to hudGroupWindows (Mode A or Mode B)
    for (EffectWindow *w : candidates) {
        if (m_markedWindowLetters.contains(w)) {
            const QChar ch = m_markedWindowLetters.value(w).toUpper();
            m_letterWindowGroups[ch].append(w);
            continue;
        }
        unassignedCandidates.append(w);
    }

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

void ActiveWindowOutlineEffect::exitSubLeapMode()
{
    m_subLeapActive = false;
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
    const bool hudActive = m_jumpMode || (m_badgePreviewActive && m_previewShowHud);
    if (hudActive) {
        if (m_hudHeaderScreenRect.isValid() && !m_hudHeaderScreenRect.isEmpty() && m_hudHeaderScreenRect.intersects(badgeRect)) {
            return true;
        }
        if (m_hudHintBarScreenRect.isValid() && !m_hudHintBarScreenRect.isEmpty() && m_hudHintBarScreenRect.intersects(badgeRect)) {
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
            || candidate->isDesktop() || candidate->isDock()
            || candidate->isNotification() || candidate->isPopupWindow()
            || candidate->isTooltip() || candidate->isLockScreen()
            || candidate->isInputMethod()
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

QRectF ActiveWindowOutlineEffect::findBestBadgePositionForWindow(
    EffectWindow *target,
    const QRectF &visibleWindowGeo,
    int defaultPosition,
    qreal badgeW,
    qreal badgeH,
    qreal margin,
    const QRectF &screenRect,
    const QList<QRectF> &hudRects,
    bool *outFoundVisible) const
{
    auto clampRect = [](QRectF r, const QRectF &sRect) -> QRectF {
        if (sRect.isValid() && !sRect.isEmpty()) {
            if (r.left() < sRect.left() + 4.0) r.moveLeft(sRect.left() + 4.0);
            if (r.right() > sRect.right() - 4.0) r.moveRight(sRect.right() - 4.0);
            if (r.top() < sRect.top() + 4.0) r.moveTop(sRect.top() + 4.0);
            if (r.bottom() > sRect.bottom() - 4.0) r.moveBottom(sRect.bottom() - 4.0);
        }
        return r;
    };

    QRectF badgeRect = clampRect(OutlineBadge::computeBadgeRect(visibleWindowGeo, defaultPosition, badgeW, badgeH, margin), screenRect);

    if (!isBadgeOccluded(target, badgeRect, hudRects)) {
        if (outFoundVisible) {
            *outFoundVisible = true;
        }
        return badgeRect;
    }

    // Choose alternative search order based on the default position family
    QList<int> altOrder;
    if (defaultPosition == 7 || defaultPosition == 6 || defaultPosition == 8) {
        // Bottom family: prefer other bottom positions, then sides, then center, then top
        altOrder = { 7, 6, 8, 3, 5, 4, 1, 0, 2 };
    } else if (defaultPosition == 1 || defaultPosition == 0 || defaultPosition == 2) {
        // Top family: prefer other top positions, then sides, then center, then bottom
        altOrder = { 1, 0, 2, 3, 5, 4, 7, 6, 8 };
    } else {
        // Middle / Center: prefer top, then bottom, then sides, then center
        altOrder = { 4, 1, 7, 3, 5, 0, 2, 6, 8 };
    }

    for (int altPos : altOrder) {
        if (altPos == defaultPosition) continue;
        QRectF altRect = clampRect(OutlineBadge::computeBadgeRect(visibleWindowGeo, altPos, badgeW, badgeH, margin), screenRect);

        if (!isBadgeOccluded(target, altRect, hudRects)) {
            if (outFoundVisible) {
                *outFoundVisible = true;
            }
            return altRect;
        }
    }

    // 3. Aggressive unoccluded visible region search:
    // When the 9 standard points are obstructed, compute the exact unoccluded visible region
    // of the window and place the badge inside the largest available unoccluded rectangle.
    QRegion unoccluded(visibleWindowGeo.toAlignedRect());
    bool above = false;
    const auto stacking = effects->stackingOrder();
    for (EffectWindow *candidate : stacking) {
        if (candidate == target) {
            above = true;
            continue;
        }
        if (!above) {
            continue;
        }
        if (!candidate || candidate->isDeleted() || !candidate->isVisible()
            || candidate->isMinimized() || !candidate->isOnCurrentDesktop()
            || candidate->isHiddenByShowDesktop() || candidate->isOutline()
            || candidate->isDesktop() || candidate->isDock()
            || candidate->isNotification() || candidate->isPopupWindow()
            || candidate->isTooltip() || candidate->isLockScreen()
            || candidate->isInputMethod()
            || candidate->opacity() <= 0.05
            || candidate->frameGeometry().width() < 10
            || candidate->frameGeometry().height() < 10) {
            continue;
        }
        const auto cg = candidate->frameGeometry();
        unoccluded -= QRect(qRound(cg.x()), qRound(cg.y()), qRound(cg.width()), qRound(cg.height()));
    }

    // Also subtract active HUD collision geometries
    for (const QRectF &hr : hudRects) {
        if (hr.isValid() && !hr.isEmpty()) {
            unoccluded -= hr.toAlignedRect();
        }
    }
    const bool hudActive = m_jumpMode || (m_badgePreviewActive && m_previewShowHud);
    if (hudActive) {
        if (m_hudHeaderScreenRect.isValid() && !m_hudHeaderScreenRect.isEmpty()) {
            unoccluded -= m_hudHeaderScreenRect.toAlignedRect();
        }
        if (m_hudHintBarScreenRect.isValid() && !m_hudHintBarScreenRect.isEmpty()) {
            unoccluded -= m_hudHintBarScreenRect.toAlignedRect();
        }
    }

    // A) Search for largest unoccluded rectangle that can contain the badge
    QRect bestSubRect;
    int maxArea = 0;
    for (const QRect &r : unoccluded) {
        if (r.width() >= badgeW && r.height() >= badgeH) {
            int area = r.width() * r.height();
            if (area > maxArea) {
                maxArea = area;
                bestSubRect = r;
            }
        }
    }
    if (!bestSubRect.isEmpty()) {
        qreal bx = bestSubRect.center().x() - (badgeW / 2.0);
        qreal by = bestSubRect.center().y() - (badgeH / 2.0);
        QRectF candidateRect = clampRect(QRectF(bx, by, badgeW, badgeH), screenRect);
        if (!isBadgeOccluded(target, candidateRect, hudRects)) {
            if (outFoundVisible) {
                *outFoundVisible = true;
            }
            return candidateRect;
        }
    }

    // B) Sample 5 anchor points in all visible rectangles that are at least 80% of badge dimensions
    for (const QRect &r : unoccluded) {
        if (r.width() >= badgeW * 0.8 && r.height() >= badgeH * 0.8) {
            const QPoint anchors[] = {
                r.center(),
                QPoint(r.left() + qRound(badgeW / 2.0) + 2, r.top() + qRound(badgeH / 2.0) + 2),
                QPoint(r.right() - qRound(badgeW / 2.0) - 2, r.top() + qRound(badgeH / 2.0) + 2),
                QPoint(r.left() + qRound(badgeW / 2.0) + 2, r.bottom() - qRound(badgeH / 2.0) - 2),
                QPoint(r.right() - qRound(badgeW / 2.0) - 2, r.bottom() - qRound(badgeH / 2.0) - 2)
            };
            for (const QPoint &pt : anchors) {
                QRectF testRect(pt.x() - badgeW / 2.0, pt.y() - badgeH / 2.0, badgeW, badgeH);
                testRect = clampRect(testRect, screenRect);
                if (!isBadgeOccluded(target, testRect, hudRects)) {
                    if (outFoundVisible) {
                        *outFoundVisible = true;
                    }
                    return testRect;
                }
            }
        }
    }

    // C) Check center of the overall bounding rect of unoccluded visible region
    const QRect unoccludedBounds = unoccluded.boundingRect();
    if (!unoccludedBounds.isEmpty() && (unoccludedBounds.width() >= badgeW || unoccludedBounds.height() >= badgeH)) {
        qreal bx = qBound(screenRect.left() + 4.0, (qreal)unoccludedBounds.center().x() - (badgeW / 2.0), screenRect.right() - badgeW - 4.0);
        qreal by = qBound(screenRect.top() + 4.0, (qreal)unoccludedBounds.center().y() - (badgeH / 2.0), screenRect.bottom() - badgeH - 4.0);
        QRectF candidateRect(bx, by, badgeW, badgeH);
        if (candidateRect.intersects(visibleWindowGeo) && !isBadgeOccluded(target, candidateRect, hudRects)) {
            if (outFoundVisible) {
                *outFoundVisible = true;
            }
            return candidateRect;
        }
    }

    // D) Fallback if substantial visible area remains (HUD is absolute last resort)
    if (!unoccluded.isEmpty()) {
        int totalVisibleArea = 0;
        for (const QRect &r : unoccluded) {
            totalVisibleArea += r.width() * r.height();
        }
        if (totalVisibleArea >= badgeW * badgeH * 1.2) {
            QRect bestR;
            int maxA = 0;
            for (const QRect &r : unoccluded) {
                if (r.width() * r.height() > maxA) {
                    maxA = r.width() * r.height();
                    bestR = r;
                }
            }
            if (!bestR.isEmpty()) {
                qreal bx = bestR.center().x() - (badgeW / 2.0);
                qreal by = bestR.center().y() - (badgeH / 2.0);
                QRectF candidateRect = clampRect(QRectF(bx, by, badgeW, badgeH), screenRect);
                if (outFoundVisible) {
                    *outFoundVisible = true;
                }
                return candidateRect;
            }
        }
    }

    if (outFoundVisible) {
        *outFoundVisible = false;
    }
    return badgeRect;
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
                                           bool isRecent,
                                           qreal opacity)
{
    const qreal scale = viewport.scale();
    const bool isStashed = isWindowInStash(target);
    const bool isPlayingAudio = target ? m_audioTracker.isWindowPlaying(target->pid(), target->caption()) : false;
    const quint64 scaleKey = static_cast<quint64>(qRound(scale * 100));
    const quint64 sizeKey = static_cast<quint64>(badgeSize);
    const quint64 labelKey = static_cast<quint64>(qHash(customLabel));
    const quint64 rKeyHash = static_cast<quint64>(qHash(recentKey));
    const quint64 windowKey = target ? static_cast<quint64>(qHash(target->internalId())) : 0;
    const quint64 previewKey = m_badgePreviewActive ? 0x8000 : 0;
    const quint64 stashedKey = isStashed ? 0x10000 : 0;
    const quint64 audioKey = isPlayingAudio ? 0x20000 : 0;
    const quint64 flagsKey = (m_badgeShowIcon ? 0x1000 : 0) | (m_badgeShowThumbnail ? 0x2000 : 0) | (isRecent ? 0x4000 : 0) | (isJumpMode ? 1 : 0) | previewKey | stashedKey | audioKey;
    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const quint64 themeHashVal = static_cast<quint64>(qHash(cfg.activeThemeId)) ^ static_cast<quint64>(qHash(cfg.activeThemeParamsJson));
    const quint64 key = (static_cast<quint64>(letter.unicode()) << 48)
        ^ (static_cast<quint64>(bgColor.rgba()) << 16)
        ^ (static_cast<quint64>(textColor.rgba()))
        ^ (scaleKey << 8)
        ^ (sizeKey << 32)
        ^ (labelKey << 24)
        ^ (rKeyHash << 20)
        ^ (windowKey << 12)
        ^ (themeHashVal << 4)
        ^ flagsKey;

    std::shared_ptr<GLTexture> texture;
    auto it = m_badgeTextureCache.find(key);
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
                                                          icon, thumbnail, recentKey, isRecent, isStashed, isPlayingAudio);
        auto uploaded = GLTexture::upload(img);
        if (!uploaded) {
            return;
        }
        uploaded->setFilter(GL_LINEAR);
        uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
        texture = std::shared_ptr<GLTexture>(uploaded.release());
        m_badgeTextureCache[key] = texture;
    }

    const qreal texW = texture->width() / scale;
    const qreal texH = texture->height() / scale;
    const QPointF bCenter = badgeRect.center();
    QRectF finalBadgeRect(bCenter.x() - texW / 2.0, bCenter.y() - texH / 2.0, texW, texH);

    const bool pulseBadge = cfg.activeTheme.resolveBool(QStringLiteral("$audioPulseBadge"), cfg.activeTheme.windowBadge.audioPulseBadge);
    const double pulseIntensity = cfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseIntensity"), cfg.activeTheme.windowBadge.audioPulseIntensity);
    const double pulseSpeed = cfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseSpeed"), cfg.activeTheme.windowBadge.audioPulseSpeed);
    if (isPlayingAudio && pulseBadge && pulseIntensity > 0.001) {
        const qint64 elapsedMs = m_animationClock.isValid() ? m_animationClock.elapsed() : 0;
        const qreal elapsedSec = elapsedMs / 1000.0;
        const qreal pulse = 1.0 + (0.08 * pulseIntensity) * std::sin(elapsedSec * 8.0 * pulseSpeed);
        const qreal pw = texW * pulse;
        const qreal ph = texH * pulse;
        finalBadgeRect = QRectF(bCenter.x() - pw / 2.0, bCenter.y() - ph / 2.0, pw, ph);
    }

    const float x0 = finalBadgeRect.left() * scale;
    const float y0 = finalBadgeRect.top() * scale;
    const float x1 = finalBadgeRect.right() * scale;
    const float y1 = finalBadgeRect.bottom() * scale;

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

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, texture->matrix(NormalizedCoordinates));
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    const float effAlpha = float(qBound(0.0, opacity, 1.0));
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(effAlpha, effAlpha, effAlpha, effAlpha));

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

void ActiveWindowOutlineEffect::renderFloatingPill(const RenderTarget &renderTarget,
                                                  const RenderViewport &viewport,
                                                  const QString &text,
                                                  const QPointF &centerPos,
                                                  const QColor &bgColor,
                                                  const QColor &textColor,
                                                  int fontSizePt,
                                                  qreal maxAllowedWidth,
                                                  const QString &shapeOverride)
{
    if (text.trimmed().isEmpty()) {
        return;
    }

    const qreal scale = viewport.scale();
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const QImage img = OutlineBadge::renderFloatingPillImage(text, scale, bgColor, textColor, fontSizePt, maxAllowedWidth, theme, shapeOverride);
    auto uploaded = GLTexture::upload(img);
    if (!uploaded) {
        return;
    }
    uploaded->setFilter(GL_LINEAR);
    uploaded->setWrapMode(GL_CLAMP_TO_EDGE);

    const qreal w = img.width() / scale;
    const qreal h = img.height() / scale;
    const QRectF destRect(centerPos.x() - w / 2.0, centerPos.y() - h / 2.0, w, h);

    const float x0 = destRect.left() * scale;
    const float y0 = destRect.top() * scale;
    const float x1 = destRect.right() * scale;
    const float y1 = destRect.bottom() * scale;

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

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, uploaded->matrix(NormalizedCoordinates));
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    uploaded->bind();
    vbo->render(GL_TRIANGLES);
    uploaded->unbind();

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
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    if (isWindowInStash(target)) {
        const QString stashIcon = theme.hudNav.stashIcon;
        const QString stashLabel = theme.hudNav.stashText.isEmpty() ? QStringLiteral("Stash") : theme.hudNav.stashText;
        const QString stashPrefix = stashIcon.isEmpty() ? stashLabel : (stashIcon + QLatin1Char(' ') + stashLabel);
        if (!target->isOnCurrentDesktop() && !target->isOnAllDesktops()) {
            const auto desktops = target->desktops();
            const QString dName = (!desktops.isEmpty() && !desktops.first()->name().isEmpty())
                ? desktops.first()->name()
                : (desktops.isEmpty() ? QString() : QStringLiteral("Desktop %1").arg(desktops.first()->x11DesktopNumber()));
            return dName.isEmpty() ? stashPrefix : QStringLiteral("%1 · %2").arg(stashPrefix, dName);
        }
        return stashPrefix;
    }

    if (isWindowPinned(target)) {
        return QStringLiteral("● Pinned");
    }

    const int wsIdx = findWorkspaceForWindow(target);
    QString wsPrefix;
    if (wsIdx >= 1 && wsIdx <= m_workspaces.size()) {
        const auto &ws = m_workspaces.at(wsIdx - 1);
        const QString wsIcon = theme.hudNav.workspaceIcon;
        wsPrefix = (wsIcon.isEmpty() ? QString() : (wsIcon + QLatin1Char(' '))) + ws.name;
    }

    QString loc;
    if (!target->isOnCurrentDesktop()) {
        const auto desktops = target->desktops();
        if (!desktops.isEmpty() && !target->isOnAllDesktops()) {
            loc = desktops.first()->name();
        }
        if (loc.isEmpty()) {
            const uint num = desktops.isEmpty() ? 0 : desktops.first()->x11DesktopNumber();
            loc = QStringLiteral("Desktop %1").arg(num);
        }
    } else if (target->isMinimized()) {
        loc = QStringLiteral("Minimized");
    } else if (m_badgePreviewActive || target->isVisible()) {
        loc = QStringLiteral("Current Desktop");
    } else {
        loc = QStringLiteral("Covered");
    }

    if (!wsPrefix.isEmpty()) {
        if (m_superJumpActive || m_activeHudView == m_workspaces.size() + 1) {
            return QStringLiteral("%1 · %2").arg(wsPrefix, loc);
        }
    }
    return loc;
}

QList<QRectF> ActiveWindowOutlineEffect::computeHudPillRects(
    const QList<QPair<QChar, EffectWindow *>> &targets,
    const RenderViewport &viewport,
    int badgeSize,
    const OutlineBadge::BadgeConfig &cfg,
    int dividerAfterIndex,
    QList<QRectF> *outDividerRects) const
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
        const QString recentKey = (isMostRecent && m_jumpRecentKey.compare(QLatin1String("Space"), Qt::CaseInsensitive) != 0)
            ? (m_badgePreviewActive ? cfg.jumpRecentKey : m_jumpRecentKey)
            : QString();
        const QString loc = getWindowLocationString(target);
        const QString caption = target->caption();
        QString customLabel = m_markedWindowLabels.value(target);
        if (isWindowPinned(target) && !customLabel.isEmpty()) {
            customLabel = QStringLiteral("● %1").arg(customLabel);
        }
        const bool hasIcon = cfg.hudShowIcon && !target->icon().isNull();
        const bool hasThumb = cfg.hudShowThumbnail && m_thumbnailCache.contains(target);
        const QSize thumbSize = hasThumb ? m_thumbnailCache.value(target).size() : QSize();
        const bool isStashed = isWindowInStash(target);
        const bool isPinned = isWindowPinned(target);
        const bool isPlayingAudio = m_audioTracker.isWindowPlaying(target->pid(), target->caption());

        const QSizeF sz = OutlineBadge::computeHudPillSize(cfg, loc, caption, badgeSize, scale,
                                                           customLabel, hasIcon, hasThumb, thumbSize,
                                                           recentKey, isRecent, 0, isStashed, isPinned, isPlayingAudio);
        pillSizes.append(sz);
    }

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    const OutlineBadge::BadgeConfig &hudCfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    const qreal hudRowGap = (!hudCfg.hudGroupWindows && !m_subLeapActive) ? 40.0 : 12.0;
    const qreal headerH = ensureHudHeaderTexture(scale).height();
    const qreal effMargin = computeHudEffectiveMargin(hudPos, scale, screenRect, headerH);
    return OutlineBadge::computeHudPillPositions(pillSizes, screenRect, hudPos, effMargin, 12.0, hudRowGap, dividerAfterIndex, 28.0, outDividerRects);
}

QRectF ActiveWindowOutlineEffect::computeActiveMarkerRectForTargets(
    const QList<QRectF> &pillRects,
    const QList<QPair<QChar, EffectWindow *>> &targets,
    int dividerAfterIndex,
    int hudPos) const
{
    if (pillRects.isEmpty() || targets.isEmpty()) {
        return QRectF();
    }

    EffectWindow *currActive = effects ? effects->activeWindow() : m_activeWindow;
    if (!currActive || currActive->isDeleted()) {
        return QRectF();
    }

    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;

    for (int i = 0; i < targets.size(); ++i) {
        if (targets.at(i).second == currActive) {
            return QRectF();
        }
        if (!m_subLeapActive && cfg.hudGroupWindows && m_groupJumpLetters.contains(targets.at(i).first)) {
            const auto &winList = m_groupJumpLetters.value(targets.at(i).first);
            if (winList.contains(currActive)) {
                return QRectF();
            }
        }
    }

    const auto &history = !m_hudSnapshotHistory.isEmpty() ? m_hudSnapshotHistory : m_windowHistory;
    auto recencyRank = [&history](EffectWindow *w) -> int {
        if (!w) return 999999;
        int idx = history.indexOf(w);
        return idx >= 0 ? idx : 999999;
    };

    const bool activeIsLeft = m_stashModeActive ? !isWindowInStash(currActive) : isWindowPinned(currActive);
    int startIdx = 0;
    int endIdx = targets.size() - 1;

    if (dividerAfterIndex >= 0) {
        if (activeIsLeft) {
            startIdx = 0;
            endIdx = dividerAfterIndex;
        } else {
            startIdx = dividerAfterIndex + 1;
            endIdx = targets.size() - 1;
        }
    }

    int insertIndex = startIdx;
    const int activeRank = recencyRank(currActive);

    if (startIdx <= endIdx && startIdx >= 0 && endIdx < targets.size()) {
        bool found = false;
        for (int k = startIdx; k <= endIdx; ++k) {
            int targetRank = recencyRank(targets.at(k).second);
            if (activeRank > targetRank) {
                insertIndex = k;
                found = true;
                break;
            }
        }
        if (!found) {
            insertIndex = endIdx + 1;
        }
    } else if (activeIsLeft) {
        insertIndex = 0;
    } else {
        insertIndex = targets.size();
    }

    return OutlineBadge::computeActiveMarkerRect(pillRects, insertIndex, hudPos, 12.0, dividerAfterIndex);
}

QList<QRectF> ActiveWindowOutlineEffect::computeAllHudCollisionRects(
    const QList<QPair<QChar, EffectWindow *>> &targets,
    const RenderViewport &viewport,
    int badgeSize,
    const OutlineBadge::BadgeConfig &cfg,
    int dividerAfterIndex) const
{
    const QRectF screenRect = viewport.renderRect();

    QList<QRectF> collisionRects;
    QList<QRectF> dividerRects;

    // 1. Compute pills rects
    const QList<QRectF> pillRects = computeHudPillRects(targets, viewport, badgeSize, cfg, dividerAfterIndex, &dividerRects);
    collisionRects.append(pillRects);
    collisionRects.append(dividerRects);

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    const QRectF activeMarkerRect = computeActiveMarkerRectForTargets(pillRects, targets, dividerAfterIndex, hudPos);
    if (activeMarkerRect.isValid() && !activeMarkerRect.isEmpty()) {
        collisionRects.append(activeMarkerRect);
    }

    // 2. Compute group frames if grouping is off and multiple windows share group letters
    QRectF allPillsBounds;
    for (const QRectF &r : pillRects) {
        allPillsBounds = allPillsBounds.isEmpty() ? r : allPillsBounds.united(r);
    }
    if (activeMarkerRect.isValid() && !activeMarkerRect.isEmpty()) {
        allPillsBounds = allPillsBounds.isEmpty() ? activeMarkerRect : allPillsBounds.united(activeMarkerRect);
    }

    if (!cfg.hudGroupWindows && !m_subLeapActive) {
        const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
        const int effGroupFontSize = (cfg.hudGroupFontSize > 0) ? cfg.hudGroupFontSize : 11;
        const int tabH = qMax(22, effGroupFontSize + 10);
        const bool tabAtBottom = (hudPos == 1);
        const qreal padX = 8.0;

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
            const auto &indices = it.value();
            QMap<int, QList<int>> segmentToIndices;
            for (int idx : indices) {
                if (idx >= 0 && idx < pillRects.size()) {
                    const int bucket = (hudPos == 2 || hudPos == 3)
                        ? qRound(pillRects.at(idx).center().x() / 20.0)
                        : qRound(pillRects.at(idx).center().y() / 20.0);
                    segmentToIndices[bucket].append(idx);
                }
            }
            for (auto segIt = segmentToIndices.constBegin(); segIt != segmentToIndices.constEnd(); ++segIt) {
                QRectF groupBoundingRect;
                for (int idx : segIt.value()) {
                    groupBoundingRect = groupBoundingRect.isEmpty() ? pillRects.at(idx) : groupBoundingRect.united(pillRects.at(idx));
                }
                if (!groupBoundingRect.isEmpty()) {
                    const qreal padBottom = tabAtBottom ? (tabH + 8.0) : 6.0;
                    const qreal padTop = tabAtBottom ? 6.0 : (tabH + 8.0);
                    QRectF frameRect = groupBoundingRect.adjusted(-padX, -padTop, padX, padBottom);
                    if (frameRect.left() < screenRect.left() + 4.0) frameRect.setLeft(screenRect.left() + 4.0);
                    if (frameRect.right() > screenRect.right() - 4.0) frameRect.setRight(screenRect.right() - 4.0);
                    if (frameRect.top() < screenRect.top() + 4.0) frameRect.setTop(screenRect.top() + 4.0);
                    if (frameRect.bottom() > screenRect.bottom() - 4.0) frameRect.setBottom(screenRect.bottom() - 4.0);
                    collisionRects.append(frameRect);
                    allPillsBounds = allPillsBounds.isEmpty() ? frameRect : allPillsBounds.united(frameRect);
                }
            }
        }
    }

    if (!allPillsBounds.isEmpty()) {
        collisionRects.append(allPillsBounds);
    }

    // 3. Compute HUD hint bar rect
    const QRectF hintRect = computeHudHintBarRect(viewport, allPillsBounds);
    if (hintRect.isValid() && !hintRect.isEmpty()) {
        collisionRects.append(hintRect);
        m_hudHintBarScreenRect = hintRect;
    } else {
        m_hudHintBarScreenRect = QRectF();
    }

    // 4. Compute HUD mode header rect
    const QRectF headerRect = computeHudHeaderRect(viewport, allPillsBounds, hintRect);
    if (headerRect.isValid() && !headerRect.isEmpty()) {
        collisionRects.append(headerRect);
        m_hudHeaderScreenRect = headerRect;
    } else {
        m_hudHeaderScreenRect = QRectF();
    }

    return collisionRects;
}

void ActiveWindowOutlineEffect::renderOffscreenJumpTargets(const RenderTarget &renderTarget,
                                                           const RenderViewport &viewport,
                                                           const QList<QPair<QChar, EffectWindow *>> &targets,
                                                           int badgeSize,
                                                           int dividerAfterIndex)
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

    const OutlineBadge::BadgeConfig &cfg = m_badgePreviewActive ? m_previewBadgeConfig : m_badgeConfig;
    EffectWindow *currActive = effects ? effects->activeWindow() : m_activeWindow;
    int activeDockIndex = -1;
    if (currActive) {
        for (int k = 0; k < targets.size(); ++k) {
            if (targets.at(k).second == currActive) {
                activeDockIndex = k;
                break;
            }
            if (!m_subLeapActive && cfg.hudGroupWindows && m_groupJumpLetters.contains(targets.at(k).first)) {
                const auto &winList = m_groupJumpLetters.value(targets.at(k).first);
                if (winList.contains(currActive)) {
                    activeDockIndex = k;
                    break;
                }
            }
        }
    }

    for (int i = 0; i < targets.size(); ++i) {
        const auto &pair = targets.at(i);
        const QChar letter = pair.first;
        EffectWindow *target = pair.second;
        if (!target || target->isDeleted()) {
            continue;
        }

        int groupCount = 0;
        if (!m_subLeapActive && cfg.hudGroupWindows && m_groupJumpLetters.contains(letter)) {
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
        const QString recentKey = (isMostRecent && m_jumpRecentKey.compare(QLatin1String("Space"), Qt::CaseInsensitive) != 0)
            ? (m_badgePreviewActive ? m_previewBadgeConfig.jumpRecentKey : m_jumpRecentKey)
            : QString();
        const bool isStashed = isWindowInStash(target);
        const bool isPinned = isWindowPinned(target);
        const bool isPlayingAudio = m_audioTracker.isWindowPlaying(target->pid(), target->caption());

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
        } else {
            bgCol = cfg.defaultBgColor;
        }
        const QColor textCol = OutlineBadge::contrastingTextColor(bgCol);

        const QString loc = getWindowLocationString(target);

        const QString caption = target->caption();
        QString customLabel = m_markedWindowLabels.value(target);
        if (m_closeWindowModeActive) {
            customLabel = customLabel.isEmpty() ? QStringLiteral("✕") : QStringLiteral("✕ %1").arg(customLabel);
        } else if (m_pinModeActive) {
            customLabel = isPinned ? QStringLiteral("📌") : QStringLiteral("+📌");
        } else if (isPinned && !customLabel.isEmpty()) {
            customLabel = QStringLiteral("● %1").arg(customLabel);
        } else if (isStashed && !customLabel.isEmpty()) {
            customLabel = QStringLiteral("📦 %1").arg(customLabel);
        }

        const bool showIconOnHud = cfg.hudShowIcon;
        const bool showThumbOnHud = cfg.hudShowThumbnail;
        const QIcon icon = (showIconOnHud && target) ? target->icon() : QIcon();
        const QImage thumbnail = (showThumbOnHud && target)
            ? captureWindowThumbnail(target, renderTarget, viewport, QSize(cfg.hudThumbnailWidth * 2, cfg.hudThumbnailHeight * 2))
            : QImage();

        const bool isSelected = m_searchModeActive ? (i == m_searchSelectedIndex) : (i == activeDockIndex);
        const QImage img = OutlineBadge::renderOffscreenTargetPill(cfg, letter, bgCol, textCol, loc, caption, badgeSize, scale, customLabel,
                                                                   icon, thumbnail, isMarked, recentKey, isRecent, groupCount, isStashed, isPinned, isPlayingAudio, isSelected);
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
    const qreal headerH = ensureHudHeaderTexture(scale).height();
    const qreal effMargin = computeHudEffectiveMargin(hudPos, scale, screenRect, headerH);
    QList<QRectF> dividerRects;
    const QList<QRectF> rects = OutlineBadge::computeHudPillPositions(pillSizes, screenRect, hudPos, effMargin, 12.0, hudRowGap, dividerAfterIndex, 28.0, &dividerRects);
    const QRectF activeMarkerRect = computeActiveMarkerRectForTargets(rects, targets, dividerAfterIndex, hudPos);

    m_hudTargetPillRects.clear();
    for (int i = 0; i < targets.size() && i < rects.size(); ++i) {
        if (targets.at(i).second) {
            m_hudTargetPillRects.append({rects.at(i), targets.at(i).second});
        }
    }
    m_hudCurrentActiveDockIndex = activeDockIndex;
    m_hudCurrentMarkerRect = activeMarkerRect;

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

    // 2a. Render Audio Shader Glow behind audio HUD cards
    if (m_audioHudShaderEnabled && m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty() && m_audioTracker.hasPlayingAudio()) {
        for (int i = 0; i < pills.size(); ++i) {
            const bool pillAudio = (i < targets.size() && targets.at(i).second)
                ? m_audioTracker.isWindowPlaying(targets.at(i).second->pid(), targets.at(i).second->caption())
                : false;
            if (pillAudio) {
                QRectF rect = rects.at(i);
                const bool pulseCard = hudCfg.activeTheme.resolveBool(QStringLiteral("$audioPulseBadge"), hudCfg.activeTheme.hudCard.audioPulseBadge);
                const double pulseIntensity = hudCfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseIntensity"), hudCfg.activeTheme.hudCard.audioPulseIntensity);
                const double pulseSpeed = hudCfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseSpeed"), hudCfg.activeTheme.hudCard.audioPulseSpeed);
                if (pulseCard && pulseIntensity > 0.001) {
                    const qint64 elapsedMs = m_animationClock.isValid() ? m_animationClock.elapsed() : 0;
                    const qreal elapsedSec = elapsedMs / 1000.0;
                    const qreal pulse = 1.0 + (0.05 * pulseIntensity) * std::sin(elapsedSec * 8.0 * pulseSpeed);
                    const QPointF center = rect.center();
                    const qreal pw = rect.width() * pulse;
                    const qreal ph = rect.height() * pulse;
                    rect = QRectF(center.x() - pw / 2.0, center.y() - ph / 2.0, pw, ph);
                }
                renderHudPillAudioShader(renderTarget, viewport, rect, scale, hudCfg, false /* isOutlinePass = glow */);
            }
        }
    }

    // 2b. Render HUD pills on top with full modulation
    ShaderBinder pillBinder(shader);
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));
    for (int i = 0; i < pills.size(); ++i) {
        const PillItem &item = pills.at(i);
        QRectF rect = rects.at(i);

        const bool pillAudio = (i < targets.size() && targets.at(i).second)
            ? m_audioTracker.isWindowPlaying(targets.at(i).second->pid(), targets.at(i).second->caption())
            : false;
        const bool pulseCard = hudCfg.activeTheme.resolveBool(QStringLiteral("$audioPulseBadge"), hudCfg.activeTheme.hudCard.audioPulseBadge);
        const double pulseIntensity = hudCfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseIntensity"), hudCfg.activeTheme.hudCard.audioPulseIntensity);
        const double pulseSpeed = hudCfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseSpeed"), hudCfg.activeTheme.hudCard.audioPulseSpeed);
        if (pillAudio && pulseCard && pulseIntensity > 0.001) {
            const qint64 elapsedMs = m_animationClock.isValid() ? m_animationClock.elapsed() : 0;
            const qreal elapsedSec = elapsedMs / 1000.0;
            const qreal pulse = 1.0 + (0.05 * pulseIntensity) * std::sin(elapsedSec * 8.0 * pulseSpeed);
            const QPointF center = rect.center();
            const qreal pw = rect.width() * pulse;
            const qreal ph = rect.height() * pulse;
            rect = QRectF(center.x() - pw / 2.0, center.y() - ph / 2.0, pw, ph);
        }

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

    // 2c. Render Audio Shader Outline on top of audio HUD cards (if outline target)
    if (m_audioHudShaderEnabled && m_audioShaderRuleEnabled && !m_audioShaderId.isEmpty() && m_audioTracker.hasPlayingAudio()) {
        for (int i = 0; i < pills.size(); ++i) {
            const bool pillAudio = (i < targets.size() && targets.at(i).second)
                ? m_audioTracker.isWindowPlaying(targets.at(i).second->pid(), targets.at(i).second->caption())
                : false;
            if (pillAudio) {
                QRectF rect = rects.at(i);
                const bool pulseCard = hudCfg.activeTheme.resolveBool(QStringLiteral("$audioPulseBadge"), hudCfg.activeTheme.hudCard.audioPulseBadge);
                const double pulseIntensity = hudCfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseIntensity"), hudCfg.activeTheme.hudCard.audioPulseIntensity);
                const double pulseSpeed = hudCfg.activeTheme.resolveDouble(QStringLiteral("$audioPulseSpeed"), hudCfg.activeTheme.hudCard.audioPulseSpeed);
                if (pulseCard && pulseIntensity > 0.001) {
                    const qint64 elapsedMs = m_animationClock.isValid() ? m_animationClock.elapsed() : 0;
                    const qreal elapsedSec = elapsedMs / 1000.0;
                    const qreal pulse = 1.0 + (0.05 * pulseIntensity) * std::sin(elapsedSec * 8.0 * pulseSpeed);
                    const QPointF center = rect.center();
                    const qreal pw = rect.width() * pulse;
                    const qreal ph = rect.height() * pulse;
                    rect = QRectF(center.x() - pw / 2.0, center.y() - ph / 2.0, pw, ph);
                }
                renderHudPillAudioShader(renderTarget, viewport, rect, scale, hudCfg, true /* isOutlinePass = outline */);
            }
        }
    }

    // 3. Render HUD divider lines between groups
    for (const QRectF &dRect : dividerRects) {
        const bool isVert = (dRect.height() >= dRect.width());
        const QImage divImg = OutlineBadge::renderHudDividerLine(dRect.size(), scale, isVert);
        auto divTex = GLTexture::upload(divImg);
        if (!divTex) {
            continue;
        }
        divTex->setFilter(GL_LINEAR);
        divTex->setWrapMode(GL_CLAMP_TO_EDGE);

        const float x0 = dRect.left() * scale;
        const float y0 = dRect.top() * scale;
        const float x1 = dRect.right() * scale;
        const float y1 = dRect.bottom() * scale;

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

        shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, divTex->matrix(NormalizedCoordinates));
        divTex->bind();
        vbo->render(GL_TRIANGLES);
        divTex->unbind();
    }

    // 4. Render active HUD marker line with smooth transition animation
    const qint64 animElapsed = (m_hudSelectAnimActive && m_hudSelectAnimTimer.isValid())
        ? m_hudSelectAnimTimer.elapsed()
        : 999;
    const bool isAnimActive = m_jumpMode && m_hudSelectAnimActive && (animElapsed < 250);
    const qreal progress = isAnimActive ? qBound(0.0, animElapsed / 250.0, 1.0) : 1.0;
    const qreal easeOut = 1.0 - std::pow(1.0 - progress, 3.0);

    auto drawMarker = [&](const QRectF &mRect, float alpha) {
        if (!mRect.isValid() || mRect.isEmpty() || alpha <= 0.005f) {
            return;
        }
        const bool isVert = (mRect.height() >= mRect.width());
        const QColor markerColor = hudCfg.activeTheme.resolveColor(
            hudCfg.activeTheme.hudCard.borderActive,
            hudCfg.activeTheme.resolveColor(QStringLiteral("$borderColor"), QColor(0, 195, 255)));

        const QImage markerImg = OutlineBadge::renderHudActiveMarkerLine(mRect.size(), scale, markerColor, isVert);
        auto markerTex = GLTexture::upload(markerImg);
        if (!markerTex) {
            return;
        }
        markerTex->setFilter(GL_LINEAR);
        markerTex->setWrapMode(GL_CLAMP_TO_EDGE);

        const float x0 = mRect.left() * scale;
        const float y0 = mRect.top() * scale;
        const float x1 = mRect.right() * scale;
        const float y1 = mRect.bottom() * scale;

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

        shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(alpha, alpha, alpha, alpha));
        shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, markerTex->matrix(NormalizedCoordinates));
        markerTex->bind();
        vbo->render(GL_TRIANGLES);
        markerTex->unbind();
    };

    if (isAnimActive) {
        if (m_hudPrevMarkerRect.isValid() && activeMarkerRect.isValid()) {
            QRectF animRect;
            const qreal curX = m_hudPrevMarkerRect.left() + (activeMarkerRect.left() - m_hudPrevMarkerRect.left()) * easeOut;
            const qreal curY = m_hudPrevMarkerRect.top() + (activeMarkerRect.top() - m_hudPrevMarkerRect.top()) * easeOut;
            animRect = QRectF(curX, curY, activeMarkerRect.width(), activeMarkerRect.height());

            drawMarker(m_hudPrevMarkerRect, float((1.0 - easeOut) * 0.45));
            drawMarker(animRect, 1.0f);
            m_hudLastDrawnMarkerRect = animRect;
        } else if (m_hudPrevMarkerRect.isValid() && !activeMarkerRect.isValid()) {
            drawMarker(m_hudPrevMarkerRect, float(1.0 - easeOut));
            m_hudLastDrawnMarkerRect = m_hudPrevMarkerRect;
        } else if (!m_hudPrevMarkerRect.isValid() && activeMarkerRect.isValid()) {
            drawMarker(activeMarkerRect, float(easeOut));
            m_hudLastDrawnMarkerRect = activeMarkerRect;
        }
    } else {
        if (activeMarkerRect.isValid()) {
            drawMarker(activeMarkerRect, 1.0f);
            m_hudLastDrawnMarkerRect = activeMarkerRect;
        } else {
            m_hudLastDrawnMarkerRect = QRectF();
        }
    }

    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

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
    if (activeMarkerRect.isValid() && !activeMarkerRect.isEmpty()) {
        allPillsBounds = allPillsBounds.isEmpty() ? activeMarkerRect : allPillsBounds.united(activeMarkerRect);
    }
    if (m_hudPrevMarkerRect.isValid() && !m_hudPrevMarkerRect.isEmpty()) {
        allPillsBounds = allPillsBounds.isEmpty() ? m_hudPrevMarkerRect : allPillsBounds.united(m_hudPrevMarkerRect);
    }
    for (const GroupFrameItem &gf : groupFrames) {
        allPillsBounds = allPillsBounds.united(gf.rect);
    }
    const QRectF hintRect = renderHudHintBar(renderTarget, viewport, allPillsBounds);
    const QRectF headerRect = renderHudModeHeader(renderTarget, viewport, allPillsBounds, hintRect);
    if (m_badgePreviewActive) {
        m_previewHudBounds = allPillsBounds;
        if (headerRect.isValid()) {
            m_previewHudBounds = m_previewHudBounds.united(headerRect);
        } else if (m_hudHeaderScreenRect.isValid()) {
            m_previewHudBounds = m_previewHudBounds.united(m_hudHeaderScreenRect);
        }
        if (hintRect.isValid()) {
            m_previewHudBounds = m_previewHudBounds.united(hintRect);
        } else if (m_hudHintBarScreenRect.isValid()) {
            m_previewHudBounds = m_previewHudBounds.united(m_hudHintBarScreenRect);
        }
    }
}

QSizeF ActiveWindowOutlineEffect::ensureHudHintBarTexture(qreal scale, const QRectF &screenRect) const
{
    const bool showHints = m_badgePreviewActive ? m_previewBadgeConfig.hudShowHints : m_badgeConfig.hudShowHints;
    if (!showHints) {
        return QSizeF(0, 0);
    }

    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    QString customViewName;
    QColor customViewColor;
    if (m_stashModeActive) {
        const QString stashIcon = theme.hudNav.stashIcon;
        const QString stashTxt = theme.hudNav.stashText.isEmpty() ? QStringLiteral("Stash") : theme.hudNav.stashText;
        customViewName = QStringLiteral("View: ") + (stashIcon.isEmpty() ? stashTxt : (stashIcon + QLatin1Char(' ') + stashTxt));
        customViewColor = QColor(46, 204, 113); // Emerald Green
    } else if (m_activeHudView == 0) {
        const QString stdIcon = theme.hudNav.standardIcon;
        const QString stdTxt = theme.hudNav.standardText.isEmpty() ? QStringLiteral("Standard") : theme.hudNav.standardText;
        customViewName = QStringLiteral("View: ") + (stdIcon.isEmpty() ? stdTxt : (stdIcon + QLatin1Char(' ') + stdTxt));
        customViewColor = QColor(230, 126, 34); // Orange
    } else if (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) {
        const auto &ws = m_workspaces.at(m_activeHudView - 1);
        const QString wsIcon = theme.hudNav.workspaceIcon;
        customViewName = QStringLiteral("View: ") + (wsIcon.isEmpty() ? ws.name : (wsIcon + QLatin1Char(' ') + ws.name));
        customViewColor = ws.color.isValid() ? ws.color : QColor(52, 152, 219);
    } else {
        const QString allIcon = theme.hudNav.allIcon;
        const QString allTxt = theme.hudNav.allText.isEmpty() ? QStringLiteral("All") : theme.hudNav.allText;
        customViewName = QStringLiteral("View: ") + (allIcon.isEmpty() ? allTxt : (allIcon + QLatin1Char(' ') + allTxt));
        customViewColor = QColor(155, 89, 182); // Purple
    }

    const int hintsFontSize = m_badgePreviewActive ? m_previewBadgeConfig.hudHintsFontSize : m_badgeConfig.hudHintsFontSize;
    const int themeHash = qHash(theme.id) % 10000;
    const qreal maxAllowedForFont = (hintsFontSize > 12)
        ? qMin(qreal(screenRect.width() - 80.0), qreal(1050.0 + (hintsFontSize - 12) * 50.0))
        : 1050.0;
    const qreal maxHintBarWidth = qMin(qreal(screenRect.width() - 80.0), maxAllowedForFont) * scale;
    const int candCount = m_subLeapActive ? m_subLeapLetterToWindow.size() : m_jumpLetterToWindow.size();
    const int nameHash = (m_activeHudView >= 1 && m_activeHudView <= m_workspaces.size()) ? (qHash(m_workspaces.at(m_activeHudView - 1).name) % 10000) : 0;
    const int hintBarKey = (m_subLeapActive ? 100000 : 0) + (m_stashModeActive ? 500000 : 0) + (m_superJumpActive ? 700000 : 0)
        + (m_audioFilterModeActive ? 900000 : 0)
        + (m_searchModeActive ? 1100000 : 0)
        + (m_tilingPresetsModeActive ? 1300000 : 0)
        + (m_closeWindowModeActive ? 1500000 : 0)
        + (m_pinModeActive ? 1700000 : 0)
        + (m_crossWsActionActive ? 3500000 : 0)
        + (candCount == 1 ? 2500000 : 0)
        + m_activeHudView * 1000 + nameHash + qRound(scale * 10) + (qRound(maxHintBarWidth) % 5000)
        + hintsFontSize * 19 + themeHash;
    if (!m_hudHintBarTexture || m_hudHintBarCacheKey != hintBarKey) {
        const QImage hintImg = OutlineBadge::renderHudActionHintBar(
            scale, m_subLeapActive, m_activeHudView, customViewName, customViewColor,
            m_stashModeActive, m_superJumpActive, m_audioFilterModeActive, m_searchModeActive,
            maxHintBarWidth, hintsFontSize, theme, m_tilingPresetsModeActive,
            m_closeWindowModeActive, m_pinModeActive, candCount, m_crossWsActionActive);
        auto uploaded = GLTexture::upload(hintImg);
        if (uploaded) {
            uploaded->setFilter(GL_LINEAR);
            uploaded->setWrapMode(GL_CLAMP_TO_EDGE);
            m_hudHintBarTexture = std::shared_ptr<GLTexture>(uploaded.release());
            m_hudHintBarCacheKey = hintBarKey;
        }
    }

    if (!m_hudHintBarTexture) {
        return QSizeF(0, 0);
    }

    return QSizeF(m_hudHintBarTexture->width() / scale, m_hudHintBarTexture->height() / scale);
}

qreal ActiveWindowOutlineEffect::computeHudEffectiveMargin(int hudPos, qreal scale, const QRectF &screenRect, qreal headerH) const
{
    const qreal effHeaderH = headerH > 0.0 ? headerH : 44.0;
    if (hudPos == 1) { // Top HUD: badges below header
        return qMax(24.0, 12.0 + effHeaderH + 12.0);
    }
    if (hudPos == 0) { // Bottom HUD: badges on top, hint bar in middle, navigation header pinned at bottom
        const bool showHints = m_badgePreviewActive ? m_previewBadgeConfig.hudShowHints : m_badgeConfig.hudShowHints;
        qreal bottomSpace = 12.0 + effHeaderH;
        if (showHints) {
            const qreal hintH = ensureHudHintBarTexture(scale, screenRect).height();
            const qreal effHintH = hintH > 0.0 ? hintH : 28.0;
            bottomSpace += 8.0 + effHintH;
        }
        bottomSpace += 12.0;
        return qMax(24.0, bottomSpace);
    }
    return 24.0;
}

QRectF ActiveWindowOutlineEffect::computeHudHintBarRect(const RenderViewport &viewport,
                                                       const QRectF &pillsBounds) const
{
    const bool showHints = m_badgePreviewActive ? m_previewBadgeConfig.hudShowHints : m_badgeConfig.hudShowHints;
    if (!showHints) {
        return QRectF();
    }

    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();

    const QSizeF hintSz = ensureHudHintBarTexture(scale, screenRect);
    if (!m_hudHintBarTexture || hintSz.isEmpty()) {
        return QRectF();
    }

    const qreal hintW = hintSz.width();
    const qreal hintH = hintSz.height();

    const QSizeF headerSz = ensureHudHeaderTexture(scale);
    const qreal headerH = headerSz.height() > 0.0 ? headerSz.height() : 44.0;
    const qreal headerBottom = screenRect.top() + 12.0 + headerH;

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    qreal hintY = 0.0;
    if (hudPos == 2 || hudPos == 3) {
        // Left (2) and Right (3): Pills are on the side margins, hint bar is pinned below header at top center
        hintY = headerBottom + 12.0;
    } else if (hudPos == 1) { // Top HUD: place below pills
        if (pillsBounds.isValid() && !pillsBounds.isEmpty()) {
            hintY = pillsBounds.bottom() + 12.0;
            if (hintY + hintH > screenRect.bottom() - 10.0) {
                hintY = screenRect.bottom() - hintH - 10.0;
            }
        } else {
            hintY = headerBottom + 12.0;
        }
    } else { // Bottom (0): place directly above bottom header
        const qreal headerTop = screenRect.bottom() - headerH - 12.0;
        hintY = headerTop - hintH - 8.0;
        if (hintY < screenRect.top() + 10.0) {
            hintY = screenRect.top() + 10.0;
        }
    }

    const qreal hintX = screenRect.center().x() - (hintW / 2.0);
    return QRectF(hintX, hintY, hintW, hintH);
}

QRectF ActiveWindowOutlineEffect::renderHudHintBar(const RenderTarget &renderTarget,
                                                  const RenderViewport &viewport,
                                                  const QRectF &pillsBounds)
{
    const QRectF hintRect = computeHudHintBarRect(viewport, pillsBounds);
    if (!hintRect.isValid() || hintRect.isEmpty() || !m_hudHintBarTexture) {
        return QRectF();
    }

    const qreal scale = viewport.scale();
    m_hudHintBarScreenRect = hintRect;

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

QSizeF ActiveWindowOutlineEffect::ensureHudHeaderTexture(qreal scale) const
{
    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;

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
            if (isWindowInStash(w)) {
                stashCount++;
            }
        }

        auto formatWsName = [&](int wsIdx) -> QString {
            if (wsIdx == 0) {
                const QString icon = theme.hudNav.standardIcon;
                const QString txt = theme.hudNav.standardText.isEmpty() ? QStringLiteral("Standard") : theme.hudNav.standardText;
                return icon.isEmpty() ? txt : (icon + QLatin1Char(' ') + txt);
            } else if (wsIdx >= 1 && wsIdx <= m_workspaces.size()) {
                const auto &ws = m_workspaces.at(wsIdx - 1);
                const QString icon = theme.hudNav.workspaceIcon;
                return icon.isEmpty() ? ws.name : (icon + QLatin1Char(' ') + ws.name);
            } else {
                const QString icon = theme.hudNav.allIcon;
                const QString txt = theme.hudNav.allText.isEmpty() ? QStringLiteral("All") : theme.hudNav.allText;
                return icon.isEmpty() ? txt : (icon + QLatin1Char(' ') + txt);
            }
        };

        targetWorkspaceName = formatWsName(m_activeHudView);

        // 1. Standard count: windows eligible on current desktop (not in stash)
        int standardCount = 0;
        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }
            if (isWindowInStash(w)) {
                continue;
            }
            if (w->isOnCurrentDesktop()) {
                standardCount++;
            }
        }
        const bool standardAudio = isWorkspacePlayingAudio(0);
        viewTabs.append({
            theme.hudNav.standardText.isEmpty() ? QStringLiteral("Standard") : theme.hudNav.standardText,
            QColor(230, 126, 34),
            standardCount,
            m_activeHudView == 0,
            standardAudio,
            theme.hudNav.standardIcon
        });

        // 2. Custom Workspaces (not in stash)
        for (int i = 0; i < m_workspaces.size(); ++i) {
            const auto &ws = m_workspaces.at(i);
            int wsCount = 0;
            for (EffectWindow *w : windows) {
                if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                    continue;
                }
                if (isWindowInStash(w)) {
                    continue;
                }
                const auto desktops = w->desktops();
                const int dNum = (!desktops.isEmpty() && !w->isOnAllDesktops()) ? desktops.first()->x11DesktopNumber() : 0;
                if (LeapKWorkspace::matchesWorkspace(ws, w->windowClass(), w->caption(), dNum)) {
                    wsCount++;
                }
            }
            const bool wsAudio = isWorkspacePlayingAudio(i + 1);
            viewTabs.append({
                ws.name,
                ws.color.isValid() ? ws.color : QColor(52, 152, 219),
                wsCount,
                m_activeHudView == (i + 1),
                wsAudio,
                theme.hudNav.workspaceIcon
            });
        }

        // 3. All windows (excluding stashed)
        int allCount = 0;
        for (EffectWindow *w : windows) {
            if (!w || w->isDeleted() || !isEligibleOutlineWindow(w) || w->isSkipSwitcher() || !w->acceptsFocus() || isBlockingPopup(w)) {
                continue;
            }
            if (!isWindowInStash(w)) {
                allCount++;
            }
        }
        const bool allAudio = m_audioTracker.hasPlayingAudio();
        viewTabs.append({
            theme.hudNav.allText.isEmpty() ? QStringLiteral("All") : theme.hudNav.allText,
            QColor(155, 89, 182),
            allCount,
            m_activeHudView == (m_workspaces.size() + 1),
            allAudio,
            theme.hudNav.allIcon
        });
    }

    if (m_superJumpActive) {
        if (m_superJumpTargetWorkspace == 0) {
            const QString icon = theme.hudNav.standardIcon;
            const QString txt = theme.hudNav.standardText.isEmpty() ? QStringLiteral("Standard") : theme.hudNav.standardText;
            targetWorkspaceName = icon.isEmpty() ? txt : (icon + QLatin1Char(' ') + txt);
        } else if (m_superJumpTargetWorkspace >= 1 && m_superJumpTargetWorkspace <= m_workspaces.size()) {
            const auto &ws = m_workspaces.at(m_superJumpTargetWorkspace - 1);
            const QString icon = theme.hudNav.workspaceIcon;
            targetWorkspaceName = icon.isEmpty() ? ws.name : (icon + QLatin1Char(' ') + ws.name);
        } else {
            const QString icon = theme.hudNav.allIcon;
            const QString txt = theme.hudNav.allText.isEmpty() ? QStringLiteral("All") : theme.hudNav.allText;
            targetWorkspaceName = icon.isEmpty() ? txt : (icon + QLatin1Char(' ') + txt);
        }
    }

    int countsHash = 0;
    for (const auto &vt : viewTabs) {
        countsHash = (countsHash * 31 + vt.windowCount + (vt.isActive ? 1000 : 0) + (vt.isPlayingAudio ? 500 : 0)) % 100000;
    }
    const int hudSize = m_badgePreviewActive ? m_previewBadgeConfig.hudBadgeSize : m_badgeConfig.hudBadgeSize;
    const int navFontSize = m_badgePreviewActive ? m_previewBadgeConfig.hudNavFontSize : m_badgeConfig.hudNavFontSize;
    const int themeHash = qHash(theme.id) % 10000;
    const int headerKey = (m_subLeapActive ? 2000000 : 0) + (m_stashModeActive ? 4000000 : 0) + (m_superJumpActive ? 6000000 : 0)
        + (m_audioFilterModeActive ? 8000000 : 0)
        + (m_tilingPresetsModeActive ? 12000000 : 0)
        + (m_closeWindowModeActive ? 14000000 : 0)
        + (m_pinModeActive ? 16000000 : 0)
        + (m_searchModeActive ? (10000000 + m_searchScope * 100000 + (qHash(m_searchQuery) % 50000) + m_searchMatches.size() * 10) : 0)
        + m_activeHudView * 100000 + countsHash + stashCount * 13 + qRound(scale * 10) + hudSize * 7 + navFontSize * 17 + themeHash;

    if (!m_hudHeaderTexture || m_hudHeaderCacheKey != headerKey) {
        const QImage headerImg = OutlineBadge::renderHudModeHeader(
            scale, m_subLeapActive, subLeapTitle, subLeapColor, subLeapCount, viewTabs, stashCount, m_stashModeActive, targetWorkspaceName, m_superJumpActive, m_audioFilterModeActive, m_jumpLetterToWindow.size(),
            m_searchModeActive, m_searchQuery, m_searchScope, m_searchMatches.size(), hudSize, navFontSize, theme, &m_hudCloseButtonLocalRect,
            m_tilingPresetsModeActive, m_tilingPresets.size(), &m_hudTabLocalRects, m_closeWindowModeActive, m_pinModeActive);
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
        return QSizeF(0, 0);
    }

    return QSizeF(m_hudHeaderTexture->width() / scale, m_hudHeaderTexture->height() / scale);
}

QRectF ActiveWindowOutlineEffect::computeHudHeaderRect(const RenderViewport &viewport,
                                                      const QRectF &pillsBounds,
                                                      const QRectF &hintBarBounds) const
{
    Q_UNUSED(pillsBounds);
    Q_UNUSED(hintBarBounds);
    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();

    const QSizeF hSize = ensureHudHeaderTexture(scale);
    if (!m_hudHeaderTexture || hSize.isEmpty()) {
        return QRectF();
    }

    const qreal headerW = hSize.width();
    const qreal headerH = hSize.height();

    const int hudPos = m_badgePreviewActive ? m_previewBadgeHudPosition : m_badgeHudPosition;
    qreal headerY = 0.0;
    if (hudPos == 1 || hudPos == 2 || hudPos == 3) { // Top (1), Left (2), Right (3): Header pinned at top center
        headerY = screenRect.top() + 12.0;
    } else { // Bottom (0): Header pinned at bottom center
        headerY = screenRect.bottom() - headerH - 12.0;
        if (headerY < screenRect.top() + 10.0) {
            headerY = screenRect.top() + 10.0;
        }
    }

    const qreal headerX = screenRect.center().x() - (headerW / 2.0);
    return QRectF(headerX, headerY, headerW, headerH);
}

QRectF ActiveWindowOutlineEffect::renderHudModeHeader(const RenderTarget &renderTarget,
                                                     const RenderViewport &viewport,
                                                     const QRectF &pillsBounds,
                                                     const QRectF &hintBarBounds)
{
    const QRectF headerRect = computeHudHeaderRect(viewport, pillsBounds, hintBarBounds);
    if (!headerRect.isValid() || headerRect.isEmpty() || !m_hudHeaderTexture) {
        return QRectF();
    }

    const qreal scale = viewport.scale();

    m_hudHeaderScreenRect = headerRect;
    if (m_hudCloseButtonLocalRect.isValid() && !m_hudCloseButtonLocalRect.isEmpty()) {
        m_hudCloseButtonScreenRect = QRectF(
            headerRect.left() + m_hudCloseButtonLocalRect.left(),
            headerRect.top() + m_hudCloseButtonLocalRect.top(),
            m_hudCloseButtonLocalRect.width(),
            m_hudCloseButtonLocalRect.height()
        );
    } else {
        m_hudCloseButtonScreenRect = QRectF();
    }

    m_hudTabScreenRects.clear();
    for (const QRectF &localRect : m_hudTabLocalRects) {
        if (localRect.isValid() && !localRect.isEmpty()) {
            m_hudTabScreenRects.append(QRectF(
                headerRect.left() + localRect.left(),
                headerRect.top() + localRect.top(),
                localRect.width(),
                localRect.height()
            ));
        } else {
            m_hudTabScreenRects.append(QRectF());
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

    return headerRect;
}

void ActiveWindowOutlineEffect::renderKeyboardArrowsOverlay(const RenderTarget &renderTarget,
                                                            const RenderViewport &viewport)
{
    const qreal scale = viewport.scale();
    const QRectF screenRect = viewport.renderRect();

    QRectF hudBounds;
    for (const auto &pair : m_hudTargetPillRects) {
        if (pair.first.isValid()) {
            hudBounds = hudBounds.isEmpty() ? pair.first : hudBounds.united(pair.first);
        }
    }

    const auto &theme = m_badgePreviewActive ? m_previewBadgeConfig.activeTheme : m_badgeConfig.activeTheme;
    const auto &st = theme.spatialKeyboard;

    LeapKKeyboardOverlay::OverlayDisplayConfig cfg;
    cfg.enabled = m_keyboardOverlayEnabled;
    cfg.extendToHud = m_keyboardOverlayExtendToHud;
    cfg.keySize = m_keyboardOverlayKeySize;
    cfg.keyFontSize = m_keyboardOverlayKeyFontSize;
    cfg.lineWidth = m_keyboardOverlayLineWidth;
    cfg.badgeRadius = m_keyboardOverlayBadgeRadius;
    cfg.badgeFontSize = m_keyboardOverlayBadgeFontSize;
    cfg.targetLabelFontSize = m_keyboardOverlayTargetLabelFontSize;
    cfg.fontFamily = !m_keyboardOverlayFontFamily.isEmpty() ? m_keyboardOverlayFontFamily : st.fontFamily;
    cfg.targetBadgeShape = !st.targetBadgeShape.isEmpty() ? st.targetBadgeShape : QStringLiteral("circle");
    cfg.stackScaleReduction = (m_keyboardOverlayStackScaleReduction >= 0)
        ? (m_keyboardOverlayStackScaleReduction / 100.0)
        : st.stackScaleReduction;
    cfg.stackOpacityReduction = (m_keyboardOverlayStackOpacityReduction >= 0)
        ? (m_keyboardOverlayStackOpacityReduction / 100.0)
        : st.stackOpacityReduction;
    cfg.stackBadgeGap = st.stackBadgeGap > 0.0 ? st.stackBadgeGap : 8.0;
    cfg.targetLabelVisible = st.targetLabelVisible;
    cfg.laserGlow = st.laserGlow;
    cfg.laserGlowIntensity = st.laserGlowIntensity;
    cfg.laserDashStyle = st.laserDashStyle;
    cfg.targetLabelBg = theme.resolveColor(st.targetLabelBg, QColor(12, 15, 22, 255));
    cfg.targetLabelBg.setAlpha(255);
    cfg.targetLabelTextColor = theme.resolveColor(st.targetLabelTextColor, QColor(255, 255, 255));
    cfg.targetLabelTextColor.setAlpha(255);

    m_keyboardOverlayLayout = LeapKKeyboardOverlay::computeOverlayGeometry(screenRect, hudBounds, cfg);
    m_keyboardOverlayWindowMap.clear();

    const auto &palette = LeapKKeyboardOverlay::distinctArrowColors();
    int colorIdx = 0;

    const auto stacking = effects ? effects->stackingOrder() : QList<EffectWindow *>();
    EffectWindow *currActive = effects ? effects->activeWindow() : m_activeWindow;
    QMap<QChar, LeapKKeyboardOverlay::WindowTargetItem> assignedMap;
    QSet<EffectWindow *> seenWindows;

    for (auto it = m_jumpLetterToWindow.constBegin(); it != m_jumpLetterToWindow.constEnd(); ++it) {
        const QChar key = it.key().toUpper();
        EffectWindow *w = it.value();
        if (!w || w->isDeleted() || !isEligibleTargetWindow(w) || seenWindows.contains(w)) {
            continue;
        }
        seenWindows.insert(w);
        m_keyboardOverlayWindowMap.insert(key, w);

        LeapKKeyboardOverlay::WindowTargetItem item;
        item.windowId = w;
        item.geometry = w->frameGeometry();
        item.preferredKey = key;
        item.title = w->caption();
        item.appClass = w->windowClass().split(QLatin1Char(' ')).first().toLower();
        item.customLabel = m_markedWindowLabels.value(w);
        item.isVisibleOnScreen = w->isOnCurrentDesktop() && !w->isMinimized() && item.geometry.intersects(screenRect);
        item.isCurrentFocused = (w == currActive);
        item.zOrder = stacking.indexOf(w);
        item.isOccluded = isWindowObscured(w);

        for (const auto &pair : m_hudTargetPillRects) {
            if (pair.second == w && pair.first.isValid()) {
                item.hudCardRect = pair.first;
                break;
            }
        }

        if (m_markedWindows.contains(w) && m_markedWindows.value(w).isValid()) {
            item.customColor = m_markedWindows.value(w);
        } else {
            item.customColor = palette[colorIdx % palette.size()];
            colorIdx++;
        }
        assignedMap.insert(key, item);
    }

    const QChar invalidKey = (m_invalidJumpKeyTimer.isValid() && m_invalidJumpKeyTimer.elapsed() < 900)
        ? m_invalidJumpKey
        : QChar();
    const QImage img = LeapKKeyboardOverlay::renderOverlayImage(m_keyboardOverlayLayout, assignedMap, screenRect, scale, cfg, invalidKey);

    auto uploaded = GLTexture::upload(img);
    if (!uploaded) {
        return;
    }
    uploaded->setFilter(GL_LINEAR);
    uploaded->setWrapMode(GL_CLAMP_TO_EDGE);

    const float x0 = screenRect.left() * scale;
    const float y0 = screenRect.top() * scale;
    const float x1 = screenRect.right() * scale;
    const float y1 = screenRect.bottom() * scale;

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

    ShaderBinder binder(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::TransformColorspace);
    GLShader *shader = binder.shader();
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, viewport.projectionMatrix());
    shader->setUniform(GLShader::Mat4Uniform::TextureMatrix, uploaded->matrix(NormalizedCoordinates));
    shader->setColorspaceUniforms(ColorDescription::sRGB, renderTarget.colorDescription(), RenderingIntent::Perceptual);
    shader->setUniform(GLShader::Vec4Uniform::ModulationConstant, QVector4D(1.0f, 1.0f, 1.0f, 1.0f));

    uploaded->bind();
    vbo->render(GL_TRIANGLES);
    uploaded->unbind();

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

} // namespace KWin
