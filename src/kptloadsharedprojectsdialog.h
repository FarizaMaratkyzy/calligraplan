/* This file is part of the KDE project
 * Copyright (C) 2019 Dag Andersen <dag.andersen@kdemail.net>
 *  
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *  
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *  
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KPTLOADSHAREDPROJECTSDIALOG_H
#define KPTLOADSHAREDPROJECTSDIALOG_H

#include <KoDialog.h>

class QUrl;
class QTreeView;

namespace KPlato
{

class Project;

class LoadSharedProjectsDialog : public KoDialog
{
    Q_OBJECT
public:
    LoadSharedProjectsDialog(Project &project, const QUrl &own, QWidget *parent=0);

    QList<QUrl> urls() const;

private:
    QTreeView *m_view;
};


} //KPlato namespace

#endif
