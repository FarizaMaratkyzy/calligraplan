/* This file is part of the KDE project
   Copyright (C) 2005 - 2007 Dag Andersen <dag.andersen@kdemail.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KPTMILESTONEPROGRESSDIALOG_H
#define KPTMILESTONEPROGRESSDIALOG_H

#include "planui_export.h"

#include <KoDialog.h>


namespace KPlato
{

class MilestoneProgressPanel;
class Task;
class Node;
class MacroCommand;

class PLANUI_EXPORT MilestoneProgressDialog : public KoDialog {
    Q_OBJECT
public:
    explicit MilestoneProgressDialog(Task &task, QWidget *parent=0);

    MacroCommand *buildCommand();

protected Q_SLOTS:
    void slotChanged();
    void slotNodeRemoved(KPlato::Node *node);

private:
    Node *m_node;
    MilestoneProgressPanel *m_panel;

};

} //KPlato namespace

#endif // MILESTONEPROGRESSDIALOG_H
