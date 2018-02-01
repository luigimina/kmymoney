/***************************************************************************
                          kcurrencyeditdlg.cpp  -  description
                             -------------------
    begin                : Wed Mar 24 2004
    copyright            : (C) 2000-2004 by Michael Edwardes
    email                : mte@users.sourceforge.net
                           Javier Campos Morales <javi_c@users.sourceforge.net>
                           Felix Rodriguez <frodriguez@users.sourceforge.net>
                           John C <thetacoturtle@users.sourceforge.net>
                           Thomas Baumgart <ipwizard@users.sourceforge.net>
                           Kevin Tambascio <ktambascio@users.sourceforge.net>
                           Alvaro Soliverez <asoliverez@gmail.com>
                           (C) 2017 Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kcurrencyeditdlg.h"

#include <locale.h>

// ----------------------------------------------------------------------------
// QT Includes

#include <QTimer>
#include <QPixmap>
#include <QBitmap>
#include <QList>
#include <QTreeWidget>
#include <QStyledItemDelegate>
#include <QIcon>
#include <QPushButton>
#include <QBitArray>
#include <QInputDialog>
#include <QMenu>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KTreeWidgetSearchLineWidget>
#include <KMessageBox>

// ----------------------------------------------------------------------------
// Project Includes

#include "ui_kcurrencyeditdlg.h"
#include "ui_kcurrencyeditordlg.h"
#include "ui_kavailablecurrencydlg.h"

#include "mymoneyexception.h"
#include "mymoneysecurity.h"
#include "mymoneyfile.h"
#include "mymoneyprice.h"
#include "kavailablecurrencydlg.h"
#include "kcurrencyeditordlg.h"
#include "kmymoneyutils.h"
#include "icons/icons.h"
#include "storageenums.h"

using namespace Icons;

// duplicated eMenu namespace from menuenums.h for consistency
// there shouldn't be any clash, because we don't need menuenums.h here
namespace eMenu {
  enum class Action {
    // *************
    // The currency menu
    // *************
    NewCurrency, RenameCurrency, DeleteCurrency,
    SetBaseCurrency,
  };
  inline uint qHash(const Action key, uint seed) { return ::qHash(static_cast<uint>(key), seed); }
}

// this delegate is needed to disable editing the currency id (column 1)
// since QTreeWidgetItem has only one set of flags for the whole row
// the column editable property couldn't be set in an easier way
class KCurrencyEditDelegate : public QStyledItemDelegate
{
public:
  explicit KCurrencyEditDelegate(QObject *parent = 0);

protected:
  QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

KCurrencyEditDelegate::KCurrencyEditDelegate(QObject* parent): QStyledItemDelegate(parent)
{
}

QWidget *KCurrencyEditDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
  if (index.column() == 1)
    return 0;
  return QStyledItemDelegate::createEditor(parent, option, index);
}

class KCurrencyEditDlgPrivate
{
  Q_DISABLE_COPY(KCurrencyEditDlgPrivate)
  Q_DECLARE_PUBLIC(KCurrencyEditDlg)

public:
  explicit KCurrencyEditDlgPrivate(KCurrencyEditDlg *qq) :
    q_ptr(qq),
    ui(new Ui::KCurrencyEditDlg)
  {
  }

  ~KCurrencyEditDlgPrivate()
  {
    delete ui;
  }

  enum removalModeE :int { RemoveSelected, RemoveUnused };

  void removeCurrency(const removalModeE& mode)
  {
    Q_Q(KCurrencyEditDlg);
    auto file = MyMoneyFile::instance();
    MyMoneyFileTransaction ft;
    QBitArray skip((int)eStorage::Reference::Count);
    skip.fill(false);                                 // check reference to all...
    skip.setBit((int)eStorage::Reference::Price);      // ...except price

    QTreeWidgetItemIterator it (ui->m_currencyList);  // iterate over whole tree
    if (mode == RemoveUnused) {
      while (*it) {
        MyMoneySecurity currency = (*it)->data(0, Qt::UserRole).value<MyMoneySecurity>();
        if (file->baseCurrency() != currency && !file->isReferenced(currency, skip))
          KMyMoneyUtils::deleteSecurity(currency, q);
        ++it;
      }
    } else if (mode == RemoveSelected) {
      QList<QTreeWidgetItem*> currencyRows = ui->m_currencyList->selectedItems();
      foreach(auto currencyRow, currencyRows) {
        MyMoneySecurity currency = currencyRow->data(0, Qt::UserRole).value<MyMoneySecurity>();
        if (file->baseCurrency() != currency && !file->isReferenced(currency, skip))
          KMyMoneyUtils::deleteSecurity(currency, q);
      }
    }
    ft.commit();
    ui->m_removeUnusedCurrencyButton->setDisabled(file->currencyList().count() <= 1);
  }

  void setBaseCurrency(const MyMoneySecurity& cur)
  {
    Q_Q(KCurrencyEditDlg);
    if (!cur.id().isEmpty()) {
      if (cur.id() != MyMoneyFile::instance()->baseCurrency().id()) {
        MyMoneyFileTransaction ft;
        try {
          MyMoneyFile::instance()->setBaseCurrency(cur);
          ft.commit();
        } catch (const MyMoneyException &e) {
          KMessageBox::sorry(q, i18n("Cannot set %1 as base currency: %2", cur.name(), e.what()), i18n("Set base currency"));
        }
      }
    }
  }

  void updateCurrency(const QString &currencyId, const QString& currencyName, const QString& currencyTradingSymbol)
  {
    Q_Q(KCurrencyEditDlg);
    const auto file = MyMoneyFile::instance();
    try {
      if (currencyName != m_currentCurrency.name() || currencyTradingSymbol != m_currentCurrency.tradingSymbol()) {
        MyMoneySecurity currency = file->currency(currencyId);
        currency.setName(currencyName);
        currency.setTradingSymbol(currencyTradingSymbol);
        MyMoneyFileTransaction ft;
        try {
          file->modifyCurrency(currency);
          m_currentCurrency = currency;
          ft.commit();
        } catch (const MyMoneyException &e) {
          KMessageBox::sorry(q, i18n("Cannot update currency. %1", e.what()), i18n("Update currency"));
        }
      }
    } catch (const MyMoneyException &e) {
      KMessageBox::sorry(q, i18n("Cannot update currency. %1", e.what()), i18n("Update currency"));
    }
  }

  KCurrencyEditDlg      *q_ptr;
  Ui::KCurrencyEditDlg  *ui;

  KAvailableCurrencyDlg *m_availableCurrencyDlg;
  KCurrencyEditorDlg    *m_currencyEditorDlg;
  MyMoneySecurity        m_currentCurrency;
  /**
    * Search widget for the list
    */
  KTreeWidgetSearchLineWidget*    m_searchWidget;
};

