/* This file is part of the KDE project
 *  Copyright (C) 2019 Dag Andersen <danders@get2net.dk>
 *  Copyright (C) 2005 Dag Andersen <danders@get2net.dk>
 *  Copyright (C) 2006 Raphael Langerhorst <raphael.langerhorst@kdemail.net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

// clazy:excludeall=qstring-arg
#include "kptganttview.h"
#include "kptnodeitemmodel.h"
#include "kptappointment.h"
#include "kptnode.h"
#include "kptproject.h"
#include "kpttask.h"
#include "kptresource.h"
#include "kptrelation.h"
#include "kptschedule.h"
#include "kptviewbase.h"
#include "kptitemviewsettup.h"
#include "kptduration.h"
#include "kptdatetime.h"
#include "kptresourceappointmentsmodel.h"
#include "Help.h"
#include "kptdebug.h"
#include "gantt/DateTimeTimeLine.h"
#include "gantt/DateTimeGrid.h"
#include "config.h"

#include <KGanttProxyModel>
#include <KGanttConstraintModel>
#include <KGanttConstraint>
#include <KGanttGraphicsView>
#include <KGanttTreeViewRowController>

#include <KoDocument.h>
#include <KoXmlReader.h>
#include <KoPageLayoutWidget.h>
#include <KoIcon.h>

#include <QSplitter>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QDateTime>
#include <QModelIndex>
#include <QPainter>
#include <QTabWidget>
#include <QPushButton>
#include <QLocale>
#include <QAction>
#include <QMenu>
#include <QHoverEvent>
#include <QScrollBar>
#include <QDrag>
#include <QClipboard>
#include <QAbstractSlider>
#include <QGraphicsTextItem>
#include <QFont>
#include <QFontMetricsF>

#include <ktoggleaction.h>
#include <KActionCollection>

#include <KGanttGlobal>
#include <KGanttStyleOptionGanttItem>


/// The main namespace
namespace KPlato
{

class GanttItemDelegate;

//-------------------------------------------------
GanttChartDisplayOptionsPanel::GanttChartDisplayOptionsPanel(GanttViewBase *gantt, GanttItemDelegate *delegate, QWidget *parent)
    : QWidget(parent)
    , m_delegate(delegate)
    , m_gantt(gantt)
{
    setupUi(this);
    setValues(*delegate);

    connect(ui_showTaskName, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showResourceNames, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showDependencies, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showPositiveFloat, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showNegativeFloat, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showCriticalPath, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showCriticalTasks, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showCompletion, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showSchedulingError, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
    connect(ui_showTimeConstraint, &QCheckBox::stateChanged, this, &GanttChartDisplayOptionsPanel::changed);
}

void GanttChartDisplayOptionsPanel::slotOk()
{
    m_delegate->showTaskName = ui_showTaskName->checkState() == Qt::Checked;
    m_delegate->showResources = ui_showResourceNames->checkState() == Qt::Checked;
    m_delegate->showTaskLinks = ui_showDependencies->checkState() == Qt::Checked;
    m_delegate->showPositiveFloat = ui_showPositiveFloat->checkState() == Qt::Checked;
    m_delegate->showNegativeFloat = ui_showNegativeFloat->checkState() == Qt::Checked;
    m_delegate->showCriticalPath = ui_showCriticalPath->checkState() == Qt::Checked;
    m_delegate->showCriticalTasks = ui_showCriticalTasks->checkState() == Qt::Checked;
    m_delegate->showProgress = ui_showCompletion->checkState() == Qt::Checked;
    m_delegate->showSchedulingError = ui_showSchedulingError->checkState() == Qt::Checked;
    m_delegate->showTimeConstraint = ui_showTimeConstraint->checkState() == Qt::Checked;

    DateTimeTimeLine *timeline = m_gantt->timeLine();

    timeline->setInterval(ui_timeLineInterval->value() * 60000);
    QPen pen;
    pen.setWidth(ui_timeLineStroke->value());
    pen.setColor(ui_timeLineColor->color());
    timeline->setPen(pen);

    DateTimeTimeLine::Options opt = timeline->options();
    opt.setFlag(DateTimeTimeLine::Foreground, ui_timeLineForeground->isChecked());
    opt.setFlag(DateTimeTimeLine::Background, ui_timeLineBackground->isChecked());
    opt.setFlag(DateTimeTimeLine::UseCustomPen, ui_timeLineUseCustom->isChecked());
    timeline->setOptions(opt);
}

void GanttChartDisplayOptionsPanel::setValues(const GanttItemDelegate &del)
{
    ui_showTaskName->setCheckState(del.showTaskName ? Qt::Checked : Qt::Unchecked);
    ui_showResourceNames->setCheckState(del.showResources ? Qt::Checked : Qt::Unchecked);
    ui_showDependencies->setCheckState(del.showTaskLinks ? Qt::Checked : Qt::Unchecked);
    ui_showPositiveFloat->setCheckState(del.showPositiveFloat ? Qt::Checked : Qt::Unchecked);
    ui_showNegativeFloat->setCheckState(del.showNegativeFloat ? Qt::Checked : Qt::Unchecked);
    ui_showCriticalPath->setCheckState(del.showCriticalPath ? Qt::Checked : Qt::Unchecked);
    ui_showCriticalTasks->setCheckState(del.showCriticalTasks ? Qt::Checked : Qt::Unchecked);
    ui_showCompletion->setCheckState(del.showProgress ? Qt::Checked : Qt::Unchecked);
    ui_showSchedulingError->setCheckState(del.showSchedulingError ? Qt::Checked : Qt::Unchecked);
    ui_showTimeConstraint->setCheckState(del.showTimeConstraint ? Qt::Checked : Qt::Unchecked);

    DateTimeTimeLine *timeline = m_gantt->timeLine();

    ui_timeLineInterval->setValue(timeline->interval() / 60000);

    QPen pen = timeline->pen();
    ui_timeLineStroke->setValue(pen.width());
    ui_timeLineColor->setColor(pen.color());

    ui_timeLineHide->setChecked(true);
    DateTimeTimeLine::Options opt = timeline->options();
    ui_timeLineBackground->setChecked(opt & DateTimeTimeLine::Background);
    ui_timeLineForeground->setChecked(opt & DateTimeTimeLine::Foreground);
    ui_timeLineUseCustom->setChecked(opt & DateTimeTimeLine::UseCustomPen);
}

void GanttChartDisplayOptionsPanel::setDefault()
{
    GanttItemDelegate del;
    setValues(del);
}

//----
GanttViewSettingsDialog::GanttViewSettingsDialog(GanttViewBase *gantt, GanttItemDelegate *delegate, ViewBase *view, bool selectPrint)
    : ItemViewSettupDialog(view, gantt->treeView(), true, view),
    m_gantt(gantt)
{
    GanttChartDisplayOptionsPanel *panel = new GanttChartDisplayOptionsPanel(gantt, delegate);
    /*KPageWidgetItem *page = */insertWidget(1, panel, i18n("Chart"), i18n("Gantt Chart Settings"));
    QTabWidget *tab = new QTabWidget();
    QWidget *w = ViewBase::createPageLayoutWidget(view);
    tab->addTab(w, w->windowTitle());
    m_pagelayout = w->findChild<KoPageLayoutWidget*>();
    Q_ASSERT(m_pagelayout);

    m_printingoptions = new GanttPrintingOptionsWidget(this);
    QRectF rect = m_gantt->graphicsView()->sceneRect();
    QDateTime start = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid())->mapToDateTime(rect.left());
    QDateTime end = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid())->mapToDateTime(rect.right());
    m_printingoptions->ui_startTime->setMinimumDateTime(start);
    m_printingoptions->ui_startTime->setMaximumDateTime(end);
    m_printingoptions->ui_endTime->setMinimumDateTime(start);
    m_printingoptions->ui_endTime->setMaximumDateTime(end);
    m_printingoptions->setOptions(gantt->printingOptions());
    tab->addTab(m_printingoptions, m_printingoptions->windowTitle());
    KPageWidgetItem *page = insertWidget(2, tab, i18n("Printing"), i18n("Printing Options"));
    if (selectPrint) {
        setCurrentPage(page);
    }
    connect(this, SIGNAL(accepted()), this, SLOT(slotOk()));
    connect(this, &QDialog::accepted, panel, &GanttChartDisplayOptionsPanel::slotOk);
    connect(button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, panel, &GanttChartDisplayOptionsPanel::setDefault);
}

void GanttViewSettingsDialog::slotOk()
{
    debugPlan;
    m_gantt->setPrintingOptions(m_printingoptions->options());
    ItemViewSettupDialog::slotOk();
}

//-------------------------
GanttPrintingOptions::GanttPrintingOptions()
    : useStartTime(false)
    , useEndTime(false)
{
}

GanttPrintingOptions::GanttPrintingOptions(const GanttPrintingOptions &other)
{
    operator=(other);
}

GanttPrintingOptions &GanttPrintingOptions::operator=(const GanttPrintingOptions &other)
{
    context = other.context;
    useStartTime = other.useStartTime;
    diagramStart = other.diagramStart;
    useEndTime = other.useEndTime;
    diagramEnd = other.diagramEnd;
    return *this;
}

bool GanttPrintingOptions::loadContext(const KoXmlElement &settings)
{
    KoXmlElement e = settings.namedItem("print-options").toElement();
    if (! e.isNull()) {
        context.setDrawRowLabels((bool)(e.attribute("print-rowlabels", "1").toInt()));
        context.setDrawColumnLabels((bool)(e.attribute("print-columnlabels", "1").toInt()));
        if ((e.attribute("print-singlepage", "0").toInt())) {
            context.setFitting(KGantt::PrintingContext::FitSinglePage);
        } else if ((bool)(e.attribute("print-pageheight", "0").toInt())) {
            context.setFitting(KGantt::PrintingContext::FitPageHeight);
        } else if ((bool)(e.attribute("print-nofitting", "1").toInt())) {
            context.setFitting(KGantt::PrintingContext::NoFitting);
        }
        useStartTime = (bool)(e.attribute("print-use-starttime", "0").toInt());
        diagramStart = QDateTime::fromString(e.attribute("print-starttime"), Qt::ISODate);
        useEndTime = (bool)(e.attribute("print-use-endtime", "0").toInt());
        diagramEnd = QDateTime::fromString(e.attribute("print-endtime"), Qt::ISODate);
    }
    return true;
}

