/****************************************************************************
**
** Copyright (C) 2012 author Laszlo Papp <lpapp@kde.org>
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QCOMMANDLINEOPTION_H
#define QCOMMANDLINEOPTION_H

#include "qstringlist.h"

QT_BEGIN_HEADER

QT_BEGIN_NAMESPACE


class QCommandLineOptionPrivate;

class Q_CORE_EXPORT QCommandLineOption
{
    public:
    enum OptionType {
        NoValue,
        OneValue,
        MultipleValues
    };

    QCommandLineOption();
    explicit QCommandLineOption(const QStringList& names, const QString& description = QString(),
                                OptionType optionType = NoValue, bool required = false,
                                const QStringList& defaultValues = QStringList());
    QCommandLineOption(const QCommandLineOption& other);
    QCommandLineOption& operator=(const QCommandLineOption& other);

    ~QCommandLineOption();

    bool operator==(const QCommandLineOption &other) const;

    void setNames(const QStringList& names);
    QStringList names() const;

    void setOptionType(OptionType optionType);
    OptionType optionType() const;

    void setRequired(bool required);
    bool required() const;

    void setDescription(const QString& description);
    QString description() const;

    void setDefaultValues(const QStringList& defaultValues);
    QStringList defaultValues() const;

    private:
        QCommandLineOptionPrivate* d;
};

QT_END_NAMESPACE

QT_END_HEADER

#endif // QCOMMANDLINEOPTION_H
