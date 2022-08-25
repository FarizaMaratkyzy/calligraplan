/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2021 Dag Andersen <dag.andersen@kdemail.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

// clazy:excludeall=qstring-arg
#include "SchedulingView.h"
#include "SchedulingModel.h"
#include "MainDocument.h"
#include "PlanGroupDebug.h"

#include <KoApplication.h>
#include <KoComponentData.h>
#include <KoDocument.h>
#include <KoPart.h>
#include <KoIcon.h>

#include <kptproject.h>
#include <kptitemmodelbase.h>
#include <kptproject.h>
#include <kptscheduleeditor.h>
#include <kptdatetime.h>
#include <kptschedulerplugin.h>
#include <kptcommand.h>
#include <kpttaskdescriptiondialog.h>
#include <kptcommand.h>

#include <KActionCollection>
#include <KXMLGUIFactory>
#include <KMessageBox>

#include <QTreeView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QDir>
#include <QItemSelectionModel>
#include <QSplitter>
#include <QAction>
#include <QMenu>
#include <QComboBox>
#include <QProgressDialog>

SchedulingView::SchedulingView(KoPart *part, KoDocument *doc, QWidget *parent)
    : KoView(part, doc, parent)
    , m_readWrite(false)
{
    //debugPlan;
    if (doc && doc->isReadWrite()) {
        setXMLFile(QStringLiteral("Portfolio_SchedulingViewUi.rc"));
    } else {
        setXMLFile(QStringLiteral("Portfolio_SchedulingViewUi_readonly.rc"));
    }
    setupGui();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(0);
    QSplitter *sp = new QSplitter(this);
    sp->setContextMenuPolicy(Qt::CustomContextMenu);
    sp->setOrientation(Qt::Vertical);
    layout->addWidget(sp);

    QWidget *w = new QWidget(this);
    ui.setupUi(w);
    sp->addWidget(w);

    SchedulingModel *model = new SchedulingModel(ui.schedulingView);
    model->setPortfolio(qobject_cast<MainDocument*>(doc));
    ui.schedulingView->setModel(model);
    model->setDelegates(ui.schedulingView);
    model->setCalculateFrom(ui.calculationDateTime->dateTime());

    // move some column after our extracolumns
    ui.schedulingView->header()->moveSection(1, model->columnCount()-1); // target start
    ui.schedulingView->header()->moveSection(1, model->columnCount()-1); // target finish
    ui.schedulingView->header()->moveSection(1, model->columnCount()-1); // description
    connect(ui.schedulingView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SchedulingView::selectionChanged);
    connect(ui.schedulingView, &QTreeView::customContextMenuRequested, this, &SchedulingView::slotContextMenuRequested);
    connect(ui.schedulingView, &QAbstractItemView::doubleClicked, this, &SchedulingView::itemDoubleClicked);

    m_logView = new QTreeView(sp);
    m_logView->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_logView->header()->setContextMenuPolicy(Qt::ActionsContextMenu);
    auto a = new QAction(QStringLiteral("Debug"), m_logView);
    a->setCheckable(true);
    m_logView->insertAction(nullptr, a);
    m_logView->header()->insertAction(nullptr, a);
    connect(a, &QAction::toggled, this, &SchedulingView::updateLogFilter);
    m_logView->setRootIsDecorated(false);
    sp->addWidget(m_logView);
    SchedulingLogFilterModel *fm = new SchedulingLogFilterModel(m_logView);
    fm->setSeveritiesDenied(QList<int>() << KPlato::Schedule::Log::Type_Debug);
    fm->setSourceModel(&m_logModel);
    m_logView->setModel(fm);
    updateActionsEnabled();

    updateSchedulingProperties();
    ui.todayRB->setChecked(true);
    slotTodayToggled(true);
    QTimer::singleShot(0, this, &SchedulingView::calculateFromChanged); // update model

    connect(model, &QAbstractItemModel::dataChanged, this, &SchedulingView::updateActionsEnabled);
    connect(model, &QAbstractItemModel::rowsInserted, this, &SchedulingView::updateSchedulingProperties);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &SchedulingView::updateSchedulingProperties);
    //connect(model, &QAbstractItemModel::modelReset, this, &SchedulingView::updateSchedulingProperties);
    connect(ui.schedulersCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotSchedulersComboChanged(int)));
    connect(ui.granularities, SIGNAL(currentIndexChanged(int)), this, SLOT(slotGranularitiesChanged(int)));
    connect(ui.sequential, &QRadioButton::toggled, this, &SchedulingView::slotSequentialChanged);
    connect(ui.todayRB, &QRadioButton::toggled, this, &SchedulingView::slotTodayToggled);
    connect(ui.tomorrowRB, &QRadioButton::toggled, this, &SchedulingView::slotTomorrowToggled);
    connect(ui.timeRB, &QRadioButton::toggled, this, &SchedulingView::slotTimeToggled);
    connect(ui.calculate, &QPushButton::clicked, this, &SchedulingView::calculate);
    connect(ui.calculationDateTime, &QDateTimeEdit::dateTimeChanged, this, &SchedulingView::calculateFromChanged);

    connect(static_cast<MainDocument*>(doc), &MainDocument::documentInserted, this, &SchedulingView::portfolioChanged);
    connect(static_cast<MainDocument*>(doc), &MainDocument::documentRemoved, this, &SchedulingView::portfolioChanged);

    setWhatsThis(xi18nc("@info:whatsthis",
        "<title>Scheduling</title>"
        "<para>"
        "The Scheduling view lets you schedule the projects in your portfolio. The projects are scheduled according to their priority."
        "</para><para>"
        "Set <emphasis>Control</emphasis> to <emphasis>Schedule</emphasis> for the project or projects you want to schedule."
        "<nl/>Set <emphasis>Control</emphasis> to <emphasis>Include</emphasis> to have resource assignments included in resource leveling."
        "<nl/>Set <emphasis>Control</emphasis> to <emphasis>Exclude</emphasis> if you want to exclude the project."
        "</para><para>"
        "<note>If your projects share resources with projects that are not part of your portfolio, these projects needs to be included for resource leveling to work properly.</note>"
        "</para><para>"
        "<link url='%1'>More...</link>"
        "</para>", QStringLiteral("portfolio:scheduling")));
}

