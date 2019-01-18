 /* This file is part of the KDE project
   Copyright (C) 2005 - 2010 Dag Andersen <danders@get2net.dk>

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

#ifndef KPTRESOURCEAPPOINTMENTSVIEW_H
#define KPTRESOURCEAPPOINTMENTSVIEW_H

#include "planui_export.h"

#include "ui_kptresourceappointmentsdisplayoptions.h"

#include "kptviewbase.h"
#include "kptresourceappointmentsmodel.h"

#include <kpagedialog.h>

#include <QDomDocument>

class KoPageLayoutWidget;
class KoDocument;

class QPoint;

namespace KPlato
{

class View;
class Project;
class Resource;
class ResourceGroup;
class ScheduleManager;
class ResourceAppointmentsItemModel;

//-------------------------------------------------
class ResourceAppointmentsDisplayOptionsPanel : public QWidget, public Ui::ResourceAppointmentsDisplayOptions
{
    Q_OBJECT
public:
    explicit ResourceAppointmentsDisplayOptionsPanel( ResourceAppointmentsItemModel *model, QWidget *parent = 0 );

    void setValues( const ResourceAppointmentsItemModel &del );

public Q_SLOTS:
    void slotOk();
    void setDefault();

Q_SIGNALS:
    void changed();

private:
    ResourceAppointmentsItemModel *m_model;
};

class ResourceAppointmentsSettingsDialog : public KPageDialog
{
    Q_OBJECT
public:
    explicit ResourceAppointmentsSettingsDialog( ViewBase *view, ResourceAppointmentsItemModel *model, QWidget *parent = 0, bool selectPrint = false );

public Q_SLOTS:
    void slotOk();

private:
    ViewBase *m_view;
    KoPageLayoutWidget *m_pagelayout;
    PrintingHeaderFooter *m_headerfooter;
};

//------------------------
class PLANUI_EXPORT ResourceAppointmentsTreeView : public DoubleTreeViewBase
{
    Q_OBJECT
public:
    explicit ResourceAppointmentsTreeView(QWidget *parent);

    ResourceAppointmentsItemModel *model() const { return static_cast<ResourceAppointmentsItemModel*>( DoubleTreeViewBase::model() ); }

    Project *project() const { return model()->project(); }
    void setProject( Project *project ) { model()->setProject( project ); }
    void setScheduleManager( ScheduleManager *sm ) { model()->setScheduleManager( sm ); }

    QModelIndex currentIndex() const;
    
    /// Load context info into this view.
    virtual bool loadContext( const KoXmlElement &context );
    using DoubleTreeViewBase::loadContext;
    /// Save context info from this view.
    virtual void saveContext( QDomElement &context ) const;
    using DoubleTreeViewBase::saveContext;

protected Q_SLOTS:
    void slotRefreshed();

private:
    ViewBase *m_view;
};

class PLANUI_EXPORT ResourceAppointmentsView : public ViewBase
{
    Q_OBJECT
public:
    ResourceAppointmentsView(KoPart *part, KoDocument *doc, QWidget *parent);
    
    void setupGui();
    virtual void setProject( Project *project );
    Project *project() const { return m_view->project(); }
    virtual void draw( Project &project );
    virtual void draw();

    ResourceAppointmentsItemModel *model() const { return m_view->model(); }
    
    virtual void updateReadWrite( bool /*readwrite*/ ) {};

    virtual Node *currentNode() const;
    virtual Resource *currentResource() const;
    virtual ResourceGroup *currentResourceGroup() const;
    
    /// Loads context info into this view. Reimplement.
    virtual bool loadContext( const KoXmlElement &/*context*/ );
    /// Save context info from this view. Reimplement.
    virtual void saveContext( QDomElement &/*context*/ ) const;
    
    KoPrintJob *createPrintJob();
    
Q_SIGNALS:
    void addResource(KPlato::ResourceGroup*);
    void deleteObjectList( const QObjectList& );
    
public Q_SLOTS:
    /// Activate/deactivate the gui
    virtual void setGuiActive( bool activate );
    
    void setScheduleManager(KPlato::ScheduleManager *sm);

protected Q_SLOTS:
    virtual void slotOptions();

protected:
    void updateActionsEnabled(  bool on = true );

private Q_SLOTS:
    void slotContextMenuRequested( const QModelIndex &index, const QPoint& pos );
    
    void slotSelectionChanged( const QModelIndexList& );
    void slotCurrentChanged( const QModelIndex& );
    void slotEnableActions( bool on );

    void slotAddResource();
    void slotAddGroup();
    void slotDeleteSelection();

private:
    ResourceAppointmentsTreeView *m_view;

    QAction *actionAddResource;
    QAction *actionAddGroup;
    QAction *actionDeleteSelection;
    QDomDocument m_domdoc;

};

}  //KPlato namespace

#endif // KPTRESOURCEAPPOINTMENTSVIEW_H
