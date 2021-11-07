#!/bin/bash
svn co svn://anonsvn.kde.org/home/kde/branches/stable/l10n-kf5/it/messages/kmymoney kmymoney_it
cd kmymoney_it
sudo msgfmt kmymoney.po -o /usr/share/locale/it/LC_MESSAGES/kmymoney.mo
cd ..
sudo rm -r kmymoney_it