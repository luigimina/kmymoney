/***************************************************************************
                          ofximporterplugin.cpp
                             -------------------
    begin                : Sat Jan 01 2005
    copyright            : (C) 2005 by Ace Jones
    email                : Ace Jones <acejones@users.sourceforge.net>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ofximporterplugin.h"

// ----------------------------------------------------------------------------
// QT Includes

#include <QFile>
#include <QTextStream>
#include <QRadioButton>
#include <QSpinBox>
#include <QByteArray>

// ----------------------------------------------------------------------------
// KDE Includes

#include <kgenericfactory.h>
#include <kdebug.h>
#include <kfile.h>
#include <kurl.h>
#include <kaction.h>
#include <kmessagebox.h>
#include <kactioncollection.h>
#include <KWallet/Wallet>

// ----------------------------------------------------------------------------
// Project Includes

#include "konlinebankingstatus.h"
#include "konlinebankingsetupwizard.h"
#include "kofxdirectconnectdlg.h"

K_EXPORT_COMPONENT_FACTORY(kmm_ofximport,
                           KGenericFactory<OfxImporterPlugin>("kmm_ofximport"))

using KWallet::Wallet;

class OfxImporterPlugin::Private
{
public:
  Private() : m_valid(false), m_preferName(false), m_walletIsOpen(false), m_statusDlg(0), m_wallet(0) {}

  bool m_valid;
  bool m_preferName;
  bool m_walletIsOpen;
  QList<MyMoneyStatement> m_statementlist;
  QList<MyMoneyStatement::Security> m_securitylist;
  QString m_fatalerror;
  QStringList m_infos;
  QStringList m_warnings;
  QStringList m_errors;
  KOnlineBankingStatus* m_statusDlg;
  Wallet  *m_wallet;
};




OfxImporterPlugin::OfxImporterPlugin(QObject *parent, const QStringList&) :
    KMyMoneyPlugin::Plugin(parent, "KMyMoney OFX"),
    /*
     * the string in the line above must be the same as
     * X-KDE-PluginInfo-Name and the provider name assigned in
     * OfxImporterPlugin::onlineBankingSettings()
     */
    KMyMoneyPlugin::ImporterPlugin(),
    d(new Private)
{
  setComponentData(KGenericFactory<OfxImporterPlugin>::componentData());
  setXMLFile("kmm_ofximport.rc");
  createActions();

  // For ease announce that we have been loaded.
  qDebug("KMyMoney ofximport plugin loaded");
}

OfxImporterPlugin::~OfxImporterPlugin()
{
  delete d;
}

void OfxImporterPlugin::createActions(void)
{
  KAction *action = actionCollection()->addAction("file_import_ofx");
  action->setText(i18n("OFX..."));
  connect(action, SIGNAL(triggered(bool)), this, SLOT(slotImportFile()));
}

void OfxImporterPlugin::slotImportFile(void)
{
  KUrl url = importInterface()->selectFile(i18n("OFX import file selection"),
             "",
             "*.ofx *.qfx *.ofc|OFX files (*.ofx, *.qfx, *.ofc)\n*.*|All files (*.*)",
             static_cast<KFile::Mode>((int)(KFile::File | KFile::ExistingOnly)));
  if (url.isValid()) {
    if (isMyFormat(url.path())) {
      slotImportFile(url.path());
    } else {
      KMessageBox::error(0, i18n("Unable to import %1 using the OFX importer plugin.  This file is not the correct format.", url.prettyUrl()), i18n("Incorrect format"));
    }

  }
}

QString OfxImporterPlugin::formatName(void) const
{
  return "OFX";
}

QString OfxImporterPlugin::formatFilenameFilter(void) const
{
  return "*.ofx *.qfx *.ofc";
}


bool OfxImporterPlugin::isMyFormat(const QString& filename) const
{
  // filename is considered an Ofx file if it contains
  // the tag "<OFX>" or "<OFC>" in the first 20 lines.
  bool result = false;

  QFile f(filename);
  if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream ts(&f);

    int lineCount = 20;
    while (!ts.atEnd() && !result  && lineCount != 0) {
      QString line = ts.readLine();
      if (line.contains("<OFX>", Qt::CaseInsensitive)
          || line.contains("<OFC>", Qt::CaseInsensitive))
        result = true;
      lineCount--;
    }
    f.close();
  }

  return result;
}