SchedulingView::~SchedulingView()
{
}

void SchedulingView::portfolioChanged()
{
    ui.schedulingProperties->setDisabled(static_cast<MainDocument*>(koDocument())->documents().isEmpty());
}

void SchedulingView::updateLogFilter()
{
    auto a = qobject_cast<QAction*>(sender());
    if (!a) {
        return;
    }
    auto fm = qobject_cast<SchedulingLogFilterModel*>(m_logView->model());
    QList<int> filter;
    if (!a->isChecked()) {
        filter << KPlato::Schedule::Log::Type_Debug;
    }
    fm->setSeveritiesDenied(filter);
}

void SchedulingView::updateSchedulingProperties()
{
    MainDocument *portfolio = static_cast<MainDocument*>(koDocument());
    if (portfolio->documents().isEmpty()) {
        ui.schedulingProperties->setEnabled(false);
        return;
    }
    ui.schedulingProperties->setEnabled(true);
    ui.schedulersCombo->clear();
    const QMap<QString, KPlato::SchedulerPlugin*> plugins = portfolio->schedulerPlugins();
    QMap<QString, KPlato::SchedulerPlugin*>::const_iterator it;
    for (it = plugins.constBegin(); it != plugins.constEnd(); ++it) {
        ui.schedulersCombo->addItem(it.value()->name(), it.key());
    }
    slotSchedulersComboChanged(ui.schedulersCombo->currentIndex());
}

void SchedulingView::slotSchedulersComboChanged(int idx)
{
    ui.granularities->blockSignals(true);
    ui.granularities->clear();
    MainDocument *portfolio = static_cast<MainDocument*>(koDocument());
    const auto scheduler = portfolio->schedulerPlugin(ui.schedulersCombo->itemData(idx).toString());
    if (scheduler) {
        const auto lst = scheduler->granularities();
        for (auto v : lst) {
            ui.granularities->addItem(i18ncp("@label:listbox range: 0-60", "%1 minute", "%1 minutes", v/(60*1000)));
        }
        ui.granularities->setCurrentIndex(scheduler->granularityIndex());
        ui.sequential->setChecked(!scheduler->scheduleInParallel());
        ui.sequential->setEnabled(true);
        ui.parallel->setChecked(scheduler->scheduleInParallel());
        ui.parallel->setEnabled(scheduler->capabilities() & KPlato::SchedulerPlugin::ScheduleInParallel);
        ui.schedulersCombo->setWhatsThis(scheduler->comment());
    } else {
        ui.sequential->setEnabled(false);
        ui.parallel->setEnabled(false);
        ui.schedulersCombo->setWhatsThis(QString());
    }
    ui.granularities->blockSignals(false);
}

void SchedulingView::slotGranularitiesChanged(int idx)
{
    MainDocument *portfolio = static_cast<MainDocument*>(koDocument());
    const auto scheduler = portfolio->schedulerPlugin(ui.schedulersCombo->currentData().toString());
    if (scheduler) {
        scheduler->setGranularityIndex(idx);
    }
}

void SchedulingView::slotSequentialChanged(bool state)
{
    MainDocument *portfolio = static_cast<MainDocument*>(koDocument());
    const auto scheduler = portfolio->schedulerPlugin(ui.schedulersCombo->currentData().toString());
    if (scheduler) {
        scheduler->setScheduleInParallel(!state);
    }
}

