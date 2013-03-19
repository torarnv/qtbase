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

#include "pointervalue.h"

#include <wrl.h>
#include <windows.system.h>
#include <windows.devices.input.h>
#include <windows.ui.h>
#include <windows.ui.core.h>
#include <windows.ui.input.h>

using namespace ABI::Windows::UI::Input;
using namespace ABI::Windows::Devices::Input;

class PointerValuePrivate
{
    Microsoft::WRL::ComPtr<IPointerPoint> point;
    Microsoft::WRL::ComPtr<IPointerPointProperties> properties;
    Microsoft::WRL::ComPtr<IPointerDevice> device;
    ABI::Windows::System::VirtualKeyModifiers mods;
    friend class PointerValue;
};

PointerValue::PointerValue(ABI::Windows::UI::Core::IPointerEventArgs *args)
    : d(new PointerValuePrivate)
{
    args->get_CurrentPoint(&d->point);
    d->point->get_Properties(&d->properties);
    d->point->get_PointerDevice(&d->device);
    args->get_KeyModifiers(&d->mods);
}

PointerValue::~PointerValue()
{
}

Qt::KeyboardModifiers PointerValue::modifiers() const
{
    return (d->mods & 1 ? Qt::ControlModifier : 0)
            | (d->mods & 4 ? Qt::ShiftModifier : 0)
            | (d->mods & 8 ? Qt::MetaModifier : 0);
}

bool PointerValue::isMouse() const
{
    if (!d->device)
        return false;
    PointerDeviceType type;
    d->device->get_PointerDeviceType(&type);
    return type == PointerDeviceType_Mouse;
}

Qt::MouseButtons PointerValue::buttons() const
{
    boolean isPressed;
    Qt::MouseButtons buttons;
    d->properties->get_IsLeftButtonPressed(&isPressed);
    buttons |= isPressed ? Qt::LeftButton : Qt::NoButton;
    d->properties->get_IsMiddleButtonPressed(&isPressed);
    buttons |= isPressed ? Qt::MiddleButton : Qt::NoButton;
    d->properties->get_IsRightButtonPressed(&isPressed);
    buttons |= isPressed ? Qt::RightButton : Qt::NoButton;
    d->properties->get_IsXButton1Pressed(&isPressed);
    buttons |= isPressed ? Qt::XButton1 : Qt::NoButton;
    d->properties->get_IsXButton2Pressed(&isPressed);
    buttons |= isPressed ? Qt::XButton2 : Qt::NoButton;
    return buttons;
}

QPointF PointerValue::pos() const
{
    ABI::Windows::Foundation::Point pos;
    d->point->get_Position(&pos);
    return QPointF(pos.X, pos.Y);
}

int PointerValue::delta() const
{
    int delta;
    d->properties->get_MouseWheelDelta(&delta);
    return delta;
}

Qt::Orientation PointerValue::orientation() const
{
    boolean isHorizontal;
    d->properties->get_IsHorizontalMouseWheel(&isHorizontal);
    return isHorizontal ? Qt::Horizontal : Qt::Vertical;
}

QWindowSystemInterface::TouchPoint PointerValue::touchPoint() const
{
    QWindowSystemInterface::TouchPoint touchPoint;

    quint32 id;
    d->point->get_PointerId(&id);
    touchPoint.id = id;

    ABI::Windows::Foundation::Rect rect;
    d->properties->get_ContactRect(&rect);
    touchPoint.area = QRectF(rect.X, rect.Y, rect.Width, rect.Height);
    ABI::Windows::Foundation::Point pos;
    d->point->get_Position(&pos);
    if (d->device) {
        ABI::Windows::Foundation::Rect screenRect;
        d->device->get_ScreenRect(&screenRect);
        touchPoint.normalPosition = QPointF(pos.X/screenRect.Width, pos.Y/screenRect.Height);
    } else {
        touchPoint.normalPosition = QPointF(pos.X, pos.Y);
    }
    return touchPoint;
}
