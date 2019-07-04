/* This file is part of the KDE project
  Copyright (C) 2006 - 2011, 2012 Dag Andersen <danders@get2net.dk>

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
#include "kptresourceeditor.h"

#include "kptresourcemodel.h"
#include "kptcommand.h"
#include "kptitemmodelbase.h"
#include "kptcalendar.h"
#include "kptduration.h"
#include "kptnode.h"
#include "kptproject.h"
#include "kpttask.h"
#include "kptresource.h"
#include "kptdatetime.h"
#include "kptitemviewsettup.h"
#include "Help.h"
#include "kptdebug.h"

#include <KoDocument.h>
#include <KoIcon.h>

#include <QList>
#include <QVBoxLayout>
#include <QDragMoveEvent>
#include <QAction>
#include <QMenu>

#include <KLocalizedString>
#include <kactioncollection.h>

namespace KPlato
{


ResourceTreeView::ResourceTreeView( QWidget *parent )
    : DoubleTreeViewBase( parent )
{
    setDragPixmap(koIcon("resource-group").pixmap(32));
//    header()->setContextMenuPolicy( Qt::CustomContextMenu );
    setStretchLastSection( false );
    ResourceItemModel *m = new ResourceItemModel( this );
    setModel( m );
    
    setSelectionMode( QAbstractItemView::ExtendedSelection );
    setSelectionBehavior( QAbstractItemView::SelectRows );

    createItemDelegates( m );

    connect( this, &DoubleTreeViewBase::dropAllowed, this, &ResourceTreeView::slotDropAllowed );

}

void ResourceTreeView::slotDropAllowed( const QModelIndex &index, int dropIndicatorPosition, QDragMoveEvent *event )
{
    event->ignore();
    if ( model()->dropAllowed( index, dropIndicatorPosition, event->mimeData() ) ) {
        event->accept();
    }
}

QObject *ResourceTreeView::currentObject() const
{
    return model()->object( selectionModel()->currentIndex() );
}

QList<QObject*> ResourceTreeView::selectedObjects() const
{
    QList<QObject*> lst;
    foreach (const QModelIndex &i, selectionModel()->selectedRows() ) {
        lst << static_cast<QObject*>( i.internalPointer() );
    }
    return lst;
}

QList<ResourceGroup*> ResourceTreeView::selectedGroups() const
{
    QList<ResourceGroup*> gl;
    foreach ( QObject *o, selectedObjects() ) {
        ResourceGroup* g = qobject_cast<ResourceGroup*>( o );
        if ( g ) {
            gl << g;
        }
    }
    return gl;
}

QList<Resource*> ResourceTreeView::selectedResources() const
{
    QList<Resource*> rl;
    foreach ( QObject *o, selectedObjects() ) {
        Resource* r = qobject_cast<Resource*>( o );
        if ( r ) {
            rl << r;
        }
    }
    return rl;
}

//-----------------------------------
ResourceEditor::ResourceEditor(KoPart *part, KoDocument *doc, QWidget *parent)
    : ViewBase(part, doc, parent)
{
    Help::add(this,
        xi18nc("@info:whatsthis", 
               "<title>Resource Editor</title>"
               "<para>"
               "Resources are organized in a Resource Breakdown Structure. "
               "Resources can be of type <emphasis>Work</emphasis> or <emphasis>Material</emphasis>. "
               "When assigned to a task, a resource of type <emphasis>Work</emphasis> can affect the duration of the task, while a resource of type <emphasis>Material</emphasis> does not. "
               "A resource must refer to a <emphasis>Calendar</emphasis> defined in the <emphasis>Work and Vacation Editor</emphasis>."
               "<nl/><link url='%1'>More...</link>"
               "</para>", Help::page("Manual/Resource_Editor")));

    QVBoxLayout * l = new QVBoxLayout( this );
    l->setMargin( 0 );
    m_view = new ResourceTreeView( this );
    m_doubleTreeView = m_view;
    connect(this, &ViewBase::expandAll, m_view, &DoubleTreeViewBase::slotExpand);
    connect(this, &ViewBase::collapseAll, m_view, &DoubleTreeViewBase::slotCollapse);

    l->addWidget( m_view );
    setupGui();
    
    m_view->setEditTriggers( m_view->editTriggers() | QAbstractItemView::EditKeyPressed );
    m_view->setDragDropMode( QAbstractItemView::DragDrop );
    m_view->setDropIndicatorShown( true );
    m_view->setDragEnabled ( true );
    m_view->setAcceptDrops( true );
//    m_view->setAcceptDropsOnView( true );


    QList<int> lst1; lst1 << 1 << -1;
    QList<int> lst2; lst2 << 0 << ResourceModel::ResourceOvertimeRate;
    m_view->hideColumns( lst1, lst2 );
    
    m_view->masterView()->setDefaultColumns( QList<int>() << 0 );
    QList<int> show;
    for ( int c = 1; c < model()->columnCount(); ++c ) {
        if (c != ResourceModel::ResourceOvertimeRate) {
            show << c;
        }
    }
    m_view->slaveView()->setDefaultColumns( show );

    connect( model(), &ItemModelBase::executeCommand, doc, &KoDocument::addCommand );

    connect( m_view, &DoubleTreeViewBase::currentChanged, this, &ResourceEditor::slotCurrentChanged );

    connect( m_view, &DoubleTreeViewBase::selectionChanged, this, &ResourceEditor::slotSelectionChanged );

    connect( m_view, &DoubleTreeViewBase::contextMenuRequested, this, &ResourceEditor::slotContextMenuRequested );
    
    connect( m_view, &DoubleTreeViewBase::headerContextMenuRequested, this, &ViewBase::slotHeaderContextMenuRequested );

}

void ResourceEditor::updateReadWrite( bool readwrite )
{
    m_view->setReadWrite( readwrite );
}

void ResourceEditor::setProject( Project *project )
{
    debugPlan<<project;
    m_view->setProject( project );
    ViewBase::setProject( project );
}

void ResourceEditor::setGuiActive( bool activate )
{
    debugPlan<<activate;
    updateActionsEnabled( true );
    ViewBase::setGuiActive( activate );
    if ( activate && !m_view->selectionModel()->currentIndex().isValid() ) {
        m_view->selectionModel()->setCurrentIndex(m_view->model()->index( 0, 0 ), QItemSelectionModel::NoUpdate);
    }
}

void ResourceEditor::slotContextMenuRequested( const QModelIndex &index, const QPoint& pos )
{
    //debugPlan<<index.row()<<","<<index.column()<<":"<<pos;
    QString name;
    if ( index.isValid() ) {
        QObject *obj = m_view->model()->object( index );
        ResourceGroup *g = qobject_cast<ResourceGroup*>( obj );
        if ( g ) {
            //name = "resourceeditor_group_popup";
        } else {
            Resource *r = qobject_cast<Resource*>( obj );
            if ( r && !r->isShared() ) {
                name = "resourceeditor_resource_popup";
            }
        }
    }
    m_view->setContextMenuIndex(index);
    if ( name.isEmpty() ) {
        slotHeaderContextMenuRequested( pos );
        m_view->setContextMenuIndex(QModelIndex());
        return;
    }
    emit requestPopupMenu( name, pos );
    m_view->setContextMenuIndex(QModelIndex());
}

Resource *ResourceEditor::currentResource() const
{
    return qobject_cast<Resource*>( m_view->currentObject() );
}

ResourceGroup *ResourceEditor::currentResourceGroup() const
{
    return qobject_cast<ResourceGroup*>( m_view->currentObject() );
}

void ResourceEditor::slotCurrentChanged(  const QModelIndex & )
{
    //debugPlan<<curr.row()<<","<<curr.column();
//    slotEnableActions();
}

void ResourceEditor::slotSelectionChanged( const QModelIndexList& )
{
    //debugPlan<<list.count();
    updateActionsEnabled();
}

void ResourceEditor::slotEnableActions( bool on )
{
    updateActionsEnabled( on );
}

void ResourceEditor::updateActionsEnabled(  bool on )
{
    bool o = on && m_view->project();

    QList<ResourceGroup*> groupList = m_view->selectedGroups();
    bool nogroup = groupList.isEmpty();
    bool group = groupList.count() == 1;
    QList<Resource*> resourceList = m_view->selectedResources();
    bool noresource = resourceList.isEmpty(); 
    bool resource = resourceList.count() == 1;

    bool any = !nogroup || !noresource;

    actionAddResource->setEnabled( o && ( (group  && noresource) || (resource && nogroup) ) );
    actionAddGroup->setEnabled( o );

    if ( o && any ) {
        foreach ( ResourceGroup *g, groupList ) {
            if ( g->isBaselined() ) {
                o = false;
                break;
            }
        }
    }
    if ( o && any ) {
        foreach ( Resource *r, resourceList ) {
            if ( r->isBaselined() ) {
                o = false;
                break;
            }
        }
    }
    actionDeleteSelection->setEnabled( o && any );
}

void ResourceEditor::setupGui()
{
    QString name = "resourceeditor_edit_list";
    actionAddGroup  = new QAction(koIcon("resource-group-new"), i18n("Add Resource Group"), this);
    actionCollection()->addAction("add_group", actionAddGroup );
    actionCollection()->setDefaultShortcut(actionAddGroup, Qt::CTRL + Qt::Key_I);
    connect( actionAddGroup, &QAction::triggered, this, &ResourceEditor::slotAddGroup );
    addAction( name, actionAddGroup );
    
    actionAddResource  = new QAction(koIcon("list-add-user"), i18n("Add Resource"), this);
    actionCollection()->addAction("add_resource", actionAddResource );
    actionCollection()->setDefaultShortcut(actionAddResource, Qt::CTRL + Qt::SHIFT + Qt::Key_I);
    connect( actionAddResource, &QAction::triggered, this, &ResourceEditor::slotAddResource );
    addAction( name, actionAddResource );
    
    actionDeleteSelection  = new QAction(koIcon("edit-delete"), xi18nc("@action", "Delete"), this);
    actionCollection()->addAction("delete_selection", actionDeleteSelection );
    actionCollection()->setDefaultShortcut(actionDeleteSelection, Qt::Key_Delete);
    connect( actionDeleteSelection, &QAction::triggered, this, &ResourceEditor::slotDeleteSelection );
    addAction( name, actionDeleteSelection );
    
    // Add the context menu actions for the view options
    connect(m_view->actionSplitView(), &QAction::triggered, this, &ResourceEditor::slotSplitView);
    addContextAction( m_view->actionSplitView() );
    
    createOptionActions(ViewBase::OptionAll);
    addActionList("viewmenu", contextActionList());
}

void ResourceEditor::slotSplitView()
{
    debugPlan;
    m_view->setViewSplitMode( ! m_view->isViewSplit() );
    emit optionsModified();
}

void ResourceEditor::slotOptions()
{
    debugPlan;
    SplitItemViewSettupDialog *dlg = new SplitItemViewSettupDialog( this, m_view, this );
    dlg->addPrintingOptions(sender()->objectName() == "print_options");
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOptionsFinished(int)));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}


void ResourceEditor::slotAddResource()
{
    //debugPlan;
    QList<ResourceGroup*> gl = m_view->selectedGroups();
    if ( gl.count() > 1 ) {
        return;
    }
    m_view->closePersistentEditor( m_view->selectionModel()->currentIndex() );
    ResourceGroup *g = 0;
    if ( !gl.isEmpty() ) {
        g = gl.first();
    } else {
        QList<Resource*> rl = m_view->selectedResources();
        if ( rl.count() != 1 ) {
            return;
        }
        g = rl.first()->parentGroup();
    }
    if ( g == 0 ) {
        return;
    }
    Resource *r = new Resource();
    if ( g->type() == ResourceGroup::Type_Material ) {
        r->setType( Resource::Type_Material );
    }
    QModelIndex i = m_view->model()->insertResource( g, r );
    if ( i.isValid() ) {
        m_view->selectionModel()->select( i, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect );
        m_view->selectionModel()->setCurrentIndex( i, QItemSelectionModel::NoUpdate );
        m_view->edit( i );
    }

}

void ResourceEditor::slotAddGroup()
{
    //debugPlan;
    m_view->closePersistentEditor( m_view->selectionModel()->currentIndex() );
    ResourceGroup *g = new ResourceGroup();
    QModelIndex i = m_view->model()->insertGroup( g );
    if ( i.isValid() ) {
        m_view->selectionModel()->select( i, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect );
        m_view->selectionModel()->setCurrentIndex( i, QItemSelectionModel::NoUpdate );
        m_view->edit( i );
    }
}

void ResourceEditor::slotDeleteSelection()
{
    QObjectList lst = m_view->selectedObjects();
    //debugPlan<<lst.count()<<" objects";
    if ( ! lst.isEmpty() ) {
        emit deleteObjectList( lst );
        QModelIndex i = m_view->selectionModel()->currentIndex();
        if ( i.isValid() ) {
            m_view->selectionModel()->select( i, QItemSelectionModel::Rows | QItemSelectionModel::ClearAndSelect );
            m_view->selectionModel()->setCurrentIndex( i, QItemSelectionModel::NoUpdate );
        }
    }
}

bool ResourceEditor::loadContext( const KoXmlElement &context )
{
    debugPlan<<objectName();
    ViewBase::loadContext( context );
    return m_view->loadContext( model()->columnMap(), context );
}

void ResourceEditor::saveContext( QDomElement &context ) const
{
    debugPlan<<objectName();
    ViewBase::saveContext( context );
    m_view->saveContext( model()->columnMap(), context );
}

KoPrintJob *ResourceEditor::createPrintJob()
{
    return m_view->createPrintJob( this );
}

void ResourceEditor::slotEditCopy()
{
    m_view->editCopy();
}

} // namespace KPlato
