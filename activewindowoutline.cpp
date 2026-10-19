/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "activewindowoutline.h"
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/glvertexbuffer.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#ifndef ACTIVE_WINDOW_OUTLINE_LEGACY_PAINT_TYPES
#include <core/rect.h>
#endif
#include <KConfigGroup>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QPalette>
#include <QStandardPaths>
#include <QTimer>
#include <QtMath>
#include <utility>

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(ActiveWindowOutlineEffect, "metadata.json",
                              return ActiveWindowOutlineEffect::supported();)

ActiveWindowOutlineEffect::ActiveWindowOutlineEffect()
{
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
    }

    connect(effects, &EffectsHandler::windowActivated,
            this, &ActiveWindowOutlineEffect::slotWindowActivated);
    connect(effects, &EffectsHandler::windowAdded,
            this, &ActiveWindowOutlineEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted,
            this, &ActiveWindowOutlineEffect::slotWindowDeleted);
    connect(effects, &EffectsHandler::showingDesktopChanged,
            this, &ActiveWindowOutlineEffect::slotShowingDesktopChanged);
}

bool ActiveWindowOutlineEffect::supported()
{
    return effects->isOpenGLCompositing();
}

void ActiveWindowOutlineEffect::reconfigure(ReconfigureFlags)
{
    KConfigGroup config = effects->config()->group(QStringLiteral("Effect-active-window-outline"));
    m_useAccentColor = config.readEntry("UseAccentColor", true);
    m_customColor = config.readEntry("CustomColor", QColor(61, 174, 233));
    m_width = qBound(1, config.readEntry("BorderWidth", 4), 20);
    m_gap = qBound(0, config.readEntry("OutlineGap", 2), 30);
    m_radius = qBound(0, config.readEntry("BorderRadius", 10), 30);
    m_drawInside = config.readEntry("DrawInside", true);

    m_outlineCache.clear();
    effects->addRepaintFull();
}

QColor ActiveWindowOutlineEffect::resolveColor() const
{
    if (m_useAccentColor) {
        return qApp->palette().color(QPalette::Highlight);
    }
    return m_customColor;
}

void ActiveWindowOutlineEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::windowFrameGeometryChanged,
            this, &ActiveWindowOutlineEffect::slotWindowFrameGeometryChanged);
    connect(w, &EffectWindow::minimizedChanged,
            this, &ActiveWindowOutlineEffect::slotWindowMinimizedChanged);
    connect(w, &EffectWindow::windowHiddenChanged,
            this, &ActiveWindowOutlineEffect::slotWindowHiddenChanged);

    if (isBlockingPopup(w)) {
        repaintAllOutlines();
    }
}

bool ActiveWindowOutlineEffect::shouldOutlineWindow(EffectWindow *w) const
{
    return w && w == m_activeWindow && !w->isDesktop() && !w->isDock()
        && !isBlockingPopup(w);
}

bool ActiveWindowOutlineEffect::isBlockingPopup(EffectWindow *w) const
{
    return w && (w->isPopupWindow() || w->isMenu() || w->isDropdownMenu()
        || w->isPopupMenu() || w->isTooltip() || w->isComboBox()
        || w->isAppletPopup());
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

void ActiveWindowOutlineEffect::renderOutline(const RenderTarget &renderTarget,
                                           const RenderViewport &viewport,
                                           const OutlineCache &cache,
                                           const OutlineRegion &clipRegion)
{
    if (cache.borderVerts.isEmpty()) {
        return;
    }

    const qreal scale = viewport.scale();
    auto vbo = GLVertexBuffer::streamingBuffer();

    QList<QVector2D> scaledVerts;
    for (const auto &v : cache.borderVerts) {
        scaledVerts.append(QVector2D(v.x() * scale, v.y() * scale));
    }

    auto mapped = vbo->map<QVector2D>(scaledVerts.size());
    if (!mapped) {
        return;
    }
    memcpy(mapped->data(), scaledVerts.data(), scaledVerts.size() * sizeof(QVector2D));
    vbo->unmap();
    vbo->setVertexCount(scaledVerts.size());

    static constexpr GLVertexAttrib layout{
        .attributeIndex = VA_Position,
        .componentCount = 2,
        .type = GL_FLOAT,
        .relativeOffset = 0,
    };
    vbo->setAttribLayout(std::span(&layout, 1), sizeof(QVector2D));

    ShaderBinder binder(ShaderTrait::UniformColor | ShaderTrait::TransformColorspace);
    binder.shader()->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix,
                                viewport.projectionMatrix());
    binder.shader()->setColorspaceUniforms(ColorDescription::sRGB,
                                           renderTarget.colorDescription(),
                                           RenderingIntent::Perceptual);
    binder.shader()->setUniform(GLShader::ColorUniform::Color, cache.borderColor);

    // The custom color may carry an alpha component, so blend the outline over
    // whatever is already on screen. The color is passed straight (not
    // premultiplied), which is why we use SRC_ALPHA here. Opaque colours don't
    // need blending, so skip the state change for them.
    const bool translucent = cache.borderColor.alpha() < 255;
    if (translucent) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    vbo->render(clipRegion, GL_TRIANGLES, true);

    if (translucent) {
        glDisable(GL_BLEND);
    }
}

