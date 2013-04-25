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
#include <qpa/qwindowsysteminterface.h>
#include "qwinrtscreen.h"
#include "qwinrtbackingstore.h"
#include "qwinrtkeymapper.h"
#include "qwinrtinputcontext.h"
#include "qwinrtcursor.h"
#ifdef Q_WINRT_GL
#  include <QtPlatformSupport/private/qeglconvenience_p.h>
#  include "qwinrteglcontext.h"
#else
#  include "qwinrtpageflipper.h"
#endif

#include <QGuiApplication>
#include <QSurfaceFormat>

#include <wrl.h>
#include <windows.system.h>
#include <windows.devices.input.h>
#include <windows.ui.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>
#include <windows.graphics.display.h>
#include <windows.foundation.h>

using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::System;
using namespace ABI::Windows::UI::Core;
using namespace ABI::Windows::UI::Input;
using namespace ABI::Windows::Devices::Input;
using namespace ABI::Windows::Graphics::Display;

typedef ITypedEventHandler<CoreWindow*, WindowActivatedEventArgs*> ActivatedHandler;
typedef ITypedEventHandler<CoreWindow*, CoreWindowEventArgs*> ClosedHandler;
typedef ITypedEventHandler<CoreWindow*, CharacterReceivedEventArgs*> CharacterReceivedHandler;
typedef ITypedEventHandler<CoreWindow*, InputEnabledEventArgs*> InputEnabledHandler;
typedef ITypedEventHandler<CoreWindow*, KeyEventArgs*> KeyHandler;
typedef ITypedEventHandler<CoreWindow*, PointerEventArgs*> PointerHandler;
typedef ITypedEventHandler<CoreWindow*, WindowSizeChangedEventArgs*> SizeChangedHandler;
typedef ITypedEventHandler<CoreWindow*, VisibilityChangedEventArgs*> VisibilityChangedHandler;
typedef ITypedEventHandler<CoreWindow*, AutomationProviderRequestedEventArgs*> AutomationProviderRequestedHandler;

static inline Qt::ScreenOrientation qOrientationFromNative(DisplayOrientations orientation)
{
    switch (orientation) {
    default:
    case DisplayOrientations_None:
        return Qt::PrimaryOrientation;
    case DisplayOrientations_Landscape:
        return Qt::LandscapeOrientation;
    case DisplayOrientations_LandscapeFlipped:
        return Qt::InvertedLandscapeOrientation;
    case DisplayOrientations_Portrait:
        return Qt::PortraitOrientation;
    case DisplayOrientations_PortraitFlipped:
        return Qt::InvertedPortraitOrientation;
    }
}

QWinRTScreen::QWinRTScreen(ICoreWindow *window)
    : m_window(window)
    , m_depth(32)
    , m_format(QImage::Format_ARGB32_Premultiplied)
    , m_keyMapper(new QWinRTKeyMapper())
#ifdef Q_OS_WINPHONE
    , m_inputContext(new QWinRTInputContext(m_window))
#else
    , m_inputContext(Make<QWinRTInputContext>(m_window).Detach())
#endif
    , m_cursor(new QWinRTCursor(window))
    , m_orientation(Qt::PrimaryOrientation)
#ifndef Q_WINRT_GL
    , m_pageFlipper(new QWinRTPageFlipper(window))