void GanttPrintingOptions::saveContext(QDomElement &settings) const
{
    QDomElement e = settings.ownerDocument().createElement("print-options");
    settings.appendChild(e);
    e.setAttribute("print-rowlabels", QString::number(context.drawRowLabels()));
    e.setAttribute("print-columnlabels", QString::number(context.drawColumnLabels()));
    if (context.fitting() & KGantt::PrintingContext::FitSinglePage) {
        e.setAttribute("print-singlepage", QString::number(1));
    } else if (context.fitting() & KGantt::PrintingContext::FitPageHeight) {
        e.setAttribute("print-pageheight", QString::number(1));
    } else {
        e.setAttribute("print-nofitting", QString::number(1));
    }
    e.setAttribute("print-use-starttime", QString::number(useStartTime));
    e.setAttribute("print-starttime", diagramStart.toString(Qt::ISODate));
    e.setAttribute("print-use-endtime", QString::number(useEndTime));
    e.setAttribute("print-endtime", diagramEnd.toString(Qt::ISODate));
}



GanttPrintingOptionsWidget::GanttPrintingOptionsWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    setWindowTitle(xi18nc("@title:tab", "Chart"));
}

GanttPrintingOptions GanttPrintingOptionsWidget::options() const
{
    GanttPrintingOptions opt;
    opt.context.setDrawRowLabels(printRowLabels());
    opt.context.setDrawColumnLabels(printColumnLabels());
    opt.context.setFitting(fitting());
    opt.useStartTime = ui_startTimeBtn->isChecked();
    opt.diagramStart = ui_startTime->dateTime();
    opt.useEndTime = ui_endTimeBtn->isChecked();
    opt.diagramEnd = ui_endTime->dateTime();
    return opt;
}

void GanttPrintingOptionsWidget::setOptions(const GanttPrintingOptions &opt)
{
    setPrintRowLabels(opt.context.drawRowLabels());
    setPrintColumnLabels(opt.context.drawColumnLabels());
    setFitting(opt.context.fitting());
    ui_projectStartBtn->setChecked(!opt.useStartTime);
    ui_startTimeBtn->setChecked(opt.useStartTime);
    ui_startTime->setDateTime(opt.diagramStart);
    ui_projectEndBtn->setChecked(!opt.useEndTime);
    ui_endTimeBtn->setChecked(opt.useEndTime);
    ui_endTime->setDateTime(opt.diagramEnd);
}


//----------------
GanttPrintingDialog::GanttPrintingDialog(ViewBase *view, GanttViewBase *gantt)
    : PrintingDialog(view, QPrinter::HighResolution),
    m_gantt(gantt),
    m_options(0)
{
    m_headerHeight = gantt->treeView()->header()->height(); // same header hight
    m_sceneRect = gantt->graphicsView()->sceneRect();
    const GanttPrintingOptions opt = m_gantt->m_printOptions;
    if (opt.useStartTime) {
        qreal x = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid())->mapFromDateTime(opt.diagramStart);
        m_sceneRect.setX(x);
    }
    if (opt.useEndTime) {
        qreal x = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid())->mapFromDateTime(opt.diagramEnd);
        m_sceneRect.setRight(x);
    }
    m_horPages = 1;
    qreal c = m_sceneRect.width() - printer().pageRect().width();
    while (c > 0) {
        ++m_horPages;
        c -= printer().pageRect().width();
    }
    m_vertPages = 1;
    c = m_sceneRect.height() - printer().pageRect().height() - m_headerHeight;
    while (c > 0) {
        ++m_vertPages;
        c -= printer().pageRect().height();
    }
    debugPlan<<m_sceneRect<<printer().pageRect()<<m_horPages<<m_vertPages;
    printer().setFromTo(documentFirstPage(), documentLastPage());
}

QRectF GanttPrintingDialog::calcSceneRect(const QDateTime &startDateTime, const QDateTime &endDateTime) const
{
    //qInfo()<<Q_FUNC_INFO<<startDateTime<<endDateTime;
    QRectF rect = m_gantt->graphicsView()->sceneRect();
    if (!startDateTime.isValid() && !endDateTime.isValid()) {
        return rect;
    }
    const KGantt::DateTimeGrid *grid = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);

    QDateTime startTime = startDateTime.isValid() ? startDateTime : grid->mapToDateTime(rect.left());
    rect.setLeft(std::max(rect.left(), grid->mapFromDateTime(startTime)));

    QDateTime endTime = endDateTime.isValid() ? endDateTime : grid->mapToDateTime(rect.right());
    rect.setRight(std::min(rect.right(), grid->mapFromDateTime(endTime)));

    QTreeView *tv = qobject_cast<QTreeView*>(m_gantt->leftView());
    Q_ASSERT(tv);
    QModelIndexList items;
    for (QModelIndex idx = tv->indexAt(QPoint(0, 0)); idx.isValid(); idx = tv->indexBelow(idx)) {
        items << idx;
    }
    const int startRole = static_cast<KGantt::ProxyModel*>(m_gantt->ganttProxyModel())->role(KGantt::StartTimeRole);
    const int endRole = static_cast<KGantt::ProxyModel*>(m_gantt->ganttProxyModel())->role(KGantt::EndTimeRole);
    const int startCol = static_cast<KGantt::ProxyModel*>(m_gantt->ganttProxyModel())->column(KGantt::StartTimeRole);
    const int endCol = static_cast<KGantt::ProxyModel*>(m_gantt->ganttProxyModel())->column(KGantt::EndTimeRole);

    // Find the first item that end after rect.left AND start before rect.right
    // Remove all items that is completely outside rect
    bool startFound = false;
    QModelIndexList lst = items;
    for (const QModelIndex &idx : qAsConst(lst)) {
        const QDateTime sdt = idx.sibling(idx.row(), startCol).data(startRole).toDateTime();
        const QDateTime edt = idx.sibling(idx.row(), endCol).data(endRole).toDateTime();
        //qInfo()<<Q_FUNC_INFO<<idx<<"start:"<<startTime<<idx.data().toString()<<idx.sibling(idx.row(), endCol).data(endRole);
        if (edt <= startTime) {
            items.removeAll(idx);
            continue;
        }
        if (!startFound && sdt >= endTime) {
            items.removeAll(idx);
            continue;
        }
        if (!startFound) {
            rect.setTop(tv->visualRect(idx).top());
            startFound = true;
            //qInfo()<<Q_FUNC_INFO<<idx<<"start first:"<<idx.data().toString()<<rect.top();
        }
    }
    // All items ending before rect.left has been removed,
    // so now find the first item (from bottom) that start before rect.right
    for (int i = items.count() - 1; i >= 0; --i) {
        const QModelIndex idx = items.at(i);
        //qInfo()<<Q_FUNC_INFO<<idx<<"end:"<<endTime<<idx.data().toString()<<idx.sibling(idx.row(), startCol).data(startRole);
        const QDateTime sdt = idx.sibling(idx.row(), startCol).data(startRole).toDateTime();
        if (!sdt.isValid() || endTime < sdt) {
            continue;
        }
        rect.setBottom(tv->visualRect(idx).bottom());
        //qInfo()<<Q_FUNC_INFO<<idx<<"end last:"<<idx.data().toString()<<rect.bottom();
        break;
    }
    return rect;
}

qreal GanttPrintingDialog::rowLabelsWidth(const QPaintDevice *device) const
{
    qreal labelsWidth = 0.0;
    QFont sceneFont(m_gantt->graphicsView()->font(), device);
    QGraphicsTextItem dummyTextItem( QLatin1String("X") );
    dummyTextItem.adjustSize();
    QFontMetricsF fm(dummyTextItem.font());
    sceneFont.setPixelSize( fm.height() );

    /* row labels */
    QVector<QGraphicsTextItem*> textLabels;
    qreal charWidth = QFontMetricsF(sceneFont).boundingRect(QString::fromLatin1("X")).width();
    const auto *summaryHandlingModel = m_gantt->graphicsView()->summaryHandlingModel();
    const auto *rowController = m_gantt->graphicsView()->rowController();
    QModelIndex sidx = summaryHandlingModel->mapToSource(summaryHandlingModel->index(0, 0));
    do {
        QModelIndex idx = summaryHandlingModel->mapFromSource(sidx);
        const KGantt::Span rg=rowController->rowGeometry(sidx);
        const QString txt = idx.data(Qt::DisplayRole).toString();
        qreal textWidth = QFontMetricsF(sceneFont).boundingRect(txt).width() + charWidth;
        labelsWidth = std::max(labelsWidth, textWidth);
    } while ((sidx = rowController->indexBelow(sidx)).isValid());
    //qInfo()<<Q_FUNC_INFO<<labelsWidth;
    return labelsWidth;
}

void GanttPrintingDialog::startPrinting(RemovePolicy removePolicy)
{
    if (printer().fromPage() <= 0) {
        return;
    }
    KGantt::PrintingContext ctx = m_gantt->m_printOptions.context;
    //qInfo()<<Q_FUNC_INFO<<m_gantt->m_printOptions.context<<':'<<ctx;
    QDateTime start, end;
    if (m_gantt->m_printOptions.useStartTime) {
        start = m_gantt->m_printOptions.diagramStart;
    }
    if (m_gantt->m_printOptions.useEndTime) {
        end = m_gantt->m_printOptions.diagramEnd;
    }
    ctx.setSceneRect(calcSceneRect(start, end));
    //qInfo()<<Q_FUNC_INFO<<m_gantt->m_printOptions.context<<':'<<ctx;
    printer().setFullPage(true);
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    m_gantt->printDiagram(&printer(), ctx);
    QApplication::restoreOverrideCursor();
}

QList<QWidget*> GanttPrintingDialog::createOptionWidgets() const
{
    //debugPlan;
    GanttPrintingOptionsWidget *w = new GanttPrintingOptionsWidget();
    QRectF rect = m_gantt->graphicsView()->sceneRect();
    QDateTime start = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid())->mapToDateTime(rect.left());
    QDateTime end = qobject_cast<KGantt::DateTimeGrid*>(m_gantt->grid())->mapToDateTime(rect.right());
    w->ui_startTime->setMinimumDateTime(start);
    w->ui_startTime->setMaximumDateTime(end);
    w->ui_endTime->setMinimumDateTime(start);
    w->ui_endTime->setMaximumDateTime(end);
    w->setOptions(m_gantt->m_printOptions);
    const_cast<GanttPrintingDialog*>(this)->m_options = w;
    //qInfo()<<Q_FUNC_INFO<<w<<rect<<start<<end;

    return QList<QWidget*>() << createPageLayoutWidget() << m_options;
}

