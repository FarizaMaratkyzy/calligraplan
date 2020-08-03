/* This file is part of the KDE project
  Copyright (C) 2007 Florian Piquemal <flotueur@yahoo.fr>
  Copyright (C) 2007 Alexis Ménard <darktears31@gmail.com>
  Copyright (C) 2007 Dag Andersen <dag.andersen@kdemail.net>

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

#ifndef KPTPERTEDITOR_H
#define KPTPERTEDITOR_H

#include "planui_export.h"

#include "kptviewbase.h"
#include "kptitemmodelbase.h"
#include "kpttaskeditor.h"
#include <ui_kptperteditor.h>

#include "kptcommand.h"
#include "kptnode.h"

#include <QList>


class KoDocument;

class QTreeWidgetItem;
class QTableWidgetItem;
class QModelIndex;
class KUndo2Command;

namespace KPlato
{

class View;
class Project;
class RelationTreeView;

class PLANUI_EXPORT PertEditor : public ViewBase
{
    Q_OBJECT
public:

    enum Roles { NodeRole = Qt::UserRole + 1, EnabledRole };
    
    PertEditor(KoPart *part, KoDocument *doc, QWidget *parent);
    void updateReadWrite(bool readwrite) override;
    void setProject(Project *project) override;
    Project *project() const override { return m_project; }
    void draw(Project &project) override;
    void draw() override;
    void drawSubTasksName(QTreeWidgetItem *parent,Node * currentNode);
    void clearRequiredList();
    void loadRequiredTasksList(Node * taskNode);
    Node *itemToNode(QTreeWidgetItem *item);
    QTreeWidgetItem *nodeToItem(Node *node, QTreeWidgetItem *item);
    QList<Node*> listNodeNotView(Node * node);

    void updateAvailableTasks(QTreeWidgetItem *item = 0);
    void setAvailableItemEnabled(QTreeWidgetItem *item);
    void setAvailableItemEnabled(Node *node);
    
Q_SIGNALS:
    void executeCommand(KUndo2Command*);

protected:
    bool isInRequiredList(Node *node);
    QTreeWidgetItem *findNodeItem(Node *node, QTreeWidgetItem *item);
    QTableWidgetItem *findRequiredItem(Node *node);
    
private Q_SLOTS:
    void slotNodeAdded(KPlato::Node*);
    void slotNodeRemoved(KPlato::Node*);
    void slotNodeMoved(KPlato::Node*);
    void slotNodeChanged(KPlato::Node*);
    void slotRelationAdded(KPlato::Relation *rel);
    void slotRelationRemoved(KPlato::Relation *rel);
    
    void dispAvailableTasks();
    void dispAvailableTasks(KPlato::Node *parent, KPlato::Node *selectedTask);
    void dispAvailableTasks(KPlato::Relation *rel);
    void addTaskInRequiredList(QTreeWidgetItem * currentItem);
    void removeTaskFromRequiredList();
    void slotUpdate();

    void slotCurrentTaskChanged(QTreeWidgetItem *curr, QTreeWidgetItem *prev);
    void slotAvailableChanged(QTreeWidgetItem *item);
    void slotRequiredChanged(const QModelIndex &index);
    void slotAddClicked();
    void slotRemoveClicked();

private:
    Project * m_project;
    QTreeWidget *m_tasktree;
    QTreeWidget *m_availableList;
    RelationTreeView *m_requiredList;
    
    Ui::PertEditor widget;
};

}  //KPlato namespace

#endif
