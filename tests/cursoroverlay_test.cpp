// SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
// SPDX-License-Identifier: GPL-3.0-or-later
#include "cursoroverlay.h"
#include <QCoreApplication>
#include <cstdio>

using namespace KWin;

// Exercise the real KWin scene filters without starting a compositor or GPU.
class TestScene final : public Scene
{
public:
    TestScene()
        : Scene(nullptr)
    {
    }

#ifdef ACTIVE_WINDOW_OUTLINE_PREPAINT_PRESENT_TIME
    OverlayCandidates overlayCandidates(ssize_t, ssize_t, ssize_t) const override { return {}; }
    void prePaint(SceneView *) override {}
#else
    QList<Item *> layerCandidates(ssize_t) const override { return {}; }
    void prePaint(SceneView *, OutputFrame *) override {}
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
    void attachRenderer(RenderDevice *, std::unique_ptr<ItemRenderer> &&) override {}
    void detachRenderer(RenderDevice *) override {}
#else
    void attachRenderer(std::unique_ptr<ItemRenderer> &&) override {}
    void detachRenderer() override {}
#endif
#endif
    Region collectDamage() override { return {}; }
    void paint(const RenderTarget &, const QPoint &, const Region &) override {}
    void postPaint() override {}
    void frame(SceneView *, OutputFrame *) override {}
    double desiredHdrHeadroom() const override { return 1; }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    int checks = 0;
    int failures = 0;
    const auto check = [&](bool ok, const char *name) {
        ++checks;
        if (!ok) {
            ++failures;
            std::fprintf(stderr, "FAIL: %s\n", name);
        }
    };
    TestScene scene;
#ifdef ACTIVE_WINDOW_OUTLINE_BOOL_PAINT_SCREEN
    SceneView view(&scene, nullptr, nullptr, nullptr, nullptr);
    SceneView otherOutput(&scene, nullptr, nullptr, nullptr, nullptr);
#else
    SceneView view(&scene, nullptr, nullptr, nullptr);
    SceneView otherOutput(&scene, nullptr, nullptr, nullptr);
#endif
    Item overlay;
    Item cursor(&overlay);
    Item cursorImage(&cursor);
    Item dragIcon(&overlay);
    check(view.shouldRenderItem(&cursor), "software cursor initially belongs to the scene");
    {
        OutlineCursorOverlay deferred(&view, &cursor);
        check(!view.shouldRenderItem(&cursor), "cursor omitted from the base pass");
        check(!view.shouldRenderItem(&cursorImage), "cursor descendants omitted");
        check(view.shouldRenderItem(&overlay), "overlay parent preserved");
        check(view.shouldRenderItem(&dragIcon), "drag icon preserved");
        check(otherOutput.shouldRenderItem(&cursor), "other output untouched");
        check(scene.views().size() == 2, "no extra registered scene view");
        deferred.restore();
        check(view.shouldRenderItem(&cursor), "cursor available for final pass");
        deferred.restore();
        check(view.shouldRenderItem(&cursor), "restore is idempotent");
    }
    const auto interruptedPass = [&] {
        OutlineCursorOverlay deferred(&view, &cursor);
        return;
    };
    interruptedPass();
    check(view.shouldRenderItem(&cursor), "early return restores the scene");
    {
        OutlineCursorOverlay hardwarePlane(&view, &cursor);
        {
            OutlineCursorOverlay deferred(&view, &cursor);
            deferred.restore();
            check(!view.shouldRenderItem(&cursor), "existing hardware exclusion preserved");
        }
        check(!view.shouldRenderItem(&cursor), "destructor preserves hardware exclusion");
    }
    check(view.shouldRenderItem(&cursor), "all temporary exclusions removed");
    {
        auto temporaryCursor = std::make_unique<Item>();
        OutlineCursorOverlay deferred(&view, temporaryCursor.get());
        temporaryCursor.reset();
        check(view.shouldRenderItem(&dragIcon), "deleted cursor does not leave a dangling filter");
    }
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
