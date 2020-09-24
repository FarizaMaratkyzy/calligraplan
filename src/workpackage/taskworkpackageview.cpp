/* This file is part of the KDE project
  Copyright (C) 2007 - 2009, 2012 Dag Andersen <dag.andersen@kdemail.net>

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
#include "taskworkpackageview.h"
#include "taskworkpackagemodel.h"
#include "workpackage.h"

#include "part.h"
#include "kptglobal.h"
#include "kptcommand.h"
#include "kptproject.h"
#include "kptschedule.h"
#include "kpteffortcostmap.h"
#include "kptitemviewsettup.h"
#include "calligraplanworksettings.h"

#include <KGanttGraphicsView>
#include <KGanttTreeViewRowController>
#include <KGanttProxyModel>
#include <KGanttDateTimeGrid>
#include <KGanttStyleOptionGanttItem>

#include <KoIcon.h>
#include <KoXmlReader.h>

#include <QDragMoveEvent>
#include <QMenu>
#include <QModelIndex>
#include <QWidget>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QPointer>
#include <QAction>

#include "debugarea.h"

using namespace KPlato;

namespace KPlatoWork
{


TaskWorkPackageTreeView::TaskWorkPackageTreeView(Part *part, QWidget *parent)
    : DoubleTreeViewBase(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    masterView()->header()->setSortIndicatorShown(true);
    masterView()->header()->setSectionsClickable(true);
    slaveView()->header()->setSortIndicatorShown(true);
    slaveView()->header()->setSectionsClickable(true);

    QSortFilterProxyModel *sf = new QSortFilterProxyModel(this);
    TaskWorkPackageModel *m = new TaskWorkPackageModel(part, sf);
    sf->setSourceModel(m);
    setModel(sf);
    //setSelectionBehavior(QAbstractItemView::SelectItems);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setStretchLastSection(false);

    createItemDelegates(m);

    QList<int> lst1; lst1 << 1 << -1; // display column 0 (NodeName) in left view
    masterView()->setDefaultColumns(QList<int>() << TaskWorkPackageModel::NodeName);
    QList<int> show;
    show << TaskWorkPackageModel::NodeCompleted
            << TaskWorkPackageModel::NodeActualEffort
            << TaskWorkPackageModel::NodeRemainingEffort
            << TaskWorkPackageModel::NodePlannedEffort
            << TaskWorkPackageModel::NodeStartTime
            << TaskWorkPackageModel::NodeActualStart
            << TaskWorkPackageModel::NodeEndTime
            << TaskWorkPackageModel::NodeActualFinish
            << TaskWorkPackageModel::ProjectName
            << TaskWorkPackageModel::ProjectManager;

    QList<int> lst2;
    for (int i = 0; i < m->columnCount(); ++i) {
        if (! show.contains(i)) {
            lst2 << i;
        }
    }
    hideColumns(lst1, lst2);
    slaveView()->setDefaultColumns(show);
    setViewSplitMode(false);
    masterView()->setFocus();

    debugPlanWork<<PlanWorkSettings::self()->taskWorkPackageView();

    connect(masterView()->header(), &QHeaderView::sortIndicatorChanged, this, &TaskWorkPackageTreeView::setSortOrder);
    connect(slaveView()->header(), &QHeaderView::sortIndicatorChanged, this, &TaskWorkPackageTreeView::setSortOrder);

    connect(masterView()->header(), &QHeaderView::sectionMoved, this, &TaskWorkPackageTreeView::sectionsMoved);
    connect(slaveView()->header(), &QHeaderView::sectionMoved, this, &TaskWorkPackageTreeView::sectionsMoved);

    masterView()->header()->setSortIndicator(TaskWorkPackageModel::NodeStartTime, Qt::AscendingOrder);
    sf->sort(TaskWorkPackageModel::NodeStartTime, Qt::AscendingOrder);
}

void TaskWorkPackageTreeView::setSortOrder(int col, Qt::SortOrder order)
{
    static_cast<QSortFilterProxyModel*>(model())->setSortRole(Qt::EditRole);
    model()->sort(col, order);
}

TaskWorkPackageModel *TaskWorkPackageTreeView::itemModel() const
{
    return static_cast<TaskWorkPackageModel*>(static_cast<QSortFilterProxyModel*>(model())->sourceModel());
}

Project *TaskWorkPackageTreeView::project() const
{
    return itemModel()->project();
}

Document *TaskWorkPackageTreeView::currentDocument() const
{
    QSortFilterProxyModel *sf = qobject_cast<QSortFilterProxyModel*>(model());
    Q_ASSERT(sf);
    if (sf == nullptr) {
        return nullptr;
    }
    return itemModel()->documentForIndex(sf->mapToSource(selectionModel()->currentIndex()));
}

Node *TaskWorkPackageTreeView::currentNode() const
{
    QSortFilterProxyModel *sf = qobject_cast<QSortFilterProxyModel*>(model());
    Q_ASSERT(sf);
    if (sf == nullptr) {
        return nullptr;
    }
    return itemModel()->nodeForIndex(sf->mapToSource(selectionModel()->currentIndex()));
}

QList<Node*> TaskWorkPackageTreeView::selectedNodes() const
{
    QList<Node*> lst;
    QSortFilterProxyModel *sf = qobject_cast<QSortFilterProxyModel*>(model());
    Q_ASSERT(sf);
    if (sf == nullptr) {
        return lst;
    }
    foreach(const QModelIndex &idx, selectionModel()->selectedIndexes()) {
        QModelIndex i = sf->mapToSource(idx);
        Q_ASSERT(i.isValid() && i.model() == itemModel());
        Node *n = itemModel()->nodeForIndex(i);
        if (n && ! lst.contains(n)) {
            lst << n;
        }
    }
    return lst;
}

void TaskWorkPackageTreeView::setProject(Project *project)
{
    itemModel()->setProject(project);
}

void TaskWorkPackageTreeView::slotActivated(const QModelIndex &index)
{
    debugPlanWork<<index.column();
}

void TaskWorkPackageTreeView::dragMoveEvent(QDragMoveEvent */*event*/)
{
/*    if (dragDropMode() == InternalMove
        && (event->source() != this || !(event->possibleActions() & Qt::MoveAction)))
        return;

    TreeViewBase::dragMoveEvent(event);
    if (! event->isAccepted()) {
        return;
    }
    //QTreeView thinks it's ok to drop
    event->ignore();
    QModelIndex index = indexAt(event->pos());
    if (! index.isValid()) {
        event->accept();
        return; // always ok to drop on main project
    }
    Node *dn = model()->node(index);
    if (dn == 0) {
        errorPlanWork<<"no node to drop on!"
        return; // hmmm
    }
    switch (dropIndicatorPosition()) {
        case AboveItem:
        case BelowItem:
            //dn == sibling
            if (model()->dropAllowed(dn->parentNode(), event->mimeData())) {
                event->accept();
            }
            break;
        case OnItem:
            //dn == new parent
            if (model()->dropAllowed(dn, event->mimeData())) {
                event->accept();
            }
            break;
        default:
            break;
    }*/
}


