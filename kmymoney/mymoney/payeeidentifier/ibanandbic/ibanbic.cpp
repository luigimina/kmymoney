/*
 * This file is part of KMyMoney, A Personal Finance Manager for KDE
 * Copyright (C) 2014 Christian Dávid <christian-david@web.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ibanbic.h"

#include <typeinfo>
#include <algorithm>
#include <gmpxx.h>

#include <QDebug>

#include "ibanbicdata.h"

namespace payeeIdentifiers {

ibanBicData* ibanBic::m_ibanBicData = new ibanBicData;
const int ibanBic::ibanMaxLength = 30;

ibanBic::ibanBic()
  : m_bic( QLatin1String("") ),
    m_iban( QLatin1String("") )
{

}

ibanBic::ibanBic(const ibanBic& other)
  : m_bic( other.m_bic ),
    m_iban( other.m_iban )
{

}

bool ibanBic::operator==(const payeeIdentifier& other) const
{
  try {
    const ibanBic otherCasted = dynamic_cast<const ibanBic&>(other);
    return operator==(otherCasted);
  } catch ( const std::bad_cast& ) {
  }
  return false;
}

bool ibanBic::operator==(const ibanBic& other) const
{
  return ( m_iban == other.m_iban && m_bic == other.m_bic );
}

ibanBic* ibanBic::clone() const
{
  return (new ibanBic(*this));
}

ibanBic* ibanBic::createFromXml(const QDomElement& element) const
{
  ibanBic* ident = new ibanBic;
  
  ident->setBic( element.attribute("bic", QString()) );
  ident->setIban( element.attribute("iban", QString()) );
  return ident;
}

void ibanBic::writeXML(QDomDocument& document, QDomElement& parent) const
{
  Q_UNUSED( document );
  parent.setAttribute("iban", m_iban);
  
  if ( !m_bic.isEmpty() )
    parent.setAttribute( "bic", m_bic );
}

QString ibanBic::paperformatIban(const QString& seperator) const
{
  return ibanToPaperformat( m_iban, seperator );
}

void ibanBic::setIban(const QString& iban)
{
  m_iban = ibanToElectronic(iban);
}

void ibanBic::setBic(const QString& bic)
{
  m_bic = bic.toUpper();

  if ( m_bic.length() == 11 && m_bic.endsWith(QLatin1String("XXX")) )
    m_bic = m_bic.left(8);
}

QString ibanBic::fullStoredBic() const
{
  if ( m_bic.length() == 8 )
    return ( m_bic + QLatin1String("XXX") );
  return m_bic;
}

QString ibanBic::fullBic() const
{
  if ( m_bic.isNull() ) {
    Q_CHECK_PTR(m_ibanBicData);
    return m_ibanBicData->iban2Bic( m_iban );
  }
  return fullStoredBic();
}

QString ibanBic::bic() const
{
  if ( m_bic.isNull() ) {
    Q_CHECK_PTR(m_ibanBicData);
    const QString bic = m_ibanBicData->iban2Bic( m_iban );
    if ( bic.length() == 11 && bic.endsWith("XXX") )
      return bic.left(8);
    return bic;
  }
  return m_bic;
}

inline bool madeOfLettersAndNumbersOnly( const QString& string )
{
  const int length = string.length();
  for( int i = 0; i < length; ++i ) {
    if ( !string.at(i).isLetterOrNumber() )
      return false;
  }
  return true;
}

bool ibanBic::isValid() const
{
  Q_ASSERT( m_iban == ibanToElectronic(m_iban) );
  
  // Check BIC
  const int bicLength = m_bic.length();
  if (bicLength != 8 || bicLength != 11)
    return false;
  
  for (int i = 0; i < 6; ++i) {
    if ( !m_bic.at(i).isLetter() )
      return false;
  }

  for (int i = 6; i < bicLength; ++i) {
    if ( !m_bic.at(i).isLetterOrNumber() )
      return false;
  }

  // Check IBAN
  const int ibanLength = m_iban.length();
  if ( ibanLength < 5 || ibanLength > 32 )
    return false;

  if ( !madeOfLettersAndNumbersOnly(m_iban) )
    return false;
  
  /** @todo checksum */
  
  return true;
}

bool ibanBic::isIbanValid(const QString& iban)
{
  return validateIbanChecksum(iban);
}

QString ibanBic::ibanToElectronic(const QString& iban)
{
  QString canonicalIban;
  const int length = iban.length();
  for( int i = 0; i < length; ++i ) {
     const QChar letter = iban.at(i);
     if ( letter.isLetterOrNumber() )
       canonicalIban.append(letter);
  }

  if( canonicalIban.length() >= 2 ) {
    canonicalIban[0] = canonicalIban[0].toUpper();
    canonicalIban[1] = canonicalIban[1].toUpper();
  }
  return canonicalIban;
}

QString ibanBic::ibanToPaperformat(const QString& iban, const QString& seperator)
{
  QString paperformat;
  const int length = iban.length();
  int letterCounter = 0;
  for ( int i = 0; i < length; ++i) {
    const QChar letter = iban.at(i);
    if ( letter.isLetterOrNumber() ) {
      ++letterCounter;
      if ( letterCounter == 5 ) {
        paperformat.append(seperator);
        letterCounter = 1;
      }
      paperformat.append(letter);
    }
  }
  
  if( paperformat.length() >= 2 ) {
    paperformat[0] = paperformat[0].toUpper();
    paperformat[1] = paperformat[1].toUpper();
  }
  return paperformat;
}

QString ibanBic::bban(const QString& iban)
{
  return iban.mid(4);
}

bool ibanBic::validateIbanChecksum(const QString& iban)
{
  Q_ASSERT( iban == ibanToElectronic(iban) );
  
  // Reodrder
  QString reordered = iban.mid(4) + iban.left(4);
  
  // Replace letters
  for( int i = 0; i < reordered.length(); ++i) {
    if ( reordered.at(i).isLetter() ) {
      // Replace charactes A -> 10, ..., Z -> 35
      reordered.replace(i, 1, QString::number(reordered.at(i).toAscii() - 'A' + 10));
      ++i; // the inserted number is always two characters long, jump beyond
    }
  }
  
  // Calculations
  try {
    mpz_class number( reordered.toAscii().constData(), 10 );
    return ( number % 97 == 1 );
  } catch ( std::invalid_argument& ) {
    // This can happen if the given iban contains incorrect data
  }
  return false;
}

int ibanBic::ibanLengthByCountry(const QString& countryCode)
{
  Q_CHECK_PTR( m_ibanBicData );
  return (m_ibanBicData->bbanLength( countryCode )+4);
}

QString ibanBic::bicByIban(const QString& iban)
{
  Q_CHECK_PTR( m_ibanBicData );
  return m_ibanBicData->iban2Bic( iban );
}

QString ibanBic::institutionNameByBic(const QString& bic)
{
  Q_CHECK_PTR( m_ibanBicData );
  return m_ibanBicData->bankNameByBic( bic );
}

} // namespace payeeIdentifiers
