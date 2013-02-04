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

#ifdef Q_OS_WINPHONE
#include "qplatformdefs.h"
#include "qeventdispatcher_winrt_phone_p.h"

#include "qcoreapplication.h"
#include "qpair.h"
#include "qsocketnotifier.h"
#include "qthread.h"
#include "qelapsedtimer.h"

#include <private/qthread_p.h>
#include <private/qcoreapplication_p.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>

QT_BEGIN_NAMESPACE

#ifdef Q_OS_WINRT
timeval qt_gettime();

// Internal operator functions for timevals
inline timeval &normalizedTimeval(timeval &t)
{
    while (t.tv_usec > 1000000l) {
        ++t.tv_sec;
        t.tv_usec -= 1000000l;
    }
    while (t.tv_usec < 0l) {
        --t.tv_sec;
        t.tv_usec += 1000000l;
    }
    return t;
}
inline bool operator<(const timeval &t1, const timeval &t2)
{ return t1.tv_sec < t2.tv_sec || (t1.tv_sec == t2.tv_sec && t1.tv_usec < t2.tv_usec); }
inline bool operator==(const timeval &t1, const timeval &t2)
{ return t1.tv_sec == t2.tv_sec && t1.tv_usec == t2.tv_usec; }
inline timeval &operator+=(timeval &t1, const timeval &t2)
{
    t1.tv_sec += t2.tv_sec;
    t1.tv_usec += t2.tv_usec;
    return normalizedTimeval(t1);
}
inline timeval operator+(const timeval &t1, const timeval &t2)
{
    timeval tmp;
    tmp.tv_sec = t1.tv_sec + t2.tv_sec;
    tmp.tv_usec = t1.tv_usec + t2.tv_usec;
    return normalizedTimeval(tmp);
}
inline timeval operator-(const timeval &t1, const timeval &t2)
{
    timeval tmp;
    tmp.tv_sec = t1.tv_sec - (t2.tv_sec - 1);
    tmp.tv_usec = t1.tv_usec - (t2.tv_usec + 1000000);
    return normalizedTimeval(tmp);
}
inline timeval operator*(const timeval &t1, int mul)
{
    timeval tmp;
    tmp.tv_sec = t1.tv_sec * mul;
    tmp.tv_usec = t1.tv_usec * mul;
    return normalizedTimeval(tmp);
}
#endif

/*****************************************************************************
 SOCKET handling
 *****************************************************************************/

#define EINTR_LOOP(var, cmd)                    \
    do {                                        \
        var = cmd;                              \
    } while (var == -1 && errno == EINTR)

static inline bool time_update(struct timeval *tv, const struct timeval &start,
                               const struct timeval &timeout)
{
    if (!QElapsedTimer::isMonotonic()) {
        // we cannot recalculate the timeout without a monotonic clock as the time may have changed
        return false;
    }

    // clock source is monotonic, so we can recalculate how much timeout is left
    struct timeval now = qt_gettime();
    *tv = timeout + start - now;
    return tv->tv_sec >= 0;
}

int qt_safe_select(int nfds, fd_set *fdread, fd_set *fdwrite, fd_set *fdexcept,
                   const struct timeval *orig_timeout)
{
    if (!orig_timeout) {
        // no timeout -> block forever
        register int ret;
        EINTR_LOOP(ret, select(nfds, fdread, fdwrite, fdexcept, 0));
        return ret;
    }

    timeval start = qt_gettime();
    timeval timeout = *orig_timeout;

    // loop and recalculate the timeout as needed
    int ret;
    forever {
        ret = ::select(nfds, fdread, fdwrite, fdexcept, &timeout);
        if (ret != -1 || errno != EINTR)
            return ret;

        // recalculate the timeout
        if (!time_update(&timeout, start, *orig_timeout)) {
            // timeout during update
            // or clock reset, fake timeout error
            return 0;
        }
    }
}

static inline int qt_safe_close(int fd)
{
    register int ret;
    EINTR_LOOP(ret, QT_CLOSE(fd));
    return ret;
}

static inline qint64 qt_safe_read(int fd, void *data, qint64 maxlen)
{
    qint64 ret = 0;
    EINTR_LOOP(ret, QT_READ(fd, data, maxlen));
    return ret;
}

static inline qint64 qt_safe_write(int fd, const void *data, qint64 len)
{
    qint64 ret = 0;
    EINTR_LOOP(ret, QT_WRITE(fd, data, len));
    return ret;
}