//-----------------------------------
AbstractView::AbstractView(Part *part, QWidget *parent)
    : QWidget(parent),
      m_part(part)
{
}

void AbstractView::updateReadWrite(bool /*rw*/)
{
}

QList<Node*> AbstractView::selectedNodes() const
{
    return QList<Node*>();
}

Node *AbstractView::currentNode() const
{
    return nullptr;
}

Document *AbstractView::currentDocument() const
{
    return nullptr;
}


void AbstractView::slotHeaderContextMenuRequested(const QPoint &pos)
{
    debugPlanWork;
    QList<QAction*> lst = contextActionList();
    if (! lst.isEmpty()) {
        QMenu::exec(lst, pos, lst.first());
    }
}

void AbstractView::slotContextMenuRequested(const QModelIndex &/*index*/, const QPoint& pos)
{
    return slotHeaderContextMenuRequested(pos);
}

void AbstractView::slotContextMenuRequested(Node *node, const QPoint& pos)
{
    debugPlanWork<<node->name()<<" :"<<pos;
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
    debugPlanWork<<name;
    if (name.isEmpty()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }
    emit requestPopupMenu(name, pos);
}

void AbstractView::slotContextMenuRequested(Document *doc, const QPoint& pos)
{
    debugPlanWork<<doc->url()<<" :"<<pos;
    QString name;
    switch (doc->type()) {
        case Document::Type_Product:
            name = "editdocument_popup";
            break;
        default:
            name = "viewdocument_popup";
            break;
    }
    debugPlanWork<<name;
    if (name.isEmpty()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }
    emit requestPopupMenu(name, pos);
}

