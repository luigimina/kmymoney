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


// ----------------------------------------------------------------------------
// QT Includes

#include <QRegularExpression>

// ----------------------------------------------------------------------------
// Project Includes

#include "kmmurl.h"

#undef QUrl

KMMUrl::KMMUrl()
    : QUrl()
{}

KMMUrl::KMMUrl(const KMMUrl &copy)
    : QUrl(copy)
{}

KMMUrl::KMMUrl(const QUrl &copy)
    : QUrl(copy)
{}

KMMUrl KMMUrl::fromUserInput(const QString &userInput)
{
    auto properUrl = normalizeUrlString(userInput);

    if (userInput.at(0) == ':') {
        auto properQUrl = QUrl::fromLocalFile(properUrl.remove(0, 1));
        properQUrl.setScheme(QStringLiteral("qrc"));

        return properQUrl;
    }
    else {
        return QUrl::fromUserInput(properUrl);
    }
}

QString KMMUrl::toLocalFile() const
{
    if (this->scheme() == QStringLiteral("qrc") || this->url().startsWith(QStringLiteral("qrc"))) {
        return normalizeUrlString(this->url());
    }
    else {
        return QUrl::toLocalFile();
    }
}

QString KMMUrl::normalizeUrlString(const QString &url)
{
    return QString(url).remove(QRegularExpression("^(file:|qrc)", QRegularExpression::CaseInsensitiveOption));
}