int socketpair(int socks[2], int make_overlapped = 0)
{
    union {
       struct sockaddr_in inaddr;
       struct sockaddr addr;
    } a;
    int listener;
    int e;
    int addrlen = sizeof(a.inaddr);
    DWORD flags = (make_overlapped ? WSA_FLAG_OVERLAPPED : 0);
    int reuse = 1;

    if (socks == 0) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        return SOCKET_ERROR;

    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0;

    socks[0] = socks[1] = INVALID_SOCKET;
    do {
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               (char*) &reuse, sizeof(reuse)) == -1)
            break;
        if (bind(listener, &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        memset(&a, 0, sizeof(a));
        if (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
            break;
        // win32 getsockname may only set the port number, p=0.0005.
        // ( http://msdn.microsoft.com/library/ms738543.aspx ):
        a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.inaddr.sin_family = AF_INET;

        if (listen(listener, 1) == SOCKET_ERROR)
            break;

        socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
        if (socks[0] == INVALID_SOCKET)
            break;
        if (connect(socks[0], &a.addr, sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        socks[1] = accept(listener, NULL, NULL);
        if (socks[1] == INVALID_SOCKET)
            break;

        closesocket(listener);
        return 0;

    } while (0);

    e = WSAGetLastError();
    closesocket(listener);
    closesocket(socks[0]);
    closesocket(socks[1]);
    WSASetLastError(e);
    return SOCKET_ERROR;
}

static void initThreadPipeFD(int fd)
{
    u_long mode = 1;
    int ret = ioctlsocket(fd, FIONBIO, &mode);
    if (ret == -1)
        perror("QEventDispatcherUNIXPrivate: Unable to set flags on thread pipe");
}

QEventDispatcherWinRTPrivate::QEventDispatcherWinRTPrivate()
{
    bool pipefail = false;

    // INTEGRITY doesn't like a "select" on pipes, so use socketpair instead
    if (socketpair(thread_pipe) == -1) {
        perror("QEventDispatcherUNIXPrivate(): Unable to create socket pair");
        pipefail = true;
    } else {
        initThreadPipeFD(thread_pipe[0]);
        initThreadPipeFD(thread_pipe[1]);
    }

    if (pipefail)
        qFatal("QEventDispatcherWinRTPrivate(): Can not continue without a thread pipe");

    sn_highest = -1;

    interrupt = false;
}

QEventDispatcherWinRTPrivate::~QEventDispatcherWinRTPrivate()
{
    // cleanup the common parts of the event loop
    closesocket(thread_pipe[0]);
    closesocket(thread_pipe[1]);

    // cleanup timers
    qDeleteAll(timerList);
}

int QEventDispatcherWinRTPrivate::doSelect(QEventLoop::ProcessEventsFlags flags, timeval *timeout)
{
    Q_Q(QEventDispatcherWinRT);

    // needed in QEventDispatcherUNIX::select()
    timerList.updateCurrentTime();

    int nsel;
    do {
        // Process timers and socket notifiers - the common UNIX stuff
        int highest = 0;
        if (! (flags & QEventLoop::ExcludeSocketNotifiers) && (sn_highest >= 0)) {
            // return the highest fd we can wait for input on
                sn_vec[0].select_fds = sn_vec[0].enabled_fds;
                sn_vec[1].select_fds = sn_vec[1].enabled_fds;
                sn_vec[2].select_fds = sn_vec[2].enabled_fds;
            highest = sn_highest;
        } else {
            FD_ZERO(&sn_vec[0].select_fds);
            FD_ZERO(&sn_vec[1].select_fds);
            FD_ZERO(&sn_vec[2].select_fds);
        }

        int wakeUpFd = initThreadWakeUp();
        highest = qMax(highest, wakeUpFd);

        nsel = q->select(highest + 1,
                         &sn_vec[0].select_fds,
                         &sn_vec[1].select_fds,
                         &sn_vec[2].select_fds,
                         timeout);
    } while (nsel == -1 && (errno == EINTR || errno == EAGAIN));

    if (nsel == -1) {
        if (errno == EBADF) {
            // it seems a socket notifier has a bad fd... find out
            // which one it is and disable it
            fd_set fdset;
            timeval tm;
            tm.tv_sec = tm.tv_usec = 0l;

            for (int type = 0; type < 3; ++type) {
                QSockNotType::List &list = sn_vec[type].list;
                if (list.size() == 0)
                    continue;

                for (int i = 0; i < list.size(); ++i) {
                    QSockNot *sn = list[i];

                    FD_ZERO(&fdset);
                    FD_SET(sn->fd, &fdset);

                    int ret = -1;
                    do {
                        switch (type) {
                        case 0: // read
                            ret = select(sn->fd + 1, &fdset, 0, 0, &tm);
                            break;
                        case 1: // write
                            ret = select(sn->fd + 1, 0, &fdset, 0, &tm);
                            break;
                        case 2: // except
                            ret = select(sn->fd + 1, 0, 0, &fdset, &tm);
                            break;
                        }
                    } while (ret == -1 && (errno == EINTR || errno == EAGAIN));

                    if (ret == -1 && errno == EBADF) {
                        // disable the invalid socket notifier
                        static const char *t[] = { "Read", "Write", "Exception" };
                        qWarning("QSocketNotifier: Invalid socket %d and type '%s', disabling...",
                                 sn->fd, t[type]);
                        sn->obj->setEnabled(false);
                    }
                }
            }
        } else {
            // EINVAL... shouldn't happen, so let's complain to stderr
            // and hope someone sends us a bug report
            perror("select");
        }
    }

    int nevents = processThreadWakeUp(nsel);

    // activate socket notifiers
    if (! (flags & QEventLoop::ExcludeSocketNotifiers) && nsel > 0 && sn_highest >= 0) {
        // if select says data is ready on any socket, then set the socket notifier
        // to pending
        for (int i=0; i<3; i++) {
            QSockNotType::List &list = sn_vec[i].list;
            for (int j = 0; j < list.size(); ++j) {
                QSockNot *sn = list[j];
                if (FD_ISSET(sn->fd, &sn_vec[i].select_fds))
                    q->setSocketNotifierPending(sn->obj);
            }
        }
    }
    return (nevents + q->activateSocketNotifiers());
}

int QEventDispatcherWinRTPrivate::initThreadWakeUp()
{
    FD_SET(thread_pipe[0], &sn_vec[0].select_fds);
    return thread_pipe[0];
}

int QEventDispatcherWinRTPrivate::processThreadWakeUp(int nsel)
{
    if (nsel > 0 && FD_ISSET(thread_pipe[0], &sn_vec[0].select_fds)) {
        // some other thread woke us up... consume the data on the thread pipe so that
        // select doesn't immediately return next time
        char c[16];
        while (::recv(thread_pipe[0], c, sizeof(c), 0) > 0)
            ;
        if (!wakeUps.testAndSetRelease(1, 0)) {
            // hopefully, this is dead code
            qWarning("QEventDispatcherWinRT: internal error, wakeUps.testAndSetRelease(1, 0) failed!");
        }
        return 1;
    }
    return 0;
}

QEventDispatcherWinRT::QEventDispatcherWinRT(QObject *parent)
    : QAbstractEventDispatcher(*new QEventDispatcherWinRTPrivate, parent)
{ }

QEventDispatcherWinRT::QEventDispatcherWinRT(QEventDispatcherWinRTPrivate &dd, QObject *parent)
    : QAbstractEventDispatcher(dd, parent)
{ }

QEventDispatcherWinRT::~QEventDispatcherWinRT()
{
    Q_D(QEventDispatcherWinRT);
    d->threadData->eventDispatcher = 0;
}

int QEventDispatcherWinRT::select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                                 timeval *timeout)
{
    return qt_safe_select(nfds, readfds, writefds, exceptfds, timeout);
}

/*!
    \internal
*/
void QEventDispatcherWinRT::registerTimer(int timerId, int interval, Qt::TimerType timerType, QObject *obj)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1 || interval < 0 || !obj) {
        qWarning("QEventDispatcherWinRT::registerTimer: invalid arguments");
        return;
    } else if (obj->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QObject::startTimer: timers cannot be started from another thread");
        return;
    }
#endif

    Q_D(QEventDispatcherWinRT);
    d->timerList.registerTimer(timerId, interval, timerType, obj);
}