void GanttPrintingDialog::slotPrintRowLabelsToogled(bool on)
{
    m_gantt->m_printOptions.context.setDrawRowLabels(on);
}

void GanttPrintingDialog::slotSinglePageToogled(bool on)
{
//     m_gantt->m_printOptions.singlePage = on;
//     printer().setFromTo(documentFirstPage(), documentLastPage());
}

int GanttPrintingDialog::documentLastPage() const
{
    //debugPlan<<m_gantt->m_printOptions.singlePage<<m_horPages<<m_vertPages;
    return (m_gantt->m_printOptions.context.fitting() & KGantt::PrintingContext::FitSinglePage) ? documentFirstPage() : m_horPages * m_vertPages;
}


void GanttPrintingDialog::printPage(int page, QPainter &painter)
{
#if 0
    debugPlan<<"page:"<<page<<"first"<<documentFirstPage()<<"last:"<<documentLastPage()<<m_horPages<<m_vertPages;
    int p = page - documentFirstPage();
    QRectF pageRect = printer().pageRect();
    pageRect.moveTo(0, 0);
    bool singlePage = m_gantt->m_printOptions.singlePage;
    int vert = singlePage ? 0 : p / m_horPages;
    int hor = singlePage ? 0 : p % m_horPages;
//     painter.setClipRect(pageRect.adjusted(-1.0, -1.0, 1.0, 1.0));
    if (singlePage) {
        // single page: use KGantt
        m_gantt->print(&painter, m_sceneRect.left(), m_sceneRect.right(), pageRect, m_gantt->m_printOptions.printRowLabels, true);
    } else if (m_vertPages == 1) {
        // single vertical page: use KGantt
        qreal hh = vert == 0 ? m_headerHeight : 0;
        qreal ho = vert > 0 ? m_headerHeight : 0;
        QRectF sourceRect = QRectF(m_sceneRect.x() + (pageRect.width() * hor), m_sceneRect.y() + ((pageRect.height() * vert) - ho), pageRect.width(), pageRect.height() - hh);
        debugPlan<<p<<hor<<vert<<sourceRect;
        m_gantt->print(&painter, sourceRect.left(), sourceRect.right(), pageRect, hor == 0 && m_gantt->m_printOptions.printRowLabels, true);
    } else {
        // print on multiple vertical pages: use pixmap
        // QT5TODO Make KGantt able to print multiple pages vertically
        QRectF sourceRect = m_image.rect();
        qreal hh = vert == 0 ? m_headerHeight : 0;
        qreal ho = vert > 0 ? m_headerHeight : 0;
        sourceRect = QRectF(sourceRect.x() + (pageRect.width() * hor), sourceRect.y() + ((pageRect.height() * vert) - ho), pageRect.width(), pageRect.height() - hh);
        debugPlan<<p<<hor<<vert<<sourceRect;
        painter.drawImage(pageRect, m_image, sourceRect);
    }
#endif
}

//---------------------
class HeaderView : public QHeaderView
{
public:
    explicit HeaderView(QWidget* parent=0) : QHeaderView(Qt::Horizontal, parent) {
    }

    QSize sizeHint() const override { QSize s = QHeaderView::sizeHint(); s.rheight() *= 2; return s; }
};

GanttTreeView::GanttTreeView(QWidget* parent)
    : TreeViewBase(parent)
{
    disconnect(header());
    setHeader(new HeaderView);

    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setTreePosition(-1); // always visual index 0

    header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(slotHeaderContextMenuRequested(QPoint)));
}

//-------------------------------------------
class MyGraphicsView : public KGantt::GraphicsView
{
public:
    MyGraphicsView(GanttViewBase *parent) : KGantt::GraphicsView(parent), m_parent(parent)
    {
        setMouseTracking(true);
    }
protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            event->ignore();
            return;
        }
        KGantt::GraphicsView::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton) {
            event->ignore();
            return;
        }
        KGantt::GraphicsView::mouseMoveEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            event->ignore();
            return;
        }
        KGantt::GraphicsView::mouseMoveEvent(event);
    }
    void contextMenuEvent(QContextMenuEvent *event) override {
        m_parent->handleContextMenuEvent(indexAt(event->pos()), event->globalPos());
    }
private:
    GanttViewBase *m_parent;
};
//-------------------------------------------
GanttViewBase::GanttViewBase(QWidget *parent)
    : KGantt::View(parent)
    , m_mouseButton(Qt::NoButton)
{
    setGraphicsView(new MyGraphicsView(this));
    DateTimeGrid *g = new DateTimeGrid();
    setGrid(g);
    g->setUserDefinedUpperScale(new KGantt::DateTimeScaleFormatter(KGantt::DateTimeScaleFormatter::Month, QString::fromLatin1("yyyy-MMMM")));
    g->setUserDefinedLowerScale(new KGantt::DateTimeScaleFormatter(KGantt::DateTimeScaleFormatter::Day, QString::fromLatin1("ddd")));

    g->timeNow()->setInterval(5000);

    QLocale locale;

    g->setWeekStart(locale.firstDayOfWeek());

    const QList<Qt::DayOfWeek> weekdays = locale.weekdays();
    QSet<Qt::DayOfWeek> fd;
    for (int i = Qt::Monday; i <= Qt::Sunday; ++i) {
        if (!weekdays.contains(static_cast<Qt::DayOfWeek>(i))) {
            fd << static_cast<Qt::DayOfWeek>(i);
        }
    }
    g->setFreeDays(fd);

    graphicsView()->installEventFilter(this);
}

GanttViewBase::~GanttViewBase()
{
    // HACK: avoid crash due to access of graphicsview scrollbar after death
    // KGantt tries to sync leftview scrollbar with graphicsview scrollbar
    // and seems sometimes graphicsview has already been deleted.
    // Note: this will be fixed in next KGantt release
    leftView()->verticalScrollBar()->disconnect();
}

void GanttViewBase::editCopy()
{
    QMimeData *mimeData = new QMimeData;
    QPixmap pixmap(size());
    render(&pixmap);
    mimeData->setImageData(pixmap);
    QGuiApplication::clipboard()->setMimeData(mimeData);
}

DateTimeTimeLine *GanttViewBase::timeLine() const
{
    DateTimeGrid *g = static_cast<DateTimeGrid*>(grid());
    return g->timeNow();
}

GanttTreeView *GanttViewBase::treeView() const
{
    GanttTreeView *tv = qobject_cast<GanttTreeView*>(const_cast<QAbstractItemView*>(leftView()));
    Q_ASSERT(tv);
    return tv;
}

void GanttViewBase::setPrintingOptions(const KPlato::GanttPrintingOptions &opt)
{
    m_printOptions = opt;
}

bool GanttViewBase::loadContext(const KoXmlElement &settings)
{
    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    g->setScale(static_cast<KGantt::DateTimeGrid::Scale>(settings.attribute("chart-scale", "0").toInt()));
    g->setDayWidth(settings.attribute("chart-daywidth", "30").toDouble());

    DateTimeTimeLine::Options opt;
    opt.setFlag(DateTimeTimeLine::Foreground, settings.attribute("timeline-foreground").toInt());
    opt.setFlag(DateTimeTimeLine::Background, settings.attribute("timeline-background").toInt());
    opt.setFlag(DateTimeTimeLine::UseCustomPen, settings.attribute("timeline-custom").toInt());
    timeLine()->setOptions(opt);

    timeLine()->setInterval(settings.attribute("timeline-interval", 0).toInt() * 60000);

    QPen pen;
    pen.setWidth(settings.attribute("timeline-width").toInt());
    pen.setColor(QColor(settings.attribute("timeline-color")));
    timeLine()->setPen(pen);

    return true;
}

void GanttViewBase::saveContext(QDomElement &settings) const
{
    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    settings.setAttribute("chart-scale", QString::number(g->scale()));
    settings.setAttribute("chart-daywidth", QString::number(g->dayWidth()));

    settings.setAttribute("timeline-foreground", timeLine()->options() & DateTimeTimeLine::Foreground);
    settings.setAttribute("timeline-background", timeLine()->options() & DateTimeTimeLine::Background);
    settings.setAttribute("timeline-interval", timeLine()->interval() / 60000);
    settings.setAttribute("timeline-custom", timeLine()->options() & DateTimeTimeLine::UseCustomPen);
    settings.setAttribute("timeline-width", timeLine()->pen().width());
    settings.setAttribute("timeline-color", timeLine()->pen().color().name());
}

void GanttViewBase::mousePressEvent(QMouseEvent *event)
{
    m_mouseButton = event->button();
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = event->pos();
    } else if (event->button() == Qt::RightButton) {
        // contextmenu is activated, so we do not get a mouseReleaseEvent
        m_mouseButton = Qt::NoButton;
    }
    event->ignore();
}

void GanttViewBase::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton)) {
        event->ignore();
        return;
    }
    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance()) {
        event->ignore();
        return;
    }
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    QPixmap pixmap(size());
    render(&pixmap);
    mimeData->setImageData(pixmap);
    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
    m_mouseButton = Qt::NoButton;
}

void GanttViewBase::mouseReleaseEvent(QMouseEvent *event)
{
    m_mouseButton = Qt::NoButton;
    event->ignore();
}

void GanttViewBase::handleContextMenuEvent(const QModelIndex &idx, const QPoint &globalPos)
{
    emit contextMenuRequested(idx, globalPos);
}
//-------------------------------------------
NodeGanttViewBase::NodeGanttViewBase(QWidget *parent)
    : GanttViewBase(parent),
    m_project(0),
    m_ganttdelegate(new GanttItemDelegate(this))
{
    debugPlan<<"------------------- create NodeGanttViewBase -----------------------";
    graphicsView()->setItemDelegate(m_ganttdelegate);
    GanttTreeView *tv = new GanttTreeView(this);
    tv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    tv->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // needed since qt 4.2
    setLeftView(tv);
    m_rowController = new KGantt::TreeViewRowController(tv, ganttProxyModel());
    setRowController(m_rowController);
    tv->header()->setStretchLastSection(true);

    NodeSortFilterProxyModel *m = new NodeSortFilterProxyModel(&m_defaultModel, this, true);
    KGantt::View::setModel(m);
}

