/* This file is part of the KDE project
   Copyright (C) 2005 - 2010, 2012 Dag Andersen <dag.andersen@kdemail.net>

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
#include "kptresourceappointmentsview.h"

#include "kptappointment.h"
#include "kptcommand.h"
#include "kpteffortcostmap.h"
#include "kptitemmodelbase.h"
#include "kptcalendar.h"
#include "kptduration.h"
#include "kptnode.h"
#include "kptproject.h"
#include "kpttask.h"
#include "kptresource.h"
#include "kptdatetime.h"
#include "kptitemviewsettup.h"
#include "kptviewbase.h"
#include "Help.h"
#include "kptdebug.h"

#include "KoPageLayoutWidget.h"
#include <KoDocument.h>
#include <KoXmlReader.h>

#include <KLocalizedString>

#include <QList>
#include <QVBoxLayout>
#include <QTabWidget>


namespace KPlato
{


//--------------------

ResourceAppointmentsDisplayOptionsPanel::ResourceAppointmentsDisplayOptionsPanel(ResourceAppointmentsItemModel *model, QWidget *parent)
    : QWidget(parent),
    m_model(model)
{
    setupUi(this);
    setValues(*model);

    connect(ui_internalAppointments, &QCheckBox::stateChanged, this, &ResourceAppointmentsDisplayOptionsPanel::changed);
    connect(ui_externalAppointments, &QCheckBox::stateChanged, this, &ResourceAppointmentsDisplayOptionsPanel::changed);
}

void ResourceAppointmentsDisplayOptionsPanel::slotOk()
{
    m_model->setShowInternalAppointments(ui_internalAppointments->checkState() == Qt::Checked);
    m_model->setShowExternalAppointments(ui_externalAppointments->checkState() == Qt::Checked);
}

void ResourceAppointmentsDisplayOptionsPanel::setValues(const ResourceAppointmentsItemModel &m)
{
    ui_internalAppointments->setCheckState(m.showInternalAppointments() ? Qt::Checked : Qt::Unchecked);
    ui_externalAppointments->setCheckState(m.showExternalAppointments() ? Qt::Checked : Qt::Unchecked);
}

void ResourceAppointmentsDisplayOptionsPanel::setDefault()
{
    ResourceAppointmentsItemModel m;
    setValues(m);
}

//----
ResourceAppointmentsSettingsDialog::ResourceAppointmentsSettingsDialog(ViewBase *view, ResourceAppointmentsItemModel *model, QWidget *parent, bool selectPrint)
    : KPageDialog(parent),
    m_view(view)
{
    ResourceAppointmentsDisplayOptionsPanel *panel = new ResourceAppointmentsDisplayOptionsPanel(model);
    KPageWidgetItem *page = addPage(panel, i18n("General"));
    page->setHeader(i18n("Resource Assignments View Settings"));

    QTabWidget *tab = new QTabWidget();

    QWidget *w = ViewBase::createPageLayoutWidget(view);
    tab->addTab(w, w->windowTitle());
    m_pagelayout = w->findChild<KoPageLayoutWidget*>();
    Q_ASSERT(m_pagelayout);

    m_headerfooter = ViewBase::createHeaderFooterWidget(view);
    m_headerfooter->setOptions(view->printingOptions());
    tab->addTab(m_headerfooter, m_headerfooter->windowTitle());

    page = addPage(tab, i18n("Printing"));
    page->setHeader(i18n("Printing Options"));
    if (selectPrint) {
        setCurrentPage(page);
    }
    connect(this, &QDialog::accepted, this, &ResourceAppointmentsSettingsDialog::slotOk);
    connect(this, &QDialog::accepted, panel, &ResourceAppointmentsDisplayOptionsPanel::slotOk);
    //TODO: there was no default button configured, should there?
//     connect(button(QDialogButtonBox::RestoreDefaults), SIGNAL(clicked(bool)), panel, SLOT(setDefault()));
}

void ResourceAppointmentsSettingsDialog::slotOk()
{
    m_view->setPageLayout(m_pagelayout->pageLayout());
    m_view->setPrintingOptions(m_headerfooter->options());
}

//---------------------------------------
ResourceAppointmentsTreeView::ResourceAppointmentsTreeView(QWidget *parent)
    : DoubleTreeViewBase(true, parent)
{
//    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_rightview->setStretchLastSection(false);

    ResourceAppointmentsItemModel *m = new ResourceAppointmentsItemModel(this);
    setModel(m);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    QList<int> lst1; lst1 << 2 << -1;
    QList<int> lst2; lst2 << 0 << 1;
    hideColumns(lst1, lst2);

    m_leftview->resizeColumnToContents (1);
    connect(m, &QAbstractItemModel::modelReset, this, &ResourceAppointmentsTreeView::slotRefreshed);

    m_rightview->setObjectName("ResourceAppointments");
}

bool ResourceAppointmentsTreeView::loadContext(const KoXmlElement &context)
{
    debugPlan;
    KoXmlElement e = context.namedItem("common").toElement();
    if (! e.isNull()) {
        model()->setShowInternalAppointments((bool)(e.attribute("show-internal-appointments", "0").toInt()));
        model()->setShowExternalAppointments((bool)(e.attribute("show-external-appointments", "0").toInt()));
    }
    DoubleTreeViewBase::loadContext(QMetaEnum(), context);
    return true;
}

void ResourceAppointmentsTreeView::saveContext(QDomElement &settings) const
{
    debugPlan;
    QDomElement e = settings.ownerDocument().createElement("common");
    settings.appendChild(e);
    e.setAttribute("show-internal-appointments", QString::number(model()->showInternalAppointments()));
    e.setAttribute("show-external-appointments", QString::number(model()->showExternalAppointments()));

    DoubleTreeViewBase::saveContext(QMetaEnum(), settings);
}

void ResourceAppointmentsTreeView::slotRefreshed()
{
    //debugPlan<<model()->columnCount()<<", "<<m_leftview->header()->count()<<", "<<m_rightview->header()->count()<<", "<<m_leftview->header()->hiddenSectionCount()<<", "<<m_rightview->header()->hiddenSectionCount();
    ResourceAppointmentsItemModel *m = model();
    setModel(0);
    setModel(m);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    QList<int> lst1; lst1 << 2 << -1;
    QList<int> lst2; lst2 << 0 << 1;
    hideColumns(lst1, lst2);
}

QModelIndex ResourceAppointmentsTreeView::currentIndex() const
{
    return selectionModel()->currentIndex();
}

//-----------------------------------

ResourceAppointmentsView::ResourceAppointmentsView(KoPart *part, KoDocument *doc, QWidget *parent)
    : ViewBase(part, doc, parent)
{
    debugPlan<<"------------------- ResourceAppointmentsView -----------------------";
    setXMLFile("ResourceAppointmentsViewUi.rc");
    setupGui();

    QVBoxLayout * l = new QVBoxLayout(this);
    l->setMargin(0);
    m_view = new ResourceAppointmentsTreeView(this);
    connect(this, &ViewBase::expandAll, m_view, &DoubleTreeViewBase::slotExpand);
    connect(this, &ViewBase::collapseAll, m_view, &DoubleTreeViewBase::slotCollapse);

    l->addWidget(m_view);

    m_view->setEditTriggers(m_view->editTriggers() | QAbstractItemView::EditKeyPressed);

    connect(model(), &ItemModelBase::executeCommand, doc, &KoDocument::addCommand);

    connect(m_view, &DoubleTreeViewBase::currentChanged, this, &ResourceAppointmentsView::slotCurrentChanged);

    connect(m_view, &DoubleTreeViewBase::selectionChanged, this, &ResourceAppointmentsView::slotSelectionChanged);

    connect(m_view, &DoubleTreeViewBase::contextMenuRequested, this, &ResourceAppointmentsView::slotContextMenuRequested);

    connect(m_view, &DoubleTreeViewBase::headerContextMenuRequested, this, &ViewBase::slotHeaderContextMenuRequested);

    Help::add(this,
              xi18nc("@info:whatsthis",
                     "<title>Resource Assignments View</title>"
                     "<para>"
                     "Displays the scheduled resource - task assignments."
                     "</para><para>"
                     "This view supports configuration and printing using the context menu."
                     "<nl/><link url='%1'>More...</link>"
                     "</para>", Help::page("Resource_Assignment_View")));
}

void ResourceAppointmentsView::draw(Project &project)
{
    setProject(&project);
}

void ResourceAppointmentsView::setProject(Project *project)
{
    m_view->setProject(project);
}

void ResourceAppointmentsView::setScheduleManager(ScheduleManager *sm)
{
    if (!sm && scheduleManager()) {
        // we should only get here if the only schedule manager is scheduled,
        // or when last schedule manager is deleted
        m_domdoc.clear();
        QDomElement element = m_domdoc.createElement("expanded");
        m_domdoc.appendChild(element);
        m_view->masterView()->saveExpanded(element);
    }
    bool tryexpand = sm && !scheduleManager();
    bool expand = sm && scheduleManager() && sm != scheduleManager();
    QDomDocument doc;
    if (expand) {
        QDomElement element = doc.createElement("expanded");
        doc.appendChild(element);
        m_view->masterView()->saveExpanded(element);
    }
    ViewBase::setScheduleManager(sm);
    m_view->setScheduleManager(sm);

    if (expand) {
        m_view->masterView()->doExpand(doc);
    } else if (tryexpand) {
        m_view->masterView()->doExpand(m_domdoc);
    }
}

void ResourceAppointmentsView::draw()
{
}

void ResourceAppointmentsView::setGuiActive(bool activate)
{
    debugPlan<<activate;
    updateActionsEnabled(true);
    ViewBase::setGuiActive(activate);
    if (activate && !m_view->selectionModel()->currentIndex().isValid()) {
        m_view->selectionModel()->setCurrentIndex(m_view->model()->index(0, 0), QItemSelectionModel::NoUpdate);
    }
}

void ResourceAppointmentsView::slotContextMenuRequested(const QModelIndex &index, const QPoint& pos)
{
    debugPlan<<index<<pos;
    QString name;
    if (index.isValid()) {
        Node *n = m_view->model()->node(index);
        if (n) {
            name = "taskview_popup";
        }
    }
    m_view->setContextMenuIndex(index);
    if (name.isEmpty()) {
        slotHeaderContextMenuRequested(pos);
        m_view->setContextMenuIndex(QModelIndex());
        return;
    }
    emit requestPopupMenu(name, pos);
    m_view->setContextMenuIndex(QModelIndex());
}

Node *ResourceAppointmentsView::currentNode() const
{
    return m_view->model()->node(m_view->currentIndex());
}

Resource *ResourceAppointmentsView::currentResource() const
{
    //return qobject_cast<Resource*>(m_view->currentObject());
    return 0;
}

ResourceGroup *ResourceAppointmentsView::currentResourceGroup() const
{
    //return qobject_cast<ResourceGroup*>(m_view->currentObject());
    return 0;
}

void ResourceAppointmentsView::slotCurrentChanged(const QModelIndex &)
{
    //debugPlan<<curr.row()<<", "<<curr.column();
//    slotEnableActions();
}

void ResourceAppointmentsView::slotSelectionChanged(const QModelIndexList&)
{
    //debugPlan<<list.count();
    updateActionsEnabled();
}

void ResourceAppointmentsView::slotEnableActions(bool on)
{
    updateActionsEnabled(on);
}

void ResourceAppointmentsView::updateActionsEnabled(bool /*on*/)
{
/*    bool o = on && m_view->project();

    QList<ResourceGroup*> groupList = m_view->selectedGroups();
    bool nogroup = groupList.isEmpty();
    bool group = groupList.count() == 1;
    QList<Resource*> resourceList = m_view->selectedResources();
    bool noresource = resourceList.isEmpty();
    bool resource = resourceList.count() == 1;

    bool any = !nogroup || !noresource;

    actionAddResource->setEnabled(o && ((group  && noresource) || (resource && nogroup)));
    actionAddGroup->setEnabled(o);
    actionDeleteSelection->setEnabled(o && any);*/
}

