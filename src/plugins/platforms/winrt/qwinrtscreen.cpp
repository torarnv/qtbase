/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qt_windows.h>
#include "qwinrtscreen.h"
#include "qwinrtbackingstore.h"
#include "qwinrtkeymapper.h"
#include "qwinrtpageflipper.h"
#include "qwinrtcursor.h"
#include "pointervalue.h"
#include <qpa/qwindowsysteminterface.h>

#include <QtGui/QGuiApplication>

#include <wrl.h>
#include <windows.system.h>
#include <windows.devices.input.h>
#include <windows.ui.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>

using namespace Microsoft::WRL;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::UI::Input;
using namespace ABI::Windows::Devices::Input;

typedef ITypedEventHandler<CoreWindow*, WindowActivatedEventArgs*> ActivatedHandler;
typedef ITypedEventHandler<CoreWindow*, CoreWindowEventArgs*> ClosedHandler;
typedef ITypedEventHandler<CoreWindow*, CharacterReceivedEventArgs*> CharacterReceivedHandler;
typedef ITypedEventHandler<CoreWindow*, InputEnabledEventArgs*> InputEnabledHandler;
typedef ITypedEventHandler<CoreWindow*, KeyEventArgs*> KeyHandler;
typedef ITypedEventHandler<CoreWindow*, PointerEventArgs*> PointerHandler;
typedef ITypedEventHandler<CoreWindow*, WindowSizeChangedEventArgs*> SizeChangedHandler;
typedef ITypedEventHandler<CoreWindow*, VisibilityChangedEventArgs*> VisibilityChangedHandler;

QWinRTScreen::QWinRTScreen(ICoreWindow *window)
    : m_window(window)
    , m_depth(32)
    , m_format(QImage::Format_ARGB32_Premultiplied)
    , m_keyMapper(new QWinRTKeyMapper())
    , m_pageFlipper(new QWinRTPageFlipper(window))
    , m_cursor(new QWinRTCursor(window))
{
    // TODO: query touch device capabilities
    m_touchDevice.setCapabilities(QTouchDevice::Position | QTouchDevice::Area);
    m_touchDevice.setType(QTouchDevice::TouchScreen);
    m_touchDevice.setName("WinRTTouchDevice1");
    QWindowSystemInterface::registerTouchDevice(&m_touchDevice);

    Rect rect;
    window->get_Bounds(&rect);
    m_geometry = QRect(0, 0, rect.Width, rect.Height);
    m_pageFlipper->resize(m_geometry.size());
    QWindowSystemInterface::handleScreenGeometryChange(screen(), m_geometry);
    QWindowSystemInterface::handleScreenAvailableGeometryChange(screen(), m_geometry);

    // Event handlers mapped to QEvents
    m_window->add_KeyDown(Callback<KeyHandler>(this, &QWinRTScreen::onKeyDown).Get(), &m_tokens[QEvent::KeyPress]);
    m_window->add_KeyUp(Callback<KeyHandler>(this, &QWinRTScreen::onKeyUp).Get(), &m_tokens[QEvent::KeyRelease]);
    m_window->add_PointerEntered(Callback<PointerHandler>(this, &QWinRTScreen::onPointerEntered).Get(), &m_tokens[QEvent::Enter]);
    m_window->add_PointerExited(Callback<PointerHandler>(this, &QWinRTScreen::onPointerExited).Get(), &m_tokens[QEvent::Leave]);
    m_window->add_PointerMoved(Callback<PointerHandler>(this, &QWinRTScreen::onPointerMoved).Get(), &m_tokens[QEvent::MouseMove]);
    m_window->add_PointerPressed(Callback<PointerHandler>(this, &QWinRTScreen::onPointerPressed).Get(), &m_tokens[QEvent::MouseButtonPress]);
    m_window->add_PointerReleased(Callback<PointerHandler>(this, &QWinRTScreen::onPointerReleased).Get(), &m_tokens[QEvent::MouseButtonRelease]);
    m_window->add_PointerWheelChanged(Callback<PointerHandler>(this, &QWinRTScreen::onPointerWheelChanged).Get(), &m_tokens[QEvent::Wheel]);
    m_window->add_SizeChanged(Callback<SizeChangedHandler>(this, &QWinRTScreen::onSizeChanged).Get(), &m_tokens[QEvent::Resize]);

    // Window event handlers
    m_window->add_Activated(Callback<ActivatedHandler>(this, &QWinRTScreen::onActivated).Get(), &m_tokens[QEvent::WindowActivate]);
    m_window->add_Closed(Callback<ClosedHandler>(this, &QWinRTScreen::onClosed).Get(), &m_tokens[QEvent::WindowDeactivate]);
    m_window->add_VisibilityChanged(Callback<VisibilityChangedHandler>(this, &QWinRTScreen::onVisibilityChanged).Get(), &m_tokens[QEvent::Show]);
}

QRect QWinRTScreen::geometry() const
{
    return m_geometry;
}

int QWinRTScreen::depth() const
{
    return m_depth;
}

QImage::Format QWinRTScreen::format() const
{
    return m_format;
}