NodeGanttViewBase::~NodeGanttViewBase()
{
    delete m_rowController;
}

NodeSortFilterProxyModel *NodeGanttViewBase::sfModel() const
{
    return static_cast<NodeSortFilterProxyModel*>(KGantt::View::model());
}

void NodeGanttViewBase::setItemModel(ItemModelBase *model)
{
    sfModel()->setSourceModel(model);
}

ItemModelBase *NodeGanttViewBase::model() const
{
    return sfModel()->itemModel();
}

void NodeGanttViewBase::setProject(Project *project)
{
    model()->setProject(project);
    m_project = project;
}

bool NodeGanttViewBase::loadContext(const KoXmlElement &settings)
{
    treeView()->loadContext(model()->columnMap(), settings);

    KoXmlElement e = settings.namedItem("ganttchart").toElement();
    if (! e.isNull()) {
        m_ganttdelegate->showTaskLinks = (bool)(e.attribute("show-dependencies", "0").toInt());
        m_ganttdelegate->showTaskName = (bool)(e.attribute("show-taskname", "0").toInt());
        m_ganttdelegate->showResources = (bool)(e.attribute("show-resourcenames", "0").toInt());
        m_ganttdelegate->showProgress = (bool)(e.attribute("show-completion", "0").toInt());
        m_ganttdelegate->showCriticalPath = (bool)(e.attribute("show-criticalpath", "0").toInt());
        m_ganttdelegate->showCriticalTasks = (bool)(e.attribute("show-criticaltasks", "0").toInt());
        m_ganttdelegate->showPositiveFloat = (bool)(e.attribute("show-positivefloat", "0").toInt());
        m_ganttdelegate->showSchedulingError = (bool)(e.attribute("show-schedulingerror", "0").toInt());
        m_ganttdelegate->showTimeConstraint = (bool)(e.attribute("show-timeconstraint", "0").toInt());
        m_ganttdelegate->showNegativeFloat = (bool)(e.attribute("show-negativefloat", "0").toInt());

        GanttViewBase::loadContext(e);

        m_printOptions.loadContext(e);
    }
    return true;
}

void NodeGanttViewBase::saveContext(QDomElement &settings) const
{
    debugPlan;
    treeView()->saveContext(model()->columnMap(), settings);

    QDomElement e = settings.ownerDocument().createElement("ganttchart");
    settings.appendChild(e);
    e.setAttribute("show-dependencies", QString::number(m_ganttdelegate->showTaskLinks));
    e.setAttribute("show-taskname", QString::number(m_ganttdelegate->showTaskName));
    e.setAttribute("show-resourcenames", QString::number(m_ganttdelegate->showResources));
    e.setAttribute("show-completion", QString::number(m_ganttdelegate->showProgress));
    e.setAttribute("show-criticalpath", QString::number(m_ganttdelegate->showCriticalPath));
    e.setAttribute("show-criticaltasks",QString::number(m_ganttdelegate->showCriticalTasks));
    e.setAttribute("show-positivefloat", QString::number(m_ganttdelegate->showPositiveFloat));
    e.setAttribute("show-schedulingerror", QString::number(m_ganttdelegate->showSchedulingError));
    e.setAttribute("show-timeconstraint", QString::number(m_ganttdelegate->showTimeConstraint));
    e.setAttribute("show-negativefloat", QString::number(m_ganttdelegate->showNegativeFloat));

    GanttViewBase::saveContext(e);

    m_printOptions.saveContext(e);
}

void NodeGanttViewBase::setShowUnscheduledTasks(bool show)
{
    NodeSortFilterProxyModel *m = qobject_cast<NodeSortFilterProxyModel*>(KGantt::View::model());
    if (m) {
        m->setFilterUnscheduled(!show);
    }
}

//-------------------------------------------
MyKGanttView::MyKGanttView(QWidget *parent)
    : NodeGanttViewBase(parent),
    m_manager(0)
{
    debugPlan<<"------------------- create MyKGanttView -----------------------";
    GanttItemModel *gm = new GanttItemModel(this);
    for (int i = 0; i < gm->columnCount(); ++i) {
        if (i != NodeModel::NodeCompleted) {
            gm->setReadOnly(i, true);
        }
    }
    setItemModel(gm);
    treeView()->createItemDelegates(gm);

    QList<int> show;
    show << NodeModel::NodeName
            << NodeModel::NodeCompleted
            << NodeModel::NodeStartTime
            << NodeModel::NodeEndTime;

    treeView()->setDefaultColumns(show);
    for (int i = 0; i < model()->columnCount(); ++i) {
        if (! show.contains(i)) {
            treeView()->hideColumn(i);
        }
    }

    setConstraintModel(new KGantt::ConstraintModel(this));
    KGantt::ProxyModel *m = static_cast<KGantt::ProxyModel*>(ganttProxyModel());

    m->setRole(KGantt::ItemTypeRole, KGantt::ItemTypeRole); // To provide correct format
    m->setRole(KGantt::StartTimeRole, Qt::EditRole); // To provide correct format
    m->setRole(KGantt::EndTimeRole, Qt::EditRole); // To provide correct format

    m->removeColumn(Qt::DisplayRole);
    m->setColumn(KGantt::ItemTypeRole, NodeModel::NodeType);
    m->setColumn(KGantt::StartTimeRole, NodeModel::NodeStartTime);
    m->setColumn(KGantt::EndTimeRole, NodeModel::NodeEndTime);
    m->setColumn(KGantt::TaskCompletionRole, NodeModel::NodeCompleted);

    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    g->setDayWidth(30);
    // TODO: extend QLocale/KGantt to support formats for hourly time display
    // see bug #349030
    // removed custom code here

    connect(model(), &NodeItemModel::nodeInserted, this, &MyKGanttView::slotNodeInserted);
}

GanttItemModel *MyKGanttView::model() const
{
    return static_cast<GanttItemModel*>(NodeGanttViewBase::model());
}

void MyKGanttView::setProject(Project *proj)
{
    clearDependencies();
    if (project()) {
        disconnect(project(), &Project::relationToBeModified, this, &MyKGanttView::removeDependency);
        disconnect(project(), &Project::relationModified, this, &MyKGanttView::addDependency);
        disconnect(project(), &Project::relationAdded, this, &MyKGanttView::addDependency);
        disconnect(project(), &Project::relationToBeRemoved, this, &MyKGanttView::removeDependency);
        disconnect(project(), &Project::projectCalculated, this, &MyKGanttView::slotProjectCalculated);
    }
    NodeGanttViewBase::setProject(proj);
    if (proj) {
        connect(project(), &Project::relationToBeModified, this, &MyKGanttView::removeDependency);
        connect(project(), &Project::relationModified, this, &MyKGanttView::addDependency);
        connect(proj, &Project::relationAdded, this, &MyKGanttView::addDependency);
        connect(proj, &Project::relationToBeRemoved, this, &MyKGanttView::removeDependency);
        connect(proj, &Project::projectCalculated, this, &MyKGanttView::slotProjectCalculated);
    }

    createDependencies();
}

void MyKGanttView::slotProjectCalculated(ScheduleManager *sm)
{
    if (m_manager == sm) {
        setScheduleManager(sm);
    }
}

void MyKGanttView::setScheduleManager(ScheduleManager *sm)
{
    constraintModel()->clear();
    m_manager = sm;
    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    if (sm && project()) {
        QDateTime start = project()->startTime(sm->scheduleId());
        if (start.isValid() && g->startDateTime() != start) {
            g->setStartDateTime(start);
        }
    }
    if (! g->startDateTime().isValid()) {
        g->setStartDateTime(QDateTime::currentDateTime());
    }
    model()->setScheduleManager(sm);
    createDependencies();
    graphicsView()->updateScene();
}

void MyKGanttView::slotNodeInserted(Node *node)
{
    foreach(Relation *r, node->dependChildNodes()) {
        addDependency(r);
    }
    foreach(Relation *r, node->dependParentNodes()) {
        addDependency(r);
    }
}

void MyKGanttView::addDependency(Relation *rel)
{
    QModelIndex par = sfModel()->mapFromSource(model()->index(rel->parent()));
    QModelIndex ch = sfModel()->mapFromSource(model()->index(rel->child()));
//    debugPlan<<"addDependency() "<<model()<<par.model();
    if (par.isValid() && ch.isValid()) {
        KGantt::Constraint con(par, ch, KGantt::Constraint::TypeSoft,
                                 static_cast<KGantt::Constraint::RelationType>(rel->type())/*NOTE!!*/
                               );
        if (! constraintModel()->hasConstraint(con)) {
            constraintModel()->addConstraint(con);
        }
    }
}

void MyKGanttView::removeDependency(Relation *rel)
{
    QModelIndex par = sfModel()->mapFromSource(model()->index(rel->parent()));
    QModelIndex ch = sfModel()->mapFromSource(model()->index(rel->child()));
    KGantt::Constraint con(par, ch, KGantt::Constraint::TypeSoft,
                             static_cast<KGantt::Constraint::RelationType>(rel->type())/*NOTE!!*/
                           );
    constraintModel()->removeConstraint(con);
}

void MyKGanttView::clearDependencies()
{
    constraintModel()->clear();
    // Remove old deps from view
    // NOTE: This should be handled by KGantt
    graphicsView()->updateScene();
}

void MyKGanttView::createDependencies()
{
    clearDependencies();
    if (project() == 0 || m_manager == 0) {
        return;
    }
    foreach (Node* n, project()->allNodes()) {
        foreach (Relation *r, n->dependChildNodes()) {
            addDependency(r);
        }
    }
}