KCurrencyEditDlg::KCurrencyEditDlg(QWidget *parent) :
  QDialog(parent),
  d_ptr(new KCurrencyEditDlgPrivate(this))
{
  Q_D(KCurrencyEditDlg);
  d->ui->setupUi(this);
  d->m_searchWidget = new KTreeWidgetSearchLineWidget(this, d->ui->m_currencyList);
  d->m_searchWidget->setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed));
  d->m_searchWidget->setFocus();
  d->ui->verticalLayout->insertWidget(0, d->m_searchWidget);
  d->ui->m_currencyList->setItemDelegate(new KCurrencyEditDelegate(d->ui->m_currencyList));
  d->ui->m_closeButton->setIcon(Icons::get(Icon::DialogClose));
  d->ui->m_editCurrencyButton->setIcon(Icons::get(Icon::DocumentEdit));
  d->ui->m_selectBaseCurrencyButton->setIcon(Icons::get(Icon::KMyMoney));

  connect(d->ui->m_currencyList, &QWidget::customContextMenuRequested, this, &KCurrencyEditDlg::slotShowCurrencyMenu);
  connect(MyMoneyFile::instance(), &MyMoneyFile::dataChanged, this, &KCurrencyEditDlg::slotLoadCurrencies);
  connect(d->ui->m_currencyList, &QTreeWidget::itemChanged, this, static_cast<void (KCurrencyEditDlg::*)(QTreeWidgetItem *, int)>(&KCurrencyEditDlg::slotUpdateCurrency));
  connect(d->ui->m_currencyList, &QTreeWidget::itemSelectionChanged, this, &KCurrencyEditDlg::slotItemSelectionChanged);

  connect(d->ui->m_selectBaseCurrencyButton, &QAbstractButton::clicked, this, &KCurrencyEditDlg::slotSelectBaseCurrency);
  connect(d->ui->m_addCurrencyButton, &QAbstractButton::clicked, this, &KCurrencyEditDlg::slotAddCurrency);
  connect(d->ui->m_removeCurrencyButton, &QAbstractButton::clicked, this, &KCurrencyEditDlg::slotRemoveCurrency);
  connect(d->ui->m_editCurrencyButton, &QAbstractButton::clicked, this, &KCurrencyEditDlg::slotEditCurrency);
  connect(d->ui->m_removeUnusedCurrencyButton, &QAbstractButton::clicked, this, &KCurrencyEditDlg::slotRemoveUnusedCurrency);

  QTimer::singleShot(10, this, SLOT(timerDone()));
}

