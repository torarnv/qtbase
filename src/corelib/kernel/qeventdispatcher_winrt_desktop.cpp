/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtCore module of the Qt Toolkit.
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

#ifndef Q_OS_WINPHONE
#include "qeventdispatcher_winrt_desktop_p.h"

#include "qcoreapplication.h"
#include "qthread.h"

#include <private/qcoreapplication_p.h>
#include <private/qthread_p.h>

QT_BEGIN_NAMESPACE

QEventDispatcherWinRT::QEventDispatcherWinRT(QObject *parent)
    : QAbstractEventDispatcher(parent)
{
}

QEventDispatcherWinRT::~QEventDispatcherWinRT()
{
}


bool QEventDispatcherWinRT::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    Q_UNUSED(flags)
    // we are awake, broadcast it
    emit awake();
    QCoreApplicationPrivate::sendPostedEvents(0, 0, QThreadData::current());

    return false;
}

bool QEventDispatcherWinRT::hasPendingEvents()
{
    return false;
}

void QEventDispatcherWinRT::registerSocketNotifier(QSocketNotifier *notifier)
{
    Q_UNUSED(notifier);
}
void QEventDispatcherWinRT::unregisterSocketNotifier(QSocketNotifier *notifier)
{
    Q_UNUSED(notifier);
}

void QEventDispatcherWinRT::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object)
{
    Q_UNUSED(timerId);
    Q_UNUSED(interval);
    Q_UNUSED(timerType);
    Q_UNUSED(object);
}
bool QEventDispatcherWinRT::unregisterTimer(int timerId)
{
    Q_UNUSED(timerId);
    return false;
}
bool QEventDispatcherWinRT::unregisterTimers(QObject *object)
{
    Q_UNUSED(object);
    return false;
}
QList<QAbstractEventDispatcher::TimerInfo> QEventDispatcherWinRT::registeredTimers(QObject *object) const
{
    Q_UNUSED(object);
    return QList<QAbstractEventDispatcher::TimerInfo>();
}

bool QEventDispatcherWinRT::registerEventNotifier(QWinEventNotifier *notifier)
{
    Q_UNUSED(notifier);
    return false;
}
void QEventDispatcherWinRT::unregisterEventNotifier(QWinEventNotifier *notifier)
{
    Q_UNUSED(notifier);
}

int QEventDispatcherWinRT::remainingTime(int timerId)
{
    Q_UNUSED(timerId)
    return -1;
}

void QEventDispatcherWinRT::wakeUp()
{

}
void QEventDispatcherWinRT::interrupt()
{

}
void QEventDispatcherWinRT::flush()
{

}

void QEventDispatcherWinRT::startingUp()
{
}

void QEventDispatcherWinRT::closingDown()
{
}

bool QEventDispatcherWinRT::event(QEvent *e)
{
    return QAbstractEventDispatcher::event(e);
}

QEventDispatcherWinRTPrivate::QEventDispatcherWinRTPrivate()
{
}

QEventDispatcherWinRTPrivate::~QEventDispatcherWinRTPrivate()
{
}

QT_END_NAMESPACE
#endif // !Q_OS_WINPHONE