//------------------------------------------
GanttView::GanttView(KoPart *part, KoDocument *doc, QWidget *parent, bool readWrite)
    : ViewBase(part, doc, parent),
    m_readWrite(readWrite),
    m_project(0)
{
    debugPlan <<" ---------------- KPlato: Creating GanttView ----------------";

    setXMLFile("GanttViewUi.rc");

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setMargin(0);
    m_splitter = new QSplitter(this);
    l->addWidget(m_splitter);
    m_splitter->setOrientation(Qt::Vertical);

    m_gantt = new MyKGanttView(m_splitter);
    m_gantt->graphicsView()->setHeaderContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &ViewBase::expandAll, m_gantt->treeView(), &TreeViewBase::slotExpand);
    connect(this, &ViewBase::collapseAll, m_gantt->treeView(), &TreeViewBase::slotCollapse);

    setupGui();

    updateReadWrite(readWrite);
    //connect(m_gantt->constraintModel(), SIGNAL(constraintAdded(Constraint)), this, SLOT(update()));
    debugPlan <<m_gantt->constraintModel();

    connect(m_gantt->treeView(), &TreeViewBase::contextMenuRequested, this, &GanttView::slotContextMenuRequested);

    connect(m_gantt->treeView(), &TreeViewBase::headerContextMenuRequested, this, &ViewBase::slotHeaderContextMenuRequested);

    connect(m_gantt->graphicsView(), &KGantt::GraphicsView::headerContextMenuRequested, this, &GanttView::slotGanttHeaderContextMenuRequested);

    connect(qobject_cast<KGantt::DateTimeGrid*>(m_gantt->graphicsView()->grid()), &KGantt::DateTimeGrid::gridChanged, this, &GanttView::slotDateTimeGridChanged);

    connect(m_gantt->leftView(), &GanttTreeView::doubleClicked, this, &GanttView::itemDoubleClicked);

    connect(m_gantt, &MyKGanttView::contextMenuRequested, this, &GanttView::slotContextMenuRequested);

    Help::add(this,
              xi18nc("@info:whatsthis",
                     "<title>Gantt View</title>"
                     "<para>"
                     "Displays scheduled tasks in a Gantt diagram."
                     " The chart area can be zoomed in and out with a slider"
                     " positioned in the upper left corner of the time scale."
                     " <note>You need to hoover over it with the mouse for it to show.</note>"
                     "</para><para>"
                     "This view supports configuration and printing using the context menu of the tree view."
                     "<nl/><link url='%1'>More...</link>"
                     "</para>", Help::page("Task_Gantt_View")));
}

void GanttView::slotEditCopy()
{
    m_gantt->editCopy();
}

void GanttView::itemDoubleClicked(const QPersistentModelIndex &idx)
{
    if (idx.column() == NodeModel::NodeDescription) {
        emit openTaskDescription(isReadWrite() && (idx.flags() & Qt::ItemIsEditable));
    }
}

void GanttView::slotGanttHeaderContextMenuRequested(const QPoint &pt)
{
    QMenu *menu = popupMenu("gantt_datetimegrid_popup");
    if (menu) {
        menu->exec(pt);
    }
}

KoPrintJob *GanttView::createPrintJob()
{
    return new GanttPrintingDialog(this, m_gantt);
}

void GanttView::setZoom(double)
{
    //debugPlan <<"setting gantt zoom:" << zoom;
    //m_gantt->setZoomFactor(zoom,true); NO!!! setZoomFactor() is something else
}

void GanttView::setupGui()
{
    // create context menu actions
    actionShowProject = new KToggleAction(i18n("Show Project"), this);
    actionCollection()->addAction("show_project", actionShowProject);
    // FIXME: Dependencies depend on these methods being called in the correct order
    connect(actionShowProject, &QAction::triggered, m_gantt, &MyKGanttView::clearDependencies);
    connect(actionShowProject, &QAction::triggered, m_gantt->model(), &NodeItemModel::setShowProject);
    connect(actionShowProject, &QAction::triggered, m_gantt, &MyKGanttView::createDependencies);
    addContextAction(actionShowProject);

    actionShowUnscheduled = new KToggleAction(i18n("Show Unscheduled Tasks"), this);
    actionCollection()->addAction("show_unscheduled_tasks", actionShowUnscheduled);
    connect(actionShowUnscheduled, &QAction::triggered, m_gantt, &MyKGanttView::setShowUnscheduledTasks);
    addContextAction(actionShowUnscheduled);

    createOptionActions(ViewBase::OptionAll);
    for (QAction *a : contextActionList()) {
        actionCollection()->addAction(a->objectName(), a);
    }

    m_scalegroup = new QActionGroup(this);
    QAction *a = new QAction(i18nc("@action:inmenu", "Auto"), this);
    a->setCheckable(true);
    a->setChecked(true);
    actionCollection()->addAction("scale_auto", a);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Month"), this);
    actionCollection()->addAction("scale_month", a);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Week"), this);
    actionCollection()->addAction("scale_week", a);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Day"), this);
    a->setCheckable(true);
    actionCollection()->addAction("scale_day", a);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Hour"), this);
    a->setCheckable(true);
    actionCollection()->addAction("scale_hour", a);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Zoom In"), this);
    a->setIcon(koIcon("zoom-in"));
    actionCollection()->addAction("zoom_in", a);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);

    a = new QAction(i18nc("@action:inmenu", "Zoom Out"), this);
    a->setIcon(koIcon("zoom-out"));
    actionCollection()->addAction("zoom_out", a);
    connect(a, &QAction::triggered, this, &GanttView::ganttActions);
}

void GanttView::slotDateTimeGridChanged()
{
    DateTimeGrid *grid = qobject_cast<DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);
    if (!grid) {
        return;
    }
    QAction *a = m_scalegroup->checkedAction();
    switch (grid->scale()) {
        case KGantt::DateTimeGrid::ScaleAuto: actionCollection()->action("scale_auto")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleHour: actionCollection()->action("scale_hour")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleDay: actionCollection()->action("scale_day")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleWeek: actionCollection()->action("scale_week")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleMonth: actionCollection()->action("scale_month")->setChecked(true); break;
        default:
            warnPlan<<"Unused scale:"<<grid->scale();
            break;
    }
}

void GanttView::ganttActions()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    DateTimeGrid *grid = qobject_cast<DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);
    if (!grid) {
        return;
    }
    if (action->objectName() == "scale_auto") {
        grid->setScale(DateTimeGrid::ScaleAuto);
    } else if (action->objectName() == "scale_month") {
        grid->setScale(DateTimeGrid::ScaleMonth);
    } else if (action->objectName() == "scale_week") {
        grid->setScale(DateTimeGrid::ScaleWeek);
    } else if (action->objectName() == "scale_day") {
        grid->setScale(DateTimeGrid::ScaleDay);
    } else if (action->objectName() == "scale_hour") {
        grid->setScale(DateTimeGrid::ScaleHour);
    } else if (action->objectName() == "zoom_in") {
        grid->setDayWidth(grid->dayWidth() * 1.25);
    } else if (action->objectName() == "zoom_out") {
        // daywidth *MUST NOT* go below 1.0, it is used as an integer later on
        grid->setDayWidth(qMax<qreal>(1.0, grid->dayWidth() * 0.8));
    } else {
        warnPlan<<"Unknown gantt action:"<<action;
    }
}

void GanttView::slotOptions()
{
    debugPlan;
    GanttViewSettingsDialog *dlg = new GanttViewSettingsDialog(m_gantt, m_gantt->delegate(), this, sender()->objectName() == "print_options");
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOptionsFinished(int)));
    dlg->open();
}

void GanttView::slotOptionsFinished(int result)
{
    GanttViewSettingsDialog *dlg = qobject_cast<GanttViewSettingsDialog*>(sender());
    if (dlg && result == QDialog::Accepted) {
        m_gantt->graphicsView()->updateScene();
    }
    ViewBase::slotOptionsFinished(result);
}

void GanttView::clear()
{
//    m_gantt->clear();
}

void GanttView::setShowResources(bool on)
{
    m_gantt->delegate()->showResources = on;
}

void GanttView::setShowTaskName(bool on)
{
    m_gantt->delegate()->showTaskName = on;
}

void GanttView::setShowProgress(bool on)
{
    m_gantt->delegate()->showProgress = on;
}

void GanttView::setShowPositiveFloat(bool on)
{
    m_gantt->delegate()->showPositiveFloat = on;
}

void GanttView::setShowCriticalTasks(bool on)
{
    m_gantt->delegate()->showCriticalTasks = on;
}

void GanttView::setShowCriticalPath(bool on)
{
    m_gantt->delegate()->showCriticalPath = on;
}

void GanttView::setShowNoInformation(bool on)
{
    m_gantt->delegate()->showNoInformation = on;
}

void GanttView::setShowAppointments(bool on)
{
    m_gantt->delegate()->showAppointments = on;
}

void GanttView::setShowTaskLinks(bool on)
{
    m_gantt->delegate()->showTaskLinks = on;
}

void GanttView::setProject(Project *project)
{
    if (this->project()) {
        disconnect(project, &Project::scheduleManagerChanged, this, &GanttView::setScheduleManager);
    }
    m_gantt->setProject(project);
    if (project) {
        connect(project, &Project::scheduleManagerChanged, this, &GanttView::setScheduleManager);
    }
}

void GanttView::setScheduleManager(ScheduleManager *sm)
{
    if (!sm && scheduleManager()) {
        // we should only get here if the only schedule manager is scheduled,
        // or when last schedule manager is deleted
        m_domdoc.clear();
        QDomElement element = m_domdoc.createElement("expanded");
        m_domdoc.appendChild(element);
        m_gantt->treeView()->saveExpanded(element);
    }
    bool tryexpand = sm && !scheduleManager();
    bool expand = sm && scheduleManager() && sm != scheduleManager();
    QDomDocument doc;
    if (expand) {
        QDomElement element = doc.createElement("expanded");
        doc.appendChild(element);
        m_gantt->treeView()->saveExpanded(element);
    }
    ViewBase::setScheduleManager(sm);
    m_gantt->setScheduleManager(sm);

    if (expand) {
        m_gantt->treeView()->doExpand(doc);
    } else if (tryexpand) {
        m_gantt->treeView()->doExpand(m_domdoc);
    }
}

void GanttView::draw(Project &project)
{
    setProject(&project);
}

void GanttView::drawChanges(Project &project)
{
    if (m_project != &project) {
        setProject(&project);
    }
}

