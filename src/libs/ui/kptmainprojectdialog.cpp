/* This file is part of the KDE project
   Copyright (C) 2003 - 2007 Dag Andersen <dag.andersen@kdemail.net>

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
 * Boston, MA 02110-1301, USA.
*/

// clazy:excludeall=qstring-arg
#include "kptmainprojectdialog.h"

#include "kptproject.h"
#include "kptmainprojectpanel.h"
#include "kptcommand.h"

#include <KLocalizedString>


namespace KPlato
{

MainProjectDialog::MainProjectDialog(Project &p, QWidget *parent, bool edit)
    : KoDialog(parent),
      project(p)
{
    setWindowTitle(i18n("Project Settings"));
    setButtons(Ok|Cancel);
    setDefaultButton(Ok);
    showButtonSeparator(true);
    panel = new MainProjectPanel(project, this);
    panel->projectsLoadBtn->setVisible(edit);
    panel->projectsClearBtn->setVisible(edit);

    setMainWidget(panel);
    enableButtonOk(false);
    resize(QSize(500, 410).expandedTo(minimumSizeHint()));

    connect(this, &QDialog::rejected, this, &MainProjectDialog::slotRejected);
    connect(this, &QDialog::accepted, this, &MainProjectDialog::slotOk);
    connect(panel, &MainProjectPanel::obligatedFieldsFilled, this, &KoDialog::enableButtonOk);

    connect(panel, &MainProjectPanel::loadResourceAssignments, this, &MainProjectDialog::loadResourceAssignments);
    connect(panel, &MainProjectPanel::clearResourceAssignments, this, &MainProjectDialog::clearResourceAssignments);
}

void MainProjectDialog::slotRejected()
{
    emit dialogFinished(QDialog::Rejected);
}

void MainProjectDialog::slotOk() {
    if (!panel->ok()) {
        return;
    }
    if (panel->loadSharedResources()) {
        QString file = panel->resourcesFile->text();
        if (file.startsWith('/')) {
            file.prepend("file:/");
        }
        QString place = panel->projectsPlace->text();
        if (panel->projectsType->currentIndex() == 0 /*dir*/ && !place.isEmpty() && !place.endsWith('/')) {
            place.append('/');
        }
        QUrl url(place);
        emit sigLoadSharedResources(file, url, panel->projectsLoadAtStartup->isChecked());
    }
    emit dialogFinished(QDialog::Accepted);
}

MacroCommand *MainProjectDialog::buildCommand() {
    MacroCommand *m = nullptr;
    KUndo2MagicString c = kundo2_i18n("Modify main project");
    MacroCommand *cmd = panel->buildCommand();
    if (cmd) {
        if (!m) m = new MacroCommand(c);
        m->addCommand(cmd);
    }
    return m;
}

}  //KPlato namespace