void AbstractView::sectionsMoved()
{
}

bool AbstractView::loadContext()
{
    return true;
}

void AbstractView::saveContext()
{
}

KoPrintJob *AbstractView::createPrintJob()
{
    return nullptr;
}

//-----------------------------------
TaskWorkPackageView::TaskWorkPackageView(Part *part, QWidget *parent)
    : AbstractView(part, parent)
{
    debugPlanWork<<"-------------------- creating TaskWorkPackageView -------------------";
    QVBoxLayout * l = new QVBoxLayout(this);
    l->setMargin(0);
    m_view = new TaskWorkPackageTreeView(part, this);
    l->addWidget(m_view);
    setupGui();

    connect(itemModel(), &KPlato::ItemModelBase::executeCommand, part, &Part::addCommand);

    connect(m_view, SIGNAL(contextMenuRequested(QModelIndex,QPoint,QModelIndexList)), SLOT(slotContextMenuRequested(QModelIndex,QPoint)));

    connect(m_view, &KPlato::DoubleTreeViewBase::headerContextMenuRequested, this, &TaskWorkPackageView::slotHeaderContextMenuRequested);

    connect(m_view, &KPlato::DoubleTreeViewBase::selectionChanged, this, &TaskWorkPackageView::slotSelectionChanged);

    loadContext();

    connect(m_view, &TaskWorkPackageTreeView::sectionsMoved, this, &TaskWorkPackageView::sectionsMoved);
}

TaskWorkPackageView::~TaskWorkPackageView()
{
    saveContext();
}

void TaskWorkPackageView::updateReadWrite(bool rw)
{
    m_view->setReadWrite(rw);
}

void TaskWorkPackageView::slotSelectionChanged(const QModelIndexList &/*lst*/)
{
    emit selectionChanged();
}

QList<Node*> TaskWorkPackageView::selectedNodes() const
{
    return m_view->selectedNodes();
}

Node *TaskWorkPackageView::currentNode() const
{
    return m_view->currentNode();
}

Document *TaskWorkPackageView::currentDocument() const
{
    return m_view->currentDocument();
}

void TaskWorkPackageView::slotContextMenuRequested(const QModelIndex &index, const QPoint& pos)
{
    debugPlanWork<<index<<pos;
    if (! index.isValid()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }
    QSortFilterProxyModel *sf = qobject_cast<QSortFilterProxyModel*>(m_view->model());
    Q_ASSERT(sf);
    if (sf == nullptr) {
        return;
    }
    QModelIndex idx = sf->mapToSource(index);
    if (! idx.isValid()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }

    Node *node = itemModel()->nodeForIndex(idx);
    if (node) {
        return slotContextMenuRequested(node, pos);
    }
    Document *doc = itemModel()->documentForIndex(idx);
    if (doc) {
        return slotContextMenuRequested(doc, pos);
    }
    return slotHeaderContextMenuRequested(pos);
}

void TaskWorkPackageView::setupGui()
{
    // Add the context menu actions for the view options
    connect(m_view->actionSplitView(), &QAction::triggered, this, &TaskWorkPackageView::slotSplitView);
    addContextAction(m_view->actionSplitView());

    actionOptions = new QAction(koIcon("configure"), i18n("Configure View..."), this);
    connect(actionOptions, &QAction::triggered, this, &TaskWorkPackageView::slotOptions);
    addContextAction(actionOptions);
}

void TaskWorkPackageView::slotSplitView()
{
    debugPlanWork;
    m_view->setViewSplitMode(! m_view->isViewSplit());
}


