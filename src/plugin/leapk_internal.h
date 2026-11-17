/*
    SPDX-FileCopyrightText: Matthias Bauer
    SPDX-FileCopyrightText: 2026 Grzegorz Malinowski
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include "leapk.h"
#include "popupfilter.h"
#include "precedencehelper.h"
#include "kwintilehelper.h"
#include "cursoroverlay.h"
#include "presethelper.h"
#include "soundhelper.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QCursor>
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
#include <QDBusMessage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <window.h>
#include <workspace.h>
#include <rules.h>
#include <core/output.h>
#include <virtualdesktops.h>
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
#ifndef ACTIVE_WINDOW_OUTLINE_X11
#include <cursor.h>
#endif
#include <scene/workspacescene.h>
#include <scene/windowitem.h>
#ifdef ACTIVE_WINDOW_OUTLINE_HAS_OPENGL_SURFACE_TEXTURE
#include <scene/surfaceitem.h>
#include <core/graphicsbuffer.h>
#endif
#endif

#include <KConfigGroup>
#include <KSharedConfig>
#include <cmath>
#include <utility>

namespace LeapKCompat
{

template<typename Handler, typename Option, typename Screen, typename Desktop>
inline QRectF clientArea(Handler *handler, Option opt, Screen *screen, Desktop *desktop)
{
    if (!handler) {
        return QRectF();
    }
    if constexpr (requires { handler->clientArea(opt, screen, desktop); }) {
        auto r = handler->clientArea(opt, screen, desktop);
        return QRectF(r.x(), r.y(), r.width(), r.height());
    } else if constexpr (requires { handler->clientArea(opt, screen); }) {
        auto r = handler->clientArea(opt, screen);
        return QRectF(r.x(), r.y(), r.width(), r.height());
    } else {
        return QRectF();
    }
}

template<typename Handler>
inline QRectF virtualScreenGeometry(Handler *handler)
{
    if (!handler) {
        return QRectF();
    }
    auto g = handler->virtualScreenGeometry();
    return QRectF(g.x(), g.y(), g.width(), g.height());
}

} // namespace LeapKCompat