Node *GanttView::currentNode() const
{
    QModelIndex idx = m_gantt->treeView()->selectionModel()->currentIndex();
    return m_gantt->model()->node(m_gantt->sfModel()->mapToSource(idx));
}

void GanttView::slotContextMenuRequested(const QModelIndex &idx, const QPoint &pos)
{
    debugPlan<<idx<<idx.data();
    QString name;
    QModelIndex sidx = idx;
    if (sidx.isValid()) {
        const QAbstractProxyModel *proxy = qobject_cast<const QAbstractProxyModel*>(sidx.model());
        while (proxy != m_gantt->sfModel()) {
            sidx = proxy->mapToSource(sidx);
            proxy = qobject_cast<const QAbstractProxyModel*>(sidx.model());
        }
        if (!sidx.isValid()) {
            warnPlan<<Q_FUNC_INFO<<"Failed to find item model";
            return;
        }
    }
    m_gantt->treeView()->selectionModel()->setCurrentIndex(sidx, QItemSelectionModel::ClearAndSelect);

    Node *node = m_gantt->model()->node(m_gantt->sfModel()->mapToSource(sidx));
    if (node) {
        switch (node->type()) {
            case Node::Type_Project:
                name = "taskview_project_popup";
                break;
            case Node::Type_Task:
                name = "taskview_popup";
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
    } else debugPlan<<"No node";

    m_gantt->treeView()->setContextMenuIndex(sidx);
    if (name.isEmpty()) {
        slotHeaderContextMenuRequested(pos);
        m_gantt->treeView()->setContextMenuIndex(QModelIndex());
        debugPlan<<"No menu";
        return;
    }
    emit requestPopupMenu(name, pos);
    m_gantt->treeView()->setContextMenuIndex(QModelIndex());
}

bool GanttView::loadContext(const KoXmlElement &settings)
{
    debugPlan;
    ViewBase::loadContext(settings);
    bool show = (bool)(settings.attribute("show-project", "0").toInt());
    actionShowProject->setChecked(show);
    m_gantt->model()->setShowProject(show); // why is this not called by the action?
    show = (bool)(settings.attribute("show-unscheduled", "1").toInt());
    actionShowUnscheduled->setChecked(show);
    m_gantt->setShowUnscheduledTasks(show);

    return m_gantt->loadContext(settings);
}

void GanttView::saveContext(QDomElement &settings) const
{
    debugPlan;
    ViewBase::saveContext(settings);
    settings.setAttribute("show-project", QString::number(actionShowProject->isChecked()));
    settings.setAttribute("show-unscheduled", QString::number(actionShowUnscheduled->isChecked()));

    m_gantt->saveContext(settings);

}

void GanttView::updateReadWrite(bool on)
{
    // TODO: KGanttView needs read/write mode
    m_readWrite = on;
    if (m_gantt->model()) {
        m_gantt->model()->setReadWrite(on);
    }
}

//----
MilestoneGanttViewSettingsDialog::MilestoneGanttViewSettingsDialog(GanttViewBase *gantt, ViewBase *view, bool selectPrint)
    : ItemViewSettupDialog(view, gantt->treeView(), true, view),
    m_gantt(gantt)
{
    QTabWidget *tab = new QTabWidget();
    QWidget *w = ViewBase::createPageLayoutWidget(view);
    tab->addTab(w, w->windowTitle());
    m_pagelayout = w->findChild<KoPageLayoutWidget*>();
    Q_ASSERT(m_pagelayout);

    m_printingoptions = new GanttPrintingOptionsWidget(this);
    m_printingoptions->setOptions(gantt->printingOptions());
    tab->addTab(m_printingoptions, m_printingoptions->windowTitle());
    KPageWidgetItem *page = insertWidget(-1, tab, i18n("Printing"), i18n("Printing Options"));
    if (selectPrint) {
        setCurrentPage(page);
    }
    connect(this, SIGNAL(accepted()), this, SLOT(slotOk()));
}

void MilestoneGanttViewSettingsDialog::slotOk()
{
    debugPlan;
    m_gantt->setPrintingOptions(m_printingoptions->options());
    ItemViewSettupDialog::slotOk();
}

//------------------------
MilestoneKGanttView::MilestoneKGanttView(QWidget *parent)
    : NodeGanttViewBase(parent),
    m_manager(0)
{
    debugPlan<<"------------------- create MilestoneKGanttView -----------------------";
    MilestoneItemModel *mm = new MilestoneItemModel(this);
    for (int i = 0; i < mm->columnCount(); ++i) {
        if (i != NodeModel::NodeCompleted) {
            mm->setReadOnly(i, true);
        }
    }
    setItemModel(mm);
    treeView()->createItemDelegates(mm);

    sfModel()->setFilterRole (Qt::EditRole);
    sfModel()->setFilterFixedString(QString::number(Node::Type_Milestone));
    sfModel()->setFilterKeyColumn(NodeModel::NodeType);

    QList<int> show;
    show << NodeModel::NodeWBSCode
            << NodeModel::NodeName
            << NodeModel::NodeStartTime;

    treeView()->setDefaultColumns(show);
    for (int i = 0; i < model()->columnCount(); ++i) {
        if (! show.contains(i)) {
            treeView()->hideColumn(i);
        }
    }
    treeView()->header()->moveSection(NodeModel::NodeWBSCode, show.indexOf(NodeModel::NodeWBSCode));
    treeView()->setRootIsDecorated (false);

    KGantt::ProxyModel *m = static_cast<KGantt::ProxyModel*>(ganttProxyModel());

    m->setRole(KGantt::ItemTypeRole, KGantt::ItemTypeRole); // To provide correct format
    m->setRole(KGantt::StartTimeRole, Qt::EditRole); // To provide correct format
    m->setRole(KGantt::EndTimeRole, Qt::EditRole); // To provide correct format

    m->removeColumn(Qt::DisplayRole);
    m->setColumn(KGantt::ItemTypeRole, NodeModel::NodeType);
    m->setColumn(KGantt::StartTimeRole, NodeModel::NodeStartTime);
    m->setColumn(KGantt::EndTimeRole, NodeModel::NodeEndTime);
    m->setColumn(KGantt::TaskCompletionRole, NodeModel::NodeCompleted);

    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    g->setDayWidth(30);
    // TODO: extend QLocale/KGantt to support formats for hourly time display
    // see bug #349030
    // removed custom code here

    // TODO: add to context
    treeView()->sortByColumn(NodeModel::NodeWBSCode, Qt::AscendingOrder);
    treeView()->setSortingEnabled(true);

    Help::add(this,
              xi18nc("@info:whatsthis",
                     "<title>Milestone Gantt View</title>"
                     "<para>"
                     "Displays scheduled milestones in a Gantt diagram."
                     " The chart area can be zoomed in and out with a slider"
                     " positioned in the upper left corner of the time scale."
                     " <note>You need to hoover over it with the mouse for it to show.</note>"
                     "</para><para>"
                     "This view supports configuration and printing using the context menu."
                     "<nl/><link url='%1'>More...</link>"
                     "</para>", Help::page("Milestone_Gantt_View")));
}

MilestoneItemModel *MilestoneKGanttView::model() const
{
    return static_cast<MilestoneItemModel*>(NodeGanttViewBase::model());
}

void MilestoneKGanttView::setProject(Project *proj)
{
    if (project()) {
        disconnect(project(), &Project::projectCalculated, this, &MilestoneKGanttView::slotProjectCalculated);
    }
    NodeGanttViewBase::setProject(proj);
    if (proj) {
        connect(proj, &Project::projectCalculated, this, &MilestoneKGanttView::slotProjectCalculated);
    }
}

void MilestoneKGanttView::slotProjectCalculated(ScheduleManager *sm)
{
    if (m_manager == sm) {
        setScheduleManager(sm);
    }
}

void MilestoneKGanttView::setScheduleManager(ScheduleManager *sm)
{
    //debugPlan<<id<<endl;
    model()->setScheduleManager(0);
    m_manager = sm;
    KGantt::DateTimeGrid *g = static_cast<KGantt::DateTimeGrid*>(grid());
    if (sm && m_project) {
        QDateTime start;
        foreach (const Node *n, model()->mileStones()) {
            QDateTime nt = n->startTime(sm->scheduleId());
            if (! nt.isValid()) {
                continue;
            }
            if (! start.isValid() || start > nt) {
                start = nt;
                debugPlan<<n->name()<<start;
            }
        }
        if (! start.isValid()) {
            start = project()->startTime(sm->scheduleId());
        }
        if (start.isValid() && g->startDateTime() !=  start) {
            g->setStartDateTime(start);
        }
    }
    if (! g->startDateTime().isValid()) {
        g->setStartDateTime(QDateTime::currentDateTime());
    }
    model()->setScheduleManager(sm);
    graphicsView()->updateScene();
}

//------------------------------------------

MilestoneGanttView::MilestoneGanttView(KoPart *part, KoDocument *doc, QWidget *parent, bool readWrite)
    : ViewBase(part, doc, parent),
        m_readWrite(readWrite),
        m_project(0)
{
    debugPlan <<" ---------------- Plan: Creating Milesone GanttView ----------------";

    setXMLFile("GanttViewUi.rc");

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setMargin(0);
    m_splitter = new QSplitter(this);
    l->addWidget(m_splitter);
    m_splitter->setOrientation(Qt::Vertical);

    setupGui();

    m_gantt = new MilestoneKGanttView(m_splitter);
    m_gantt->graphicsView()->setHeaderContextMenuPolicy(Qt::CustomContextMenu);

    m_showTaskName = false; // FIXME
    m_showProgress = false; //FIXME
    m_showPositiveFloat = false; //FIXME
    m_showCriticalTasks = false; //FIXME
    m_showNoInformation = false; //FIXME

    updateReadWrite(readWrite);

    connect(m_gantt->treeView(), &TreeViewBase::contextMenuRequested, this, &MilestoneGanttView::slotContextMenuRequested);
    connect(m_gantt, &MyKGanttView::contextMenuRequested, this, &MilestoneGanttView::slotContextMenuRequested);

    connect(m_gantt->treeView(), &TreeViewBase::headerContextMenuRequested, this, &ViewBase::slotHeaderContextMenuRequested);
    connect(m_gantt->graphicsView(), &KGantt::GraphicsView::headerContextMenuRequested, this, &MilestoneGanttView::slotGanttHeaderContextMenuRequested);
    connect(qobject_cast<KGantt::DateTimeGrid*>(m_gantt->graphicsView()->grid()), &KGantt::DateTimeGrid::gridChanged, this, &MilestoneGanttView::slotDateTimeGridChanged);

    connect(m_gantt->treeView(), &GanttTreeView::doubleClicked, this, &MilestoneGanttView::itemDoubleClicked);
}

void MilestoneGanttView::slotEditCopy()
{
    m_gantt->editCopy();
}

void MilestoneGanttView::itemDoubleClicked(const QPersistentModelIndex &idx)
{
    if (idx.column() == NodeModel::NodeDescription) {
        emit openTaskDescription(isReadWrite() && (idx.flags() & Qt::ItemIsEditable));
    }
}

void MilestoneGanttView::slotGanttHeaderContextMenuRequested(const QPoint &pt)
{
    QMenu *menu = popupMenu("gantt_datetimegrid_popup");
    if (menu) {
        menu->exec(pt);
    }
}

void MilestoneGanttView::setZoom(double)
{
    //debugPlan <<"setting gantt zoom:" << zoom;
    //m_gantt->setZoomFactor(zoom,true); NO!!! setZoomFactor() is something else
}

void MilestoneGanttView::show()
{
}

void MilestoneGanttView::clear()
{
}

void MilestoneGanttView::setProject(Project *project)
{
    m_gantt->setProject(project);
}

void MilestoneGanttView::setScheduleManager(ScheduleManager *sm)
{
    //debugPlan<<id<<endl;
    m_gantt->setScheduleManager(sm);
}

void MilestoneGanttView::draw(Project &project)
{
    setProject(&project);
}

void MilestoneGanttView::drawChanges(Project &project)
{
    if (m_project != &project) {
        setProject(&project);
    }
}

Node *MilestoneGanttView::currentNode() const
{
    QModelIndex idx = m_gantt->treeView()->selectionModel()->currentIndex();
    return m_gantt->model()->node(m_gantt->sfModel()->mapToSource(idx));
}

void MilestoneGanttView::setupGui()
{
    createOptionActions(ViewBase::OptionAll & ~(ViewBase::OptionExpand|ViewBase::OptionCollapse));

    for (QAction *a : contextActionList()) {
        actionCollection()->addAction(a->objectName(), a);
    }

    m_scalegroup = new QActionGroup(this);
    QAction *a = new QAction(i18nc("@action:inmenu", "Auto"), this);
    a->setCheckable(true);
    a->setChecked(true);
    actionCollection()->addAction("scale_auto", a);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Month"), this);
    actionCollection()->addAction("scale_month", a);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Week"), this);
    actionCollection()->addAction("scale_week", a);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Day"), this);
    a->setCheckable(true);
    actionCollection()->addAction("scale_day", a);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Hour"), this);
    a->setCheckable(true);
    actionCollection()->addAction("scale_hour", a);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Zoom In"), this);
    a->setIcon(koIcon("zoom-in"));
    actionCollection()->addAction("zoom_in", a);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);

    a = new QAction(i18nc("@action:inmenu", "Zoom Out"), this);
    a->setIcon(koIcon("zoom-out"));
    actionCollection()->addAction("zoom_out", a);
    connect(a, &QAction::triggered, this, &MilestoneGanttView::ganttActions);
}

