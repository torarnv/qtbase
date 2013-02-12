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

#ifndef QEVENTDISPATCHER_WINRT_P_H
#define QEVENTDISPATCHER_WINRT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#define FD_SETSIZE 10*1024

#include "QtCore/qabstracteventdispatcher.h"
#include "QtCore/qlist.h"
#include "private/qabstracteventdispatcher_p.h"
#include "private/qpodlist_p.h"
#include "QtCore/qvarlengtharray.h"
#include "private/qtimerinfo_unix_p.h"

#include <winsock2.h>

QT_BEGIN_NAMESPACE

struct QSockNot
{
    QSocketNotifier *obj;
    int fd;
    fd_set *queue;
};

class QSockNotType
{
public:
    QSockNotType();
    ~QSockNotType();

    typedef QPodList<QSockNot*, 32> List;

    List list;
    fd_set select_fds;
    fd_set enabled_fds;
    fd_set pending_fds;

};

class QEventDispatcherWinRTPrivate;

class Q_CORE_EXPORT QEventDispatcherWinRT : public QAbstractEventDispatcher
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QEventDispatcherWinRT)

public:
    explicit QEventDispatcherWinRT(QObject *parent = 0);
    ~QEventDispatcherWinRT();

    bool processEvents(QEventLoop::ProcessEventsFlags flags);
    bool hasPendingEvents();

    void registerSocketNotifier(QSocketNotifier *notifier);
    void unregisterSocketNotifier(QSocketNotifier *notifier);

    void registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *object);
    bool unregisterTimer(int timerId);
    bool unregisterTimers(QObject *object);
    QList<TimerInfo> registeredTimers(QObject *object) const;

    bool registerEventNotifier(QWinEventNotifier *notifier)
    {
        Q_UNUSED(notifier);
        return false;
    }

    void unregisterEventNotifier(QWinEventNotifier *notifier)
    {
        Q_UNUSED(notifier);
    }

    int remainingTime(int timerId);

    void wakeUp();
    void interrupt();
    void flush();

protected:
    QEventDispatcherWinRT(QEventDispatcherWinRTPrivate &dd, QObject *parent = 0);

    void setSocketNotifierPending(QSocketNotifier *notifier);

    int activateTimers();
    int activateSocketNotifiers();

    virtual int select(int nfds,
                       fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                       timeval *timeout);
};

class Q_CORE_EXPORT QEventDispatcherWinRTPrivate : public QAbstractEventDispatcherPrivate
{
    Q_DECLARE_PUBLIC(QEventDispatcherWinRT)

public:
    QEventDispatcherWinRTPrivate();
    ~QEventDispatcherWinRTPrivate();

    int doSelect(QEventLoop::ProcessEventsFlags flags, timeval *timeout);
    virtual int initThreadWakeUp();
    virtual int processThreadWakeUp(int nsel);

    int thread_pipe[2];

    // highest fd for all socket notifiers
    int sn_highest;
    // 3 socket notifier types - read, write and exception
    QSockNotType sn_vec[3];

    QTimerInfoList timerList;

    // pending socket notifiers list
    QSockNotType::List sn_pending_list;

    QAtomicInt wakeUps;
    bool interrupt;
};

QT_END_NAMESPACE

#endif // QEVENTDISPATCHER_WINRT_P_H
