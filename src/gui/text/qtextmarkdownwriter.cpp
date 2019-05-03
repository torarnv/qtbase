/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qtextmarkdownwriter_p.h"
#include "qtextdocumentlayout_p.h"
#include "qfontinfo.h"
#include "qfontmetrics.h"
#include "qtextdocument_p.h"
#include "qtextlist.h"
#include "qtexttable.h"
#include "qtextcursor.h"
#include "qtextimagehandler_p.h"

QT_BEGIN_NAMESPACE

static const QChar Space = QLatin1Char(' ');
static const QChar Newline = QLatin1Char('\n');
static const QChar Backtick = QLatin1Char('`');

QTextMarkdownWriter::QTextMarkdownWriter(QTextStream &stream, QTextDocument::MarkdownFeatures features)
  : m_stream(stream), m_features(features)
{
}

bool QTextMarkdownWriter::writeAll(const QTextDocument &document)
{
    writeFrame(document.rootFrame());
    return true;
}

void QTextMarkdownWriter::writeTable(const QAbstractTableModel &table)
{
    QVector<int> tableColumnWidths(table.columnCount());
    for (int col = 0; col < table.columnCount(); ++col) {
        tableColumnWidths[col] = table.headerData(col, Qt::Horizontal).toString().length();
        for (int row = 0; row < table.rowCount(); ++row) {
            tableColumnWidths[col] = qMax(tableColumnWidths[col],
                table.data(table.index(row, col)).toString().length());
        }
    }

    // write the header and separator
    for (int col = 0; col < table.columnCount(); ++col) {
        QString s = table.headerData(col, Qt::Horizontal).toString();
        m_stream << "|" << s << QString(tableColumnWidths[col] - s.length(), Space);
    }
    m_stream << "|" << Qt::endl;
    for (int col = 0; col < tableColumnWidths.length(); ++col)
        m_stream << '|' << QString(tableColumnWidths[col], QLatin1Char('-'));
    m_stream << '|'<< Qt::endl;

    // write the body
    for (int row = 0; row < table.rowCount(); ++row) {
        for (int col = 0; col < table.columnCount(); ++col) {
            QString s = table.data(table.index(row, col)).toString();
            m_stream << "|" << s << QString(tableColumnWidths[col] - s.length(), Space);
        }
        m_stream << '|'<< Qt::endl;
    }
}

void QTextMarkdownWriter::writeFrame(const QTextFrame *frame)
{
    Q_ASSERT(frame);
    const QTextTable *table = qobject_cast<const QTextTable*> (frame);
    QTextFrame::iterator iterator = frame->begin();
    QTextFrame *child = 0;
    int tableRow = -1;
    bool lastWasList = false;
    QVector<int> tableColumnWidths;
    if (table) {
        tableColumnWidths.resize(table->columns());
        for (int col = 0; col < table->columns(); ++col) {
            for (int row = 0; row < table->rows(); ++ row) {
                QTextTableCell cell = table->cellAt(row, col);
                int cellTextLen = 0;
                auto it = cell.begin();
                while (it != cell.end()) {
                    QTextBlock block = it.currentBlock();
                    if (block.isValid())
                        cellTextLen += block.text().length();
                    ++it;
                }
                if (cell.columnSpan() == 1 && tableColumnWidths[col] < cellTextLen)
                    tableColumnWidths[col] = cellTextLen;
            }
        }
    }
    while (!iterator.atEnd()) {
        if (iterator.currentFrame() && child != iterator.currentFrame())
            writeFrame(iterator.currentFrame());
        else { // no frame, it's a block
            QTextBlock block = iterator.currentBlock();
            if (table) {
                QTextTableCell cell = table->cellAt(block.position());
                if (tableRow < cell.row()) {
                    if (tableRow == 0) {
                        m_stream << Newline;
                        for (int col = 0; col < tableColumnWidths.length(); ++col)
                            m_stream << '|' << QString(tableColumnWidths[col], QLatin1Char('-'));
                        m_stream << '|';
                    }
                    m_stream << Newline << "|";
                    tableRow = cell.row();
                }
            } else if (!block.textList()) {
                if (lastWasList)
                    m_stream << Newline;
            }
            int endingCol = writeBlock(block, !table, table && tableRow == 0);
            if (table) {
                QTextTableCell cell = table->cellAt(block.position());
                int paddingLen = -endingCol;
                int spanEndCol = cell.column() + cell.columnSpan();
                for (int col = cell.column(); col < spanEndCol; ++col)
                    paddingLen += tableColumnWidths[col];
                if (paddingLen > 0)
                    m_stream << QString(paddingLen, Space);
                for (int col = cell.column(); col < spanEndCol; ++col)
                    m_stream << "|";
            } else if (block.textList()) {
                m_stream << Newline;
            } else if (endingCol > 0) {
                m_stream << Newline << Newline;
            }
            lastWasList = block.textList();
        }
        child = iterator.currentFrame();
        ++iterator;
    }
    if (table)
        m_stream << Newline << Newline;
}