void TaskWorkPackageView::slotOptions()
{
    debugPlanWork;
    QPointer<SplitItemViewSettupDialog> dlg = new SplitItemViewSettupDialog(nullptr, m_view, this);
    dlg->exec();
    delete dlg;
}

bool TaskWorkPackageView::loadContext()
{
    KoXmlDocument doc;
    doc.setContent(PlanWorkSettings::self()->taskWorkPackageView());
    KoXmlElement context = doc.namedItem("TaskWorkPackageViewSettings").toElement();
    if (context.isNull()) {
        debugPlanWork<<"No settings";
        return false;
    }
    debugPlanWork<<KoXml::asQDomDocument(doc).toString();
    return m_view->loadContext(itemModel()->columnMap(), context);
}

void TaskWorkPackageView::saveContext()
{
    QDomDocument doc ("TaskWorkPackageView");
    QDomElement context = doc.createElement("TaskWorkPackageViewSettings");
    doc.appendChild(context);
    m_view->saveContext(itemModel()->columnMap(), context);
    PlanWorkSettings::self()->setTaskWorkPackageView(doc.toString());
    debugPlanWork<<"saved context:"<<'\n'<<doc.toString();
}

//-------------------------------------------
GanttItemDelegate::GanttItemDelegate(QObject *parent)
    : KPlato::GanttItemDelegate(parent)
{
    showResources = false;
    showTaskName = true;
    showTaskLinks = false;
    showProgress = true;
    showPositiveFloat = false;
    showNegativeFloat = false;
    showCriticalPath = false;
    showCriticalTasks = false;
    showAppointments = false;
    showNoInformation = false;
    showTimeConstraint = false;
    showSchedulingError = false;
    showStatus = true;

    QLinearGradient b(0., 0., 0., QApplication::fontMetrics().height());
    b.setColorAt(0., Qt::green);
    b.setColorAt(1., Qt::darkGreen);
    m_brushes.insert(Brush_Normal, QBrush(b));

    b.setColorAt(0., Qt::red);
    b.setColorAt(1., Qt::darkRed);
    m_brushes.insert(Brush_Late, QBrush(b));

    b.setColorAt(0., Qt::gray);
    b.setColorAt(1., Qt::darkGray);
    m_brushes.insert(Brush_Finished, QBrush(b));

    b.setColorAt(0., Qt::blue);
    b.setColorAt(1., Qt::darkBlue);
    m_brushes.insert(Brush_ReadyToStart, QBrush(b));

    b.setColorAt(0., Qt::white);
    b.setColorAt(1., Qt::gray);
    m_brushes.insert(Brush_NotReadyToStart, QBrush(b));

    b.setColorAt(0., Qt::white);
    b.setColorAt(1., Qt::white);
    m_brushes.insert(Brush_NotScheduled, QBrush(b));
}

