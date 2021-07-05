/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2012 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KOAUTOSAVERECOVERYDIALOG_H
#define KOAUTOSAVERECOVERYDIALOG_H

#include <KoDialog.h>
#include <QStringList>
#include <QModelIndex>

class QListView;

Q_DECLARE_METATYPE(QModelIndex)

class KoAutoSaveRecoveryDialog : public KoDialog
{
    Q_OBJECT
public:

    explicit KoAutoSaveRecoveryDialog(const QStringList &filenames, QWidget *parent = nullptr);
    QStringList recoverableFiles();

public Q_SLOTS:

    void toggleFileItem(bool toggle);

private:

    QListView *m_listView;

    class FileItemModel;
    FileItemModel *m_model;
};


#endif // KOAUTOSAVERECOVERYDIALOG_H