QPlatformScreenPageFlipper *QWinRTScreen::pageFlipper() const
{
    return m_pageFlipper;
}

QPlatformCursor *QWinRTScreen::cursor() const
{
    return m_cursor;
}

void QWinRTScreen::update(const QRegion &region, const QPoint &offset, const void *handle, int stride)
{
    m_pageFlipper->update(region, offset, handle, stride);
}

HRESULT QWinRTScreen::handleKeyEvent(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IKeyEventArgs *args)
{
    Q_UNUSED(window);

    return m_keyMapper->translateKeyEvent(0, args) ? S_OK : S_FALSE;
}

HRESULT QWinRTScreen::handlePointerEvent(Qt::TouchPointState pointState, ICoreWindow *window, IPointerEventArgs *args)
{
    Q_UNUSED(window);

    PointerValue point(args);

    QPointF pos = point.pos();

    if (point.isMouse()) {
        QWindowSystemInterface::handleMouseEvent(0, pos, pos, point.buttons(), point.modifiers());
    } else {
        QWindowSystemInterface::TouchPoint tp = point.touchPoint();
        tp.state = pointState;
        // ###FIXME: points should be in a running map so that multitouch can be used
        QList<QWindowSystemInterface::TouchPoint> points;
        points.append(tp);
        QWindowSystemInterface::handleTouchEvent(0, &m_touchDevice, points, point.modifiers());
    }
    return S_OK;
}

HRESULT QWinRTScreen::onKeyDown(ICoreWindow *window, IKeyEventArgs *args)
{
    return handleKeyEvent(window, args);
}

HRESULT QWinRTScreen::onKeyUp(ICoreWindow *window, IKeyEventArgs *args)
{
    return handleKeyEvent(window, args);
}

HRESULT QWinRTScreen::onPointerEntered(ICoreWindow *window, IPointerEventArgs *args)
{
    Q_UNUSED(window);
    Q_UNUSED(args);
    QWindowSystemInterface::handleEnterEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onPointerExited(ICoreWindow *window, IPointerEventArgs *args)
{
    Q_UNUSED(window);
    Q_UNUSED(args);
    QWindowSystemInterface::handleLeaveEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onPointerMoved(ICoreWindow *window, IPointerEventArgs *args)
{
    return handlePointerEvent(Qt::TouchPointMoved, window, args);
}

HRESULT QWinRTScreen::onPointerPressed(ICoreWindow *window, IPointerEventArgs *args)
{
    return handlePointerEvent(Qt::TouchPointPressed, window, args);
}

HRESULT QWinRTScreen::onPointerReleased(ICoreWindow *window, IPointerEventArgs *args)
{
    return handlePointerEvent(Qt::TouchPointReleased, window, args);
}

HRESULT QWinRTScreen::onPointerWheelChanged(ICoreWindow *window, IPointerEventArgs *args)
{
    Q_UNUSED(window);

    PointerValue point(args);
    QPointF pos = point.pos();
    QWindowSystemInterface::handleWheelEvent(0, pos, pos, point.delta(), point.orientation(), point.modifiers());
    return S_OK;
}

HRESULT QWinRTScreen::onSizeChanged(ICoreWindow *window, IWindowSizeChangedEventArgs *args)
{
    Q_UNUSED(window);

    Size size;
    if (FAILED(args->get_Size(&size))) {
        qWarning(Q_FUNC_INFO ": failed to get size");
        return S_OK;
    }

    m_geometry.setSize(QSize(size.Width, size.Height));
    m_pageFlipper->resize(m_geometry.size());

    resizeMaximizedWindows();
    QWindowSystemInterface::handleScreenGeometryChange(screen(), m_geometry);
    QWindowSystemInterface::handleScreenAvailableGeometryChange(screen(), m_geometry);

    foreach (QWindow *w, qGuiApp->topLevelWindows())
        QWindowSystemInterface::handleExposeEvent(w, QRect(QPoint(), w->size()));
    return S_OK;
}

HRESULT QWinRTScreen::onActivated(ICoreWindow *window, IWindowActivatedEventArgs *args)
{
    Q_UNUSED(window);
    Q_UNUSED(args);

    foreach (QWindow *w, qGuiApp->topLevelWindows())
        QWindowSystemInterface::handleWindowActivated(w);
    return S_OK;
}

HRESULT QWinRTScreen::onClosed(ICoreWindow *window, ICoreWindowEventArgs *args)
{
    Q_UNUSED(window);
    Q_UNUSED(args);

    QWindowSystemInterface::handleCloseEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onVisibilityChanged(ICoreWindow *window, IVisibilityChangedEventArgs *args)
{
    Q_UNUSED(window);
    Q_UNUSED(args);

    boolean visible = FALSE;
    if (FAILED(args->get_Visible(&visible))) {
        qWarning(Q_FUNC_INFO ": failed to get visible state");
        return S_OK;
    }

    foreach (QWindow *w, qGuiApp->topLevelWindows())
        QWindowSystemInterface::handleWindowStateChanged(w, visible ? Qt::WindowFullScreen : Qt::WindowMinimized);
    return S_OK;
}

