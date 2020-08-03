/* This file is part of the KDE project
  Copyright (C) 2009, 2011 Dag Andersen <dag.andersen@kdemail.net>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version..

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#ifndef TASKWORKPACKAGEMODEL_H
#define TASKWORKPACKAGEMODEL_H

#include "planwork_export.h"

#include "kptitemmodelbase.h"
#include "kptnodeitemmodel.h"
#include "kptschedule.h"

#include <QMetaEnum>


class QModelIndex;
class QAbstractItemDelegate;

namespace KPlato
{

class Project;
class Node;
class Resource;
class Document;

}

using namespace KPlato;

/// The main namespace
namespace KPlatoWork
{

class Part;
class WorkPackage;

/**
 * The TaskWorkPackageModel class gives access to workpackage status
 * for the resources assigned to the task in this package.
 *
 * The model stores a nodes parentNode() in the index's internalPointer().
 */
class PLANWORK_EXPORT TaskWorkPackageModel : public ItemModelBase
{
    Q_OBJECT
public:
    explicit TaskWorkPackageModel(Part *part, QObject *parent = 0);
    ~TaskWorkPackageModel() override {}

    enum Properties {
        NodeName = 0,
        NodeType,
        NodeResponsible,
        NodeDescription,

        // After scheduling
        NodeStartTime,
        NodeEndTime,
        NodeAssignments,

        // Completion
        NodeCompleted,
        NodeActualEffort,
        NodeRemainingEffort,
        NodePlannedEffort,
        NodeActualStart,
        NodeStarted,
        NodeActualFinish,
        NodeFinished,
        NodeStatus,
        NodeStatusNote,

        ProjectName,
        ProjectManager
    };
    Q_ENUM(Properties)
    const QMetaEnum columnMap() const override
    {
        return metaObject()->enumerator(metaObject()->indexOfEnumerator("Properties"));
    }

    WorkPackage *workPackage(int index) const;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override; 
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &index = QModelIndex()) const override;
    
    Node *nodeForIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(Node *node) const;

    QAbstractItemDelegate *createDelegate(int column, QWidget *parent) const override;

    Document *documentForIndex(const QModelIndex &idx) const;

    WorkPackage *ptrToWorkPackage(const QModelIndex &idx) const;
    Node *ptrToNode(const QModelIndex &idx) const;

    bool isNode(const QModelIndex &idx) const;
    bool isDocument(const QModelIndex &idx) const;

public Q_SLOTS:
    void addWorkPackage(KPlatoWork::WorkPackage *package, int row);
    void removeWorkPackage(KPlatoWork::WorkPackage *package, int row);

protected Q_SLOTS:
    void slotNodeChanged(KPlato::Node*);
    void slotNodeToBeInserted(KPlato::Node *node, int row);
    void slotNodeInserted(KPlato::Node *node);
    void slotNodeToBeRemoved(KPlato::Node *node);
    void slotNodeRemoved(KPlato::Node *node);

    void slotDocumentAdded(KPlato::Node *node, KPlato::Document *doc, int index);
    void slotDocumentRemoved(KPlato::Node *node, KPlato::Document *doc, int index);
    void slotDocumentChanged(KPlato::Node *node, KPlato::Document *doc, int index);

protected:
    QVariant nodeData(Node *node, int column, int role) const; 
    QVariant documentData(Document *doc, int column, int role) const; 

    QVariant name(const Resource *r, int role) const;
    QVariant email(const Resource *r, int role) const;
    QVariant sendStatus(const Resource *r, int role) const;
    QVariant sendTime(const Resource *r, int role) const;
    QVariant responseType(const Resource *r, int role) const;
    QVariant requiredTime(const Resource *r, int role) const;
    QVariant responseStatus(const Resource *r, int role) const;
    QVariant responseTime(const Resource *r, int role) const;
    QVariant lastAction(const Resource *r, int role) const;
    QVariant projectName(const Node *n, int role) const;
    QVariant projectManager(const Node *n, int role) const;
    
    bool setCompletion(Node *node, const QVariant &value, int role);
    bool setRemainingEffort(Node *node, const QVariant &value, int role);
    bool setActualEffort(Node *node, const QVariant &value, int role);
    bool setStartedTime(Node *node, const QVariant &value, int role);
    bool setFinishedTime(Node *node, const QVariant &value, int role);

    QVariant actualStart(Node *n, int role) const;
    QVariant actualFinish(Node *n, int role) const;
    QVariant plannedEffort(Node *n, int role) const;

    QVariant status(Node *n, int role) const;

private:
    NodeModel m_nodemodel;
    Part *m_part;
    QList<WorkPackage*> m_packages;
};


} //namespace KPlato

#endif //WORKPACKAGEMODEL_H