void KCurrencyEditDlg::timerDone()
{
  Q_D(KCurrencyEditDlg);
  slotLoadCurrencies();

  //resize the column widths
  for (auto i = 0; i < 3; ++i)
    d->ui->m_currencyList->resizeColumnToContents(i);

  if (!d->m_currentCurrency.id().isEmpty()) {
    QTreeWidgetItemIterator it(d->ui->m_currencyList);
    QTreeWidgetItem* q;
    while ((q = *it) != 0) {
      if (q->text(1) == d->m_currentCurrency.id()) {
        d->ui->m_currencyList->scrollToItem(q);
        break;
      }
      ++it;
    }
  }
}

KCurrencyEditDlg::~KCurrencyEditDlg()
{
  Q_D(KCurrencyEditDlg);
  delete d;
}

void KCurrencyEditDlg::slotLoadCurrencies()
{
  Q_D(KCurrencyEditDlg);
  disconnect(d->ui->m_currencyList, &QTreeWidget::currentItemChanged, this, static_cast<void (KCurrencyEditDlg::*)(QTreeWidgetItem *, QTreeWidgetItem *)>(&KCurrencyEditDlg::slotSelectCurrency));
  disconnect(d->ui->m_currencyList, &QTreeWidget::itemChanged, this, static_cast<void (KCurrencyEditDlg::*)(QTreeWidgetItem *, int)>(&KCurrencyEditDlg::slotUpdateCurrency));
  QList<MyMoneySecurity> list = MyMoneyFile::instance()->currencyList();
  QList<MyMoneySecurity>::ConstIterator it;
  QTreeWidgetItem *first = 0;

  QString localCurrency(localeconv()->int_curr_symbol);
  localCurrency.truncate(3);

  QString baseCurrency;
  try {
    baseCurrency = MyMoneyFile::instance()->baseCurrency().id();
  } catch (const MyMoneyException &e) {
    qDebug("%s", qPrintable(e.what()));
  }

  // construct a transparent 16x16 pixmap
  QPixmap empty(16, 16);
  QBitmap mask(16, 16);
  mask.clear();
  empty.setMask(mask);

  d->ui->m_currencyList->setSortingEnabled(false);
  d->ui->m_currencyList->clear();
  for (it = list.constBegin(); it != list.constEnd(); ++it) {
    QTreeWidgetItem *p = new QTreeWidgetItem(d->ui->m_currencyList);
    p->setText(0, (*it).name());
    p->setData(0, Qt::UserRole, QVariant::fromValue(*it));
    p->setFlags(p->flags() | Qt::ItemIsEditable);
    p->setText(1, (*it).id());
    p->setText(2, (*it).tradingSymbol());

    if ((*it).id() == baseCurrency) {
      p->setData(0, Qt::DecorationRole, Icons::get(Icon::KMyMoney));
      if (d->m_currentCurrency.id().isEmpty())
        first = p;
    } else {
      p->setData(0, Qt::DecorationRole, empty);
    }

    // if we had a previously selected
    if (!d->m_currentCurrency.id().isEmpty()) {
      if (d->m_currentCurrency.id() == p->text(1))
        first = p;
    } else if ((*it).id() == localCurrency && !first)
      first = p;
  }
  d->ui->m_removeUnusedCurrencyButton->setDisabled(list.count() <= 1);
  d->ui->m_currencyList->sortItems(0, Qt::AscendingOrder);
  d->ui->m_currencyList->setSortingEnabled(true);

  connect(d->ui->m_currencyList, &QTreeWidget::currentItemChanged, this, static_cast<void (KCurrencyEditDlg::*)(QTreeWidgetItem *, QTreeWidgetItem *)>(&KCurrencyEditDlg::slotSelectCurrency));
  connect(d->ui->m_currencyList, &QTreeWidget::itemChanged, this, static_cast<void (KCurrencyEditDlg::*)(QTreeWidgetItem *, int)>(&KCurrencyEditDlg::slotUpdateCurrency));

  if (first == 0)
    first = d->ui->m_currencyList->invisibleRootItem()->child(0);
  if (first != 0) {
    d->ui->m_currencyList->setCurrentItem(first);
    d->ui->m_currencyList->scrollToItem(first);
  }

  slotSelectCurrency(first);
}