#endif
{
#ifdef Q_OS_WINPHONE // On phone, there can be only one touch device
    QTouchDevice *touchDevice = new QTouchDevice;
    touchDevice->setCapabilities(QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure);
    touchDevice->setType(QTouchDevice::TouchScreen);
    touchDevice->setName("WinPhoneTouchScreen");
    Pointer pointer = { Pointer::TouchScreen, touchDevice };
    m_pointers.insert(0, pointer);
    QWindowSystemInterface::registerTouchDevice(touchDevice);
#endif

    Rect rect;
    window->get_Bounds(&rect);
    m_geometry = QRect(0, 0, rect.Width, rect.Height);

    m_surfaceFormat.setAlphaBufferSize(0);
    m_surfaceFormat.setRedBufferSize(8);
    m_surfaceFormat.setGreenBufferSize(8);
    m_surfaceFormat.setBlueBufferSize(8);

#ifdef Q_WINRT_GL
    m_surfaceFormat.setRenderableType(QSurfaceFormat::OpenGLES);
    m_surfaceFormat.setSamples(1);
    m_surfaceFormat.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    m_surfaceFormat.setDepthBufferSize(24);
    m_surfaceFormat.setStencilBufferSize(8);

    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (m_eglDisplay == EGL_NO_DISPLAY)
        qFatal("Qt WinRT platform plugin: failed to initialize EGL display.");

    if (!eglInitialize(m_eglDisplay, NULL, NULL))
        qFatal("Qt WinRT platform plugin: failed to initialize EGL. This can happen if you haven't included the D3D compiler DLL in your application package.");

    m_eglSurface = eglCreateWindowSurface(m_eglDisplay, q_configFromGLFormat(m_eglDisplay, m_surfaceFormat), window, NULL);
    if (m_eglSurface == EGL_NO_SURFACE)
        qFatal("Could not create EGL surface, error 0x%X", eglGetError());
#else
    m_pageFlipper->resize(m_geometry.size());
#endif // Q_WINRT_GL

    // Event handlers mapped to QEvents
    m_window->add_KeyDown(Callback<KeyHandler>(this, &QWinRTScreen::onKeyDown).Get(), &m_tokens[QEvent::KeyPress]);
    m_window->add_KeyUp(Callback<KeyHandler>(this, &QWinRTScreen::onKeyUp).Get(), &m_tokens[QEvent::KeyRelease]);
    m_window->add_PointerEntered(Callback<PointerHandler>(this, &QWinRTScreen::onPointerEntered).Get(), &m_tokens[QEvent::Enter]);
    m_window->add_PointerExited(Callback<PointerHandler>(this, &QWinRTScreen::onPointerExited).Get(), &m_tokens[QEvent::Leave]);
    m_window->add_PointerMoved(Callback<PointerHandler>(this, &QWinRTScreen::onPointerUpdated).Get(), &m_tokens[QEvent::MouseMove]);
    m_window->add_PointerPressed(Callback<PointerHandler>(this, &QWinRTScreen::onPointerUpdated).Get(), &m_tokens[QEvent::MouseButtonPress]);
    m_window->add_PointerReleased(Callback<PointerHandler>(this, &QWinRTScreen::onPointerUpdated).Get(), &m_tokens[QEvent::MouseButtonRelease]);
    m_window->add_PointerWheelChanged(Callback<PointerHandler>(this, &QWinRTScreen::onPointerUpdated).Get(), &m_tokens[QEvent::Wheel]);
    m_window->add_SizeChanged(Callback<SizeChangedHandler>(this, &QWinRTScreen::onSizeChanged).Get(), &m_tokens[QEvent::Resize]);

    // Window event handlers
    m_window->add_Activated(Callback<ActivatedHandler>(this, &QWinRTScreen::onActivated).Get(), &m_tokens[QEvent::WindowActivate]);
    m_window->add_Closed(Callback<ClosedHandler>(this, &QWinRTScreen::onClosed).Get(), &m_tokens[QEvent::WindowDeactivate]);
    m_window->add_VisibilityChanged(Callback<VisibilityChangedHandler>(this, &QWinRTScreen::onVisibilityChanged).Get(), &m_tokens[QEvent::Show]);
    m_window->add_AutomationProviderRequested(Callback<AutomationProviderRequestedHandler>(this, &QWinRTScreen::onAutomationProviderRequested).Get(), &m_tokens[QEvent::InputMethodQuery]);

    // Orientation handling
    if (SUCCEEDED(GetActivationFactory(HString::MakeReference(RuntimeClass_Windows_Graphics_Display_DisplayProperties).Get(),
                                       &m_displayProperties))) {
        // Set native orientation
        DisplayOrientations displayOrientation;
        m_displayProperties->get_NativeOrientation(&displayOrientation);
        m_nativeOrientation = qOrientationFromNative(displayOrientation);

        // Set initial orientation
        onOrientationChanged(0);

        m_displayProperties->add_OrientationChanged(Callback<IDisplayPropertiesEventHandler>(this, &QWinRTScreen::onOrientationChanged).Get(),
                                                    &m_tokens[QEvent::OrientationChange]);
    }
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

QSurfaceFormat QWinRTScreen::surfaceFormat() const
{
    return m_surfaceFormat;
}

QWinRTInputContext *QWinRTScreen::inputContext() const
{
    return m_inputContext;
}

QPlatformCursor *QWinRTScreen::cursor() const
{
    return m_cursor;
}

Qt::ScreenOrientation QWinRTScreen::nativeOrientation() const
{
    return m_nativeOrientation;
}

Qt::ScreenOrientation QWinRTScreen::orientation() const
{
    return m_orientation;
}

#ifdef Q_WINRT_GL

ICoreWindow *QWinRTScreen::coreWindow() const
{
    return m_window;
}

EGLDisplay QWinRTScreen::eglDisplay() const
{
    return m_eglDisplay;
}

EGLSurface QWinRTScreen::eglSurface() const
{
    return m_eglSurface;
}

#else // Q_WINRT_GL

QPlatformScreenPageFlipper *QWinRTScreen::pageFlipper() const
{
    return m_pageFlipper;
}

void QWinRTScreen::update(const QRegion &region, const QPoint &offset, const void *handle, int stride)
{
    m_pageFlipper->update(region, offset, handle, stride);
}

#endif // Q_WINRT_GL

HRESULT QWinRTScreen::handleKeyEvent(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IKeyEventArgs *args)
{
    Q_UNUSED(window);

    return m_keyMapper->translateKeyEvent(0, args) ? S_OK : S_FALSE;
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
    IPointerPoint *pointerPoint;
    if (SUCCEEDED(args->get_CurrentPoint(&pointerPoint))) {
        // Assumes full-screen window
        Point point;
        pointerPoint->get_Position(&point);
        QPoint pos(point.X, point.Y);

        QWindowSystemInterface::handleEnterEvent(0, pos, pos);
        pointerPoint->Release();
    }
    return S_OK;
}

HRESULT QWinRTScreen::onPointerExited(ICoreWindow *window, IPointerEventArgs *args)
{
    Q_UNUSED(window);
    Q_UNUSED(args);
    QWindowSystemInterface::handleLeaveEvent(0);
    return S_OK;
}

HRESULT QWinRTScreen::onPointerUpdated(ICoreWindow *window, IPointerEventArgs *args)
{
    Q_UNUSED(window);

    IPointerPoint *pointerPoint;
    if (FAILED(args->get_CurrentPoint(&pointerPoint)))
        return E_INVALIDARG;

    // Common traits - point, modifiers, properties
    Point point;
    pointerPoint->get_Position(&point);
    QPointF pos(point.X, point.Y);

    VirtualKeyModifiers modifiers;
    args->get_KeyModifiers(&modifiers);
    Qt::KeyboardModifiers mods =
            (modifiers & VirtualKeyModifiers_Control ? Qt::ControlModifier : 0)
            | (modifiers & VirtualKeyModifiers_Menu ? Qt::AltModifier : 0)
            | (modifiers & VirtualKeyModifiers_Shift ? Qt::ShiftModifier : 0)
            | (modifiers & VirtualKeyModifiers_Windows ? Qt::MetaModifier : 0);

    IPointerPointProperties *properties;
    if (FAILED(pointerPoint->get_Properties(&properties)))
        return E_INVALIDARG;

#ifdef Q_OS_WINPHONE
    quint32 pointerId = 0;
    Pointer pointer = m_pointers.value(pointerId);
#else
    Pointer pointer = { Pointer::Unknown, 0 };
    quint32 pointerId;
    pointerPoint->get_PointerId(&pointerId);
    if (m_pointers.contains(pointerId)) {
        pointer = m_pointers.value(pointerId);
    } else { // We have not yet enumerated this device. Do so now...
        IPointerDevice *device;
        if (SUCCEEDED(pointerPoint->get_PointerDevice(&device))) {
            PointerDeviceType type;
            device->get_PointerDeviceType(&type);
            switch (type) {
            case PointerDeviceType_Touch:
                pointer.type = Pointer::TouchScreen;
                pointer.device = new QTouchDevice;
                pointer.device->setName(QString("WinRT TouchScreen %1").arg(pointerId));
                // TODO: We may want to probe the device usage flags for more accurate values for these next two
                pointer.device->setType(QTouchDevice::TouchScreen);
                pointer.device->setCapabilities(QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure);
                QWindowSystemInterface::registerTouchDevice(pointer.device);
                break;

            case PointerDeviceType_Pen:
                pointer.type = Pointer::Tablet;
                break;

            case PointerDeviceType_Mouse:
                pointer.type = Pointer::Mouse;
                break;
            }

            m_pointers.insert(pointerId, pointer);
            device->Release();
        }
    }
#endif
    switch (pointer.type) {
    case Pointer::Mouse: {
        qint32 delta;
        properties->get_MouseWheelDelta(&delta);
        if (delta) {
            boolean isHorizontal;
            properties->get_IsHorizontalMouseWheel(&isHorizontal);
            QPoint angleDelta(isHorizontal ? delta : 0, isHorizontal ? 0 : delta);
            QWindowSystemInterface::handleWheelEvent(0, pos, pos, QPoint(), angleDelta, mods);
            break;
        }

        boolean isPressed;
        Qt::MouseButtons buttons = Qt::NoButton;
        properties->get_IsLeftButtonPressed(&isPressed);
        if (isPressed)
            buttons |= Qt::LeftButton;

        properties->get_IsMiddleButtonPressed(&isPressed);
        if (isPressed)
            buttons |= Qt::MiddleButton;

        properties->get_IsRightButtonPressed(&isPressed);
        if (isPressed)
            buttons |= Qt::RightButton;

        properties->get_IsXButton1Pressed(&isPressed);
        if (isPressed)
            buttons |= Qt::XButton1;

        properties->get_IsXButton2Pressed(&isPressed);
        if (isPressed)
            buttons |= Qt::XButton2;

        QWindowSystemInterface::handleMouseEvent(0, pos, pos, buttons, mods);

        break;
    }
    case Pointer::TouchScreen: {
        quint32 id;
        pointerPoint->get_PointerId(&id);

        Rect area;
        properties->get_ContactRect(&area);

        float pressure;
        properties->get_Pressure(&pressure);

        if (m_touchPoints.contains(id)) {
            boolean isPressed;
            pointerPoint->get_IsInContact(&isPressed);
            m_touchPoints[id].state = isPressed ? Qt::TouchPointMoved : Qt::TouchPointReleased;
        } else {
            m_touchPoints[id] = QWindowSystemInterface::TouchPoint();
            m_touchPoints[id].state = Qt::TouchPointPressed;
            m_touchPoints[id].id = id;
        }
        m_touchPoints[id].area = QRectF(area.X, area.Y, area.Width, area.Height);
        m_touchPoints[id].normalPosition = QPointF(pos.x()/m_geometry.width(), pos.y()/m_geometry.height());
        m_touchPoints[id].pressure = pressure;

        QWindowSystemInterface::handleTouchEvent(0, pointer.device, m_touchPoints.values(), mods);

        // Remove released points, station others
        for (QHash<quint32, QWindowSystemInterface::TouchPoint>::iterator i = m_touchPoints.begin(); i != m_touchPoints.end();) {
            if (i.value().state == Qt::TouchPointReleased)
                i = m_touchPoints.erase(i);
            else
                (i++).value().state = Qt::TouchPointStationary;
        }

        break;
    }
    case Pointer::Tablet: {
        quint32 id;
        pointerPoint->get_PointerId(&id);

        boolean isPressed;
        pointerPoint->get_IsInContact(&isPressed);

        boolean isEraser;
        properties->get_IsEraser(&isEraser);
        int pointerType = isEraser ? 3 : 1;

        float pressure;
        properties->get_Pressure(&pressure);

        float xTilt;
        properties->get_XTilt(&xTilt);

        float yTilt;
        properties->get_YTilt(&yTilt);

        float rotation;
        properties->get_Twist(&rotation);

        QWindowSystemInterface::handleTabletEvent(0, isPressed, pos, pos, pointerId,
                                                  pointerType, pressure, xTilt, yTilt,
                                                  0, rotation, 0, id, mods);

        break;
    }
    }

    properties->Release();
    pointerPoint->Release();

    return S_OK;
}

HRESULT QWinRTScreen::onAutomationProviderRequested(ICoreWindow *, IAutomationProviderRequestedEventArgs *args)
{
#ifndef Q_OS_WINPHONE
    args->put_AutomationProvider(m_inputContext);
#endif
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
#ifndef Q_WINRT_GL
    m_pageFlipper->resize(m_geometry.size());
#endif

    QWindowSystemInterface::handleScreenGeometryChange(screen(), m_geometry);
    QWindowSystemInterface::handleScreenAvailableGeometryChange(screen(), m_geometry);
    resizeMaximizedWindows();

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

HRESULT QWinRTScreen::onOrientationChanged(IInspectable *)
{
    DisplayOrientations displayOrientation;
    m_displayProperties->get_CurrentOrientation(&displayOrientation);
    Qt::ScreenOrientation newOrientation = qOrientationFromNative(displayOrientation);
    if (m_orientation != newOrientation) {
        m_orientation = newOrientation;
        QWindowSystemInterface::handleScreenOrientationChange(screen(), m_orientation);
    }

    return S_OK;
}