void GanttItemDelegate::paintGanttItem(QPainter* painter, const KGantt::StyleOptionGanttItem& opt, const QModelIndex& idx)
{
    if (!idx.isValid()) return;

    const KGantt::ItemType typ = static_cast<KGantt::ItemType>(idx.data(KGantt::ItemTypeRole).toInt());

    QString txt = itemText(idx, typ);
    QRectF itemRect = opt.itemRect;

//     painter->save();
//     painter->setPen(Qt::blue);
//     painter->drawRect(opt.boundingRect.adjusted(-1., -1., 1., 1.));
//     painter->setPen(Qt::red);
//     painter->drawRect(itemRect);
//     painter->restore();

    QRectF textRect = itemRect;
    if (! txt.isEmpty()) {
        int tw = opt.fontMetrics.horizontalAdvance(txt) + static_cast<int>(itemRect.height()/1.5);
        switch(opt.displayPosition) {
            case KGantt::StyleOptionGanttItem::Left:
                textRect.adjust(-tw, 0.0, 0.0, 0.0);
                break;
            case KGantt::StyleOptionGanttItem::Right:
                textRect.adjust(0.0, 0.0, tw, 0.0);
                break;
            default:
                break;
        }
    }
    painter->save();

    QPen pen = defaultPen(typ);
    if (opt.state & QStyle::State_Selected) pen.setWidth(2*pen.width());
    painter->setPen(pen);

    qreal pw = painter->pen().width()/2.;
    switch(typ) {
    case KGantt::TypeTask:
        if (itemRect.isValid()) {
            pw-=1;
            QRectF r = itemRect;
            r.translate(0., r.height()/6.);
            r.setHeight(2.*r.height()/3.);
            painter->save();
            painter->setBrushOrigin(itemRect.topLeft());
            painter->translate(0.5, 0.5);
            bool normal = true;
            if (showStatus) {
                int state = data(idx, TaskWorkPackageModel::NodeStatus, Qt::EditRole).toInt();
                if (state & Node::State_NotScheduled) {
                    painter->setBrush(m_brushes[ Brush_NotScheduled ]);
                    normal = false;
                } else if (state & Node::State_Finished) {
                    painter->setBrush(m_brushes[ Brush_Finished ]);
                    normal = false;
                } else if (state & Node::State_Started) {
                    if (state & Node::State_Late) {
                        painter->setBrush(m_brushes[ Brush_Late ]);
                        normal = false;
                    }
                } else  {
                    // scheduled, not started, not finished
                    if (state & Node::State_Late) {
                        painter->setBrush(m_brushes[ Brush_Late ]);
                        normal = false;
                    } else if (state & Node::State_NotReadyToStart) {
                        painter->setBrush(m_brushes[ Brush_NotReadyToStart ]);
                        normal = false;
                    } else if (state & Node::State_ReadyToStart) {
                        painter->setBrush(m_brushes[ Brush_ReadyToStart ]);
                        normal = false;
                    }
                }
            } else if (showCriticalTasks) {
                bool critical = data(idx, NodeModel::NodeCritical, Qt::DisplayRole).toBool();
                if (! critical && showCriticalPath) {
                    critical = data(idx, NodeModel::NodeCriticalPath, Qt::DisplayRole).toBool();
                }
                if (critical) {
                    QVariant br = data(idx, NodeModel::NodeCritical, Role::Foreground);
                    painter->setBrush(br.isValid() ? br.value<QBrush>() : m_criticalBrush);
                    normal = false;
                }
            }
            if (normal) {
                painter->setBrush(m_brushes[ Brush_Normal ]);
            }
            painter->drawRect(r);

            if (showProgress) {
                bool ok;
                qreal completion = idx.model()->data(idx, KGantt::TaskCompletionRole).toDouble(&ok);
                if (ok) {
                    qreal h = r.height();
                    QRectF cr(r.x(), r.y()+h/4. + 1,
                            r.width()*completion/100., h/2. - 2);
                    painter->fillRect(cr, painter->pen().brush());
                }
            }
            painter->restore();

            // only Left/Center/Right used
            const Qt::Alignment ta =
                (opt.displayPosition == KGantt::StyleOptionGanttItem::Left)  ? Qt::AlignLeft :
                (opt.displayPosition == KGantt::StyleOptionGanttItem::Right) ? Qt::AlignRight :
                /*                      KGantt::StyleOptionGanttItem::Center*/ Qt::AlignCenter;

            painter->drawText(textRect, ta, txt);
        }
        break;
    default:
        break;
    }
    painter->restore();
}

QModelIndex mapToSource(const QModelIndex &index)
{
    QModelIndex idx = index;
    const QAbstractProxyModel *proxy = qobject_cast<const QAbstractProxyModel*>(idx.model());
    while (proxy) {
        idx = proxy->mapToSource(idx);
        proxy = qobject_cast<const QAbstractProxyModel*>(idx.model());
    }
    return idx;
}