void MilestoneGanttView::slotDateTimeGridChanged()
{
    DateTimeGrid *grid = qobject_cast<DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);
    if (!grid) {
        return;
    }
    QAction *a = m_scalegroup->checkedAction();
    switch (grid->scale()) {
        case KGantt::DateTimeGrid::ScaleAuto: actionCollection()->action("scale_auto")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleHour: actionCollection()->action("scale_hour")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleDay: actionCollection()->action("scale_day")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleWeek: actionCollection()->action("scale_week")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleMonth: actionCollection()->action("scale_month")->setChecked(true); break;
        default:
            warnPlan<<"Unused scale:"<<grid->scale();
            break;
    }
}

void MilestoneGanttView::ganttActions()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    DateTimeGrid *grid = qobject_cast<DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);
    if (!grid) {
        return;
    }
    if (action->objectName() == "scale_auto") {
        grid->setScale(DateTimeGrid::ScaleAuto);
    } else if (action->objectName() == "scale_month") {
        grid->setScale(DateTimeGrid::ScaleMonth);
    } else if (action->objectName() == "scale_week") {
        grid->setScale(DateTimeGrid::ScaleWeek);
    } else if (action->objectName() == "scale_day") {
        grid->setScale(DateTimeGrid::ScaleDay);
    } else if (action->objectName() == "scale_hour") {
        grid->setScale(DateTimeGrid::ScaleHour);
    } else if (action->objectName() == "zoom_in") {
        grid->setDayWidth(grid->dayWidth() * 1.25);
    } else if (action->objectName() == "zoom_out") {
        // daywidth *MUST NOT* go below 1.0, it is used as an integer later on
        grid->setDayWidth(qMax<qreal>(1.0, grid->dayWidth() * 0.8));
    } else {
        warnPlan<<"Unknown gantt action:"<<action;
    }
}

void MilestoneGanttView::slotContextMenuRequested(const QModelIndex &idx, const QPoint &pos)
{
    debugPlan;
    QString name;
    QModelIndex sidx = idx;
    if (sidx.isValid()) {
        const QAbstractProxyModel *proxy = qobject_cast<const QAbstractProxyModel*>(sidx.model());
        while (proxy != m_gantt->sfModel()) {
            sidx = proxy->mapToSource(sidx);
            proxy = qobject_cast<const QAbstractProxyModel*>(sidx.model());
        }
        if (!sidx.isValid()) {
            warnPlan<<Q_FUNC_INFO<<"Failed to find item model";
            return;
        }
    }
    m_gantt->treeView()->selectionModel()->setCurrentIndex(sidx, QItemSelectionModel::ClearAndSelect);

    Node *node = m_gantt->model()->node(m_gantt->sfModel()->mapToSource(sidx));
    if (node) {
        switch (node->type()) {
            case Node::Type_Task:
                name = "taskview_popup";
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
    } else debugPlan<<"No node";
    m_gantt->treeView()->setContextMenuIndex(idx);
    if (name.isEmpty()) {
        debugPlan<<"No menu";
        slotHeaderContextMenuRequested(pos);
        m_gantt->treeView()->setContextMenuIndex(QModelIndex());
        return;
    }
    emit requestPopupMenu(name, pos);
    m_gantt->treeView()->setContextMenuIndex(QModelIndex());
}

void MilestoneGanttView::slotOptions()
{
    debugPlan;
    MilestoneGanttViewSettingsDialog *dlg =  new MilestoneGanttViewSettingsDialog(m_gantt, this, sender()->objectName() == "print_options");
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOptionsFinished(int)));
    dlg->open();
}

bool MilestoneGanttView::loadContext(const KoXmlElement &settings)
{
    debugPlan;
    ViewBase::loadContext(settings);
    return m_gantt->loadContext(settings);
}

void MilestoneGanttView::saveContext(QDomElement &settings) const
{
    debugPlan;
    ViewBase::saveContext(settings);
    return m_gantt->saveContext(settings);
}

void MilestoneGanttView::updateReadWrite(bool on)
{
    m_readWrite = on;
    if (m_gantt->model()) {
        m_gantt->model()->setReadWrite(on);
    }
}

KoPrintJob *MilestoneGanttView::createPrintJob()
{
    return new GanttPrintingDialog(this, m_gantt);
}

//--------------------
ResourceAppointmentsGanttViewSettingsDialog::ResourceAppointmentsGanttViewSettingsDialog(GanttViewBase *gantt,  ViewBase *view, bool selectPrint)
    : ItemViewSettupDialog(view, gantt->treeView(), true, view)
    , m_gantt(gantt)
{
    QTabWidget *tab = new QTabWidget();
    QWidget *w = ViewBase::createPageLayoutWidget(view);
    tab->addTab(w, w->windowTitle());
    m_pagelayout = w->findChild<KoPageLayoutWidget*>();
    Q_ASSERT(m_pagelayout);
    m_printingoptions = new GanttPrintingOptionsWidget(this);
    m_printingoptions->setOptions(gantt->printingOptions());
    tab->addTab(m_printingoptions, m_printingoptions->windowTitle());
    KPageWidgetItem *page = insertWidget(-1, tab, i18n("Printing"), i18n("Printing Options"));
    if (selectPrint) {
        setCurrentPage(page);
    }
    connect(this, SIGNAL(accepted()), this, SLOT(slotOk()));
}

void ResourceAppointmentsGanttViewSettingsDialog::slotOk()
{
    debugPlan;
    m_gantt->setPrintingOptions(m_printingoptions->options());
    ItemViewSettupDialog::slotOk();
}

//------------------------------------------

