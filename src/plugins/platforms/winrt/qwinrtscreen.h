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

#ifndef QWINRTSCREEN_H
#define QWINRTSCREEN_H

#include <qpa/qplatformscreen.h>

#include <QtCore/QHash>
#include <QtGui/QTouchDevice>

#include <EventToken.h>

namespace ABI {
namespace Windows {
namespace UI {
namespace Core {
struct ICoreWindow;
struct ICoreWindowEventArgs;
struct IKeyEventArgs;
struct IPointerEventArgs;
struct IVisibilityChangedEventArgs;
struct IWindowActivatedEventArgs;
struct IWindowSizeChangedEventArgs;
}
}
}
}

QT_BEGIN_NAMESPACE

class QWinRTKeyMapper;
class QWinRTPageFlipper;

class QWinRTScreen : public QPlatformScreen
{
public:
    QWinRTScreen(ABI::Windows::UI::Core::ICoreWindow *window);
    QRect geometry() const;
    int depth() const;
    QImage::Format format() const;
    QPlatformScreenPageFlipper *pageFlipper() const;
    void update(const QRegion &region, const QPoint &offset, const void *handle, int stride);

private:
    HRESULT handleKeyEvent(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IKeyEventArgs *args);
    HRESULT handlePointerEvent(Qt::TouchPointState pointState, ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);

private:
    // Event handlers
    QHash<QEvent::Type, EventRegistrationToken> m_tokens;

    HRESULT onKeyDown(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IKeyEventArgs *args);
    HRESULT onKeyUp(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IKeyEventArgs *args);
    HRESULT onPointerEntered(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);
    HRESULT onPointerExited(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);
    HRESULT onPointerPressed(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);
    HRESULT onPointerMoved(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);
    HRESULT onPointerReleased(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);
    HRESULT onPointerWheelChanged(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IPointerEventArgs *args);
    HRESULT onSizeChanged(ABI::Windows::UI::Core::ICoreWindow *window, ABI::Windows::UI::Core::IWindowSizeChangedEventArgs *args);

    HRESULT onActivated(ABI::Windows::UI::Core::ICoreWindow *, ABI::Windows::UI::Core::IWindowActivatedEventArgs *args);
    HRESULT onClosed(ABI::Windows::UI::Core::ICoreWindow *, ABI::Windows::UI::Core::ICoreWindowEventArgs *args);
    HRESULT onVisibilityChanged(ABI::Windows::UI::Core::ICoreWindow *, ABI::Windows::UI::Core::IVisibilityChangedEventArgs *args);

    ABI::Windows::UI::Core::ICoreWindow *m_window;
    QTouchDevice m_touchDevice;
    QRect m_geometry;
    QImage::Format m_format;
    int m_depth;
    QWinRTKeyMapper *m_keyMapper;
    QWinRTPageFlipper *m_pageFlipper;
};

QT_END_NAMESPACE

#endif // QWINRTSCREEN_H