QString GanttItemDelegate::toolTip(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QString();
    }
    // map to source manually, gantt models do some tricks so we only get column 0
    QModelIndex idx = mapToSource(index);
    Q_ASSERT(idx.isValid());
    if (data(idx, TaskWorkPackageModel::NodeFinished, Qt::EditRole).toBool()) {
        // finished
        return xi18nc("@info:tooltip",
                      "Task: %1<nl/>"
                      "Actual finish: %2<nl/>"
                      "Planned finish: %3<nl/>"
                      "Status: %4<nl/>"
                      "Project: %5",
                      idx.data().toString(),
                      data(idx, TaskWorkPackageModel::NodeActualFinish, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::NodeEndTime, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::NodeStatus, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::ProjectName, Qt::DisplayRole).toString()
                      );
    }
    if (data(idx, TaskWorkPackageModel::NodeStarted, Qt::EditRole).toBool()) {
        // started
        return xi18nc("@info:tooltip",
                      "Task: %1<nl/>"
                      "Completion: %2 %<nl/>"
                      "Actual start: %3<nl/>"
                      "Planned: %4 - %5<nl/>"
                      "Status: %6<nl/>"
                      "Project: %7",
                      idx.data().toString(),
                      data(idx, TaskWorkPackageModel::NodeCompleted, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::NodeActualStart, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::NodeStartTime, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::NodeEndTime, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::NodeStatus, Qt::DisplayRole).toString(),
                      data(idx, TaskWorkPackageModel::ProjectName, Qt::DisplayRole).toString()
                      );
    }
    // Planned
    KGantt::StyleOptionGanttItem opt;
    int typ = data(idx, NodeModel::NodeType, Qt::EditRole).toInt();
    switch (typ) {
        case Node::Type_Task:
            return xi18nc("@info:tooltip",
                          "Task: %1<nl/>"
                          "Planned: %2 - %3<nl/>"
                          "Status: %4<nl/>"
                          "Project: %5",
                          idx.data().toString(),
                          data(idx, TaskWorkPackageModel::NodeStartTime, Qt::DisplayRole).toString(),
                          data(idx, TaskWorkPackageModel::NodeEndTime, Qt::DisplayRole).toString(),
                          data(idx, TaskWorkPackageModel::NodeStatus, Qt::DisplayRole).toString(),
                          data(idx, TaskWorkPackageModel::ProjectName, Qt::DisplayRole).toString()
                          );
    }
    return QString();
}

GanttView::GanttView(Part *part, QWidget *parent)
    : KPlato::GanttViewBase(parent),
      m_part(part),
      m_project(nullptr),
      m_ganttdelegate(new GanttItemDelegate(this)),
      m_itemmodel(new TaskWorkPackageModel(part, this))
{
    debugPlanWork<<"------------------- create GanttView -----------------------";
    m_itemmodel->setObjectName("Gantt model");
    graphicsView()->setItemDelegate(m_ganttdelegate);
    GanttTreeView *tv = new GanttTreeView(this);
    tv->setSortingEnabled(true);
    tv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    tv->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // needed since qt 4.2
    setLeftView(tv);
    m_rowController = new KGantt::TreeViewRowController(tv, ganttProxyModel());
    setRowController(m_rowController);
    tv->header()->setStretchLastSection(true);

    QSortFilterProxyModel *sf = new QSortFilterProxyModel(tv);
    sf->setSortRole(Qt::EditRole);
    sf->setSourceModel(m_itemmodel);
    KGantt::View::setModel(sf);

    QList<int> show;
    show << TaskWorkPackageModel::NodeName << TaskWorkPackageModel::NodeDescription;
    tv->setDefaultColumns(show);
    for (int i = 0; i < m_itemmodel->columnCount(); ++i) {
        if (! show.contains(i)) {
            tv->hideColumn(i);
        }
    }
    debugPlanWork<<"mapping roles";
    KGantt::ProxyModel *m = static_cast<KGantt::ProxyModel*>(ganttProxyModel());

    m->setRole(KGantt::ItemTypeRole, KGantt::ItemTypeRole); // To provide correct format
    m->setRole(KGantt::StartTimeRole, Qt::EditRole); // To provide correct format
    m->setRole(KGantt::EndTimeRole, Qt::EditRole); // To provide correct format

    m->setColumn(KGantt::ItemTypeRole, TaskWorkPackageModel::NodeType);
    m->setColumn(KGantt::StartTimeRole, TaskWorkPackageModel::NodeStartTime);
    m->setColumn(KGantt::EndTimeRole, TaskWorkPackageModel::NodeEndTime);
    m->setColumn(KGantt::TaskCompletionRole, TaskWorkPackageModel::NodeCompleted);
    debugPlanWork<<"roles mapped";

    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    g->setDayWidth(30);
    // TODO: extend QLocale/KGantt to support formats for hourly time display
    // see bug #349030
    // removed custom code here

    for (int i = 0; i < part->workPackageCount(); ++i) {
        updateDateTimeGrid(part->workPackage(i));
    }
    connect(m_itemmodel, &QAbstractItemModel::rowsInserted, this, &GanttView::slotRowsInserted);
    connect(m_itemmodel, &QAbstractItemModel::rowsRemoved, this, &GanttView::slotRowsRemoved);

    connect(tv, &KPlato::TreeViewBase::contextMenuRequested, this, &GanttView::contextMenuRequested);
    connect(tv, &KPlato::TreeViewBase::headerContextMenuRequested, this, &GanttView::headerContextMenuRequested);

    connect(tv->selectionModel(), &QItemSelectionModel::selectionChanged, this, &GanttView::slotSelectionChanged);

    connect(tv->header(), &QHeaderView::sectionMoved, this, &GanttView::sectionsMoved);

    tv->header()->setSortIndicator(TaskWorkPackageModel::NodeStartTime, Qt::AscendingOrder);
    sf->sort(TaskWorkPackageModel::NodeStartTime, Qt::AscendingOrder);
}