void SchedulingView::slotTodayToggled(bool state)
{
    if (state) {
        ui.calculationDateTime->setDateTime(QDateTime(QDate::currentDate(), QTime()));
    }
}

void SchedulingView::slotTomorrowToggled(bool state)
{
    if (state) {
        ui.calculationDateTime->setDateTime(QDateTime(QDate::currentDate().addDays(1), QTime()));
    }
}

void SchedulingView::slotTimeToggled(bool state)
{
    ui.calculationDateTime->setEnabled(state);
}

void SchedulingView::setupGui()
{
    auto a  = new QAction(koIcon("document-edit"), i18n("Description..."), this);
    actionCollection()->addAction(QStringLiteral("project_description"), a);
    connect(a, &QAction::triggered, this, &SchedulingView::slotDescription);

}

void SchedulingView::itemDoubleClicked(const QPersistentModelIndex &idx)
{
    debugPortfolio<<idx;
    if (idx.column() == 3 /*Description*/) {
        slotDescription();
    }
}

void SchedulingView::slotContextMenuRequested(const QPoint &pos)
{
    debugPortfolio<<"Context menu"<<pos;
    if (!factory()) {
        debugPortfolio<<"No factory";
        return;
    }
    if (!ui.schedulingView->indexAt(pos).isValid()) {
        debugPortfolio<<"Nothing selected";
        return;
    }
    auto menu = static_cast<QMenu*>(factory()->container(QStringLiteral("schedulingview_popup"), this));
    Q_ASSERT(menu);
    if (menu->isEmpty()) {
        debugPortfolio<<"Menu is empty";
        return;
    }
    menu->exec(ui.schedulingView->viewport()->mapToGlobal(pos));
}

void SchedulingView::slotDescription()
{
    auto idx = ui.schedulingView->selectionModel()->currentIndex();
    if (!idx.isValid()) {
        debugPortfolio<<"No current project";
        return;
    }
    auto doc = ui.schedulingView->model()->data(idx, DOCUMENT_ROLE).value<KoDocument*>();
    auto project = doc->project();
    KPlato::TaskDescriptionDialog dia(*project, this, m_readWrite);
    if (dia.exec() == QDialog::Accepted) {
        auto m = dia.buildCommand();
        if (m) {
            doc->addCommand(m);
        }
    }
}

void SchedulingView::updateReadWrite(bool readwrite)
{
    m_readWrite = readwrite;
}

KoPrintJob *SchedulingView::createPrintJob()
{
    return nullptr;
}

void SchedulingView::updateActionsEnabled()
{
    ui.calculate->setEnabled(false);
    const auto portfolio = static_cast<MainDocument*>(koDocument());
    const auto docs = portfolio->documents();
    for (auto doc : docs) {
        if (doc->property(SCHEDULINGCONTROL).toString() == QStringLiteral("Schedule")) {
            ui.calculate->setEnabled(true);
            break;
        }
    }
}

void SchedulingView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    Q_UNUSED(selected)
    Q_UNUSED(deselected)

    KPlato::Project *project = nullptr;
    KoDocument *doc = ui.schedulingView->selectionModel()->selectedRows().value(0).data(DOCUMENT_ROLE).value<KoDocument*>();
    if (doc) {
        project = doc->project();
    }
    if (m_schedulingContext.project || m_schedulingContext.projects.key(doc, -1) == -1) {
        m_logModel.setLog(m_schedulingContext.log);
    } else if (project) {
        KPlato::ScheduleManager *sm = project->findScheduleManagerByName(doc->property(SCHEDULEMANAGERNAME).toString());
        if (sm) {
            m_logModel.setLog(sm->expected()->logs());
        } else {
            m_logModel.setLog(QVector<KPlato::Schedule::Log>());
        }
    } else {
        m_logModel.setLog(QVector<KPlato::Schedule::Log>());
    }
    updateActionsEnabled();
}

KPlato::ScheduleManager* SchedulingView::scheduleManager(const KoDocument *doc) const
{
    return static_cast<MainDocument*>(koDocument())->scheduleManager(doc);
}

QString SchedulingView::schedulerKey() const
{
    return ui.schedulersCombo->currentData().toString();
}

void SchedulingView::calculateFromChanged()
{
    auto model = static_cast<SchedulingModel*>(ui.schedulingView->model());
    model->setCalculateFrom(ui.calculationDateTime->dateTime());
}

QDateTime SchedulingView::calculationTime() const
{
    return ui.calculationDateTime->dateTime();
}