/*!
    \internal
*/
bool QEventDispatcherWinRT::unregisterTimer(int timerId)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherWinRT::unregisterTimer: invalid argument");
        return false;
    } else if (thread() != QThread::currentThread()) {
        qWarning("QObject::killTimer: timers cannot be stopped from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherWinRT);
    return d->timerList.unregisterTimer(timerId);
}

/*!
    \internal
*/
bool QEventDispatcherWinRT::unregisterTimers(QObject *object)
{
#ifndef QT_NO_DEBUG
    if (!object) {
        qWarning("QEventDispatcherWinRT::unregisterTimers: invalid argument");
        return false;
    } else if (object->thread() != thread() || thread() != QThread::currentThread()) {
        qWarning("QObject::killTimers: timers cannot be stopped from another thread");
        return false;
    }
#endif

    Q_D(QEventDispatcherWinRT);
    return d->timerList.unregisterTimers(object);
}

QList<QEventDispatcherWinRT::TimerInfo>
QEventDispatcherWinRT::registeredTimers(QObject *object) const
{
    if (!object) {
        qWarning("QEventDispatcherWinRT:registeredTimers: invalid argument");
        return QList<TimerInfo>();
    }

    Q_D(const QEventDispatcherWinRT);
    return d->timerList.registeredTimers(object);
}

/*****************************************************************************
 Socket notifier type
 *****************************************************************************/
QSockNotType::QSockNotType()
{
    FD_ZERO(&select_fds);
    FD_ZERO(&enabled_fds);
    FD_ZERO(&pending_fds);
}

QSockNotType::~QSockNotType()
{
    for (int i = 0; i < list.size(); ++i)
        delete list[i];
}

/*****************************************************************************
 QEventDispatcher implementations for UNIX
 *****************************************************************************/

void QEventDispatcherWinRT::registerSocketNotifier(QSocketNotifier *notifier)
{
    Q_ASSERT(notifier);
    int sockfd = notifier->socket();
    int type = notifier->type();
#ifndef QT_NO_DEBUG
    if (sockfd < 0
        || unsigned(sockfd) >= FD_SETSIZE) {
        qWarning("QSocketNotifier: Internal error");
        return;
    } else if (notifier->thread() != thread()
               || thread() != QThread::currentThread()) {
        qWarning("QSocketNotifier: socket notifiers cannot be enabled from another thread");
        return;
    }
#endif

    Q_D(QEventDispatcherWinRT);
    QSockNotType::List &list = d->sn_vec[type].list;
    fd_set *fds  = &d->sn_vec[type].enabled_fds;
    QSockNot *sn;

    sn = new QSockNot;
    sn->obj = notifier;
    sn->fd = sockfd;
    sn->queue = &d->sn_vec[type].pending_fds;

    int i;
    for (i = 0; i < list.size(); ++i) {
        QSockNot *p = list[i];
        if (p->fd < sockfd)
            break;
        if (p->fd == sockfd) {
            static const char *t[] = { "Read", "Write", "Exception" };
            qWarning("QSocketNotifier: Multiple socket notifiers for "
                      "same socket %d and type %s", sockfd, t[type]);
        }
    }
    list.insert(i, sn);

    FD_SET(sockfd, fds);
    d->sn_highest = qMax(d->sn_highest, sockfd);
}

void QEventDispatcherWinRT::unregisterSocketNotifier(QSocketNotifier *notifier)
{
    Q_ASSERT(notifier);
    int sockfd = notifier->socket();
    int type = notifier->type();
#ifndef QT_NO_DEBUG
    if (sockfd < 0
        || unsigned(sockfd) >= FD_SETSIZE) {
        qWarning("QSocketNotifier: Internal error");
        return;
    } else if (notifier->thread() != thread()
               || thread() != QThread::currentThread()) {
        qWarning("QSocketNotifier: socket notifiers cannot be disabled from another thread");
        return;
    }
#endif

    Q_D(QEventDispatcherWinRT);
    QSockNotType::List &list = d->sn_vec[type].list;
    fd_set *fds  =  &d->sn_vec[type].enabled_fds;
    QSockNot *sn = 0;
    int i;
    for (i = 0; i < list.size(); ++i) {
        sn = list[i];
        if (sn->obj == notifier && sn->fd == sockfd)
            break;
    }
    if (i == list.size()) // not found
        return;

    FD_CLR(sockfd, fds);                        // clear fd bit
    FD_CLR(sockfd, sn->queue);
    d->sn_pending_list.removeAll(sn);                // remove from activation list
    list.removeAt(i);                                // remove notifier found above
    delete sn;

    if (d->sn_highest == sockfd) {                // find highest fd
        d->sn_highest = -1;
        for (int i=0; i<3; i++) {
            if (!d->sn_vec[i].list.isEmpty())
                d->sn_highest = qMax(d->sn_highest,  // list is fd-sorted
                                     d->sn_vec[i].list[0]->fd);
        }
    }
}

void QEventDispatcherWinRT::setSocketNotifierPending(QSocketNotifier *notifier)
{
    Q_ASSERT(notifier);
    int sockfd = notifier->socket();
    int type = notifier->type();
#ifndef QT_NO_DEBUG
    if (sockfd < 0
        || unsigned(sockfd) >= FD_SETSIZE) {
        qWarning("QSocketNotifier: Internal error");
        return;
    }
    Q_ASSERT(notifier->thread() == thread() && thread() == QThread::currentThread());
#endif

    Q_D(QEventDispatcherWinRT);
    QSockNotType::List &list = d->sn_vec[type].list;
    QSockNot *sn = 0;
    int i;
    for (i = 0; i < list.size(); ++i) {
        sn = list[i];
        if (sn->obj == notifier && sn->fd == sockfd)
            break;
    }
    if (i == list.size()) // not found
        return;

    // We choose a random activation order to be more fair under high load.
    // If a constant order is used and a peer early in the list can
    // saturate the IO, it might grab our attention completely.
    // Also, if we're using a straight list, the callback routines may
    // delete other entries from the list before those other entries are
    // processed.
    if (! FD_ISSET(sn->fd, sn->queue)) {
        if (d->sn_pending_list.isEmpty()) {
            d->sn_pending_list.append(sn);
        } else {
            d->sn_pending_list.insert((qrand() & 0xff) %
                                      (d->sn_pending_list.size()+1), sn);
        }
        FD_SET(sn->fd, sn->queue);
    }
}

int QEventDispatcherWinRT::activateTimers()
{
    Q_ASSERT(thread() == QThread::currentThread());
    Q_D(QEventDispatcherWinRT);
    return d->timerList.activateTimers();
}

int QEventDispatcherWinRT::activateSocketNotifiers()
{
    Q_D(QEventDispatcherWinRT);
    if (d->sn_pending_list.isEmpty())
        return 0;

    // activate entries
    int n_act = 0;
    QEvent event(QEvent::SockAct);
    while (!d->sn_pending_list.isEmpty()) {
        QSockNot *sn = d->sn_pending_list.takeFirst();
        if (FD_ISSET(sn->fd, sn->queue)) {
            FD_CLR(sn->fd, sn->queue);
            QCoreApplication::sendEvent(sn->obj, &event);
            ++n_act;
        }
    }
    return n_act;
}

bool QEventDispatcherWinRT::processEvents(QEventLoop::ProcessEventsFlags flags)
{
    Q_D(QEventDispatcherWinRT);
    d->interrupt = false;

    // we are awake, broadcast it
    emit awake();
    QCoreApplicationPrivate::sendPostedEvents(0, 0, d->threadData);

    int nevents = 0;
    const bool canWait = (d->threadData->canWait
                          && !d->interrupt
                          && (flags & QEventLoop::WaitForMoreEvents));

    if (canWait)
        emit aboutToBlock();

    if (!d->interrupt) {
        // return the maximum time we can wait for an event.
        timeval *tm = 0;
        timeval wait_tm = { 0l, 0l };
        if (!(flags & QEventLoop::X11ExcludeTimers)) {
            if (d->timerList.timerWait(wait_tm))
                tm = &wait_tm;
        }

        if (!canWait) {
            if (!tm)
                tm = &wait_tm;

            // no time to wait
            tm->tv_sec  = 0l;
            tm->tv_usec = 0l;
        }

        nevents = d->doSelect(flags, tm);

        // activate timers
        if (! (flags & QEventLoop::X11ExcludeTimers)) {
            nevents += activateTimers();
        }
    }
    // return true if we handled events, false otherwise
    return (nevents > 0);
}

bool QEventDispatcherWinRT::hasPendingEvents()
{
    extern uint qGlobalPostedEventsCount(); // from qapplication.cpp
    return qGlobalPostedEventsCount();
}

int QEventDispatcherWinRT::remainingTime(int timerId)
{
#ifndef QT_NO_DEBUG
    if (timerId < 1) {
        qWarning("QEventDispatcherWinRT::remainingTime: invalid argument");
        return -1;
    }
#endif

    Q_D(QEventDispatcherWinRT);
    return d->timerList.timerRemainingTime(timerId);
}

void QEventDispatcherWinRT::wakeUp()
{
    Q_D(QEventDispatcherWinRT);
    if (d->wakeUps.testAndSetAcquire(0, 1)) {
        char c = 0;
        qint64 ret = 0;
        EINTR_LOOP(ret, send(d->thread_pipe[1], &c, 1, 0));
    }
}

void QEventDispatcherWinRT::interrupt()
{
    Q_D(QEventDispatcherWinRT);
    d->interrupt = true;
    wakeUp();
}

void QEventDispatcherWinRT::flush()
{ }




QT_END_NAMESPACE

#endif // Q_OS_WINPHONE