bool OfxImporterPlugin::import(const QString& filename)
{
  d->m_fatalerror = i18n("Unable to parse file");
  d->m_valid = false;
  d->m_errors.clear();
  d->m_warnings.clear();
  d->m_infos.clear();

  d->m_statementlist.clear();
  d->m_securitylist.clear();

  QByteArray filename_deep(filename.toUtf8());

  LibofxContextPtr ctx = libofx_get_new_context();
  Q_CHECK_PTR(ctx);

  ofx_set_transaction_cb(ctx, ofxTransactionCallback, this);
  ofx_set_statement_cb(ctx, ofxStatementCallback, this);
  ofx_set_account_cb(ctx, ofxAccountCallback, this);
  ofx_set_security_cb(ctx, ofxSecurityCallback, this);
  ofx_set_status_cb(ctx, ofxStatusCallback, this);
  libofx_proc_file(ctx, filename_deep, AUTODETECT);
  libofx_free_context(ctx);

  if (d->m_valid) {
    d->m_fatalerror.clear();
    d->m_valid = storeStatements(d->m_statementlist);
  }
  return d->m_valid;
}

QString OfxImporterPlugin::lastError(void) const
{
  if (d->m_errors.count() == 0)
    return d->m_fatalerror;
  return d->m_errors.join("<p>");
}

/* __________________________________________________________________________
 * AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
 *
 * Static callbacks for LibOFX
 *
 * YYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYYY
 */

