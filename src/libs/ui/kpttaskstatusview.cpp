/* This file is part of the KDE project
 * Copyright (C) 2019 Dag Andersen <dag.andersen@kdemail.net>
 * Copyright (C) 2007 - 2010, 2012 Dag Andersen <dag.andersen@kdemail.net>
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

// clazy:excludeall=qstring-arg
#include "kpttaskstatusview.h"
#include "kpttaskstatusmodel.h"

#include "kptglobal.h"
#include "kptlocale.h"
#include "kptcommonstrings.h"
#include "kptcommand.h"
#include "kptproject.h"
#include "kptschedule.h"
#include "kpteffortcostmap.h"
#include "Help.h"
#include "kpttaskdescriptiondialog.h"
#include "kptdebug.h"

#include <KoXmlReader.h>
#include "KoDocument.h"
#include "KoPageLayoutWidget.h"
#include <KoIcon.h>

#include <KActionCollection>

#include <QDragMoveEvent>
#include <QModelIndex>
#include <QVBoxLayout>
#include <QPushButton>
#include <QItemSelection>
#include <QApplication>
#include <QResizeEvent>
#include <QTimer>
#include <QAction>

#include <KChartChart>
#include <KChartAbstractCoordinatePlane>
#include <KChartBarDiagram>
#include <KChartLineDiagram>
#include <KChartCartesianAxis>
#include <KChartCartesianCoordinatePlane>
#include <KChartLegend>
#include <KChartBackgroundAttributes>
#include <KChartGridAttributes>


using namespace KChart;

using namespace KPlato;


TaskStatusTreeView::TaskStatusTreeView(QWidget *parent)
    : DoubleTreeViewBase(parent)
{
    setDragPixmap(koIcon("view-task").pixmap(32));
    setContextMenuPolicy(Qt::CustomContextMenu);
    TaskStatusItemModel *m = new TaskStatusItemModel(this);
    setModel(m);
    //setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setStretchLastSection(false);

    createItemDelegates(m);

    QList<int> lst1; lst1 << 1 << -1; // only display column 0 (NodeName) in left view
    masterView()->setDefaultColumns(QList<int>() << 0);
    QList<int> show;
    show << NodeModel::NodeCompleted
            << NodeModel::NodeActualEffort
            << NodeModel::NodeRemainingEffort
            << NodeModel::NodePlannedEffort
            << NodeModel::NodePlannedCost
            << NodeModel::NodeActualCost
            << NodeModel::NodeStatus
            << NodeModel::NodeActualStart
            << NodeModel::NodeActualFinish
            << NodeModel::NodeDescription;

    QList<int> lst2;
    for (int i = 0; i < m->columnCount(); ++i) {
        if (! show.contains(i)) {
            lst2 << i;
        }
    }
    hideColumns(lst1, lst2);
    slaveView()->setDefaultColumns(show);
    for (int i = 0; i < show.count(); ++i) {
        slaveView()->mapToSection(show.at(i), i);
    }
}

int TaskStatusTreeView::weekday() const
{
    return model()->weekday();
}

void TaskStatusTreeView::setWeekday(int day)
{
    model()->setWeekday(day);
    refresh();
}

int TaskStatusTreeView::defaultPeriodType() const
{
    return TaskStatusItemModel::UseCurrentDate;
}

int TaskStatusTreeView::periodType() const
{
    return model()->periodType();
}

void TaskStatusTreeView::setPeriodType(int type)
{
    model()->setPeriodType(type);
    refresh();
}

int TaskStatusTreeView::period() const
{
    return model()->period();
}

void TaskStatusTreeView::setPeriod(int days)
{
    model()->setPeriod(days);
    refresh();
}

TaskStatusItemModel *TaskStatusTreeView::model() const
{
    return static_cast<TaskStatusItemModel*>(DoubleTreeViewBase::model());
}

Project *TaskStatusTreeView::project() const
{
    return model()->project();
}

void TaskStatusTreeView::setProject(Project *project)
{
    model()->setProject(project);
}


//-----------------------------------
TaskStatusView::TaskStatusView(KoPart *part, KoDocument *doc, QWidget *parent)
    : ViewBase(part, doc, parent),
    m_id(-1)
{
    debugPlan<<"-------------------- creating TaskStatusView -------------------";
    setXMLFile("TaskStatusViewUi.rc");

    QVBoxLayout * l = new QVBoxLayout(this);
    l->setMargin(0);
    m_view = new TaskStatusTreeView(this);
    m_doubleTreeView = m_view;
    connect(this, &ViewBase::expandAll, m_view, &DoubleTreeViewBase::slotExpand);
    connect(this, &ViewBase::collapseAll, m_view, &DoubleTreeViewBase::slotCollapse);

    m_view->setDragDropMode(QAbstractItemView::DragOnly);
    m_view->setDropIndicatorShown(false);
    m_view->setDragEnabled (true);
    m_view->setAcceptDrops(false);
    m_view->setAcceptDropsOnView(false);

    l->addWidget(m_view);
    setupGui();

    connect(model(), &ItemModelBase::executeCommand, doc, &KoDocument::addCommand);

    connect(m_view, SIGNAL(contextMenuRequested(QModelIndex,QPoint,QModelIndexList)), SLOT(slotContextMenuRequested(QModelIndex,QPoint)));

    connect(m_view, &DoubleTreeViewBase::headerContextMenuRequested, this, &ViewBase::slotHeaderContextMenuRequested);

    m_view->masterView()->setExpandsOnDoubleClick(true);
    m_view->masterView()->setExpandsOnDoubleClick(false);
    connect(m_view->masterView(), &TreeViewBase::doubleClicked, this, &TaskStatusView::itemDoubleClicked);
    connect(m_view->slaveView(), &TreeViewBase::doubleClicked, this, &TaskStatusView::itemDoubleClicked);

    Help::add(this,
              xi18nc("@info:whatsthis", 
                     "<title>Task Status View</title>"
                     "<para>"
                     "The Task Status View is used to inspect task progress information. "
                     "The tasks are divided into groups dependent on the task status:"
                     "<list>"
                     "<item>Not Started 	Tasks that should have been started by now.</item>"
                     "<item>Running 	Tasks that has been started, but not yet finished.</item>"
                     "<item>Finished 	Tasks that where finished in this period.</item>"
                     "<item>Next Period 	Tasks that is scheduled to be started in the next period.</item>"
                     "</list>"
                     "The time period is configurable."
                     "</para><para>"
                     "This view supports configuration and printing using the context menu."
                     "<nl/><link url='%1'>More...</link>"
                     "</para>", Help::page("Task_Status_View")));
    
}

void TaskStatusView::itemDoubleClicked(const QPersistentModelIndex &idx)
{
    if (idx.column() == NodeModel::NodeDescription) {
        emit openTaskDescription(isReadWrite() && (idx.flags() & Qt::ItemIsEditable));
    }
}

void TaskStatusView::updateReadWrite(bool rw)
{
    m_view->setReadWrite(rw);
}

void TaskStatusView::setScheduleManager(ScheduleManager *sm)
{
    //debugPlan;
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
    static_cast<TaskStatusItemModel*>(m_view->model())->setScheduleManager(sm);

    if (expand) {
        m_view->masterView()->doExpand(doc);
    } else if (tryexpand) {
        m_view->masterView()->doExpand(m_domdoc);
    }
}

Node *TaskStatusView::currentNode() const
{
    return m_view->model()->node(m_view->selectionModel()->currentIndex());
}

void TaskStatusView::setProject(Project *project)
{
    m_project = project;
    m_view->model()->setProject(m_project);
}

void TaskStatusView::draw(Project &project)
{
    setProject(&project);
}

void TaskStatusView::setGuiActive(bool activate)
{
    debugPlan<<activate;
//    updateActionsEnabled(true);
    ViewBase::setGuiActive(activate);
}

void TaskStatusView::slotContextMenuRequested(const QModelIndex &index, const QPoint& pos)
{
    debugPlan<<index<<pos;
    if (! index.isValid()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }
    m_view->setContextMenuIndex(index);
    Node *node = m_view->model()->node(index);
    if (node == nullptr) {
        slotHeaderContextMenuRequested(pos);
        m_view->setContextMenuIndex(QModelIndex());
        return;
    }
    slotContextMenuRequested(node, pos);
    m_view->setContextMenuIndex(QModelIndex());
}

void TaskStatusView::slotContextMenuRequested(Node *node, const QPoint& pos)
{
    debugPlan<<node->name()<<" :"<<pos;
    QString name;
    switch (node->type()) {
        case Node::Type_Task:
            name = "taskstatus_popup";
            break;
        case Node::Type_Milestone:
            name = "taskview_milestone_popup";
            break;
        case Node::Type_Summarytask:
            name = "taskview_summary_popup";
            break;
        default:
            break;
    }
    debugPlan<<name;
    if (name.isEmpty()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }
    emit requestPopupMenu(name, pos);
}

void TaskStatusView::setupGui()
{
    // Add the context menu actions for the view options
    actionCollection()->addAction(m_view->actionSplitView()->objectName(), m_view->actionSplitView());
    connect(m_view->actionSplitView(), &QAction::triggered, this, &TaskStatusView::slotSplitView);
    addContextAction(m_view->actionSplitView());

    createOptionActions(ViewBase::OptionAll);
}

void TaskStatusView::slotSplitView()
{
    debugPlan;
    m_view->setViewSplitMode(! m_view->isViewSplit());
    emit optionsModified();
}

void TaskStatusView::slotRefreshView()
{
    model()->refresh();
}

void TaskStatusView::slotOptions()
{
    debugPlan;
    TaskStatusViewSettingsDialog *dlg = new TaskStatusViewSettingsDialog(this, m_view, this);
    dlg->addPrintingOptions(sender()->objectName() == "print_options");
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOptionsFinished(int)));
    dlg->open();
}

bool TaskStatusView::loadContext(const KoXmlElement &context)
{
    debugPlan;
    ViewBase::loadContext(context);
    m_view->setPeriod(context.attribute("period", QString("%1").arg(m_view->defaultPeriod())).toInt());

    m_view->setPeriodType(context.attribute("periodtype", QString("%1").arg(m_view->defaultPeriodType())).toInt());

    m_view->setWeekday(context.attribute("weekday", QString("%1").arg(m_view->defaultWeekday())).toInt());
    return m_view->loadContext(model()->columnMap(), context);
}

void TaskStatusView::saveContext(QDomElement &context) const
{
    ViewBase::saveContext(context);
    context.setAttribute("period", QString::number(m_view->period()));
    context.setAttribute("periodtype", QString::number(m_view->periodType()));
    context.setAttribute("weekday", QString::number(m_view->weekday()));
    m_view->saveContext(model()->columnMap(), context);
}

KoPrintJob *TaskStatusView::createPrintJob()
{
    return m_view->createPrintJob(this);
}


//------------------------------------------------
TaskStatusViewSettingsPanel::TaskStatusViewSettingsPanel(TaskStatusTreeView *view, QWidget *parent)
    : QWidget(parent),
    m_view(view)
{
    setupUi(this);

    QStringList lst;
    QLocale locale;
    for (int i = 1; i <= 7; ++i) {
        lst << locale.dayName(i, QLocale::ShortFormat);
    }
    weekdays->addItems(lst);
    period->setValue(view->period());
    switch (view->periodType()) {
        case TaskStatusItemModel::UseCurrentDate: useCurrentDate->setChecked(true); break;
        case TaskStatusItemModel::UseWeekday: useWeekday->setChecked(true); break;
        default: break;
    }
    weekdays->setCurrentIndex(m_view->weekday() - 1);

    connect(period, SIGNAL(valueChanged(int)), SIGNAL(changed()));
    connect(useWeekday, &QAbstractButton::toggled, this, &TaskStatusViewSettingsPanel::changed);
    connect(useCurrentDate, &QAbstractButton::toggled, this, &TaskStatusViewSettingsPanel::changed);
    connect(weekdays, SIGNAL(currentIndexChanged(int)), SIGNAL(changed()));
}

void TaskStatusViewSettingsPanel::slotOk()
{
    if (period->value() != m_view->period()) {
        m_view->setPeriod(period->value());
    }
    if (weekdays->currentIndex() != m_view->weekday() - 1) {
        m_view->setWeekday(weekdays->currentIndex() + 1);
    }
    if (useCurrentDate->isChecked() && m_view->periodType() != TaskStatusItemModel::UseCurrentDate) {
        m_view->setPeriodType(TaskStatusItemModel::UseCurrentDate);
    } else if (useWeekday->isChecked() && m_view->periodType() != TaskStatusItemModel::UseWeekday) {
        m_view->setPeriodType(TaskStatusItemModel::UseWeekday);
    }
}

void TaskStatusViewSettingsPanel::setDefault()
{
    period->setValue(m_view->defaultPeriod());
    switch (m_view->defaultPeriodType()) {
        case TaskStatusItemModel::UseCurrentDate: useCurrentDate->setChecked(true); break;
        case TaskStatusItemModel::UseWeekday: useWeekday->setChecked(true); break;
        default: break;
    }
    weekdays->setCurrentIndex(m_view->defaultWeekday() - 1);
}

TaskStatusViewSettingsDialog::TaskStatusViewSettingsDialog(ViewBase *view, TaskStatusTreeView *treeview, QWidget *parent)
    : SplitItemViewSettupDialog(view, treeview, parent)
{
    TaskStatusViewSettingsPanel *panel = new TaskStatusViewSettingsPanel(treeview);
    KPageWidgetItem *page = insertWidget(0, panel, i18n("General"), i18n("General Settings"));
    setCurrentPage(page);
    //connect(panel, SIGNAL(changed(bool)), this, SLOT(enableButtonOk(bool)));

    connect(this, &QDialog::accepted, panel, &TaskStatusViewSettingsPanel::slotOk);
    connect(button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, panel, &TaskStatusViewSettingsPanel::setDefault);
}
