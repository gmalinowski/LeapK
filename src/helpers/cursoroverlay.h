/*
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <effect/effect.h>
#include <scene/item.h>
#include <scene/itemrenderer.h>
#include <scene/scene.h>

namespace KWin
{

// A paint-pass-only exclusion, not a registered scene view or output layer.
// The scene normally paints the software cursor before effects unwind. Defer
// that subtree until after the outline, using KWin's renderer exactly once.
// Existing exclusions (notably hardware cursor planes) are left untouched.
class OutlineCursorOverlay final : public RenderView
{
public:
    OutlineCursorOverlay(SceneView *view, Item *cursor)
        : RenderView(view->logicalOutput(), view->backendOutput(), view->layer()
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
                     , view->renderDevice()
#endif
                     )
        , m_view(view)
        , m_cursor(cursor)
    {
        m_view->addExclusiveView(this);
    }

    ~OutlineCursorOverlay() override
    {
        restore();
    }

    void restore()
    {
        if (m_excluded && m_view) {
            m_view->removeExclusiveView(this);
        }
        m_excluded = false;
    }

    bool render(const RenderTarget &target, const RenderViewport &viewport,
                const Region &deviceRegion)
    {
        restore();
        if (!m_view || !m_cursor || !m_cursor->isVisible()
            || !m_view->shouldRenderItem(m_cursor)) {
            return true;
        }
        auto filter = [this](Item *item) { return !m_view->shouldRenderItem(item); };
        auto holeFilter = [this](Item *item) { return m_view->shouldRenderHole(item); };
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
        auto renderer = m_view->scene()->renderer(m_view->renderDevice());
        return renderer->renderItem(target, viewport, m_cursor, Scene::PAINT_SCREEN_TRANSFORMED,
                                    deviceRegion, WindowPaintData{}, filter, holeFilter);
#else
        m_view->scene()->renderer()->renderItem(target, viewport, m_cursor, Scene::PAINT_SCREEN_TRANSFORMED,
                                               deviceRegion, WindowPaintData{}, filter, holeFilter);
        return true;
#endif
    }

    bool shouldRenderItem(Item *item) const override
    {
        return m_cursor && (item == m_cursor || m_cursor->isAncestorOf(item));
    }

    RectF viewport() const override { return m_view->viewport(); }
    qreal scale() const override { return m_view->scale(); }
    double desiredHdrHeadroom() const override { return m_view->desiredHdrHeadroom(); }
    Region collectDamage() override { return {}; }
    void paint(const RenderTarget &, const QPoint &, const Region &) override {}
#ifdef ACTIVE_WINDOW_OUTLINE_PREPAINT_PRESENT_TIME
    QList<SurfaceItem *> scanoutCandidates(ssize_t) const override { return {}; }
    void prePaint() override {}
    void postPaint() override {}
#else
    SurfaceItem *scanoutCandidate() const override { return nullptr; }
    std::chrono::nanoseconds nextPresentationTimestamp() const override
    {
        return m_view->nextPresentationTimestamp();
    }
    uint refreshRate() const override { return m_view->refreshRate(); }
#endif

private:
    QPointer<SceneView> m_view;
    QPointer<Item> m_cursor;
    bool m_excluded = true;
};

} // namespace KWin