int OfxImporterPlugin::ofxTransactionCallback(struct OfxTransactionData data, void * pv)
{
//   kDebug(2) << Q_FUNC_INFO;

  OfxImporterPlugin* pofx = reinterpret_cast<OfxImporterPlugin*>(pv);
  MyMoneyStatement& s = pofx->back();

  MyMoneyStatement::Transaction t;

  if (data.date_posted_valid == true) {
    QDateTime dt;
    dt.setTime_t(data.date_posted);
    t.m_datePosted = dt.date();
  } else if (data.date_initiated_valid == true) {
    QDateTime dt;
    dt.setTime_t(data.date_initiated);
    t.m_datePosted = dt.date();
  }

  if (data.amount_valid == true) {
    t.m_amount = MyMoneyMoney(data.amount, 1000);
    // if this is an investment statement, reverse the sign.  not sure
    // why this is needed, so I suppose it's a bit of a hack for the moment.
    if (data.invtransactiontype_valid == true)
      t.m_amount = -t.m_amount;
  }

  if (data.check_number_valid == true) {
    t.m_strNumber = data.check_number;
  }

  if (data.fi_id_valid == true) {
    t.m_strBankID = QString("ID ") + data.fi_id;
  } else if (data.reference_number_valid == true) {
    t.m_strBankID = QString("REF ") + data.reference_number;
  }
  // Decide whether to import NAME or PAYEEID if both are present in the download
  if (pofx->d->m_preferName) {
    if (data.name_valid == true) {
      t.m_strPayee = data.name;
    } else if (data.payee_id_valid == true) {
      t.m_strPayee = data.payee_id;
    }
  } else {
    if (data.payee_id_valid == true) {
      t.m_strPayee = data.payee_id;
    } else if (data.name_valid == true) {
      t.m_strPayee = data.name;
    }
  }
  if (data.memo_valid == true) {
    t.m_strMemo = data.memo;
  }

  // If the payee or memo fields are blank, set them to
  // the other one which is NOT blank.  (acejones)
  if (t.m_strPayee.isEmpty()) {
    // But we only create a payee for non-investment transactions (ipwizard)
    if (! t.m_strMemo.isEmpty() && data.invtransactiontype_valid == false)
      t.m_strPayee = t.m_strMemo;
  } else {
    if (t.m_strMemo.isEmpty())
      t.m_strMemo = t.m_strPayee;
  }

  if (data.security_data_valid == true) {
    struct OfxSecurityData* secdata = data.security_data_ptr;

    if (secdata->ticker_valid == true) {
      t.m_strSymbol = secdata->ticker;
    }

    if (secdata->secname_valid == true) {
      t.m_strSecurity = secdata->secname;
    }
  }

  t.m_shares = MyMoneyMoney();
  if (data.units_valid == true) {
    t.m_shares = MyMoneyMoney(data.units, 100000).reduce();
  }

  t.m_price = MyMoneyMoney();
  if (data.unitprice_valid == true) {
    t.m_price = MyMoneyMoney(data.unitprice, 100000).reduce();
  }

  t.m_fees = MyMoneyMoney();
  if (data.fees_valid == true) {
    t.m_fees += MyMoneyMoney(data.fees, 1000).reduce();
  }

  if (data.commission_valid == true) {
    t.m_fees += MyMoneyMoney(data.commission, 1000).reduce();
  }

  bool unhandledtype = false;
  QString type;

  if (data.invtransactiontype_valid == true) {
    switch (data.invtransactiontype) {
      case OFX_BUYDEBT:
      case OFX_BUYMF:
      case OFX_BUYOPT:
      case OFX_BUYOTHER:
      case OFX_BUYSTOCK:
        t.m_eAction = MyMoneyStatement::Transaction::eaBuy;
        break;
      case OFX_REINVEST:
        t.m_eAction = MyMoneyStatement::Transaction::eaReinvestDividend;
        break;
      case OFX_SELLDEBT:
      case OFX_SELLMF:
      case OFX_SELLOPT:
      case OFX_SELLOTHER:
      case OFX_SELLSTOCK:
        t.m_eAction = MyMoneyStatement::Transaction::eaSell;
        break;
      case OFX_INCOME:
        t.m_eAction = MyMoneyStatement::Transaction::eaCashDividend;
        // NOTE: With CashDividend, the amount of the dividend should
        // be in data.amount.  Since I've never seen an OFX file with
        // cash dividends, this is an assumption on my part. (acejones)
        break;

        //
        // These types are all not handled.  We will generate a warning for them.
        //
      case OFX_CLOSUREOPT:
        unhandledtype = true;
        type = "CLOSUREOPT (Close a position for an option)";
        break;
      case OFX_INVEXPENSE:
        unhandledtype = true;
        type = "INVEXPENSE (Misc investment expense that is associated with a specific security)";
        break;
      case OFX_JRNLFUND:
        unhandledtype = true;
        type = "JRNLFUND (Journaling cash holdings between subaccounts within the same investment account)";
        break;
      case OFX_MARGININTEREST:
        unhandledtype = true;
        type = "MARGININTEREST (Margin interest expense)";
        break;
      case OFX_RETOFCAP:
        unhandledtype = true;
        type = "RETOFCAP (Return of capital)";
        break;
      case OFX_SPLIT:
        unhandledtype = true;
        type = "SPLIT (Stock or mutial fund split)";
        break;
      case OFX_TRANSFER:
        unhandledtype = true;
        type = "TRANSFER (Transfer holdings in and out of the investment account)";
        break;
      default:
        unhandledtype = true;
        type = QString("UNKNOWN %1").arg(data.invtransactiontype);
        break;
    }
  } else
    t.m_eAction = MyMoneyStatement::Transaction::eaNone;

  // In the case of investment transactions, the 'total' is supposed to the total amount
  // of the transaction.  units * unitprice +/- commission.  Easy, right?  Sadly, it seems
  // some ofx creators do not follow this in all circumstances.  Therefore, we have to double-
  // check the total here and adjust it if it's wrong.

#if 0
  // Even more sadly, this logic is BROKEN.  It consistently results in bogus total
  // values, because of rounding errors in the price.  A more through solution would
  // be to test if the comission alone is causing a discrepency, and adjust in that case.

  if (data.invtransactiontype_valid == true && data.unitprice_valid) {
    double proper_total = t.m_dShares * data.unitprice + t.m_moneyFees;
    if (proper_total != t.m_moneyAmount) {
      pofx->addWarning(QString("Transaction %1 has an incorrect total of %2. Using calculated total of %3 instead.").arg(t.m_strBankID).arg(t.m_moneyAmount).arg(proper_total));
      t.m_moneyAmount = proper_total;
    }
  }
#endif

  if (unhandledtype)
    pofx->addWarning(QString("Transaction %1 has an unsupported type (%2).").arg(t.m_strBankID, type));
  else
    s.m_listTransactions += t;

//   kDebug(2) << Q_FUNC_INFO << "return 0 ";

  return 0;
}