void KCurrencyEditDlg::slotUpdateCurrency(QTreeWidgetItem* citem, int)
{
  slotUpdateCurrency(citem, nullptr);
}

void KCurrencyEditDlg::slotUpdateCurrency(QTreeWidgetItem* citem, QTreeWidgetItem *pitem)
{
  Q_D(KCurrencyEditDlg);
  Q_UNUSED(pitem)
  //if there is no current item selected, exit
  if (!d->ui->m_currencyList->currentItem() || citem != d->ui->m_currencyList->currentItem())
    return;

  //verify that the stored currency id is not empty and the edited fields are not empty either
  if (!d->m_currentCurrency.id().isEmpty()
      && !d->ui->m_currencyList->currentItem()->text(2).isEmpty()
      && !d->ui->m_currencyList->currentItem()->text(0).isEmpty()) {
    //check that either the name or the id have changed
    if (d->ui->m_currencyList->currentItem()->text(2) != d->m_currentCurrency.tradingSymbol()
        || d->ui->m_currencyList->currentItem()->text(0) != d->m_currentCurrency.name()) {
      //update the name and the id
      d->m_currentCurrency.setName(d->ui->m_currencyList->currentItem()->text(0));
      d->m_currentCurrency.setTradingSymbol(d->ui->m_currencyList->currentItem()->text(2));

      d->updateCurrency(d->m_currentCurrency.id(), d->m_currentCurrency.name(), d->m_currentCurrency.tradingSymbol());
    }
  }
}

void KCurrencyEditDlg::slotSelectCurrency(const QString& id)
{
  Q_D(KCurrencyEditDlg);
  QTreeWidgetItemIterator it(d->ui->m_currencyList);

  while (*it) {
    if ((*it)->text(1) == id) {
      d->ui->m_currencyList->blockSignals(true);
      slotSelectCurrency(*it);
      d->ui->m_currencyList->setCurrentItem(*it);
      d->ui->m_currencyList->scrollToItem(*it);
      d->ui->m_currencyList->blockSignals(false);
      break;
    }
    ++it;
  }
}

void KCurrencyEditDlg::slotSelectCurrency(QTreeWidgetItem *citem, QTreeWidgetItem *pitem)
{
  Q_UNUSED(pitem)
  slotSelectCurrency(citem);
}

void KCurrencyEditDlg::slotSelectCurrency(QTreeWidgetItem *item)
{
  Q_D(KCurrencyEditDlg);
  auto file = MyMoneyFile::instance();
  QString baseId;
  try {
    baseId = MyMoneyFile::instance()->baseCurrency().id();
  } catch (const MyMoneyException &) {
  }

  if (item) {
    try {
      d->m_currentCurrency = file->security(item->text(1));

    } catch (const MyMoneyException &) {
      d->m_currentCurrency = MyMoneySecurity();
    }

    QBitArray skip((int)eStorage::Reference::Count);
    skip.fill(false);
    skip.setBit((int)eStorage::Reference::Price);

    const bool rc1 = d->m_currentCurrency.id() == baseId;
    const bool rc2 = file->isReferenced(d->m_currentCurrency, skip);
    const int count = d->ui->m_currencyList->selectedItems().count();

    d->ui->m_selectBaseCurrencyButton->setDisabled(rc1 || count != 1);
    d->ui->m_editCurrencyButton->setDisabled(count != 1);
    d->ui->m_removeCurrencyButton->setDisabled((rc1 || rc2) && count <= 1);
//    emit selectObject(d->m_currentCurrency);
  }
}

