/* This file is part of the KDE project
   Copyright (C) 2004-2007 Dag Andersen <dag.andersen@kdemail.net>

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

#ifndef KPTMAINPROJECTPANEL_H
#define KPTMAINPROJECTPANEL_H

#include "planui_export.h"

#include "ui_kptmainprojectpanelbase.h"

#include <QWidget>

class QDateTime;

namespace KPlato
{

class Project;
class MacroCommand;
class TaskDescriptionPanel;
class DocumentsPanel;

class MainProjectPanel : public QWidget, public Ui_MainProjectPanelBase {
    Q_OBJECT
public:
    explicit MainProjectPanel(Project &project, QWidget *parent=nullptr);

    virtual QDateTime startDateTime();
    virtual QDateTime endDateTime();

    MacroCommand *buildCommand();

    bool ok();

    bool loadSharedResources() const;

    void initTaskModules();
    MacroCommand *buildTaskModulesCommand();

public Q_SLOTS:
    virtual void slotCheckAllFieldsFilled();
    virtual void slotChooseLeader();
    virtual void slotStartDateClicked();
    virtual void slotEndDateClicked();
    virtual void enableDateTime();

    void insertTaskModuleClicked();
    void removeTaskModuleClicked();
    void taskModulesSelectionChanged();

private Q_SLOTS:
    void openResourcesFile();
    void openProjectsPlace();
    void loadProjects();
    void clearProjects();

Q_SIGNALS:
    void obligatedFieldsFilled(bool);
    void changed();
    void loadResourceAssignments(QUrl url);
    void clearResourceAssignments();

private:
    Project &project;
    DocumentsPanel *m_documents;
    TaskDescriptionPanel *m_description;
};


}  //KPlato namespace

#endif // MAINPROJECTPANEL_H