int OfxImporterPlugin::ofxStatementCallback(struct OfxStatementData data, void* pv)
{
//   kDebug(2) << Q_FUNC_INFO;

  OfxImporterPlugin* pofx = reinterpret_cast<OfxImporterPlugin*>(pv);
  MyMoneyStatement& s = pofx->back();

  pofx->setValid();

  if (data.currency_valid == true) {
    s.m_strCurrency = data.currency;
  }
  if (data.account_id_valid == true) {
    s.m_strAccountNumber = data.account_id;
  }

  if (data.date_start_valid == true) {
    QDateTime dt;
    dt.setTime_t(data.date_start);
    s.m_dateBegin = dt.date();
  }

  if (data.date_end_valid == true) {
    QDateTime dt;
    dt.setTime_t(data.date_end);
    s.m_dateEnd = dt.date();
  }

  if (data.ledger_balance_valid == true) {
    s.m_closingBalance = MyMoneyMoney(data.ledger_balance);
  }

//   kDebug(2) << Q_FUNC_INFO << " return 0";

  return 0;
}

int OfxImporterPlugin::ofxAccountCallback(struct OfxAccountData data, void * pv)
{
//   kDebug(2) << Q_FUNC_INFO;

  OfxImporterPlugin* pofx = reinterpret_cast<OfxImporterPlugin*>(pv);
  pofx->addnew();
  MyMoneyStatement& s = pofx->back();

  // Having any account at all makes an ofx statement valid
  pofx->d->m_valid = true;

  if (data.account_id_valid == true) {
    s.m_strAccountName = data.account_name;
    s.m_strAccountNumber = data.account_id;
  }
  if (data.bank_id_valid == true) {
    s.m_strRoutingNumber = data.bank_id;
  }
  if (data.broker_id_valid == true) {
    s.m_strRoutingNumber = data.broker_id;
  }
  if (data.currency_valid == true) {
    s.m_strCurrency = data.currency;
  }

  if (data.account_type_valid == true) {
    switch (data.account_type) {
      case OfxAccountData::OFX_CHECKING : s.m_eType = MyMoneyStatement::etCheckings;
        break;
      case OfxAccountData::OFX_SAVINGS : s.m_eType = MyMoneyStatement::etSavings;
        break;
      case OfxAccountData::OFX_MONEYMRKT : s.m_eType = MyMoneyStatement::etInvestment;
        break;
      case OfxAccountData::OFX_CREDITLINE : s.m_eType = MyMoneyStatement::etCreditCard;
        break;
      case OfxAccountData::OFX_CMA : s.m_eType = MyMoneyStatement::etCreditCard;
        break;
      case OfxAccountData::OFX_CREDITCARD : s.m_eType = MyMoneyStatement::etCreditCard;
        break;
      case OfxAccountData::OFX_INVESTMENT : s.m_eType = MyMoneyStatement::etInvestment;
        break;
    }
  }

  // ask KMyMoney for an account id
  s.m_accountId = pofx->account("kmmofx-acc-ref", QString("%1-%2").arg(s.m_strRoutingNumber, s.m_strAccountNumber)).id();

  // copy over the securities
  s.m_listSecurities = pofx->d->m_securitylist;

//   kDebug(2) << Q_FUNC_INFO << " return 0";

  return 0;
}

int OfxImporterPlugin::ofxSecurityCallback(struct OfxSecurityData data, void* pv)
{
  //   kDebug(2) << Q_FUNC_INFO;

  OfxImporterPlugin* pofx = reinterpret_cast<OfxImporterPlugin*>(pv);
  MyMoneyStatement::Security sec;

  if (data.unique_id_valid == true) {
    sec.m_strId = data.unique_id;
  }
  if (data.secname_valid == true) {
    sec.m_strName = data.secname;
  }
  if (data.ticker_valid == true) {
    sec.m_strSymbol = data.ticker;
  }

  pofx->d->m_securitylist += sec;

  return 0;
}