void KCurrencyEditDlg::slotItemSelectionChanged()
{
  Q_D(KCurrencyEditDlg);
  int count = d->ui->m_currencyList->selectedItems().count();
  if (!d->ui->m_selectBaseCurrencyButton->isEnabled() && count == 1)
    slotSelectCurrency(d->ui->m_currencyList->currentItem());
  if (count > 1)
    d->ui->m_removeCurrencyButton->setEnabled(true);
}

void KCurrencyEditDlg::slotShowCurrencyMenu(const QPoint& p)
{
  Q_D(KCurrencyEditDlg);
  auto item = d->ui->m_currencyList->itemAt(p);
  if (item) {
    const auto sec = item->data(0, Qt::UserRole).value<MyMoneySecurity>();
    const auto file = MyMoneyFile::instance();
    QBitArray skip((int)eStorage::Reference::Count);
    skip.fill(false);
    skip.setBit((int)eStorage::Reference::Transaction);
    const auto cond1 = !sec.id().isEmpty();
    const auto cond2 = cond1 && !file->isReferenced(sec, skip);
    const auto cond3 = cond1 && sec.id() != file->baseCurrency().id();
    auto menu = new QMenu;
    typedef void(KCurrencyEditDlg::*KCurrencyEditDlgFunc)();
    struct actionInfo {
      eMenu::Action        action;
      KCurrencyEditDlgFunc callback;
      QString              text;
      Icon                 icon;
      bool                 enabled;
    };

    const QVector<actionInfo> actionInfos {
      {eMenu::Action::NewCurrency,      &KCurrencyEditDlg::slotNewCurrency,     i18n("New currency"),            Icon::ListAdd,     true},
      {eMenu::Action::RenameCurrency,   &KCurrencyEditDlg::slotRenameCurrency,  i18n("Rename currency"),         Icon::EditRename,  cond1},
      {eMenu::Action::DeleteCurrency,   &KCurrencyEditDlg::slotDeleteCurrency,  i18n("Delete currency"),         Icon::EditDelete,  cond2},
      {eMenu::Action::SetBaseCurrency,  &KCurrencyEditDlg::slotSetBaseCurrency, i18n("Select as base currency"), Icon::KMyMoney,    cond3}
    };

    QList<QAction*> LUTActions;
    for (const auto& info : actionInfos) {
      auto a = new QAction(Icons::get(info.icon), info.text, nullptr); // WARNING: no empty Icon::Empty here
      a->setEnabled(info.enabled);
      connect(a, &QAction::triggered, this, info.callback);
      LUTActions.append(a);
    }
    menu->addSection(i18nc("Menu header","Currency options"));
    menu->addActions(LUTActions);
    menu->exec(QCursor::pos());
  }
}

void KCurrencyEditDlg::slotSelectBaseCurrency()
{
  Q_D(KCurrencyEditDlg);
  if (!d->m_currentCurrency.id().isEmpty()) {
    QTreeWidgetItem* p = d->ui->m_currencyList->currentItem();
    d->setBaseCurrency(d->m_currentCurrency);
    // in case the dataChanged() signal was not sent out (nested FileTransaction)
    // we update the list manually
    if (p == d->ui->m_currencyList->currentItem())
      slotLoadCurrencies();
  }
}

