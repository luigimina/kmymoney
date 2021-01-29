/*
 * Copyright 2021      Dawid Wróbel <me@dawidwrobel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MYQURL_H
#define MYQURL_H

#undef QUrl

// ----------------------------------------------------------------------------
// QT Includes

#include <QString>
#include <QUrl>


class KMMUrl: public QUrl
{
public:
    KMMUrl();
    KMMUrl(const KMMUrl &copy);
    KMMUrl(const QUrl &copy);

    static KMMUrl fromUserInput(const QString &userInput);
    QString toLocalFile() const;

private:
    static QString normalizeUrlString(const QString &url);

};

#define QUrl KMMUrl
#endif // MYQURL_H