int OfxImporterPlugin::ofxStatusCallback(struct OfxStatusData data, void * pv)
{
//   kDebug(2) << Q_FUNC_INFO;

  OfxImporterPlugin* pofx = reinterpret_cast<OfxImporterPlugin*>(pv);
  QString message;

  // if we got this far, we know we were able to parse the file.
  // so if it fails after here it can only because there were no actual
  // accounts in the file!
  pofx->d->m_fatalerror = "No accounts found.";

  if (data.ofx_element_name_valid == true)
    message.prepend(QString("%1: ").arg(data.ofx_element_name));

  if (data.code_valid == true)
    message += QString("%1 (Code %2): %3").arg(data.name).arg(data.code).arg(data.description);

  if (data.server_message_valid == true)
    message += QString(" (%1)").arg(data.server_message);

  if (data.severity_valid == true) {
    switch (data.severity) {
      case OfxStatusData::INFO:
        pofx->addInfo(message);
        break;
      case OfxStatusData::ERROR:
        pofx->addError(message);
        break;
      case OfxStatusData::WARN:
        pofx->addWarning(message);
        break;
      default:
        pofx->addWarning(message);
        pofx->addWarning("Previous message was an unknown type.  'WARNING' was assumed.");
        break;
    }
  }

//   kDebug(2) << Q_FUNC_INFO << " return 0 ";

  return 0;
}

bool OfxImporterPlugin::importStatement(const MyMoneyStatement& s)
{
  qDebug("OfxImporterPlugin::importStatement start");
  return statementInterface()->import(s);
}

const MyMoneyAccount& OfxImporterPlugin::account(const QString& key, const QString& value) const
{
  return statementInterface()->account(key, value);
}

void OfxImporterPlugin::protocols(QStringList& protocolList) const
{
  protocolList.clear();
  protocolList << "OFX";
}

QWidget* OfxImporterPlugin::accountConfigTab(const MyMoneyAccount& acc, QString& name)
{
  name = i18n("Online settings");
  d->m_statusDlg = new KOnlineBankingStatus(acc, 0);
  return d->m_statusDlg;
}

MyMoneyKeyValueContainer OfxImporterPlugin::onlineBankingSettings(const MyMoneyKeyValueContainer& current)
{
  MyMoneyKeyValueContainer kvp(current);
  // keep the provider name in sync with the one found in kmm_ofximport.desktop
  kvp["provider"] = "KMyMoney OFX";
  if (d->m_statusDlg) {
    kvp.deletePair("appId");
    kvp.deletePair("kmmofx-headerVersion");
    kvp.deletePair("password");

    d->m_wallet = Wallet::openWallet(Wallet::NetworkWallet(), d->m_statusDlg->winId(), Wallet::Synchronous);
    if (d->m_wallet && (d->m_wallet->hasFolder(KWallet::Wallet::PasswordFolder()) ||
                        d->m_wallet->createFolder(KWallet::Wallet::PasswordFolder())) &&
        d->m_wallet->setFolder(KWallet::Wallet::PasswordFolder())) {
      QString key = OFX_PASSWORD_KEY(kvp.value("url"), kvp.value("uniqueId"));
      if (d->m_statusDlg->m_storePassword->isChecked()) {
        d->m_wallet->writePassword(key, d->m_statusDlg->m_password->text());
      } else {
        if (d->m_wallet->hasEntry(key)) {
          d->m_wallet->removeEntry(key);
        }
      }
    } else {
      if (d->m_statusDlg->m_storePassword->isChecked()) {
        kvp.setValue("password", d->m_statusDlg->m_password->text());
      }
    }

    if (!d->m_statusDlg->appId().isEmpty())
      kvp.setValue("appId", d->m_statusDlg->appId());
    kvp.setValue("kmmofx-headerVersion", d->m_statusDlg->headerVersion());
    kvp.setValue("kmmofx-numRequestDays", QString::number(d->m_statusDlg->m_numdaysSpin->value()));
    kvp.setValue("kmmofx-todayMinus", QString::number(d->m_statusDlg->m_todayRB->isChecked()));
    kvp.setValue("kmmofx-lastUpdate", QString::number(d->m_statusDlg->m_lastUpdateRB->isChecked()));
    kvp.setValue("kmmofx-pickDate", QString::number(d->m_statusDlg->m_pickDateRB->isChecked()));
    kvp.setValue("kmmofx-specificDate", d->m_statusDlg->m_specificDate->date().toString());
    kvp.setValue("kmmofx-preferPayeeid", QString::number(d->m_statusDlg->m_payeeidRB->isChecked()));
    kvp.setValue("kmmofx-preferName", QString::number(d->m_statusDlg->m_nameRB->isChecked()));
  }
  return kvp;
}

