/***************************************************************************
 *   This file is part of KMyMoney, A Personal Finance Manager by KDE      *
 *                                                                         *
 *   Copyright (C) 2009      Cristian Onet <onet.cristian@gmail.com>       *
 *   Copyright (C) 2019      Thomas Baumgart <tbaumgart@kde.org>           *
 *   Copyright (C) 2021      Dawid Wróbel <me@dawidwrobel.com>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>  *
 ***************************************************************************/

#include <config-kmymoney.h>
#include "checkprinting.h"

// QT includes
#include <QAction>
#include <QFile>
#include <QDialog>
#include <QTextDocument>
#include <QStandardPaths>

// KDE includes
#include <KPluginFactory>
#include <KActionCollection>
#include <KLocalizedString>

// KMyMoney includes
#include "mymoneyfile.h"
#include "mymoneyaccount.h"
#include "mymoneyinstitution.h"
#include "mymoneymoney.h"
#include "mymoneypayee.h"
#include "mymoneysecurity.h"
#include "mymoneysplit.h"
#include "mymoneytransaction.h"
#include "mymoneyutils.h"
#include "viewinterface.h"
#include "selectedobjects.h"

#include "numbertowords.h"
#include "pluginsettings.h"
#include "mymoneyenums.h"

#include "kmm_printer.h"

struct CheckPrinting::Private
{
    QAction *m_action;
    QString m_checkTemplateHTML;
    QStringList m_printedTransactionIdList;
    SelectedObjects selections;
    // KMyMoneyRegister::SelectedTransactions m_transactions;

    bool canBePrinted(const QString &accountId) const
    {
        return MyMoneyFile::instance()->account(accountId).accountType() == eMyMoney::Account::Type::Checkings;
    }

    bool canBePrinted(const QString &accountId, const QString &transactionId) const
    {
        // can't print it twice
        if (m_printedTransactionIdList.contains(transactionId)) {
            return false;
        }
        const auto transaction = MyMoneyFile::instance()->transaction(transactionId);
        const auto split = transaction.splitByAccount(accountId);

        return split.shares().isNegative();
    }

    void markAsPrinted(const QString &transactionId)
    {
        m_printedTransactionIdList.append(transactionId);
    }

    void readCheckTemplate()
    {
        QFile *checkTemplateHTMLFile;

        if (!PluginSettings::useCustomCheckTemplate || PluginSettings::checkTemplateFile().isEmpty()) {
            checkTemplateHTMLFile = new QFile(PluginSettings::defaultCheckTemplateFileValue());
        }
        else {
            checkTemplateHTMLFile = new QFile(PluginSettings::checkTemplateFile());
        }

        if (!(checkTemplateHTMLFile->open(QIODevice::ReadOnly))) {
            qDebug() << "Failed to open the template from" << checkTemplateHTMLFile->fileName();
        }
        else {
            qDebug() << "Template successfully opened from" << checkTemplateHTMLFile->fileName();
        }

        QTextStream stream(checkTemplateHTMLFile);

        m_checkTemplateHTML = stream.readAll();

        checkTemplateHTMLFile->close();
    }

    void printCheck(const QString &accountId, const QString &transactionId)
    {
        MyMoneyMoneyToWordsConverter converter;
        auto htmlPart = new QTextDocument();

        const auto file = MyMoneyFile::instance();
        const auto transaction = file->transaction(transactionId);
        const auto split = transaction.splitByAccount(accountId);
        const auto payee = file->payee(split.payeeId());
        const auto account = file->account(split.accountId());

        const auto currency = file->currency(account.currencyId());
        const auto institution = file->institution(account.institutionId());

        QString checkHTML = m_checkTemplateHTML;
        // replace the predefined tokens
        // data about the user
        checkHTML.replace("$OWNER_NAME", file->user().name());
        checkHTML.replace("$OWNER_ADDRESS", file->user().address());
        checkHTML.replace("$OWNER_CITY", file->user().city());
        checkHTML.replace("$OWNER_STATE", file->user().state());
        // data about the account institution
        checkHTML.replace("$INSTITUTION_NAME", institution.name());
        checkHTML.replace("$INSTITUTION_STREET", institution.street());
        checkHTML.replace("$INSTITUTION_TELEPHONE", institution.telephone());
        checkHTML.replace("$INSTITUTION_TOWN", institution.town());
        checkHTML.replace("$INSTITUTION_CITY", institution.city());
        checkHTML.replace("$INSTITUTION_POSTCODE", institution.postcode());
        checkHTML.replace("$INSTITUTION_MANAGER", institution.manager());
        // data about the transaction
        checkHTML.replace("$DATE", QLocale().toString(transaction.postDate(), QLocale::ShortFormat));
        checkHTML.replace("$CHECK_NUMBER", split.number());
        checkHTML.replace("$PAYEE_NAME", payee.name());
        checkHTML.replace("$PAYEE_ADDRESS", payee.address());
        checkHTML.replace("$PAYEE_CITY", payee.city());
        checkHTML.replace("$PAYEE_POSTCODE", payee.postcode());
        checkHTML.replace("$PAYEE_STATE", payee.state());
        checkHTML.replace("$AMOUNT_STRING", converter.convert(split.value().abs(), currency.smallestAccountFraction()));
        checkHTML.replace("$AMOUNT_DECIMAL", MyMoneyUtils::formatMoney(split.value().abs(), currency));
        checkHTML.replace("$MEMO", split.memo());
        const auto currencyId = transaction.commodity();
        const auto accountcurrency = MyMoneyFile::instance()->currency(currencyId);
        checkHTML.replace("$TRANSACTIONCURRENCY", accountcurrency.tradingSymbol());
        int numSplits = (int) transaction.splitCount();
        const int maxSplits = 11;
        for (int i = maxSplits - 1; i >= 0; i--) {
            const QString valueVariable = QString("$SPLITVALUE%1").arg(i);
            const QString accountVariable = QString("$SPLITACCOUNTNAME%1").arg(i);
            if (i < numSplits) {
                checkHTML
                    .replace(valueVariable, MyMoneyUtils::formatMoney(transaction.splits()[i].value().abs(), currency));
                checkHTML.replace(accountVariable, (file->account(transaction.splits()[i].accountId())).name());
            }
            else {
                checkHTML.replace(valueVariable, " ");
                checkHTML.replace(accountVariable, " ");
            }
        }

        // print the check
        htmlPart->setHtml(checkHTML);
        auto printer = KMyMoneyPrinter::startPrint();
        if (printer != nullptr) {
            htmlPart->print(printer);
        }

        // mark the transaction as printed
        markAsPrinted(transactionId);

        delete htmlPart;
    }
};

