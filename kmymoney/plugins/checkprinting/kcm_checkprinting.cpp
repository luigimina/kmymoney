/***************************************************************************
 *   Copyright 2009  Cristian Onet onet.cristian@gmail.com                 *
 *   Copyright 2021  Dawid Wróbel  me@dawidwrobel.com                      *
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

// ----------------------------------------------------------------------------
// Qt Includes

#include <QFrame>
#include <QTextEdit>
#include <QFile>

// ----------------------------------------------------------------------------
// KDE Includes

#include <KPluginFactory>
#include <KAboutData>

// ----------------------------------------------------------------------------
// Project Includes

#include "pluginsettings.h"
#include "kcm_checkprinting.h"

PluginSettingsWidget::PluginSettingsWidget(QWidget *parent)
    :
    QWidget(parent)
{
    setupUi(this);
    m_checkTemplatePreviewHTMLPart = new QTextEdit(m_previewFrame);

    QVBoxLayout *layout = new QVBoxLayout;
    m_previewFrame->setLayout(layout);
    layout->addWidget(m_checkTemplatePreviewHTMLPart);

    if (PluginSettings::checkTemplateFile().isEmpty()) {
        restoreDefaultSettings();
    }

    connect(kcfg_checkTemplateFile, &KUrlRequester::textChanged, this, &PluginSettingsWidget::textChanged);
    connect(kcfg_checkTemplateFile,
            &KUrlRequester::urlSelected,
            this,
            QOverload<const QUrl &>::of(&PluginSettingsWidget::urlSelected));
    connect(kcfg_checkTemplateFile, QOverload<const QString &>::of(&KUrlRequester::returnPressed), this,
            QOverload<const QString &>::of(&PluginSettingsWidget::urlSelected));
    connect(kcfg_useCustomCheckTemplate, SIGNAL(toggled(bool)), kcfg_checkTemplateFile, SLOT(setEnabled(bool)));
    connect(kcfg_useCustomCheckTemplate,
            &QCheckBox::toggled,
            this,
            QOverload<>::of(&PluginSettingsWidget::urlSelected));
}

void PluginSettingsWidget::urlSelected()
{
    if (kcfg_useCustomCheckTemplate->checkState() == Qt::Unchecked
        || PluginSettings::checkTemplateFile().isEmpty()
        || kcfg_checkTemplateFile->url().isEmpty()) {
        urlSelected(QUrl::fromUserInput(PluginSettings::defaultCheckTemplateFileValue()));
    }
    else {
        urlSelected(kcfg_checkTemplateFile->url());
    }
}

void PluginSettingsWidget::urlSelected(const QString &url)
{
    urlSelected(QUrl::fromUserInput(url));
}

void PluginSettingsWidget::urlSelected(const QUrl &url)
{
    if (!url.isEmpty()) {
        m_checkTemplatePreviewHTMLPart->clear();
        QFile file(url.toLocalFile());

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            m_checkTemplatePreviewHTMLPart->setHtml(stream.readAll());
            file.close();
        }
    }
    else {
        urlSelected();
    }
}

void PluginSettingsWidget::textChanged(const QString &text)
{
    // conceal the default "qrc:/" value to avoid confusing regular users
    if (text == PluginSettings::defaultCheckTemplateFileValue()) {
        kcfg_checkTemplateFile->setText("");
    }
}

void PluginSettingsWidget::restoreDefaultSettings() const
{
    PluginSettings::setUseCustomCheckTemplate(false);
    PluginSettings::setCheckTemplateFile(PluginSettings::defaultCheckTemplateFileValue());
    PluginSettings::self()->save();
}

PluginSettingsWidget::~PluginSettingsWidget()
{
    if (kcfg_checkTemplateFile->url().isEmpty()) {
        restoreDefaultSettings();
    }
}

KCMCheckPrinting::KCMCheckPrinting(QWidget *parent, const QVariantList &args)
    : KCModule(parent, args)
{
    PluginSettingsWidget *w = new PluginSettingsWidget(this);
    addConfig(PluginSettings::self(), w);
    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);
    layout->addWidget(w);
    load();
    w->urlSelected(PluginSettings::useCustomCheckTemplate() ? PluginSettings::checkTemplateFile()
                                                            : PluginSettings::defaultCheckTemplateFileValue());
}

KCMCheckPrinting::~KCMCheckPrinting()
{
}

K_PLUGIN_FACTORY_WITH_JSON(KCMCheckPrintingFactory, "kcm_checkprinting.json", registerPlugin<KCMCheckPrinting>();)

#include "kcm_checkprinting.moc"