void KCurrencyEditDlg::slotAddCurrency()
{
  Q_D(KCurrencyEditDlg);
  d->m_availableCurrencyDlg = new KAvailableCurrencyDlg;                                   // create new dialog for selecting currencies to add
  if (d->m_availableCurrencyDlg->exec() != QDialog::Rejected) {
    auto file = MyMoneyFile::instance();
    QMap<MyMoneySecurity, MyMoneyPrice> ancientCurrencies = file->ancientCurrencies();
    MyMoneyFileTransaction ft;
    QList<QTreeWidgetItem *> currencyRows = d->m_availableCurrencyDlg->ui->m_currencyList->selectedItems(); // get selected currencies from new dialog
    foreach (auto currencyRow, currencyRows) {
      MyMoneySecurity currency = currencyRow->data(0, Qt::UserRole).value<MyMoneySecurity>();
      file->addCurrency(currency);
      if (ancientCurrencies.value(currency, MyMoneyPrice()) != MyMoneyPrice()) // if ancient currency is added...
        file->addPrice(ancientCurrencies[currency]);                           // ...we want to add last known exchange rate as well
    }
    ft.commit();
    d->ui->m_removeUnusedCurrencyButton->setDisabled(file->currencyList().count() <= 1);
  }
  delete d->m_availableCurrencyDlg;
}

void KCurrencyEditDlg::slotRemoveCurrency()
{
  Q_D(KCurrencyEditDlg);
  d->removeCurrency(KCurrencyEditDlgPrivate::RemoveSelected);
}

void KCurrencyEditDlg::slotRemoveUnusedCurrency()
{
  Q_D(KCurrencyEditDlg);
  d->removeCurrency(KCurrencyEditDlgPrivate::RemoveUnused);
}

void KCurrencyEditDlg::slotEditCurrency()
{
  Q_D(KCurrencyEditDlg);
  MyMoneySecurity currency = d->ui->m_currencyList->currentItem()->data(0, Qt::UserRole).value<MyMoneySecurity>();
  d->m_currencyEditorDlg = new KCurrencyEditorDlg(currency);                                   // create new dialog for editing currency
  if (d->m_currencyEditorDlg->exec() != QDialog::Rejected) {
    auto file = MyMoneyFile::instance();
    MyMoneyFileTransaction ft;
    currency.setPricePrecision(d->m_currencyEditorDlg->ui->m_pricePrecision->value());
    try {
      file->modifyCurrency(currency);
      ft.commit();
    } catch (const MyMoneyException &e) {
      qDebug("%s", qPrintable(e.what()));
    }
  }
  delete d->m_currencyEditorDlg;
}

void KCurrencyEditDlg::slotNewCurrency()
{
  QString sid = QInputDialog::getText(0, i18n("New currency"), i18n("Enter ISO 4217 code for the new currency"));
  if (!sid.isEmpty()) {
    QString id(sid);
    MyMoneySecurity currency(id, i18n("New currency"));
    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->addCurrency(currency);
      ft.commit();
    } catch (const MyMoneyException &e) {
      KMessageBox::sorry(this, i18n("Cannot create new currency. %1", e.what()), i18n("New currency"));
    }
    slotSelectCurrency(id);
  }
}

void KCurrencyEditDlg::slotRenameCurrency()
{
  Q_D(KCurrencyEditDlg);
  QTreeWidgetItemIterator it_l(d->ui->m_currencyList, QTreeWidgetItemIterator::Selected);
  QTreeWidgetItem* it_v;
  if ((it_v = *it_l) != 0) {
    d->ui->m_currencyList->editItem(it_v, 0);
  }
}

void KCurrencyEditDlg::slotDeleteCurrency()
{
  Q_D(KCurrencyEditDlg);
  if (!d->m_currentCurrency.id().isEmpty()) {
    MyMoneyFileTransaction ft;
    try {
      MyMoneyFile::instance()->removeCurrency(d->m_currentCurrency);
      ft.commit();
    } catch (const MyMoneyException &e) {
      KMessageBox::sorry(this, i18n("Cannot delete currency %1. %2", d->m_currentCurrency.name(), e.what()), i18n("Delete currency"));
    }
  }
}

void KCurrencyEditDlg::slotSetBaseCurrency()
{
  Q_D(KCurrencyEditDlg);
  if (!d->m_currentCurrency.id().isEmpty()) {
    if (d->m_currentCurrency.id() != MyMoneyFile::instance()->baseCurrency().id()) {
      MyMoneyFileTransaction ft;
      try {
        MyMoneyFile::instance()->setBaseCurrency(d->m_currentCurrency);
        ft.commit();
      } catch (const MyMoneyException &e) {
        KMessageBox::sorry(this, i18n("Cannot set %1 as base currency: %2", d->m_currentCurrency.name(), e.what()), i18n("Set base currency"));
      }
    }
  }
}