static int nearestWordWrapIndex(const QString &s, int before)
{
    before = qMin(before, s.length());
    for (int i = before - 1; i >= 0; --i) {
        if (s.at(i).isSpace())
            return i;
    }
    return -1;
}

static int adjacentBackticksCount(const QString &s)
{
    int start = -1, len = s.length();
    int ret = 0;
    for (int i = 0; i < len; ++i) {
        if (s.at(i) == Backtick) {
            if (start < 0)
                start = i;
        } else if (start >= 0) {
            ret = qMax(ret, i - start);
            start = -1;
        }
    }
    if (s.at(len - 1) == Backtick)
        ret = qMax(ret, len - start);
    return ret;
}

static void maybeEscapeFirstChar(QString &s)
{
    QString sTrimmed = s.trimmed();
    if (sTrimmed.isEmpty())
        return;
    char firstChar = sTrimmed.at(0).toLatin1();
    if (firstChar == '*' || firstChar == '+' || firstChar == '-') {
        int i = s.indexOf(QLatin1Char(firstChar));
        s.insert(i, QLatin1Char('\\'));
    }
}

int QTextMarkdownWriter::writeBlock(const QTextBlock &block, bool wrap, bool ignoreFormat)
{
    int ColumnLimit = 80;
    int wrapIndent = 0;
    if (block.textList()) { // it's a list-item
        auto fmt = block.textList()->format();
        const int listLevel = fmt.indent();
        const int number = block.textList()->itemNumber(block) + 1;
        QByteArray bullet = " ";
        bool numeric = false;
        switch (fmt.style()) {
        case QTextListFormat::ListDisc: bullet = "-"; break;
        case QTextListFormat::ListCircle: bullet = "*"; break;
        case QTextListFormat::ListSquare: bullet = "+"; break;
        case QTextListFormat::ListStyleUndefined: break;
        case QTextListFormat::ListDecimal:
        case QTextListFormat::ListLowerAlpha:
        case QTextListFormat::ListUpperAlpha:
        case QTextListFormat::ListLowerRoman:
        case QTextListFormat::ListUpperRoman:
            numeric = true;
            break;
        }
        switch (block.blockFormat().marker()) {
        case QTextBlockFormat::Checked:
            bullet += " [x]";
            break;
        case QTextBlockFormat::Unchecked:
            bullet += " [ ]";
            break;
        default:
            break;
        }
        QString prefix((listLevel - 1) * (numeric ? 4 : 2), Space);
        if (numeric)
            prefix += QString::number(number) + fmt.numberSuffix() + Space;
        else
            prefix += QLatin1String(bullet) + Space;
        m_stream << prefix;
        wrapIndent = prefix.length();
    }

    if (block.blockFormat().headingLevel())
        m_stream << QByteArray(block.blockFormat().headingLevel(), '#') << ' ';

    QString wrapIndentString(wrapIndent, Space);
    // It would be convenient if QTextStream had a lineCharPos() accessor,
    // to keep track of how many characters (not bytes) have been written on the current line,
    // but it doesn't.  So we have to keep track with this col variable.
    int col = wrapIndent;
    bool mono = false;
    bool startsOrEndsWithBacktick = false;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikeOut = false;
    QString backticks(Backtick);
    for (QTextBlock::Iterator frag = block.begin(); !frag.atEnd(); ++frag) {
        QString fragmentText = frag.fragment().text();
        while (fragmentText.endsWith(QLatin1Char('\n')))
            fragmentText.chop(1);
        startsOrEndsWithBacktick |= fragmentText.startsWith(Backtick) || fragmentText.endsWith(Backtick);
        QTextCharFormat fmt = frag.fragment().charFormat();
        if (fmt.isImageFormat()) {
            QTextImageFormat ifmt = fmt.toImageFormat();
            QString s = QLatin1String("![image](") + ifmt.name() + QLatin1Char(')');
            if (wrap && col + s.length() > ColumnLimit) {
                m_stream << Newline << wrapIndentString;
                col = wrapIndent;
            }
            m_stream << s;
            col += s.length();
        } else if (fmt.hasProperty(QTextFormat::AnchorHref)) {
            QString s = QLatin1Char('[') + fragmentText + QLatin1String("](") +
                    fmt.property(QTextFormat::AnchorHref).toString() + QLatin1Char(')');
            if (wrap && col + s.length() > ColumnLimit) {
                m_stream << Newline << wrapIndentString;
                col = wrapIndent;
            }
            m_stream << s;
            col += s.length();
        } else {
            QFontInfo fontInfo(fmt.font());
            bool monoFrag = fontInfo.fixedPitch();
            QString markers;
            if (!ignoreFormat) {
                if (monoFrag != mono) {
                    if (monoFrag)
                        backticks = QString::fromLatin1(QByteArray(adjacentBackticksCount(fragmentText) + 1, '`'));
                    markers += backticks;
                    if (startsOrEndsWithBacktick)
                        markers += Space;
                    mono = monoFrag;
                }
                if (!block.blockFormat().headingLevel() && !mono) {
                    if (fmt.font().bold() != bold) {
                        markers += QLatin1String("**");
                        bold = fmt.font().bold();
                    }
                    if (fmt.font().italic() != italic) {
                        markers += QLatin1Char('*');
                        italic = fmt.font().italic();
                    }
                    if (fmt.font().strikeOut() != strikeOut) {
                        markers += QLatin1String("~~");
                        strikeOut = fmt.font().strikeOut();
                    }
                    if (fmt.font().underline() != underline) {
                        // Markdown doesn't support underline, but the parser will treat a single underline
                        // the same as a single asterisk, and the marked fragment will be rendered in italics.
                        // That will have to do.
                        markers += QLatin1Char('_');
                        underline = fmt.font().underline();
                    }
                }
            }
            if (wrap && col + markers.length() * 2 + fragmentText.length() > ColumnLimit) {
                int i = 0;
                int fragLen = fragmentText.length();
                bool breakingLine = false;
                while (i < fragLen) {
                    int j = i + ColumnLimit - col;
                    if (j < fragLen) {
                        int wi = nearestWordWrapIndex(fragmentText, j);
                        if (wi < 0) {
                            j = fragLen;
                        } else {
                            j = wi;
                            breakingLine = true;
                        }
                    } else {
                        j = fragLen;
                        breakingLine = false;
                    }
                    QString subfrag = fragmentText.mid(i, j - i);
                    if (!i) {
                        m_stream << markers;
                        col += markers.length();
                    }
                    if (col == wrapIndent)
                        maybeEscapeFirstChar(subfrag);
                    m_stream << subfrag;
                    if (breakingLine) {
                        m_stream << Newline << wrapIndentString;
                        col = wrapIndent;
                    } else {
                        col += subfrag.length();
                    }
                    i = j + 1;
                }
            } else {
                m_stream << markers << fragmentText;
                col += markers.length() + fragmentText.length();
            }
        }
    }
    if (mono) {
        if (startsOrEndsWithBacktick) {
            m_stream << Space;
            col += 1;
        }
        m_stream << backticks;
        col += backticks.size();
    }
    if (bold) {
        m_stream << "**";
        col += 2;
    }
    if (italic) {
        m_stream << "*";
        col += 1;
    }
    if (underline) {
        m_stream << "_";
        col += 1;
    }
    if (strikeOut) {
        m_stream << "~~";
        col += 2;
    }
    return col;
}

QT_END_NAMESPACE