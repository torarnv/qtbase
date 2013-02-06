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

PointerValue::PointerValue(ABI::Windows::UI::Core::ICoreWindow* window, ABI::Windows::UI::Core::IPointerEventArgs *args)
{
    args->get_KeyModifiers(&mods);
    args->get_CurrentPoint(&point);
    point->get_Properties(&properties);
    point->get_PointerDevice(&device);
}

PointerValue::~PointerValue()
{
}

Qt::KeyboardModifiers PointerValue::modifiers() const
{
    return (mods & 1 ? Qt::ControlModifier : 0) | (mods & 4 ? Qt::ShiftModifier : 0) | (mods & 8 ? Qt::MetaModifier : 0);
}

ABI::Windows::Devices::Input::PointerDeviceType PointerValue::type() const
{
    if (device == nullptr)
        return ABI::Windows::Devices::Input::PointerDeviceType_Touch;
    ABI::Windows::Devices::Input::PointerDeviceType t;
    device->get_PointerDeviceType(&t);
    return t;
}

Qt::MouseButtons PointerValue::buttons() const
{
    boolean a;
    Qt::MouseButtons b;
    properties->get_IsLeftButtonPressed(&a); b |= a ? Qt::LeftButton : Qt::NoButton;
    properties->get_IsMiddleButtonPressed(&a); b |= a ? Qt::MiddleButton : Qt::NoButton;
    properties->get_IsRightButtonPressed(&a); b |= a ? Qt::RightButton : Qt::NoButton;
    properties->get_IsXButton1Pressed(&a); b |= a ? Qt::XButton1 : Qt::NoButton;
    properties->get_IsXButton2Pressed(&a); b |= a ? Qt::XButton2 : Qt::NoButton;
    return b;
}

QPointF PointerValue::pos() const
{
    ABI::Windows::Foundation::Point pt;
    point->get_Position(&pt);
    return QPointF(pt.X, pt.Y);
}

int PointerValue::delta() const
{
    int d;
    properties->get_MouseWheelDelta(&d);
    return d;
}

Qt::Orientation PointerValue::orientation() const
{
    boolean b;
    properties->get_IsHorizontalMouseWheel(&b);
    return b ? Qt::Horizontal : Qt::Vertical;
}

QWindowSystemInterface::TouchPoint PointerValue::touchPoint() const
{
    QWindowSystemInterface::TouchPoint tp;

    quint32 id;
    point->get_PointerId(&id);
    tp.id = id;

    if (device) {
        ABI::Windows::Foundation::Rect sceneRect;
        device->get_ScreenRect(&sceneRect);
        QPointF pt = pos();
        tp.area = QRectF(pt, QSize(1, 1)).translated(-0.5, -0.5);
        tp.normalPosition = QPointF(pt.x()/sceneRect.Width, pt.y()/sceneRect.Height);
    } else {
        QPointF pt = pos();
        tp.area = QRectF(pt, QSize(1, 1)).translated(-0.5, -0.5);
        tp.normalPosition = pt;
    }
    return tp;
}