void SchedulingView::calculate()
{
    MainDocument *portfolio = static_cast<MainDocument*>(koDocument());
    QVariantList managerNames;
    {const auto docs = portfolio->documents();
    for (auto doc : docs) {
        managerNames << doc->property(SCHEDULEMANAGERNAME);
    }}
    m_schedulingContext.clear();
    m_logModel.setLog(m_schedulingContext.log);
    const auto key = schedulerKey();
    if (calculateSchedule(portfolio->schedulerPlugin(key))) {
        selectionChanged(QItemSelection(), QItemSelection());
        if (!m_schedulingContext.cancelScheduling && !m_schedulingContext.projects.isEmpty()) {
            portfolio->setModified(true);
        }
    }
}

bool SchedulingView::calculateSchedule(KPlato::SchedulerPlugin *scheduler)
{
    auto portfolio = static_cast<MainDocument*>(koDocument());
    auto docs = portfolio->documents();

    // Populate scheduling context
    m_schedulingContext.scheduler = scheduler;
    m_schedulingContext.project = new KPlato::Project(); // FIXME: Set target start/end properly
    m_schedulingContext.project->setName(QStringLiteral("Project Collection"));
    m_schedulingContext.calculateFrom = calculationTime();
    m_schedulingContext.log.clear();
    m_logModel.setLog(m_schedulingContext.log);
    if (scheduler) {
        warnPortfolio<<"No scheduler plugin"<<schedulerKey();
        KPlato::Schedule::Log log(m_schedulingContext.project, KPlato::Schedule::Log::Type_Error, i18n("Internal error. No scheduler plugin found."));
        m_logModel.setLog(QVector<KPlato::Schedule::Log>() << log);
        m_logView->resizeColumnToContents(0);
        return false;
    }
    if (docs.isEmpty()) {
        warnPortfolio<<"Nothing to schedule";
        KPlato::Schedule::Log log(m_schedulingContext.project, KPlato::Schedule::Log::Type_Warning, i18n("Nothing to schedule"));
        m_logModel.setLog(QVector<KPlato::Schedule::Log>() << log);
        m_logView->resizeColumnToContents(0);
        return false;
    }
    KPlato::Schedule::Log log(m_schedulingContext.project, KPlato::Schedule::Log::Type_Info, i18n("Scheduling running..."));
    m_logModel.setLog(QVector<KPlato::Schedule::Log>() << log);
    m_logView->resizeColumnToContents(0);
    QCoreApplication::processEvents();

    QApplication::setOverrideCursor(Qt::WaitCursor); // FIXME: workaround because progress dialog shown late, why?

    for (KoDocument *doc : qAsConst(docs)) {
        int prio = doc->property(SCHEDULINGPRIORITY).isValid() ? doc->property(SCHEDULINGPRIORITY).toInt() : -1;
        if (doc->property(SCHEDULINGCONTROL).toString() == QStringLiteral("Schedule")) {
            m_schedulingContext.addProject(doc, prio);
        } else if (doc->property(SCHEDULINGCONTROL).toString() == QStringLiteral("Include")) {
            m_schedulingContext.addResourceBookings(doc);
        }
    }
    if (m_schedulingContext.projects.isEmpty()) {
        warnPortfolio<<"Nothing to schedule";
        KPlato::Schedule::Log log(m_schedulingContext.project, KPlato::Schedule::Log::Type_Warning, i18n("Nothing to schedule"));
        m_logModel.setLog(QVector<KPlato::Schedule::Log>() << log);
        if (QApplication::overrideCursor()) {
            QApplication::restoreOverrideCursor();
        }
        return false;
    }
    m_progress = new QProgressDialog(this);
    m_progress->setLabelText(i18n("Scheduling projects"));
    m_progress->setWindowModality(Qt::WindowModal);
    m_progress->setMinimumDuration(0);
    connect(scheduler, &KPlato::SchedulerPlugin::progressChanged, this, [this](int value, KPlato::ScheduleManager*) {
        if (QApplication::overrideCursor()) {
            QApplication::restoreOverrideCursor();
        }
        if (!m_progress->wasCanceled()) {
            m_progress->setValue(value);
        }
    });
    connect(m_progress, &QProgressDialog::canceled, this, [this]() {
        m_schedulingContext.scheduler->cancelScheduling(m_schedulingContext);
    });
    scheduler->schedule(m_schedulingContext);
    m_logModel.setLog(m_schedulingContext.log);

    for (QMap<int, KoDocument*>::const_iterator it = m_schedulingContext.projects.constBegin(); it != m_schedulingContext.projects.constEnd(); ++it) {
        portfolio->emitDocumentChanged(it.value());
        Q_EMIT projectCalculated(it.value()->project(), it.value()->project()->findScheduleManagerByName(it.value()->property(SCHEDULEMANAGERNAME).toString()));
    }
    m_progress->deleteLater();
    if (QApplication::overrideCursor()) {
        QApplication::restoreOverrideCursor();
    }
    return true;
}