CheckPrinting::CheckPrinting(QObject *parent, const QVariantList &args)
    :
    KMyMoneyPlugin::Plugin(parent, "checkprinting"/*must be the same as X-KDE-PluginInfo-Name*/)
{
    Q_UNUSED(args);
    // Tell the host application to load my GUI component
    const auto componentName = QLatin1String("checkprinting");
    const auto rcFileName = QLatin1String("checkprinting.rc");
    setComponentName(componentName, i18nc("It's about printing bank checks", "Check printing"));

    setXMLFile(rcFileName);

    // For ease announce that we have been loaded.
    qDebug("Plugins: checkprinting loaded");

    d = std::unique_ptr<Private>(new Private);

    // Create the actions of this plugin
    QString actionName = i18n("Print check");

    d->m_action = actionCollection()->addAction("transaction_checkprinting", this, SLOT(slotPrintCheck()));
    d->m_action->setText(actionName);

    // wait until a transaction is selected before enabling the action
    d->m_action->setEnabled(false);
    d->m_printedTransactionIdList = PluginSettings::printedChecks();
    d->readCheckTemplate();

    //! @todo Christian: Replace
#if 0
    connect(KMyMoneyPlugin::PluginLoader::instance(), SIGNAL(configChanged(Plugin*)), this, SLOT(slotUpdateConfig()));
#endif
}

/**
 * @internal Destructor is needed because destructor call of unique_ptr must be in this compile unit
 */
CheckPrinting::~CheckPrinting()
{
    actionCollection()->removeAction(d->m_action);
    qDebug("Plugins: checkprinting unloaded");
}

void CheckPrinting::slotPrintCheck()
{
    const auto transactions = d->selections.selection(SelectedObjects::Transaction);
    const auto accounts = d->selections.selection(SelectedObjects::Account);
    for (const auto &accountId : accounts) {
        if (d->canBePrinted(accountId)) {
            for (const auto &transactionId : transactions) {
                if (d->canBePrinted(accountId, transactionId)) {
                    d->printCheck(accountId, transactionId);
                }
            }
        }
    }
    updateActions(d->selections);
    PluginSettings::setPrintedChecks(d->m_printedTransactionIdList);
}

void CheckPrinting::updateActions(const SelectedObjects &selections)
{
    bool actionEnabled = false;
    // enable/disable the action depending if there are transactions selected or not
    // and whether they can be printed or not
    const auto transactions = selections.selection(SelectedObjects::Transaction);
    const auto accounts = selections.selection(SelectedObjects::Account);
    for (const auto &accountId : accounts) {
        if (d->canBePrinted(accountId)) {
            for (const auto &transactionId : transactions) {
                if (d->canBePrinted(accountId, transactionId)) {
                    actionEnabled = true;
                    break;
                }
            }
        }
        if (actionEnabled) {
            break;
        }
    }
    d->m_action->setEnabled(actionEnabled);
    d->selections = selections;
}

// the plugin's configurations has changed
void CheckPrinting::updateConfiguration()
{
    PluginSettings::self()->load();
    // re-read the data because the configuration has changed
    d->readCheckTemplate();
    d->m_printedTransactionIdList = PluginSettings::printedChecks();
}

K_PLUGIN_FACTORY_WITH_JSON(CheckPrintingFactory, "checkprinting.json", registerPlugin<CheckPrinting>();)

#include "checkprinting.moc"