GanttView::~GanttView()
{
    delete m_rowController;
}

void GanttView::slotSelectionChanged(const QItemSelection &selected, const QItemSelection&)
{
    emit selectionChanged(selected.indexes());
}

void GanttView::slotRowsInserted(const QModelIndex &parent, int start, int end)
{
    debugPlanWork<<parent<<start<<end;
    if (! parent.isValid()) {
        for (int i = start; i <= end; ++i) {
            updateDateTimeGrid(m_itemmodel->workPackage(i));
        }
    }
}

void GanttView::slotRowsRemoved(const QModelIndex &/*parent*/, int /*start*/, int /*end*/)
{
    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    QDateTime newStart;
    for (int i = 0; i < m_part->workPackageCount(); ++i) {
        WorkPackage *wp = m_part->workPackage(i);
        Task *task = static_cast<Task*>(wp->project()->childNode(0));
        if (!newStart.isValid() || newStart > task->startTime()) {
            newStart = task->startTime();
        }
    }
    if (newStart.isValid()) {
        g->setStartDateTime(newStart);
    }
}

void GanttView::updateDateTimeGrid(WorkPackage *wp)
{
    debugPlanWork<<wp;
    if (! wp || ! wp->project() || ! wp->project()->childNode(0)) {
        return;
    }
    Task *task = static_cast<Task*>(wp->project()->childNode(0));
    DateTime st = task->startTime();
    if (! st.isValid() && task->completion().startTime().isValid()) {
        st = qMin(st, task->completion().startTime());
    }
    if (! st.isValid()) {
        return;
    }
    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    QDateTime gst = g->startDateTime();
    if (! gst.isValid() || gst > st) {
        st.setTime(QTime(0, 0, 0, 0));
        g->setStartDateTime(st);
    }
}

TaskWorkPackageModel *GanttView::itemModel() const
{
    return m_itemmodel;
}

void GanttView::setProject(Project *project)
{
    itemModel()->setProject(project);
    m_project = project;
}

QList<Node*> GanttView::selectedNodes() const
{
    QList<Node*> nodes;
    foreach(const QModelIndex &idx, treeView()->selectionModel()->selectedRows()) {
        nodes << itemModel()->nodeForIndex(idx);
    }
    return nodes;
}

Node *GanttView::currentNode() const
{
    return itemModel()->nodeForIndex(treeView()->selectionModel()->currentIndex());
}

Document *GanttView::currentDocument() const
{
    return itemModel()->documentForIndex(treeView()->selectionModel()->currentIndex());
}