ResourceAppointmentsGanttView::ResourceAppointmentsGanttView(KoPart *part, KoDocument *doc, QWidget *parent, bool readWrite)
    : ViewBase(part, doc, parent),
    m_project(0),
    m_model(new ResourceAppointmentsGanttModel(this))
{
    debugPlan <<" ---------------- KPlato: Creating ResourceAppointmentsGanttView ----------------";

    setXMLFile("GanttViewUi.rc");

    m_gantt = new GanttViewBase(this);
    m_gantt->graphicsView()->setHeaderContextMenuPolicy(Qt::CustomContextMenu);
    m_gantt->graphicsView()->setItemDelegate(new ResourceGanttItemDelegate(m_gantt));

    GanttTreeView *tv = new GanttTreeView(m_gantt);
    tv->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    tv->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tv->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); // needed since qt 4.2
    m_gantt->setLeftView(tv);
    connect(this, &ViewBase::expandAll, tv, &TreeViewBase::slotExpand);
    connect(this, &ViewBase::collapseAll, tv, &TreeViewBase::slotCollapse);
    m_rowController = new KGantt::TreeViewRowController(tv, m_gantt->ganttProxyModel());
    m_gantt->setRowController(m_rowController);
    tv->header()->setStretchLastSection(true);

    tv->setTreePosition(-1);

    KGantt::ProxyModel *m = static_cast<KGantt::ProxyModel*>(m_gantt->ganttProxyModel());
    m->setRole(KGantt::ItemTypeRole, KGantt::ItemTypeRole);
    m->setRole(KGantt::StartTimeRole, KGantt::StartTimeRole);
    m->setRole(KGantt::EndTimeRole, KGantt::EndTimeRole);
    m->setRole(KGantt::TaskCompletionRole, KGantt::TaskCompletionRole);

    m_gantt->setModel(m_model);

    QVBoxLayout *l = new QVBoxLayout(this);
    l->setMargin(0);
    l->addWidget(m_gantt);

    setupGui();

    updateReadWrite(readWrite);

    connect(m_gantt->leftView(), SIGNAL(contextMenuRequested(QModelIndex,QPoint,QModelIndexList)), SLOT(slotContextMenuRequested(QModelIndex,QPoint)));
    connect(m_gantt, &GanttViewBase::contextMenuRequested, this, &ResourceAppointmentsGanttView::slotContextMenuRequestedFromGantt);

    connect(m_gantt->leftView(), SIGNAL(headerContextMenuRequested(QPoint)), SLOT(slotHeaderContextMenuRequested(QPoint)));
    connect(m_gantt->graphicsView(), &KGantt::GraphicsView::headerContextMenuRequested, this, &ResourceAppointmentsGanttView::slotGanttHeaderContextMenuRequested);
    connect(qobject_cast<KGantt::DateTimeGrid*>(m_gantt->graphicsView()->grid()), &KGantt::DateTimeGrid::gridChanged, this, &ResourceAppointmentsGanttView::slotDateTimeGridChanged);

    Help::add(this,
              xi18nc("@info:whatsthis",
                     "<title>Resource Assignments (Gantt)</title>"
                     "<para>"
                     "Displays the scheduled resource - task assignments in a Gantt diagram."
                     " The chart area can be zoomed in and out with a slider"
                     " positioned in the upper left corner of the time scale."
                     " <note>You need to hoover over it with the mouse for it to show.</note>"
                     "</para><para>"
                     "This view supports configuration and printing using the context menu."
                     "<nl/><link url='%1'>More...</link>"
                     "</para>", Help::page("Resource_Assignment_Gantt_View")));
}

ResourceAppointmentsGanttView::~ResourceAppointmentsGanttView()
{
    delete m_rowController;
}

void ResourceAppointmentsGanttView::slotEditCopy()
{
    m_gantt->editCopy();
}

void ResourceAppointmentsGanttView::slotGanttHeaderContextMenuRequested(const QPoint &pt)
{
    QMenu *menu = popupMenu("gantt_datetimegrid_popup");
    if (menu) {
        menu->exec(pt);
    }
}

void ResourceAppointmentsGanttView::setZoom(double)
{
    //debugPlan <<"setting gantt zoom:" << zoom;
    //m_gantt->setZoomFactor(zoom,true); NO!!! setZoomFactor() is something else
}

Project *ResourceAppointmentsGanttView::project() const
{
    return m_model->project();
}

void ResourceAppointmentsGanttView::setProject(Project *project)
{
    m_model->setProject(project);
}

void ResourceAppointmentsGanttView::setScheduleManager(ScheduleManager *sm)
{
    //debugPlan<<id<<endl;
    if (!sm && scheduleManager()) {
        // we should only get here if the only schedule manager is scheduled,
        // or when last schedule manager is deleted
        m_domdoc.clear();
        QDomElement element = m_domdoc.createElement("expanded");
        m_domdoc.appendChild(element);
        treeView()->saveExpanded(element);
    }
    bool tryexpand = sm && !scheduleManager();
    bool expand = sm && scheduleManager() && sm != scheduleManager();
    QDomDocument doc;
    if (expand) {
        QDomElement element = doc.createElement("expanded");
        doc.appendChild(element);
        treeView()->saveExpanded(element);
    }
    ViewBase::setScheduleManager(sm);
    m_model->setScheduleManager(sm);

    if (expand) {
        treeView()->doExpand(doc);
    } else if (tryexpand) {
        treeView()->doExpand(m_domdoc);
    }
    m_gantt->graphicsView()->updateScene();
}

void ResourceAppointmentsGanttView::setupGui()
{
    createOptionActions(ViewBase::OptionAll);

    for (QAction *a : contextActionList()) {
        actionCollection()->addAction(a->objectName(), a);
    }

    m_scalegroup = new QActionGroup(this);
    QAction *a = new QAction(i18nc("@action:inmenu", "Auto"), this);
    a->setCheckable(true);
    a->setChecked(true);
    actionCollection()->addAction("scale_auto", a);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Month"), this);
    actionCollection()->addAction("scale_month", a);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Week"), this);
    actionCollection()->addAction("scale_week", a);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Day"), this);
    a->setCheckable(true);
    actionCollection()->addAction("scale_day", a);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Hour"), this);
    a->setCheckable(true);
    actionCollection()->addAction("scale_hour", a);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);
    m_scalegroup->addAction(a);

    a = new QAction(i18nc("@action:inmenu", "Zoom In"), this);
    a->setIcon(koIcon("zoom-in"));
    actionCollection()->addAction("zoom_in", a);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);

    a = new QAction(i18nc("@action:inmenu", "Zoom Out"), this);
    a->setIcon(koIcon("zoom-out"));
    actionCollection()->addAction("zoom_out", a);
    connect(a, &QAction::triggered, this, &ResourceAppointmentsGanttView::ganttActions);
}

void ResourceAppointmentsGanttView::slotDateTimeGridChanged()
{
    DateTimeGrid *grid = qobject_cast<DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);
    if (!grid) {
        return;
    }
    QAction *a = m_scalegroup->checkedAction();
    switch (grid->scale()) {
        case KGantt::DateTimeGrid::ScaleAuto: actionCollection()->action("scale_auto")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleHour: actionCollection()->action("scale_hour")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleDay: actionCollection()->action("scale_day")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleWeek: actionCollection()->action("scale_week")->setChecked(true); break;
        case KGantt::DateTimeGrid::ScaleMonth: actionCollection()->action("scale_month")->setChecked(true); break;
        default:
            warnPlan<<"Unused scale:"<<grid->scale();
            break;
    }
}

void ResourceAppointmentsGanttView::ganttActions()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action) {
        return;
    }
    DateTimeGrid *grid = qobject_cast<DateTimeGrid*>(m_gantt->grid());
    Q_ASSERT(grid);
    if (!grid) {
        return;
    }
    if (action->objectName() == "scale_auto") {
        grid->setScale(DateTimeGrid::ScaleAuto);
    } else if (action->objectName() == "scale_month") {
        grid->setScale(DateTimeGrid::ScaleMonth);
    } else if (action->objectName() == "scale_week") {
        grid->setScale(DateTimeGrid::ScaleWeek);
    } else if (action->objectName() == "scale_day") {
        grid->setScale(DateTimeGrid::ScaleDay);
    } else if (action->objectName() == "scale_hour") {
        grid->setScale(DateTimeGrid::ScaleHour);
    } else if (action->objectName() == "zoom_in") {
        grid->setDayWidth(grid->dayWidth() * 1.25);
    } else if (action->objectName() == "zoom_out") {
        // daywidth *MUST NOT* go below 1.0, it is used as an integer later on
        grid->setDayWidth(qMax<qreal>(1.0, grid->dayWidth() * 0.8));
    } else {
        warnPlan<<"Unknown gantt action:"<<action;
    }
}

Node *ResourceAppointmentsGanttView::currentNode() const
{
    QModelIndex idx = treeView()->selectionModel()->currentIndex();
    return m_model->node(idx);
}

void ResourceAppointmentsGanttView::slotContextMenuRequestedFromGantt(const QModelIndex &idx, const QPoint &pos)
{
    QModelIndex sidx = idx;
    if (sidx.isValid()) {
        const QAbstractProxyModel *proxy = qobject_cast<const QAbstractProxyModel*>(sidx.model());
        while (proxy != nullptr) {
            sidx = proxy->mapToSource(sidx);
            proxy = qobject_cast<const QAbstractProxyModel*>(sidx.model());
        }
        if (!sidx.isValid()) {
            warnPlan<<Q_FUNC_INFO<<"Failed to find item model";
            return;
        }
    }
    if (sidx.isValid() && m_model->node(sidx.parent())) {
        // we get the individual appointment interval, task is its parent
        sidx = sidx.parent();
    }
    m_gantt->treeView()->selectionModel()->setCurrentIndex(sidx, QItemSelectionModel::ClearAndSelect);
    slotContextMenuRequested(sidx, pos);
}

void ResourceAppointmentsGanttView::slotContextMenuRequested(const QModelIndex &idx, const QPoint &pos)
{
    debugPlan<<idx;
    QString name;
    if (idx.isValid()) {
        Node *n = m_model->node(idx);
        if (n) {
            name = "taskview_popup";
        }
    }
    m_gantt->treeView()->setContextMenuIndex(idx);
    if (name.isEmpty()) {
        slotHeaderContextMenuRequested(pos);
        m_gantt->treeView()->setContextMenuIndex(QModelIndex());
        return;
    }
    emit requestPopupMenu(name, pos);
    m_gantt->treeView()->setContextMenuIndex(QModelIndex());
}

void ResourceAppointmentsGanttView::slotOptions()
{
    debugPlan;
    ItemViewSettupDialog *dlg = new ResourceAppointmentsGanttViewSettingsDialog(m_gantt, this, sender()->objectName() == "print_options");
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOptionsFinished(int)));
    dlg->open();
}

bool ResourceAppointmentsGanttView::loadContext(const KoXmlElement &settings)
{
    debugPlan;
    ViewBase::loadContext(settings);
    m_gantt->loadContext(settings);
    return treeView()->loadContext(m_model->columnMap(), settings);
}

void ResourceAppointmentsGanttView::saveContext(QDomElement &settings) const
{
    debugPlan;
    ViewBase::saveContext(settings);
    m_gantt->saveContext(settings);
    treeView()->saveContext(m_model->columnMap(), settings);
}

void ResourceAppointmentsGanttView::updateReadWrite(bool on)
{
    m_readWrite = on;
}

KoPrintJob *ResourceAppointmentsGanttView::createPrintJob()
{
    return new GanttPrintingDialog(this, m_gantt);
}

}  //KPlato namespace

#include "moc_kptganttview.cpp"