void ActiveWindowOutlineEffect::cacheWindowOutline(EffectWindow *w, const QRectF &geo)
{
    OutlineCache cache;
    cache.geometry = geo;

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

    generateRoundedRectStrip(outerRect, outerRadius, innerRect, innerRadius, 1.0, cache.borderVerts);
    cache.borderColor = resolveColor();

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
    const qreal extent = m_drawInside ? 1.0 : m_width + m_gap + 1.0;
    // Repaint the old location to erase the previous outline, and the new one
    // to draw it. Without setTransformed there is nothing else extending the
    // window's damage out to the border, so we schedule it explicitly.
    effects->addRepaint(oldGeometry.adjusted(-extent, -extent, extent, extent));
    effects->addRepaint(expandedGeometryFor(w));
    if (isBlockingPopup(w)) {
        repaintAllOutlines();
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
    const qreal extent = m_width + m_gap + 1.0;
    return m_drawInside
        ? w->frameGeometry().adjusted(-1, -1, 1, 1)
        : w->frameGeometry().adjusted(-extent, -extent, extent, extent);
}

bool ActiveWindowOutlineEffect::isActive() const
{
    return shouldOutlineWindow(m_activeWindow);
}

QString ActiveWindowOutlineEffect::debug(const QString &) const
{
    return QStringLiteral("width=%1 offset=%2 radius=%3 drawInside=%4")
        .arg(m_width)
        .arg(m_gap)
        .arg(m_radius)
        .arg(m_drawInside ? QStringLiteral("true") : QStringLiteral("false"));
}

void ActiveWindowOutlineEffect::prepareScreenPaint(ScreenPrePaintData &data)
{
    EffectWindow *w = m_activeWindow;
    if (shouldOutlineWindow(w)) {
        if (w->isMinimized() || w->isHiddenByShowDesktop()) {
            // The outline sits in a band just *outside* the window's rectangle.
            // Hiding a window only damages the window rectangle itself, not
            // that surrounding band, so the outline would be left behind as a
            // ghost. Repaint the band once in the same pass so it is cleared.
            if (!m_suppressedWindows.contains(w)) {
                m_suppressedWindows.insert(w);
                data.paint += expandedGeometryFor(w).toRect();
            }
            return;
        }

        m_suppressedWindows.remove(w);
        // Make sure the border area is part of the region being painted this
        // frame, otherwise our outline would be scissored away.
        if (w->isOnCurrentDesktop()) {
            data.paint += expandedGeometryFor(w).toRect();
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

void ActiveWindowOutlineEffect::paintScreen(const RenderTarget &renderTarget,
                                         const RenderViewport &viewport,
                                         int mask,
                                         const OutlineRegion &deviceRegion,
                                         OutlineOutput *screen)
{
    effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);

    if (!shouldOutlineWindow(m_activeWindow) || hasVisibleBlockingPopup()) {
        return;
    }

    EffectWindow *w = m_activeWindow;
    if (w->isMinimized() || w->isHiddenByShowDesktop() || !w->isOnCurrentDesktop()) {
        return;
    }

    const QRectF currentGeo = w->frameGeometry();
    const QColor currentColor = resolveColor();
    if (!m_outlineCache.contains(w) || m_lastGeometry.value(w) != currentGeo
        || m_outlineCache.value(w).borderColor != currentColor) {
        cacheWindowOutline(w, currentGeo);
        m_lastGeometry[w] = currentGeo;
    }

    renderOutline(renderTarget, viewport, m_outlineCache[w], deviceRegion);
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