bool GanttView::loadContext(const KoXmlElement &context)
{
    KoXmlElement e = context.namedItem("itemview").toElement();
    if (! e.isNull()) {
        treeView()->loadContext(itemModel()->columnMap(), e);
    }
    e = context.namedItem("ganttview").toElement();
    if (! e.isNull()) {
        KPlato::GanttViewBase::loadContext(e);
    }
    return true;
}

void GanttView::saveContext(QDomElement &context) const
{
    QDomElement e = context.ownerDocument().createElement("itemview");
    context.appendChild(e);
    treeView()->saveContext(itemModel()->columnMap(), e);
    e = context.ownerDocument().createElement("ganttview");
    context.appendChild(e);
    KPlato::GanttViewBase::saveContext(e);
}

//-----------------------------------
TaskWPGanttView::TaskWPGanttView(Part *part, QWidget *parent)
    : AbstractView(part, parent)
{
    debugPlanWork<<"-------------------- creating TaskWPGanttView -------------------";
    QVBoxLayout * l = new QVBoxLayout(this);
    l->setMargin(0);
    m_view = new GanttView(part, this);
    l->addWidget(m_view);

    setupGui();

    connect(itemModel(), &KPlato::ItemModelBase::executeCommand, part, &Part::addCommand);

    connect(m_view, SIGNAL(contextMenuRequested(QModelIndex,QPoint)), SLOT(slotContextMenuRequested(QModelIndex,QPoint)));
    connect(m_view, &GanttView::headerContextMenuRequested, this, &TaskWPGanttView::slotHeaderContextMenuRequested);

    connect(m_view, &GanttView::selectionChanged, this, &TaskWPGanttView::slotSelectionChanged);

    connect(m_view, &GanttView::sectionsMoved, this, &TaskWPGanttView::sectionsMoved);
}

TaskWPGanttView::~TaskWPGanttView()
{
    saveContext();
}

void TaskWPGanttView::slotSelectionChanged(const QModelIndexList& /*lst*/)
{
    emit selectionChanged();
}

QList<Node*> TaskWPGanttView::selectedNodes() const
{
    return m_view->selectedNodes();
}

Node *TaskWPGanttView::currentNode() const
{
    return m_view->currentNode();
}

Document *TaskWPGanttView::currentDocument() const
{
    return m_view->currentDocument();
}

void TaskWPGanttView::slotContextMenuRequested(const QModelIndex &idx, const QPoint& pos)
{
    debugPlanWork<<idx<<pos;
    if (! idx.isValid()) {
        slotHeaderContextMenuRequested(pos);
        return;
    }
    Node *node = itemModel()->nodeForIndex(idx);
    if (node) {
        return slotContextMenuRequested(node, pos);
    }
    Document *doc = itemModel()->documentForIndex(idx);
    if (doc) {
        return slotContextMenuRequested(doc, pos);
    }
    return slotHeaderContextMenuRequested(pos);
}

void TaskWPGanttView::setupGui()
{
    actionOptions = new QAction(koIcon("configure"), i18n("Configure View..."), this);
    connect(actionOptions, &QAction::triggered, this, &TaskWPGanttView::slotOptions);
    addContextAction(actionOptions);
}

void TaskWPGanttView::slotOptions()
{
    debugPlanWork;
    QPointer<ItemViewSettupDialog> dlg = new ItemViewSettupDialog(nullptr, m_view->treeView(), true, this);
    dlg->exec();
    delete dlg;
}

bool TaskWPGanttView::loadContext()
{
    KoXmlDocument doc;
    doc.setContent(PlanWorkSettings::self()->taskWPGanttView());
    KoXmlElement context = doc.namedItem("TaskWPGanttViewSettings").toElement();
    if (context.isNull()) {
        debugPlanWork<<"No settings";
        return false;
    }
    return m_view->loadContext(context);
}

void TaskWPGanttView::saveContext()
{
    QDomDocument doc ("TaskWPGanttView");
    QDomElement context = doc.createElement("TaskWPGanttViewSettings");
    doc.appendChild(context);
    m_view->saveContext(context);
    PlanWorkSettings::self()->setTaskWPGanttView(doc.toString());
    debugPlanWork<<'\n'<<doc.toString();
}

} // namespace KPlatoWork