bool OfxImporterPlugin::mapAccount(const MyMoneyAccount& acc, MyMoneyKeyValueContainer& settings)
{
  Q_UNUSED(acc);

  bool rc = false;
  QPointer<KOnlineBankingSetupWizard> wiz = new KOnlineBankingSetupWizard(0);
  if (wiz->isInit()) {
    if (wiz->exec() == QDialog::Accepted) {
      rc = wiz->chosenSettings(settings);
    }
  }

  delete wiz;

  return rc;
}

bool OfxImporterPlugin::updateAccount(const MyMoneyAccount& acc, bool moreAccounts)
{
  Q_UNUSED(moreAccounts);

  try {
    if (!acc.id().isEmpty()) {
      // Save the value of preferName to be used by ofxTransactionCallback
      d->m_preferName = acc.onlineBankingSettings().value("kmmofx-preferName").toInt() != 0;
      QPointer<KOfxDirectConnectDlg> dlg = new KOfxDirectConnectDlg(acc);

      connect(dlg, SIGNAL(statementReady(const QString&)),
              this, SLOT(slotImportFile(const QString&)));

      if (dlg->init())
        dlg->exec();
      delete dlg;
    }
  } catch (MyMoneyException *e) {
    KMessageBox::information(0 , i18n("Error connecting to bank: %1", e->what()));
    delete e;
  }

  return false;
}

void OfxImporterPlugin::slotImportFile(const QString& url)
{

  if (!import(url)) {
    KMessageBox::error(0, QString("<qt>%1</qt>").arg(i18n("<p>Unable to import <b>'%1'</b> using the OFX importer plugin.  The plugin returned the following error:</p><p>%2</p>", url, lastError())), i18n("Importing error"));
  }
}

bool OfxImporterPlugin::storeStatements(QList<MyMoneyStatement>& statements)
{
  bool hasstatements = (statements.count() > 0);
  bool ok = true;
  bool abort = false;

  // FIXME Deal with warnings/errors coming back from plugins
  /*if ( ofx.errors().count() )
  {
    if ( KMessageBox::warningContinueCancelList(this,i18n("The following errors were returned from your bank"),ofx.errors(),i18n("OFX Errors")) == KMessageBox::Cancel )
      abort = true;
  }

  if ( ofx.warnings().count() )
  {
    if ( KMessageBox::warningContinueCancelList(this,i18n("The following warnings were returned from your bank"),ofx.warnings(),i18n("OFX Warnings"),KStandardGuiItem::cont(),"ofxwarnings") == KMessageBox::Cancel )
      abort = true;
  }*/

  qDebug("OfxImporterPlugin::storeStatements() with %d statements called", static_cast<int>(statements.count()));
  QList<MyMoneyStatement>::const_iterator it_s = statements.constBegin();
  while (it_s != statements.constEnd() && !abort) {
    ok = ok && importStatement((*it_s));
    ++it_s;
  }

  if (hasstatements && !ok) {
    KMessageBox::error(0, i18n("Importing process terminated unexpectedly."), i18n("Failed to import all statements."));
  }

  return (!hasstatements || ok);
}

void OfxImporterPlugin::addnew(void)
{
  d->m_statementlist.push_back(MyMoneyStatement());
}
MyMoneyStatement& OfxImporterPlugin::back(void)
{
  return d->m_statementlist.back();
}
bool OfxImporterPlugin::isValid(void) const
{
  return d->m_valid;
}
void OfxImporterPlugin::setValid(void)
{
  d->m_valid = true;
}
void OfxImporterPlugin::addInfo(const QString& _msg)
{
  d->m_infos += _msg;
}
void OfxImporterPlugin::addWarning(const QString& _msg)
{
  d->m_warnings += _msg;
}
void OfxImporterPlugin::addError(const QString& _msg)
{
  d->m_errors += _msg;
}
const QStringList& OfxImporterPlugin::infos(void) const          // krazy:exclude=spelling
{
  return d->m_infos;
}
const QStringList& OfxImporterPlugin::warnings(void) const
{
  return d->m_warnings;
}
const QStringList& OfxImporterPlugin::errors(void) const
{
  return d->m_errors;
}

#include "ofximporterplugin.moc"
// vim:cin:si:ai:et:ts=2:sw=2:
