/* This file is part of the KDE project
 * Copyright (C) 2019 Dag Andersen <dag.andersen@kdemail.net>
 * Copyright (C) 2007, 2012 Dag Andersen <dag.andersen@kdemail.net>
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

#ifndef KPTDOCUMENTSDIALOG_H
#define KPTDOCUMENTSDIALOG_H

#include "planui_export.h"
#include "PlanMacros.h"

#include <KoDialog.h>

namespace KPlato
{

class DocumentsPanel;
class Node;
class MacroCommand;
        
class PLANUI_EXPORT DocumentsDialog : public KoDialog
{
    Q_OBJECT
public:
    /**
     * The constructor for the documents dialog.
     * @param node the task or project to show documents for
     * @param parent parent widget
     * @param readOnly determines whether the data are read-only
     */
    explicit DocumentsDialog(Node &node, QWidget *parent = 0, bool readOnly = false  );

    MacroCommand *buildCommand();

protected Q_SLOTS:
    void slotButtonClicked(int button) override;

protected:
    DocumentsPanel *m_panel;
};

} //KPlato namespace

#endif