void ResourceAppointmentsView::setupGui()
{
    // Add the context menu actions for the view options
    createOptionActions(ViewBase::OptionAll);
}

void ResourceAppointmentsView::slotOptions()
{
    debugPlan;
    ResourceAppointmentsSettingsDialog *dlg = new ResourceAppointmentsSettingsDialog(this, m_view->model(), this, sender()->objectName() == "print_options");
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOptionsFinished(int)));
    dlg->open();
}


void ResourceAppointmentsView::slotAddResource()
{
    //debugPlan;
/*    QList<ResourceGroup*> gl = m_view->selectedGroups();
    if (gl.count() > 1) {
        return;
    }
    ResourceGroup *g = 0;
    if (!gl.isEmpty()) {
        g = gl.first();
    } else {
        QList<Resource*> rl = m_view->selectedResources();
        if (rl.count() != 1) {
            return;
        }
        g = rl.first()->parentGroup();
    }
    if (g == 0) {
        return;
    }
    Resource *r = new Resource();
    QModelIndex i = m_view->model()->insertResource(g, r);
    if (i.isValid()) {
        m_view->selectionModel()->setCurrentIndex(i, QItemSelectionModel::NoUpdate);
        m_view->edit(i);
    }
*/
}

void ResourceAppointmentsView::slotAddGroup()
{
    //debugPlan;
/*    ResourceGroup *g = new ResourceGroup();
    QModelIndex i = m_view->model()->insertGroup(g);
    if (i.isValid()) {
        m_view->selectionModel()->setCurrentIndex(i, QItemSelectionModel::NoUpdate);
        m_view->edit(i);
    }*/
}

void ResourceAppointmentsView::slotDeleteSelection()
{
/*    QObjectList lst = m_view->selectedObjects();
    //debugPlan<<lst.count()<<" objects";
    if (! lst.isEmpty()) {
        emit deleteObjectList(lst);
    }*/
}

bool ResourceAppointmentsView::loadContext(const KoXmlElement &context)
{
    ViewBase::loadContext(context);
    return m_view->loadContext(context);
}

void ResourceAppointmentsView::saveContext(QDomElement &context) const
{
    ViewBase::saveContext(context);
    m_view->saveContext(context);
}

KoPrintJob *ResourceAppointmentsView::createPrintJob()
{
    return m_view->createPrintJob(this);
}

} // namespace KPlato
