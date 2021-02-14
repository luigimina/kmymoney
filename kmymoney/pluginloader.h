/*
 * Copyright 2017       Łukasz Wojniłowicz <lukasz.wojnilowicz@gmail.com>
 * Copyright 2021       Thomas Baumgart <tbaumgart@kde.org>
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

#ifndef PLUGINLOADER_H
#define PLUGINLOADER_H

// ----------------------------------------------------------------------------
// QT Includes

class QObject;
class QString;
template <class Key, class T> class QMap;

// ----------------------------------------------------------------------------
// KDE Includes

class KPluginMetaData;
class KXMLGUIFactory;

// ----------------------------------------------------------------------------
// Project Includes

class SelectedObjects;

namespace KMyMoneyPlugin
{
  struct Container;
  enum class eListing;

  /**
   * @brief The Action enum is for specifying action on plugins
   */
  enum Action {
    Load,         // load all enabled plugins
    Unload,       // unload all loaded plugins
    Reorganize    // load requested and unload unneeded plugins
  };

  /**
   * @brief The Category enum is some arbitrary categorization of plugins
   */
  enum Category {
    OnlineBankOperations,
    PayeeIdentifier,
    StandardPlugin
  };

  Category pluginCategory(const KPluginMetaData& pluginInfo);

  /**
   * @brief It lists all kmymoney plugins
   * @param onlyEnabled = true if plugins should be listed according to on/off saved state in kmymoneyrc
   * @return
   */
  QMap<QString, KPluginMetaData> listPlugins(bool onlyEnabled);

  /**
   * @brief It should be used to handle all plugin actions
   * @param action Action to be taken to all plugins
   * @param ctnPlugins Plugin container to be loaded/unloaded with plugins
   * @param parent Parent of plugins. This should be KMyMoneyApp
   * @param guiFactory GUI Factory of plugins. This should be GUI Factory of KMyMoneyApp
   */
  void pluginHandling(Action action, Container& ctnPlugins, QObject* parent, KXMLGUIFactory* guiFactory);

  /**
   * @brief Update the actions in all plugins
   * @param plugins   List of loaded plugins
   * @param selection the current selection
   */
  void updateActions(const Container& plugins, const SelectedObjects& selections);

  /**
   * @brief Update the configuration in all plugins
   * @param plugins   List of loaded plugins
   */
  void updateConfiguration(const Container& plugins);
}

#endif
