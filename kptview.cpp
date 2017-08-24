/* This file is part of the KDE project
  Copyright (C) 1998, 1999, 2000 Torben Weis <weis@kde.org>
  Copyright (C) 2002 - 2011 Dag Andersen <danders@get2net.dk>
  Copyright (C) 2012 Dag Andersen <danders@get2net.dk>

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

#include "kptview.h"

#include <kmessagebox.h>
#include <KRecentFilesAction>

#include "KoDocumentInfo.h"
#include "KoMainWindow.h"
#include <KoIcon.h>
#include <KoResourcePaths.h>

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QPrintDialog>
#include <QDomDocument>
#include <QDomElement>
#include <kundo2command.h>
#include <QTimer>
#include <QDockWidget>
#include <QMenu>
#include <QTemporaryFile>
#include <QFileDialog>

#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kstandardaction.h>
#include <ktoolbar.h>
#include <kconfigdialogmanager.h>
#include <kxmlguifactory.h>
#include <ktoggleaction.h>
#include <kconfigdialog.h>
#include <ktoolinvocation.h>
#include <krun.h>
#include <khelpclient.h>

#include <KoTemplateCreateDia.h>
#include <KoPart.h>
#include <KoComponentData.h>
#include <KoPluginLoader.h>

#include "kptlocale.h"
#include "kptviewbase.h"
#include "kptaccountsview.h"
#include "kptaccountseditor.h"
#include "kptcalendareditor.h"
#include "kptfactory.h"
#include "kptmilestoneprogressdialog.h"
#include "kpttaskdescriptiondialog.h"
#include "kptnode.h"
#include "kptmaindocument.h"
#include "kptproject.h"
#include "kptmainprojectdialog.h"
#include "kpttask.h"
#include "kptsummarytaskdialog.h"
#include "kpttaskdialog.h"
#include "kpttaskprogressdialog.h"
#include "kptganttview.h"
#include "kpttaskeditor.h"
#include "kptdependencyeditor.h"
#include "kptperteditor.h"
#include "kptdatetime.h"
#include "kptcommand.h"
#include "kptrelation.h"
#include "kptrelationdialog.h"
#include "kptresourceappointmentsview.h"
#include "kptresourceeditor.h"
#include "kptscheduleeditor.h"
#include "kptresourcedialog.h"
#include "kptresource.h"
#include "kptstandardworktimedialog.h"
#include "kptwbsdefinitiondialog.h"
#include "kptresourceassignmentview.h"
#include "kpttaskstatusview.h"
#include "kptsplitterview.h"
#include "kptpertresult.h"
#include "ConfigProjectPanel.h"
#include "ConfigWorkVacationPanel.h"
#include "kpttaskdefaultpanel.h"
#include "kptworkpackageconfigpanel.h"
#include "kptcolorsconfigpanel.h"
#include "kptinsertfiledlg.h"
#include "kpthtmlview.h"
#include "about/aboutpage.h"
#include "kptlocaleconfigmoneydialog.h"
#include "kptflatproxymodel.h"
#include "kpttaskstatusmodel.h"

#include "reportsgenerator/ReportsGeneratorView.h"

#ifdef PLAN_USE_KREPORT
#include "reports/reportview.h"
#include "reports/reportdata.h"
#endif

#include "kptviewlistdialog.h"
#include "kptviewlistdocker.h"
#include "kptviewlist.h"
#include "kptschedulesdocker.h"
#include "kptpart.h"
#include "kptdebug.h"

#include "calligraplansettings.h"
#include "kptprintingcontrolprivate.h"

// #include "KPtViewAdaptor.h"

#include <assert.h>

namespace KPlato
{

//-------------------------------
ConfigDialog::ConfigDialog(QWidget *parent, const QString& name, KConfigSkeleton *config )
    : KConfigDialog( parent, name, config ),
    m_config( config )
{
    KConfigDialogManager::changedMap()->insert("KRichTextWidget", SIGNAL(textChanged()) );
}

bool ConfigDialog::hasChanged()
{
    QRegExp kcfg( "kcfg_*" );
    foreach ( KRichTextWidget *w, findChildren<KRichTextWidget*>( kcfg ) ) {
        KConfigSkeletonItem *item = m_config->findItem( w->objectName().mid(5) );
        if (  ! item->isEqual( w->toHtml() ) ) {
            return true;
        }
    }
    return false;
}

void ConfigDialog::updateSettings()
{
    bool changed = false;
    QRegExp kcfg( "kcfg_*" );
    foreach ( KRichTextWidget *w, findChildren<KRichTextWidget*>( kcfg ) ) {
        KConfigSkeletonItem *item = m_config->findItem( w->objectName().mid(5) );
        if ( ! item ) {
            warnPlan << "The setting '" <<  w->objectName().mid(5)  << "' has disappeared!";
            continue;
        }
        if ( ! item->isEqual( QVariant( w->toHtml() ) ) ) {
            item->setProperty( QVariant( w->toHtml() ) );
            changed = true;
        }
    }
    if ( changed ) {
        m_config->save();
    }
}

void ConfigDialog::updateWidgets()
{
    QRegExp kcfg( "kcfg_*" );
    foreach ( KRichTextWidget *w, findChildren<KRichTextWidget*>( kcfg ) ) {
        KConfigSkeletonItem *item = m_config->findItem( w->objectName().mid(5) );
        if ( ! item ) {
            warnPlan << "The setting '" <<  w->objectName().mid(5)  << "' has disappeared!";
            continue;
        }
        if ( ! item->isEqual( QVariant( w->toHtml() ) ) ) {
            w->setHtml( item->property().toString() );
        }
    }
}

void ConfigDialog::updateWidgetsDefault()
{
    bool usedefault = m_config->useDefaults( true );
    updateWidgets();
    m_config->useDefaults( usedefault );
}

bool ConfigDialog::isDefault()
{
    bool bUseDefaults = m_config->useDefaults(true);
    bool result = !hasChanged();
    m_config->useDefaults(bUseDefaults);
    return result;
}

//------------------------------------
View::View(KoPart *part, MainDocument *doc, QWidget *parent)
        : KoView(part, doc, parent),
        m_currentEstimateType( Estimate::Use_Expected ),
        m_scheduleActionGroup( new QActionGroup( this ) ),
        m_trigged( false ),
        m_nextScheduleManager( 0 ),
        m_readWrite( false ),
        m_defaultView(1),
        m_partpart (part)
{
    //debugPlan;

    doc->registerView( this );

    setComponentName(Factory::global().componentName(), Factory::global().componentDisplayName());
    if ( !doc->isReadWrite() )
        setXMLFile( "calligraplan_readonly.rc" );
    else
        setXMLFile( "calligraplan.rc" );

//     new ViewAdaptor( this );

    m_sp = new QSplitter( this );
    QVBoxLayout *layout = new QVBoxLayout( this );
    layout->setMargin(0);
    layout->addWidget( m_sp );

    ViewListDocker *docker = 0;
    if ( mainWindow() == 0 ) {
        // Don't use docker if embedded
        m_viewlist = new ViewListWidget(doc, m_sp);
        m_viewlist->setProject( &( getProject() ) );
        connect( m_viewlist, SIGNAL(selectionChanged(ScheduleManager*)), SLOT(slotSelectionChanged(ScheduleManager*)) );
        connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), m_viewlist, SLOT(setSelectedSchedule(ScheduleManager*)) );
        connect( m_viewlist, SIGNAL(updateViewInfo(ViewListItem*)), SLOT(slotUpdateViewInfo(ViewListItem*)) );
    } else {
        ViewListDockerFactory vl(this);
        docker = static_cast<ViewListDocker *>(mainWindow()->createDockWidget(&vl));
        if (docker->view() != this) {
            docker->setView(this);
        }
        m_viewlist = docker->viewList();
#if 0        //SchedulesDocker
        SchedulesDockerFactory sdf;
        SchedulesDocker *sd = dynamic_cast<SchedulesDocker*>( createDockWidget( &sdf ) );
        Q_ASSERT( sd );

        sd->setProject( &getProject() );
        connect( sd, SIGNAL(selectionChanged(ScheduleManager*)), SLOT(slotSelectionChanged(ScheduleManager*)) );
        connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), sd, SLOT(setSelectedSchedule(ScheduleManager*)) );
#endif
    }

    m_tab = new QStackedWidget( m_sp );

////////////////////////////////////////////////////////////////////////////////////////////////////

    // Add sub views
    createIntroductionView();

    // The menu items
    // ------ File
    actionCreateTemplate = new QAction( i18n( "&Create Template From Document..." ), this );
    actionCollection()->addAction("file_createtemplate", actionCreateTemplate );
    connect( actionCreateTemplate, SIGNAL(triggered(bool)), SLOT(slotCreateTemplate()) );

    actionCreateNewProject = new QAction( i18n( "&Create New Project..." ), this );
    actionCollection()->addAction("file_createnewproject", actionCreateNewProject );
    connect( actionCreateNewProject, SIGNAL(triggered(bool)), SLOT(slotCreateNewProject()) );

    // ------ Edit
    actionCut = actionCollection()->addAction(KStandardAction::Cut,  "edit_cut", this, SLOT(slotEditCut()));
    actionCopy = actionCollection()->addAction(KStandardAction::Copy,  "edit_copy", this, SLOT(slotEditCopy()));
    actionPaste = actionCollection()->addAction(KStandardAction::Paste,  "edit_paste", this, SLOT(slotEditPaste()));

    // ------ View
    actionCollection()->addAction( KStandardAction::Redisplay, "view_refresh" , this, SLOT(slotRefreshView()) );

    actionViewSelector  = new KToggleAction(i18n("Show Selector"), this);
    actionCollection()->addAction("view_show_selector", actionViewSelector );
    connect( actionViewSelector, SIGNAL(triggered(bool)), SLOT(slotViewSelector(bool)) );

    // ------ Insert

    // ------ Project
    actionEditMainProject  = new QAction(koIcon("view-time-schedule-edit"), i18n("Edit Main Project..."), this);
    actionCollection()->addAction("project_edit", actionEditMainProject );
    connect( actionEditMainProject, SIGNAL(triggered(bool)), SLOT(slotProjectEdit()) );

    actionEditStandardWorktime  = new QAction(koIcon("configure"), i18n("Define Estimate Conversions..."), this);
    actionCollection()->addAction("project_worktime", actionEditStandardWorktime );
    connect( actionEditStandardWorktime, SIGNAL(triggered(bool)), SLOT(slotProjectWorktime()) );


    // ------ Tools
    actionDefineWBS  = new QAction(koIcon("configure"), i18n("Define WBS Pattern..."), this);
    actionCollection()->addAction("tools_define_wbs", actionDefineWBS );
    connect( actionDefineWBS, SIGNAL(triggered(bool)), SLOT(slotDefineWBS()) );

    actionInsertFile  = new QAction(koIcon("document-import"), i18n("Insert Project File..."), this);
    actionCollection()->addAction("insert_file", actionInsertFile );
    connect( actionInsertFile, SIGNAL(triggered(bool)), SLOT(slotInsertFile()) );

    // ------ Settings
    actionConfigure  = new QAction(koIcon("configure"), i18n("Configure Plan..."), this);
    actionCollection()->addAction("configure", actionConfigure );
    connect( actionConfigure, SIGNAL(triggered(bool)), SLOT(slotConfigure()) );

    actionCurrencyConfig  = new QAction(koIcon("configure"), i18n("Define Currency..."), this);
    actionCollection()->addAction( "config_currency", actionCurrencyConfig );
    connect( actionCurrencyConfig, SIGNAL(triggered(bool)), SLOT(slotCurrencyConfig()) );

#ifdef PLAN_USE_KREPORT
    actionOpenReportFile  = new QAction(koIcon("document-open"), i18n("Open Report Definition File..."), this);
    actionCollection()->addAction( "reportdesigner_open_file", actionOpenReportFile );
    connect( actionOpenReportFile, SIGNAL(triggered(bool)), SLOT(slotOpenReportFile()) );
#endif

    // ------ Help
    actionIntroduction  = new QAction(koIcon("dialog-information"), i18n("Introduction to Plan"), this);
    actionCollection()->addAction("plan_introduction", actionIntroduction );
    connect( actionIntroduction, SIGNAL(triggered(bool)), SLOT(slotIntroduction()) );

    // ------ Popup
    actionOpenNode  = new QAction(koIcon("document-edit"), i18n("Edit..."), this);
    actionCollection()->addAction("node_properties", actionOpenNode );
    connect( actionOpenNode, SIGNAL(triggered(bool)), SLOT(slotOpenNode()) );
    actionTaskProgress  = new QAction(koIcon("document-edit"), i18n("Progress..."), this);
    actionCollection()->addAction("task_progress", actionTaskProgress );
    connect( actionTaskProgress, SIGNAL(triggered(bool)), SLOT(slotTaskProgress()) );
    actionDeleteTask  = new QAction(koIcon("edit-delete"), i18n("Delete Task"), this);
    actionCollection()->addAction("delete_task", actionDeleteTask );
    connect( actionDeleteTask, SIGNAL(triggered(bool)), SLOT(slotDeleteTask()) );
    actionTaskDescription  = new QAction(koIcon("document-edit"), i18n("Description..."), this);
    actionCollection()->addAction("task_description", actionTaskDescription );
    connect( actionTaskDescription, SIGNAL(triggered(bool)), SLOT(slotTaskDescription()) );
    actionIndentTask = new QAction(koIcon("format-indent-more"), i18n("Indent Task"), this);
    actionCollection()->addAction("indent_task", actionIndentTask );
    connect( actionIndentTask, SIGNAL(triggered(bool)), SLOT(slotIndentTask()) );
    actionUnindentTask= new QAction(koIcon("format-indent-less"), i18n("Unindent Task"), this);
    actionCollection()->addAction("unindent_task", actionUnindentTask );
    connect( actionUnindentTask, SIGNAL(triggered(bool)), SLOT(slotUnindentTask()) );
    actionMoveTaskUp = new QAction(koIcon("arrow-up"), i18n("Move Task Up"), this);
    actionCollection()->addAction("move_task_up", actionMoveTaskUp );
    connect( actionMoveTaskUp, SIGNAL(triggered(bool)), SLOT(slotMoveTaskUp()) );
    actionMoveTaskDown = new QAction(koIcon("arrow-down"), i18n("Move Task Down"), this);
    actionCollection()->addAction("move_task_down", actionMoveTaskDown );
    connect( actionMoveTaskDown, SIGNAL(triggered(bool)), SLOT(slotMoveTaskDown()) );

    actionEditResource  = new QAction(koIcon("document-edit"), i18n("Edit Resource..."), this);
    actionCollection()->addAction("edit_resource", actionEditResource );
    connect( actionEditResource, SIGNAL(triggered(bool)), SLOT(slotEditResource()) );

    actionEditRelation  = new QAction(koIcon("document-edit"), i18n("Edit Dependency..."), this);
    actionCollection()->addAction("edit_dependency", actionEditRelation );
    connect( actionEditRelation, SIGNAL(triggered(bool)), SLOT(slotModifyRelation()) );
    actionDeleteRelation  = new QAction(koIcon("edit-delete"), i18n("Delete Dependency"), this);
    actionCollection()->addAction("delete_dependency", actionDeleteRelation );
    connect( actionDeleteRelation, SIGNAL(triggered(bool)), SLOT(slotDeleteRelation()) );

    // Viewlist popup
    connect( m_viewlist, SIGNAL(createView()), SLOT(slotCreateView()) );

    m_estlabel = new QLabel( "", 0 );
    if ( statusBar() ) {
        addStatusBarItem( m_estlabel, 0, true );
    }

    connect( &getProject(), SIGNAL(scheduleChanged(MainSchedule*)), SLOT(slotScheduleChanged(MainSchedule*)) );

    connect( &getProject(), SIGNAL(scheduleAdded(const MainSchedule*)), SLOT(slotScheduleAdded(const MainSchedule*)) );
    connect( &getProject(), SIGNAL(scheduleRemoved(const MainSchedule*)), SLOT(slotScheduleRemoved(const MainSchedule*)) );
    slotPlugScheduleActions();

    connect( doc, SIGNAL(changed()), SLOT(slotUpdate()) );

    connect( m_scheduleActionGroup, SIGNAL(triggered(QAction*)), SLOT(slotViewSchedule(QAction*)) );


    connect( getPart(), SIGNAL(workPackageLoaded()), SLOT(slotWorkPackageLoaded()) );

    // hide unused dockers
    QTimer::singleShot( 0, this, SLOT(hideToolDocker()) );
    // create views after dockers hidden, views take time for large projects
    QTimer::singleShot( 100, this, SLOT(initiateViews()) );

    const QList<KPluginFactory *> pluginFactories =
        KoPluginLoader::instantiatePluginFactories(QStringLiteral("calligraplan/extensions"));

    foreach (KPluginFactory* factory, pluginFactories) {
        QObject *object = factory->create<QObject>(this, QVariantList());
        KXMLGUIClient *clientPlugin = dynamic_cast<KXMLGUIClient*>(object);
        if (clientPlugin) {
            insertChildClient(clientPlugin);
        } else {
            // not our/valid plugin, so delete the created object
            object->deleteLater();
        }
    }
    //debugPlan<<" end";
}

View::~View()
{
    ViewBase *view = currentView();
    if (view) {
        // deactivate view to remove dockers etc
        slotGuiActivated(view, false);
    }
    /*    removeStatusBarItem( m_estlabel );
    delete m_estlabel;*/
}

// hackish way to get rid of unused dockers, but as long as no official way exists...
void View::hideToolDocker()
{
    if ( mainWindow() ) {
        QStringList lst; lst << "KPlatoViewList" << "Scripting";
        QStringList names;
        foreach ( QDockWidget *w, mainWindow()->dockWidgets() ) {
            if ( ! lst.contains( w->objectName() ) ) {
                names << w->windowTitle();
                w->setFeatures( QDockWidget::DockWidgetClosable );
                w->hide();
            }
        }
        foreach(const KActionCollection *c, KActionCollection::allCollections()) {
            KActionMenu *a = qobject_cast<KActionMenu*>(c->action("settings_dockers_menu"));
            if ( a ) {
                QList<QAction*> actions = a->menu()->actions();
                foreach ( QAction *act, actions ) {
                    if ( names.contains( act->text() ) ) {
                        a->removeAction( act );
                    }
                }
                a->addSeparator();
                break;
            }
        }
    }
}

void View::initiateViews()
{
    QApplication::setOverrideCursor( Qt::WaitCursor );
    createViews();

    connect( m_viewlist, SIGNAL(activated(ViewListItem*,ViewListItem*)), SLOT(slotViewActivated(ViewListItem*,ViewListItem*)) );
    // after createViews() !!
    connect( m_viewlist, SIGNAL(viewListItemRemoved(ViewListItem*)), SLOT(slotViewListItemRemoved(ViewListItem*)) );
    // after createViews() !!
    connect( m_viewlist, SIGNAL(viewListItemInserted(ViewListItem*,ViewListItem*,int)), SLOT(slotViewListItemInserted(ViewListItem*,ViewListItem*,int)) );

    QDockWidget *docker = qobject_cast<QDockWidget*>( m_viewlist->parent() );
    if ( docker ) {
        // after createViews() !!
        connect( m_viewlist, SIGNAL(modified()), docker, SLOT(slotModified()));
        connect( m_viewlist, SIGNAL(modified()), getPart(), SLOT(viewlistModified()));
        connect(getPart(), SIGNAL(viewlistModified(bool)), docker, SLOT(updateWindowTitle(bool)));
    }
    connect( m_tab, SIGNAL(currentChanged(int)), this, SLOT(slotCurrentChanged(int)) );

    slotSelectDefaultView();

    loadContext();

    QApplication::restoreOverrideCursor();
}

void View::slotCreateTemplate()
{
    KoTemplateCreateDia::createTemplate(koDocument()->documentPart()->templatesResourcePath(), ".plant",
                                        getPart(), this);
}

void View::slotCreateNewProject()
{
    debugPlan;
    if ( KMessageBox::Continue == KMessageBox::warningContinueCancel( this,
                      xi18nc( "@info",
                             "<note>This action cannot be undone.</note><nl/><nl/>"
                             "Create a new Project from the current project "
                             "with new project- and task identities.<nl/>"
                             "Resource- and calendar identities are not changed.<nl/>"
                             "All scheduling information is removed.<nl/>"
                             "<nl/>Do you want to continue?" ) ) )
    {
        getPart()->createNewProject();
        slotOpenNode( &getProject() );
    }
}

void View::createViews()
{
    Context *ctx = getPart()->context();
    if ( ctx && ctx->isLoaded() ) {
        debugPlan<<"isLoaded";
        KoXmlNode n = ctx->context().namedItem( "categories" );
        if ( n.isNull() ) {
            warnPlan<<"No categories";
        } else {
            n = n.firstChild();
            for ( ; ! n.isNull(); n = n.nextSibling() ) {
                if ( ! n.isElement() ) {
                    continue;
                }
                KoXmlElement e = n.toElement();
                if (e.tagName() != "category") {
                    continue;
                }
                debugPlan<<"category: "<<e.attribute( "tag" );
                ViewListItem *cat;
                QString cn = e.attribute( "name" );
                QString ct = e.attribute( "tag" );
                if ( cn.isEmpty() ) {
                    cn = defaultCategoryInfo( ct ).name;
                }
                cat = m_viewlist->addCategory( ct, cn );
                KoXmlNode n1 = e.firstChild();
                for ( ; ! n1.isNull(); n1 = n1.nextSibling() ) {
                    if ( ! n1.isElement() ) {
                        continue;
                    }
                    KoXmlElement e1 = n1.toElement();
                    if (e1.tagName() != "view") {
                        continue;
                    }
                    ViewBase *v = 0;
                    QString type = e1.attribute( "viewtype" );
                    QString tag = e1.attribute( "tag" );
                    QString name = e1.attribute( "name" );
                    QString tip = e1.attribute( "tooltip" );
                    v = createView( cat, type, tag, name, tip );
                    //KoXmlNode settings = e1.namedItem( "settings " ); ????
                    KoXmlNode settings = e1.firstChild();
                    for ( ; ! settings.isNull(); settings = settings.nextSibling() ) {
                        if ( settings.nodeName() == "settings" ) {
                            break;
                        }
                    }
                    if ( v && settings.isElement() ) {
                        debugPlan<<" settings";
                        v->loadContext( settings.toElement() );
                    }
                }
            }
        }
    } else {
        debugPlan<<"Default";
        ViewListItem *cat;
        QString ct = "Editors";
        cat = m_viewlist->addCategory( ct, defaultCategoryInfo( ct ).name );

        createCalendarEditor( cat, "CalendarEditor", QString(), TIP_USE_DEFAULT_TEXT );

        createAccountsEditor( cat, "AccountsEditor", QString(), TIP_USE_DEFAULT_TEXT );

        createResourceEditor( cat, "ResourceEditor", QString(), TIP_USE_DEFAULT_TEXT );

        createTaskEditor( cat, "TaskEditor", QString(), TIP_USE_DEFAULT_TEXT );

        createDependencyEditor( cat, "DependencyEditor", QString(), TIP_USE_DEFAULT_TEXT );

        createPertEditor( cat, "PertEditor", QString(), TIP_USE_DEFAULT_TEXT );

        createScheduleHandler( cat, "ScheduleHandlerView", QString(), TIP_USE_DEFAULT_TEXT );

        ct = "Views";
        cat = m_viewlist->addCategory( ct, defaultCategoryInfo( ct ).name );

        createGanttView( cat, "GanttView", QString(), TIP_USE_DEFAULT_TEXT );

        createMilestoneGanttView( cat, "MilestoneGanttView", QString(), TIP_USE_DEFAULT_TEXT );

        createResourceAppointmentsView( cat, "ResourceAppointmentsView", QString(), TIP_USE_DEFAULT_TEXT );

        createResourceAppointmentsGanttView( cat, "ResourceAppointmentsGanttView", QString(), TIP_USE_DEFAULT_TEXT );

        createAccountsView( cat, "AccountsView", QString(), TIP_USE_DEFAULT_TEXT );

        ct = "Execution";
        cat = m_viewlist->addCategory( ct, defaultCategoryInfo( ct ).name );

        createProjectStatusView( cat, "ProjectStatusView", QString(), TIP_USE_DEFAULT_TEXT );

        createPerformanceStatusView( cat, "PerformanceStatusView", QString(), TIP_USE_DEFAULT_TEXT );

        createTaskStatusView( cat, "TaskStatusView", QString(), TIP_USE_DEFAULT_TEXT );

        createTaskView( cat, "TaskView", QString(), TIP_USE_DEFAULT_TEXT );

        createTaskWorkPackageView( cat, "TaskWorkPackageView", QString(), TIP_USE_DEFAULT_TEXT );

        ct = "Reports";
        cat = m_viewlist->addCategory(ct, defaultCategoryInfo(ct).name);

        createReportsGeneratorView(cat, "ReportsGeneratorView", i18n("Generate reports"), TIP_USE_DEFAULT_TEXT);

#ifdef PLAN_USE_KREPORT
        // A little hack to get the user started...
        ReportView *rv = qobject_cast<ReportView*>( createReportView( cat, "ReportView", i18n( "Task Status Report" ), TIP_USE_DEFAULT_TEXT ) );
        if ( rv ) {
            QDomDocument doc;
            doc.setContent( standardTaskStatusReport() );
            rv->loadXML( doc );
        }
#endif
    }
}

ViewBase *View::createView( ViewListItem *cat, const QString &type, const QString &tag, const QString &name, const QString &tip, int index )
{
    ViewBase *v = 0;
    //NOTE: type is the same as classname (so if it is changed...)
    if ( type == "CalendarEditor" ) {
        v = createCalendarEditor( cat, tag, name, tip, index );
    } else if ( type == "AccountsEditor" ) {
        v = createAccountsEditor( cat, tag, name, tip, index );
    } else if ( type == "ResourceEditor" ) {
        v = createResourceEditor( cat, tag, name, tip, index );
    } else if ( type == "TaskEditor" ) {
        v = createTaskEditor( cat, tag, name, tip, index );
    } else if ( type == "DependencyEditor" ) {
        v = createDependencyEditor( cat, tag, name, tip, index );
    } else if ( type == "PertEditor" ) {
        v = createPertEditor( cat, tag, name, tip, index );
    } else if ( type == "ScheduleEditor" ) {
        v = createScheduleEditor( cat, tag, name, tip, index );
    } else if ( type == "ScheduleHandlerView" ) {
        v = createScheduleHandler( cat, tag, name, tip, index );
    } else if ( type == "ProjectStatusView" ) {
        v = createProjectStatusView( cat, tag, name, tip, index );
    } else if ( type == "TaskStatusView" ) {
        v = createTaskStatusView( cat, tag, name, tip, index );
    } else if ( type == "TaskView" ) {
        v = createTaskView( cat, tag, name, tip, index );
    } else if ( type == "TaskWorkPackageView" ) {
        v = createTaskWorkPackageView( cat, tag, name, tip, index );
    } else if ( type == "GanttView" ) {
        v = createGanttView( cat, tag, name, tip, index );
    } else if ( type == "MilestoneGanttView" ) {
        v = createMilestoneGanttView( cat, tag, name, tip, index );
    } else if ( type == "ResourceAppointmentsView" ) {
        v = createResourceAppointmentsView( cat, tag, name, tip, index );
    } else if ( type == "ResourceAppointmentsGanttView" ) {
        v = createResourceAppointmentsGanttView( cat, tag, name, tip, index );
    } else if ( type == "AccountsView" ) {
        v = createAccountsView( cat, tag, name, tip, index );
    } else if ( type == "PerformanceStatusView" ) {
        v = createPerformanceStatusView( cat, tag, name, tip, index );
    } else if ( type == "ReportsGeneratorView" ) {
        v = createReportsGeneratorView(cat, tag, name, tip, index);
    } else if ( type == "ReportView" ) {
#ifdef PLAN_USE_KREPORT
        v = createReportView( cat, tag, name, tip, index );
#endif
    } else  {
        warnPlan<<"Unknown viewtype: "<<type;
    }
    return v;
}

void View::slotUpdateViewInfo( ViewListItem *itm )
{
    if ( itm->type() == ViewListItem::ItemType_SubView ) {
        itm->setViewInfo( defaultViewInfo( itm->viewType() ) );
    } else if ( itm->type() == ViewListItem::ItemType_Category ) {
        ViewInfo vi = defaultCategoryInfo( itm->tag() );
        itm->setViewInfo( vi );
    }
}

ViewInfo View::defaultViewInfo( const QString &type ) const
{
    ViewInfo vi;
    if ( type == "CalendarEditor" ) {
        vi.name = i18n( "Work & Vacation" );
        vi.tip = xi18nc( "@info:tooltip", "Edit working- and vacation days for resources" );
    } else if ( type == "AccountsEditor" ) {
        vi.name = i18n( "Cost Breakdown Structure" );
        vi.tip = xi18nc( "@info:tooltip", "Edit cost breakdown structure." );
    } else if ( type == "ResourceEditor" ) {
        vi.name = i18n( "Resources" );
        vi.tip = xi18nc( "@info:tooltip", "Edit resource breakdown structure" );
    } else if ( type == "TaskEditor" ) {
        vi.name = i18n( "Tasks" );
        vi.tip = xi18nc( "@info:tooltip", "Edit work breakdown structure" );
    } else if ( type == "DependencyEditor" ) {
        vi.name = i18n( "Dependencies (Graphic)" );
        vi.tip = xi18nc( "@info:tooltip", "Edit task dependencies" );
    } else if ( type == "PertEditor" ) {
        vi.name = i18n( "Dependencies (List)" );
        vi.tip = xi18nc( "@info:tooltip", "Edit task dependencies" );
    } else if ( type == "ScheduleEditor" ) {
        // This view is not used stand-alone atm
        vi.name = i18n( "Schedules" );
    } else if ( type == "ScheduleHandlerView" ) {
        vi.name = i18n( "Schedules" );
        vi.tip = xi18nc( "@info:tooltip", "Calculate and analyze project schedules" );
    } else if ( type == "ProjectStatusView" ) {
        vi.name = i18n( "Project Performance Chart" );
        vi.tip = xi18nc( "@info:tooltip", "View project status information" );
    } else if ( type == "TaskStatusView" ) {
        vi.name = i18n( "Task Status" );
        vi.tip = xi18nc( "@info:tooltip", "View task progress information" );
    } else if ( type == "TaskView" ) {
        vi.name = i18n( "Task Execution" );
        vi.tip = xi18nc( "@info:tooltip", "View task execution information" );
    } else if ( type == "TaskWorkPackageView" ) {
        vi.name = i18n( "Work Package View" );
        vi.tip = xi18nc( "@info:tooltip", "View task work package information" );
    } else if ( type == "GanttView" ) {
        vi.name = i18n( "Gantt" );
        vi.tip = xi18nc( "@info:tooltip", "View Gantt chart" );
    } else if ( type == "MilestoneGanttView" ) {
        vi.name = i18n( "Milestone Gantt" );
        vi.tip = xi18nc( "@info:tooltip", "View milestone Gantt chart" );
    } else if ( type == "ResourceAppointmentsView" ) {
        vi.name = i18n( "Resource Assignments" );
        vi.tip = xi18nc( "@info:tooltip", "View resource assignments in a table" );
    } else if ( type == "ResourceAppointmentsGanttView" ) {
        vi.name = i18n( "Resource Assignments (Gantt)" );
        vi.tip = xi18nc( "@info:tooltip", "View resource assignments in Gantt chart" );
    } else if ( type == "AccountsView" ) {
        vi.name = i18n( "Cost Breakdown" );
        vi.tip = xi18nc( "@info:tooltip", "View planned and actual cost" );
    } else if ( type == "PerformanceStatusView" ) {
        vi.name = i18n( "Tasks Performance Chart" );
        vi.tip = xi18nc( "@info:tooltip", "View tasks performance status information" );
    } else if ( type == "ReportsGeneratorView" ) {
        vi.name = i18n( "Reports Generator" );
        vi.tip = xi18nc( "@info:tooltip", "Generate reports" );
    } else if ( type == "ReportView" ) {
        vi.name = i18n( "Report" );
        vi.tip = xi18nc( "@info:tooltip", "View report" );
    } else  {
        warnPlan<<"Unknown viewtype: "<<type;
    }
    return vi;
}

ViewInfo View::defaultCategoryInfo( const QString &type ) const
{
    ViewInfo vi;
    if ( type == "Editors" ) {
        vi.name = i18n( "Editors" );
    } else if ( type == "Views" ) {
        vi.name = i18n( "Views" );
    } else if ( type == "Execution" ) {
        vi.name = i18nc( "Project execution views", "Execution" );
    } else if ( type == "Reports" ) {
        vi.name = i18n( "Reports" );
    }
    return vi;
}

void View::slotOpenUrlRequest( HtmlView *v, const QUrl &url )
{
    debugPlan<<url;
    if ( url.scheme() == QLatin1String("about") ) {
        if ( url.url() == QLatin1String("about:close") ) {
            int view = m_visitedViews.count() < 2 ? qMin(m_defaultView, m_tab->count()-1) : m_visitedViews.at(m_visitedViews.count() - 2);
            debugPlan<<"Prev:"<<view<<m_visitedViews;
            m_tab->setCurrentIndex(view);
            return;
        }
        if ( url.url().startsWith( QLatin1String( "about:plan" ) ) ) {
            getPart()->aboutPage().generatePage( v->htmlPart(), url );
            return;
        }
    }
    if ( url.scheme() == QLatin1String("help") ) {
        KHelpClient::invokeHelp( "", url.fileName() );
        return;
    }
    // try to open the url
    debugPlan<<url<<"is external, try to open";
    new KRun( url, mainWindow() );
}

ViewBase *View::createIntroductionView()
{
    HtmlView *v = new HtmlView(getKoPart(), getPart(), m_tab );
    v->htmlPart().setJScriptEnabled(false);
    v->htmlPart().setJavaEnabled(false);
    v->htmlPart().setMetaRefreshEnabled(false);
    v->htmlPart().setPluginsEnabled(false);

    slotOpenUrlRequest( v, QUrl( "about:plan/main" ) );

    connect( v, SIGNAL(openUrlRequest(HtmlView*,QUrl)), SLOT(slotOpenUrlRequest(HtmlView*,QUrl)) );

    m_tab->addWidget( v );
    return v;
}

ViewBase *View::createResourceAppointmentsGanttView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ResourceAppointmentsGanttView *v = new ResourceAppointmentsGanttView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ResourceAppointmentsGanttView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }


    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( v, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    v->setProject( &( getProject() ) );
    v->setScheduleManager( currentScheduleManager() );
    v->updateReadWrite( m_readWrite );
    return v;
}


ViewBase *View::createResourceAppointmentsView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ResourceAppointmentsView *v = new ResourceAppointmentsView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ResourceAppointmentsView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( v, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    v->setProject( &( getProject() ) );
    v->setScheduleManager( currentScheduleManager() );
    v->updateReadWrite( m_readWrite );
    return v;
}

ViewBase *View::createResourceEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ResourceEditor *resourceeditor = new ResourceEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( resourceeditor );
    resourceeditor->setProject( &(getProject()) );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, resourceeditor, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ResourceEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    connect( resourceeditor, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( resourceeditor, SIGNAL(deleteObjectList(QObjectList)), SLOT(slotDeleteResourceObjects(QObjectList)) );

    connect( resourceeditor, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    resourceeditor->updateReadWrite( m_readWrite );
    return resourceeditor;
}

ViewBase *View::createTaskEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    TaskEditor *taskeditor = new TaskEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( taskeditor );
    m_defaultView = m_tab->count() - 1;

    ViewListItem *i = m_viewlist->addView( cat, tag, name, taskeditor, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "TaskEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    taskeditor->setProject( &(getProject()) );
    taskeditor->setScheduleManager( currentScheduleManager() );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), taskeditor, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( taskeditor, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( taskeditor, SIGNAL(addTask()), SLOT(slotAddTask()) );
    connect( taskeditor, SIGNAL(addMilestone()), SLOT(slotAddMilestone()) );
    connect( taskeditor, SIGNAL(addSubtask()), SLOT(slotAddSubTask()) );
    connect( taskeditor, SIGNAL(addSubMilestone()), SLOT(slotAddSubMilestone()) );
    connect( taskeditor, SIGNAL(deleteTaskList(QList<Node*>)), SLOT(slotDeleteTask(QList<Node*>)) );
    connect( taskeditor, SIGNAL(moveTaskUp()), SLOT(slotMoveTaskUp()) );
    connect( taskeditor, SIGNAL(moveTaskDown()), SLOT(slotMoveTaskDown()) );
    connect( taskeditor, SIGNAL(indentTask()), SLOT(slotIndentTask()) );
    connect( taskeditor, SIGNAL(unindentTask()), SLOT(slotUnindentTask()) );

    connect(taskeditor, SIGNAL(saveTaskModule(QUrl,Project*)), SLOT(saveTaskModule(QUrl,Project*)));
    connect(taskeditor, SIGNAL(removeTaskModule(QUrl)), SLOT(removeTaskModule(QUrl)));

    connect( taskeditor, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    taskeditor->updateReadWrite( m_readWrite );

    // last:
    QStringList modules = KoResourcePaths::findAllResources( "calligraplan_taskmodules", "*.plan", KoResourcePaths::NoDuplicates|KoResourcePaths::Recursive );
    debugPlan<<modules;
    taskeditor->setTaskModules( modules );
    return taskeditor;
}

ViewBase *View::createAccountsEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    AccountsEditor *ae = new AccountsEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( ae );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, ae, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "AccountsEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    ae->draw( getProject() );

    connect( ae, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );
    ae->updateReadWrite( m_readWrite );
    return ae;
}

ViewBase *View::createCalendarEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    CalendarEditor *calendareditor = new CalendarEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( calendareditor );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, calendareditor, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "CalendarEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    calendareditor->draw( getProject() );

    connect( calendareditor, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( calendareditor, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    calendareditor->updateReadWrite( m_readWrite );
    return calendareditor;
}

ViewBase *View::createScheduleHandler( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ScheduleHandlerView *handler = new ScheduleHandlerView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( handler );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, handler, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ScheduleHandlerView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    connect( handler->scheduleEditor(), SIGNAL(addScheduleManager(Project*)), SLOT(slotAddScheduleManager(Project*)) );
    connect( handler->scheduleEditor(), SIGNAL(deleteScheduleManager(Project*,ScheduleManager*)), SLOT(slotDeleteScheduleManager(Project*,ScheduleManager*)) );
    connect( handler->scheduleEditor(), SIGNAL(moveScheduleManager(ScheduleManager*,ScheduleManager*,int)), SLOT(slotMoveScheduleManager(ScheduleManager*,ScheduleManager*,int)));

    connect( handler->scheduleEditor(), SIGNAL(calculateSchedule(Project*,ScheduleManager*)), SLOT(slotCalculateSchedule(Project*,ScheduleManager*)) );

    connect( handler->scheduleEditor(), SIGNAL(baselineSchedule(Project*,ScheduleManager*)), SLOT(slotBaselineSchedule(Project*,ScheduleManager*)) );


    connect( handler, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), handler, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)) );

    connect( handler, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    connect(handler, SIGNAL(editNode(Node*)), this, SLOT(slotOpenNode(Node*)));
    connect(handler, SIGNAL(editResource(Resource*)), this, SLOT(slotEditResource(Resource*)));

    handler->draw( getProject() );
    handler->updateReadWrite( m_readWrite );
    return handler;
}

ScheduleEditor *View::createScheduleEditor( QWidget *parent )
{
    ScheduleEditor *scheduleeditor = new ScheduleEditor(getKoPart(), getPart(), parent );

    connect( scheduleeditor, SIGNAL(addScheduleManager(Project*)), SLOT(slotAddScheduleManager(Project*)) );
    connect( scheduleeditor, SIGNAL(deleteScheduleManager(Project*,ScheduleManager*)), SLOT(slotDeleteScheduleManager(Project*,ScheduleManager*)) );

    connect( scheduleeditor, SIGNAL(calculateSchedule(Project*,ScheduleManager*)), SLOT(slotCalculateSchedule(Project*,ScheduleManager*)) );

    connect( scheduleeditor, SIGNAL(baselineSchedule(Project*,ScheduleManager*)), SLOT(slotBaselineSchedule(Project*,ScheduleManager*)) );

    scheduleeditor->updateReadWrite( m_readWrite );
    return scheduleeditor;
}

ViewBase *View::createScheduleEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ScheduleEditor *scheduleeditor = new ScheduleEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( scheduleeditor );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, scheduleeditor, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ScheduleEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    scheduleeditor->setProject( &( getProject() ) );

    connect( scheduleeditor, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( scheduleeditor, SIGNAL(addScheduleManager(Project*)), SLOT(slotAddScheduleManager(Project*)) );

    connect( scheduleeditor, SIGNAL(deleteScheduleManager(Project*,ScheduleManager*)), SLOT(slotDeleteScheduleManager(Project*,ScheduleManager*)) );

    connect( scheduleeditor, SIGNAL(calculateSchedule(Project*,ScheduleManager*)), SLOT(slotCalculateSchedule(Project*,ScheduleManager*)) );

    connect( scheduleeditor, SIGNAL(baselineSchedule(Project*,ScheduleManager*)), SLOT(slotBaselineSchedule(Project*,ScheduleManager*)) );

    scheduleeditor->updateReadWrite( m_readWrite );
    return scheduleeditor;
}


ViewBase *View::createDependencyEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    DependencyEditor *editor = new DependencyEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( editor );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, editor, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "DependencyEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    editor->draw( getProject() );

    connect( editor, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( editor, SIGNAL(addRelation(Node*,Node*,int)), SLOT(slotAddRelation(Node*,Node*,int)) );
    connect( editor, SIGNAL(modifyRelation(Relation*,int)), SLOT(slotModifyRelation(Relation*,int)) );
    connect( editor, SIGNAL(modifyRelation(Relation*)), SLOT(slotModifyRelation(Relation*)) );

    connect( editor, SIGNAL(editNode(Node*)), SLOT(slotOpenNode(Node*)) );
    connect( editor, SIGNAL(addTask()), SLOT(slotAddTask()) );
    connect( editor, SIGNAL(addMilestone()), SLOT(slotAddMilestone()) );
    connect( editor, SIGNAL(addSubMilestone()), SLOT(slotAddSubMilestone()) );
    connect( editor, SIGNAL(addSubtask()), SLOT(slotAddSubTask()) );
    connect( editor, SIGNAL(deleteTaskList(QList<Node*>)), SLOT(slotDeleteTask(QList<Node*>)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), editor, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( editor, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    editor->updateReadWrite( m_readWrite );
    editor->setScheduleManager( currentScheduleManager() );
    return editor;
}

ViewBase *View::createPertEditor( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    PertEditor *perteditor = new PertEditor(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( perteditor );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, perteditor, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "PertEditor" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    perteditor->draw( getProject() );

    connect( perteditor, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );
    m_updatePertEditor = true;
    perteditor->updateReadWrite( m_readWrite );
    return perteditor;
}

ViewBase *View::createProjectStatusView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ProjectStatusView *v = new ProjectStatusView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ProjectStatusView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );

    v->updateReadWrite( m_readWrite );
    v->setProject( &getProject() );
    v->setScheduleManager( currentScheduleManager() );
    return v;
}

ViewBase *View::createPerformanceStatusView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    PerformanceStatusView *v = new PerformanceStatusView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "PerformanceStatusView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( v, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    v->updateReadWrite( m_readWrite );
    v->setProject( &getProject() );
    v->setScheduleManager( currentScheduleManager() );
    return v;
}


ViewBase *View::createTaskStatusView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    TaskStatusView *taskstatusview = new TaskStatusView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( taskstatusview );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, taskstatusview, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "TaskStatusView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    connect( taskstatusview, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), taskstatusview, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( taskstatusview, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    taskstatusview->updateReadWrite( m_readWrite );
    taskstatusview->draw( getProject() );
    taskstatusview->setScheduleManager( currentScheduleManager() );
    return taskstatusview;
}

ViewBase *View::createTaskView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    TaskView *v = new TaskView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "TaskView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    v->draw( getProject() );
    v->setScheduleManager( currentScheduleManager() );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( v, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    v->updateReadWrite( m_readWrite );
    return v;
}

ViewBase *View::createTaskWorkPackageView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    TaskWorkPackageView *v = new TaskWorkPackageView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "TaskWorkPackageView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    v->setProject( &getProject() );
    v->setScheduleManager( currentScheduleManager() );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( v, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    connect( v, SIGNAL(mailWorkpackage(Node*,Resource*)), SLOT(slotMailWorkpackage(Node*,Resource*)) );
    connect( v, SIGNAL(mailWorkpackages(QList<Node*>,Resource*)), SLOT(slotMailWorkpackages(QList<Node*>,Resource*)) );

    connect(v, SIGNAL(checkForWorkPackages()), getPart(), SLOT(checkForWorkPackages()));
    v->updateReadWrite( m_readWrite );
    return v;
}

ViewBase *View::createGanttView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    GanttView *ganttview = new GanttView(getKoPart(), getPart(), m_tab, koDocument()->isReadWrite() );
    m_tab->addWidget( ganttview );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, ganttview, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "GanttView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    ganttview->setProject( &( getProject() ) );
    ganttview->setScheduleManager( currentScheduleManager() );

    connect( ganttview, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );
/*  TODO: Review these
    connect( ganttview, SIGNAL(addRelation(Node*,Node*,int)), SLOT(slotAddRelation(Node*,Node*,int)) );
    connect( ganttview, SIGNAL(modifyRelation(Relation*,int)), SLOT(slotModifyRelation(Relation*,int)) );
    connect( ganttview, SIGNAL(modifyRelation(Relation*)), SLOT(slotModifyRelation(Relation*)) );
    connect( ganttview, SIGNAL(itemDoubleClicked()), SLOT(slotOpenNode()) );
    connect( ganttview, SIGNAL(itemRenamed(Node*,QString)), this, SLOT(slotRenameNode(Node*,QString)) );*/

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), ganttview, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( ganttview, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    ganttview->updateReadWrite( m_readWrite );
    return ganttview;
}

ViewBase *View::createMilestoneGanttView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    MilestoneGanttView *ganttview = new MilestoneGanttView(getKoPart(), getPart(), m_tab, koDocument()->isReadWrite() );
    m_tab->addWidget( ganttview );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, ganttview, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "MilestoneGanttView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    ganttview->setProject( &( getProject() ) );
    ganttview->setScheduleManager( currentScheduleManager() );

    connect( ganttview, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), ganttview, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( ganttview, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    ganttview->updateReadWrite( m_readWrite );
    return ganttview;
}


ViewBase *View::createAccountsView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    AccountsView *accountsview = new AccountsView(getKoPart(), &getProject(), getPart(), m_tab );
    m_tab->addWidget( accountsview );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, accountsview, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "AccountsView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    accountsview->setScheduleManager( currentScheduleManager() );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), accountsview, SLOT(setScheduleManager(ScheduleManager*)) );

    connect( accountsview, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );
    accountsview->updateReadWrite( m_readWrite );
    return accountsview;
}

ViewBase *View::createResourceAssignmentView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
    ResourceAssignmentView *resourceAssignmentView = new ResourceAssignmentView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( resourceAssignmentView );
    m_updateResourceAssignmentView = true;

    ViewListItem *i = m_viewlist->addView( cat, tag, name, resourceAssignmentView, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ResourceAssignmentView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    resourceAssignmentView->draw( getProject() );

    connect( resourceAssignmentView, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );

    connect( resourceAssignmentView, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );
    resourceAssignmentView->updateReadWrite( m_readWrite );
    return resourceAssignmentView;
}

ViewBase *View::createReportsGeneratorView(ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index)
{
    ReportsGeneratorView *v = new ReportsGeneratorView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView(cat, tag, name, v, getPart(), "", index);
    ViewInfo vi = defaultViewInfo( "ReportsGeneratorView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    v->setProject( &getProject() );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );
    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(slotRefreshView()));
    v->setScheduleManager( currentScheduleManager() );

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );
    connect( v, SIGNAL(requestPopupMenu(QString,QPoint)), this, SLOT(slotPopupMenu(QString,QPoint)) );

    v->updateReadWrite( m_readWrite );
    return v;
}

ViewBase *View::createReportView( ViewListItem *cat, const QString &tag, const QString &name, const QString &tip, int index )
{
#ifdef PLAN_USE_KREPORT
    ReportView *v = new ReportView(getKoPart(), getPart(), m_tab );
    m_tab->addWidget( v );

    ViewListItem *i = m_viewlist->addView( cat, tag, name, v, getPart(), "", index );
    ViewInfo vi = defaultViewInfo( "ReportView" );
    if ( name.isEmpty() ) {
        i->setText( 0, vi.name );
    }
    if ( tip == TIP_USE_DEFAULT_TEXT ) {
        i->setToolTip( 0, vi.tip );
    } else {
        i->setToolTip( 0, tip );
    }

    v->setProject( &getProject() );

    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(setScheduleManager(ScheduleManager*)) );
    connect( this, SIGNAL(currentScheduleManagerChanged(ScheduleManager*)), v, SLOT(slotRefreshView()));
    v->setScheduleManager( currentScheduleManager() );

    connect( v, SIGNAL(guiActivated(ViewBase*,bool)), SLOT(slotGuiActivated(ViewBase*,bool)) );
    v->updateReadWrite( m_readWrite );
    return v;
#else
    return 0;
#endif
}

Project& View::getProject() const
{
    return getPart() ->getProject();
}

KoPrintJob * View::createPrintJob()
{
    KoView *v = qobject_cast<KoView*>( canvas() );
    if ( v == 0 ) {
        return 0;
    }
    return v->createPrintJob();
}

ViewBase *View::currentView() const
{
    return qobject_cast<ViewBase*>( m_tab->currentWidget() );
}

void View::slotEditCut()
{
    ViewBase *v = currentView();
    if ( v ) {
        v->slotEditCut();
    }
}

void View::slotEditCopy()
{
    ViewBase *v = currentView();
    if ( v ) {
        v->slotEditCopy();
    }
}

void View::slotEditPaste()
{
    ViewBase *v = currentView();
    if ( v ) {
        v->slotEditPaste();
    }
}

void View::slotRefreshView()
{
    ViewBase *v = currentView();
    if ( v ) {
        debugPlan<<v;
        v->slotRefreshView();
    }
}

void View::slotViewSelector( bool show )
{
    //debugPlan;
    m_viewlist->setVisible( show );
}

void View::slotInsertResourcesFile(const QString &file)
{
    getPart()->insertResourcesFile(QUrl(file));
}

void View::slotInsertFile()
{
    InsertFileDialog *dlg = new InsertFileDialog( getProject(), currentTask(), this );
    connect(dlg, SIGNAL(finished(int)), SLOT(slotInsertFileFinished(int)));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void View::slotInsertFileFinished( int result )
{
    InsertFileDialog *dlg = qobject_cast<InsertFileDialog*>( sender() );
    if ( dlg == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted ) {
        getPart()->insertFile( dlg->url(), dlg->parentNode(), dlg->afterNode() );
    }
    dlg->deleteLater();
}

void View::slotProjectEdit()
{
    slotOpenNode( &getProject() );
}

void View::slotProjectWorktime()
{
    StandardWorktimeDialog *dia = new StandardWorktimeDialog( getProject(), this );
    connect(dia, SIGNAL(finished(int)), this, SLOT(slotProjectWorktimeFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotProjectWorktimeFinished( int result )
{
    StandardWorktimeDialog *dia = qobject_cast<StandardWorktimeDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * cmd = dia->buildCommand();
        if ( cmd ) {
            //debugPlan<<"Modifying calendar(s)";
            getPart() ->addCommand( cmd ); //also executes
        }
    }
    dia->deleteLater();
}

void View::slotSelectionChanged( ScheduleManager *sm ) {
    debugPlan<<sm;
    if ( sm == 0 ) {
        return;
    }
    int idx = m_scheduleActions.values().indexOf( sm->expected() );
    if ( idx < 0 ) {
        debugPlan<<sm<<"could not find schedule"<<sm->expected();
        return;
    }
    QAction *a = m_scheduleActions.keys().at( idx );
    Q_ASSERT( a );
    a->setChecked( true ); // this doesn't trigger QActionGroup
    slotViewSchedule( a );
}

QList<QAction*> View::sortedActionList()
{
    QMap<QString, QAction*> lst;
    foreach ( QAction *a, m_scheduleActions.keys() ) {
        lst.insert( a->objectName(), a );
    }
    return lst.values();
}

void View::slotScheduleRemoved( const MainSchedule *sch )
{
    debugPlan<<sch<<sch->name();
    QAction *a = 0;
    QAction *checked = m_scheduleActionGroup->checkedAction();
    QMapIterator<QAction*, Schedule*> i( m_scheduleActions );
    while (i.hasNext()) {
        i.next();
        if ( i.value() == sch ) {
            a = i.key();
            break;
        }
    }
    if ( a ) {
        unplugActionList( "view_schedule_list" );
        delete a;
        plugActionList( "view_schedule_list", sortedActionList() );
        if ( checked && checked != a ) {
            checked->setChecked( true );
        } else if ( ! m_scheduleActions.isEmpty() ) {
            m_scheduleActions.keys().first()->setChecked( true );
        }
    }
    slotViewSchedule( m_scheduleActionGroup->checkedAction() );
}

void View::slotScheduleAdded( const MainSchedule *sch )
{
    if ( sch->type() != Schedule::Expected ) {
        return; // Only view expected
    }
    MainSchedule *s = const_cast<MainSchedule*>( sch );
//     debugPlan<<sch->name()<<" deleted="<<sch->isDeleted()<<"scheduled="<<sch->isScheduled();
    QAction *checked = m_scheduleActionGroup->checkedAction();
    if ( ! sch->isDeleted() && sch->isScheduled() ) {
        unplugActionList( "view_schedule_list" );
        QAction *act = addScheduleAction( s );
        plugActionList( "view_schedule_list", sortedActionList() );
        if ( checked ) {
            checked->setChecked( true );
        } else if ( act ) {
            act->setChecked( true );
        } else if ( ! m_scheduleActions.isEmpty() ) {
            m_scheduleActions.keys().first()->setChecked( true );
        }
    }
    slotViewSchedule( m_scheduleActionGroup->checkedAction() );
}

void View::slotScheduleChanged( MainSchedule *sch )
{
//     debugPlan<<sch->name()<<" deleted="<<sch->isDeleted()<<"scheduled="<<sch->isScheduled();
    if ( sch->isDeleted() || ! sch->isScheduled() ) {
        slotScheduleRemoved( sch );
        return;
    }
    if ( m_scheduleActions.values().contains( sch ) ) {
        slotScheduleRemoved( sch ); // hmmm, how to avoid this?
    }
    slotScheduleAdded( sch );
}

QAction *View::addScheduleAction( Schedule *sch )
{
    QAction *act = 0;
    if ( ! sch->isDeleted() && sch->isScheduled() ) {
        QString n = sch->name();
        act = new KToggleAction( n, this);
        actionCollection()->addAction(n, act );
        m_scheduleActions.insert( act, sch );
        m_scheduleActionGroup->addAction( act );
        //debugPlan<<"Add:"<<n;
        connect( act, SIGNAL(destroyed(QObject*)), SLOT(slotActionDestroyed(QObject*)) );
    }
    return act;
}

void View::slotViewScheduleManager()
{
    QApplication::setOverrideCursor( Qt::WaitCursor );
    setLabel( m_nextScheduleManager );
    emit currentScheduleManagerChanged( m_nextScheduleManager );
    m_nextScheduleManager = 0;
    m_trigged = false;
    QApplication::restoreOverrideCursor();
}

void View::slotViewSchedule( QAction *act )
{
    //debugPlan<<act;
    ScheduleManager *sm = 0;
    if ( act != 0 ) {
        Schedule *sch = m_scheduleActions.value( act, 0 );
        sm = sch->manager();
    }
    emit currentScheduleManagerChanged( 0 );
    setLabel( 0 );
    m_nextScheduleManager = sm;
    // Performance is very dependent on schedule manager change since a lot is recalculated
    // In case of multiple changes, only issue the last change
    if ( ! m_trigged ) {
        m_trigged = true;
        emit currentScheduleManagerChanged( 0 );
        QTimer::singleShot( 0, this, SLOT(slotViewScheduleManager()) );
    }
}

void View::slotActionDestroyed( QObject *o )
{
    //debugPlan<<o->name();
    m_scheduleActions.remove( static_cast<QAction*>( o ) );
//    slotViewSchedule( m_scheduleActionGroup->checkedAction() );
}

void View::slotPlugScheduleActions()
{
    //debugPlan<<activeScheduleId();
    long id = activeScheduleId();
    unplugActionList( "view_schedule_list" );
    foreach( QAction *act, m_scheduleActions.keys() ) {
        m_scheduleActionGroup->removeAction( act );
        delete act;
    }
    m_scheduleActions.clear();
    QAction *ca = 0;
    foreach( ScheduleManager *sm, getProject().allScheduleManagers() ) {
        Schedule *sch = sm->expected();
        if ( sch == 0 ) {
            continue;
        }
        QAction *act = addScheduleAction( sch );
        if ( act && id == sch->id() ) {
            ca = act;
        }
    }
    plugActionList( "view_schedule_list", sortedActionList() );
    if ( ca == 0 && m_scheduleActionGroup->actions().count() > 0 ) {
        ca = m_scheduleActionGroup->actions().first();
    }
    if ( ca ) {
        ca->setChecked( true );
    }
    slotViewSchedule( ca );
}

void View::slotProjectCalculated( ScheduleManager *sm )
{
    // we only get here if current schedule was calculated
    if ( sm->isScheduled() ) {
        slotSelectionChanged( sm );
    }
}

void View::slotCalculateSchedule( Project *project, ScheduleManager *sm )
{
    if ( project == 0 || sm == 0 ) {
        return;
    }
    if ( sm->parentManager() && ! sm->parentManager()->isScheduled() ) {
        // the parent must be scheduled
        return;
    }
    if ( sm == currentScheduleManager() ) {
        connect( project, SIGNAL(projectCalculated(ScheduleManager*)), this, SLOT(slotProjectCalculated(ScheduleManager*)) );
    }
    CalculateScheduleCmd *cmd =  new CalculateScheduleCmd( *project, sm, kundo2_i18nc("@info:status 1=schedule name", "Calculate %1", sm->name() ) );
    getPart() ->addCommand( cmd );
    slotUpdate();
}

void View::slotRemoveCommands()
{
    while ( ! m_undocommands.isEmpty() ) {
        m_undocommands.last()->undo();
        delete m_undocommands.takeLast();
    }
}

void View::slotBaselineSchedule( Project *project, ScheduleManager *sm )
{
    if ( project == 0 || sm == 0 ) {
        return;
    }
    if ( ! sm->isBaselined() && project->isBaselined() ) {
        KMessageBox::sorry( this, i18n( "Cannot baseline. The project is already baselined." ) );
        return;
    }
    KUndo2Command *cmd;
    if ( sm->isBaselined() ) {
        KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel( this, i18n( "This schedule is baselined. Do you want to remove the baseline?" ) );
        if ( res == KMessageBox::Cancel ) {
            return;
        }
        cmd = new ResetBaselineScheduleCmd( *sm, kundo2_i18n( "Reset baseline %1", sm->name() ) );
    } else {
        cmd = new BaselineScheduleCmd( *sm, kundo2_i18n( "Baseline %1", sm->name() ) );
    }
    getPart() ->addCommand( cmd );
}

void View::slotAddScheduleManager( Project *project )
{
    if ( project == 0 ) {
        return;
    }
    ScheduleManager *sm = project->createScheduleManager();
    AddScheduleManagerCmd *cmd =  new AddScheduleManagerCmd( *project, sm, -1, kundo2_i18n( "Add schedule %1", sm->name() ) );
    getPart() ->addCommand( cmd );
}

void View::slotDeleteScheduleManager( Project *project, ScheduleManager *sm )
{
    if ( project == 0 || sm == 0) {
        return;
    }
    DeleteScheduleManagerCmd *cmd =  new DeleteScheduleManagerCmd( *project, sm, kundo2_i18n( "Delete schedule %1", sm->name() ) );
    getPart() ->addCommand( cmd );
}

void View::slotMoveScheduleManager( ScheduleManager *sm, ScheduleManager *parent, int index )
{
    if ( sm == 0 ) {
        return;
    }
    MoveScheduleManagerCmd *cmd =  new MoveScheduleManagerCmd( sm, parent, index, kundo2_i18n( "Move schedule %1", sm->name() ) );
    getPart() ->addCommand( cmd );
}

void View::slotAddSubTask()
{
    Task * node = getProject().createTask( getPart() ->config().taskDefaults() );
    SubTaskAddDialog *dia = new SubTaskAddDialog( getProject(), *node, currentNode(), getProject().accounts(), this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotAddSubTaskFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotAddSubTaskFinished( int result )
{
    SubTaskAddDialog *dia = qobject_cast<SubTaskAddDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result  == QDialog::Accepted) {
        KUndo2Command *m = dia->buildCommand();
        getPart() ->addCommand( m ); // add task to project
    }
    dia->deleteLater();
}

void View::slotAddTask()
{
    Task * node = getProject().createTask( getPart() ->config().taskDefaults() );
    TaskAddDialog *dia = new TaskAddDialog( getProject(), *node, currentNode(), getProject().accounts(), this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotAddTaskFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotAddTaskFinished( int result )
{
    TaskAddDialog *dia = qobject_cast<TaskAddDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command *m = dia->buildCommand();
        getPart() ->addCommand( m ); // add task to project
    }
    dia->deleteLater();
}

void View::slotAddMilestone()
{
    Task * node = getProject().createTask();
    node->estimate() ->clear();

    TaskAddDialog *dia = new TaskAddDialog( getProject(), *node, currentNode(), getProject().accounts(), this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotAddMilestoneFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotAddMilestoneFinished( int result )
{
    TaskAddDialog *dia = qobject_cast<TaskAddDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        MacroCommand *c = new MacroCommand( kundo2_i18n( "Add milestone" ) );
        c->addCommand( dia->buildCommand() );
        getPart() ->addCommand( c ); // add task to project
    }
    dia->deleteLater();
}

void View::slotAddSubMilestone()
{
    Task * node = getProject().createTask();
    node->estimate() ->clear();

    SubTaskAddDialog *dia = new SubTaskAddDialog( getProject(), *node, currentNode(), getProject().accounts(), this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotAddSubMilestoneFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotAddSubMilestoneFinished( int result )
{
    SubTaskAddDialog *dia = qobject_cast<SubTaskAddDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        MacroCommand *c = new MacroCommand( kundo2_i18n( "Add sub-milestone" ) );
        c->addCommand( dia->buildCommand() );
        getPart() ->addCommand( c ); // add task to project
    }
    dia->deleteLater();
}

void View::slotDefineWBS()
{
    //debugPlan;
    Project &p = getProject();
    WBSDefinitionDialog *dia = new WBSDefinitionDialog( p, p.wbsDefinition(), this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotDefineWBSFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotDefineWBSFinished( int result )
{
    //debugPlan;
    WBSDefinitionDialog *dia = qobject_cast<WBSDefinitionDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted ) {
        KUndo2Command *cmd = dia->buildCommand();
        if ( cmd ) {
            getPart()->addCommand( cmd );
        }
    }
    dia->deleteLater();
}

void View::slotConfigure()
{
    //debugPlan;
    if( KConfigDialog::showDialog("Plan Settings") ) {
        return;
    }
    ConfigDialog *dialog = new ConfigDialog( this, "Plan Settings", KPlatoSettings::self() );
    dialog->addPage(new ConfigProjectPanel(), i18n("Project Defaults"), koIconName("calligraplan") );
    dialog->addPage(new ConfigWorkVacationPanel(), i18n("Work & Vacation"), koIconName("view-calendar") );
    dialog->addPage(new TaskDefaultPanel(), i18n("Task Defaults"), koIconName("view-task") );
    dialog->addPage(new ColorsConfigPanel(), i18n("Task Colors"), koIconName("fill-color") );
    dialog->addPage(new WorkPackageConfigPanel(), i18n("Work Package"), koIconName("calligraplanwork") );
    dialog->show();

}

void View::slotIntroduction()
{
    m_tab->setCurrentIndex(0);
}


Calendar *View::currentCalendar()
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return 0;
    }
    return v->currentCalendar();
}

Node *View::currentNode() const
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return 0;
    }
    Node * task = v->currentNode();
    if ( 0 != task ) {
        return task;
    }
    return &( getProject() );
}

Task *View::currentTask() const
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return 0;
    }
    Node * task = v->currentNode();
    if ( task ) {
        return dynamic_cast<Task*>( task );
    }
    return 0;
}

Resource *View::currentResource()
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return 0;
    }
    return v->currentResource();
}

ResourceGroup *View::currentResourceGroup()
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return 0;
    }
    return v->currentResourceGroup();
}


void View::slotOpenNode()
{
    //debugPlan;
    Node * node = currentNode();
    slotOpenNode( node );
}

void View::slotOpenNode( Node *node )
{
    //debugPlan;
    if ( !node )
        return ;

    switch ( node->type() ) {
        case Node::Type_Project: {
                Project * project = static_cast<Project *>( node );
                MainProjectDialog *dia = new MainProjectDialog( *project, this );
                connect(dia, SIGNAL(dialogFinished(int)), SLOT(slotProjectEditFinished(int)));
                connect(dia, SIGNAL(sigLoadSharedResources(const QString&)), this, SLOT(slotInsertResourcesFile(const QString&)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        case Node::Type_Subproject:
            //TODO
            break;
        case Node::Type_Task: {
                Task *task = static_cast<Task *>( node );
                TaskDialog *dia = new TaskDialog( getProject(), *task, getProject().accounts(), this );
                connect(dia, SIGNAL(finished(int)), SLOT(slotTaskEditFinished(int)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        case Node::Type_Milestone: {
                // Use the normal task dialog for now.
                // Maybe milestone should have it's own dialog, but we need to be able to
                // enter a duration in case we accidentally set a tasks duration to zero
                // and hence, create a milestone
                Task *task = static_cast<Task *>( node );
                TaskDialog *dia = new TaskDialog( getProject(), *task, getProject().accounts(), this );
                connect(dia, SIGNAL(finished(int)), SLOT(slotTaskEditFinished(int)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        case Node::Type_Summarytask: {
                Task *task = dynamic_cast<Task *>( node );
                Q_ASSERT( task );
                SummaryTaskDialog *dia = new SummaryTaskDialog( *task, this );
                connect(dia, SIGNAL(finished(int)), SLOT(slotSummaryTaskEditFinished(int)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        default:
            break; // avoid warnings
    }
}

void View::slotProjectEditFinished( int result )
{
    MainProjectDialog *dia = qobject_cast<MainProjectDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * cmd = dia->buildCommand();
        if ( cmd ) {
            getPart() ->addCommand( cmd );
        }
    }
    dia->deleteLater();
}

void View::slotTaskEditFinished( int result )
{
    TaskDialog *dia = qobject_cast<TaskDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * cmd = dia->buildCommand();
        if ( cmd ) {
            getPart() ->addCommand( cmd );
        }
    }
    dia->deleteLater();
}

void View::slotSummaryTaskEditFinished( int result )
{
    SummaryTaskDialog *dia = qobject_cast<SummaryTaskDialog*>( sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * cmd = dia->buildCommand();
        if ( cmd ) {
            getPart() ->addCommand( cmd );
        }
    }
    dia->deleteLater();
}

ScheduleManager *View::currentScheduleManager() const
{
    Schedule *s = m_scheduleActions.value( m_scheduleActionGroup->checkedAction() );
    return s == 0 ? 0 : s->manager();
}

long View::activeScheduleId() const
{
    Schedule *s = m_scheduleActions.value( m_scheduleActionGroup->checkedAction() );
    return s == 0 ? -1 : s->id();
}

void View::setActiveSchedule( long id )
{
    if ( id != -1 ) {
        QMap<QAction*, Schedule*>::const_iterator it = m_scheduleActions.constBegin();
        for (; it != m_scheduleActions.constEnd(); ++it ) {
            if ( it.value()->id() == id ) {
                it.key()->setChecked( true );
                slotViewSchedule( it.key() ); // signal not emitted from group, so trigger it here
                break;
            }
        }
    }
}

void View::slotTaskProgress()
{
    //debugPlan;
    Node * node = currentNode();
    if ( !node )
        return ;

    switch ( node->type() ) {
        case Node::Type_Project: {
                break;
            }
        case Node::Type_Subproject:
            //TODO
            break;
        case Node::Type_Task: {
                Task *task = dynamic_cast<Task *>( node );
                Q_ASSERT( task );
                TaskProgressDialog *dia = new TaskProgressDialog( *task, currentScheduleManager(),  getProject().standardWorktime(), this );
                connect(dia, SIGNAL(finished(int)), SLOT(slotTaskProgressFinished(int)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        case Node::Type_Milestone: {
                Task *task = dynamic_cast<Task *>( node );
                Q_ASSERT( task );
                MilestoneProgressDialog *dia = new MilestoneProgressDialog( *task, this );
                connect(dia, SIGNAL(finished(int)), SLOT(slotMilestoneProgressFinished(int)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        case Node::Type_Summarytask: {
                // TODO
                break;
            }
        default:
            break; // avoid warnings
    }
}

void View::slotTaskProgressFinished( int result )
{
    TaskProgressDialog *dia = qobject_cast<TaskProgressDialog*>(sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * m = dia->buildCommand();
        if ( m ) {
            getPart() ->addCommand( m );
        }
    }
    dia->deleteLater();
}

void View::slotMilestoneProgressFinished( int result )
{
    MilestoneProgressDialog *dia = qobject_cast<MilestoneProgressDialog*>(sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * m = dia->buildCommand();
        if ( m ) {
            getPart() ->addCommand( m );
        }
    }
    dia->deleteLater();
}

void View::slotTaskDescription()
{
    //debugPlan;
    Node * node = currentNode();
    if ( !node )
        return ;

    switch ( node->type() ) {
        case Node::Type_Project: {
                break;
            }
        case Node::Type_Subproject:
            //TODO
            break;
        case Node::Type_Task:
        case Node::Type_Milestone:
        case Node::Type_Summarytask: {
                Task *task = dynamic_cast<Task *>( node );
                Q_ASSERT( task );
                TaskDescriptionDialog *dia = new TaskDescriptionDialog( *task, this );
                connect(dia, SIGNAL(finished(int)), SLOT(slotTaskDescriptionFinished(int)));
                dia->show();
                dia->raise();
                dia->activateWindow();
                break;
            }
        default:
            break; // avoid warnings
    }
}

void View::slotTaskDescriptionFinished( int result )
{
    TaskDescriptionDialog *dia = qobject_cast<TaskDescriptionDialog*>(sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * m = dia->buildCommand();
        if ( m ) {
            getPart() ->addCommand( m );
        }
    }
    dia->deleteLater();
}

void View::slotDeleteTask( QList<Node*> lst )
{
    //debugPlan;
    foreach ( Node *n, lst ) {
        if ( n->isScheduled() ) {
            KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel( this, i18n( "A task that has been scheduled will be deleted. This will invalidate the schedule." ) );
            if ( res == KMessageBox::Cancel ) {
                return;
            }
            break;
        }
    }
    if ( lst.count() == 1 ) {
        getPart()->addCommand( new NodeDeleteCmd( lst.takeFirst(), kundo2_i18n( "Delete task" ) ) );
        return;
    }
    int num = 0;
    MacroCommand *cmd = new MacroCommand( kundo2_i18np( "Delete task", "Delete tasks", lst.count() ) );
    while ( !lst.isEmpty() ) {
        Node *node = lst.takeFirst();
        if ( node == 0 || node->parentNode() == 0 ) {
            debugPlan << ( node ?"Task is main project" :"No current task" );
            continue;
        }
        bool del = true;
        foreach ( Node *n, lst ) {
            if ( node->isChildOf( n ) ) {
                del = false; // node is going to be deleted when we delete n
                break;
            }
        }
        if ( del ) {
            //debugPlan<<num<<": delete:"<<node->name();
            cmd->addCommand( new NodeDeleteCmd( node, kundo2_i18n( "Delete task" ) ) );
            num++;
        }
    }
    if ( num > 0 ) {
        getPart()->addCommand( cmd );
    } else {
        delete cmd;
    }
}

void View::slotDeleteTask( Node *node )
{
    //debugPlan;
    if ( node == 0 || node->parentNode() == 0 ) {
        debugPlan << ( node ?"Task is main project" :"No current task" );
        return ;
    }
    if ( node->isScheduled() ) {
        KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel( this, i18n( "This task has been scheduled. This will invalidate the schedule." ) );
        if ( res == KMessageBox::Cancel ) {
            return;
        }
    }
    NodeDeleteCmd *cmd = new NodeDeleteCmd( node, kundo2_i18n( "Delete task" ) );
    getPart() ->addCommand( cmd );
}

void View::slotDeleteTask()
{
    //debugPlan;
    return slotDeleteTask( currentNode() );
}

void View::slotIndentTask()
{
    //debugPlan;
    Node * node = currentNode();
    if ( node == 0 || node->parentNode() == 0 ) {
        debugPlan << ( node ?"Task is main project" :"No current task" );
        return ;
    }
    if ( getProject().canIndentTask( node ) ) {
        NodeIndentCmd * cmd = new NodeIndentCmd( *node, kundo2_i18n( "Indent task" ) );
        getPart() ->addCommand( cmd );
    }
}

void View::slotUnindentTask()
{
    //debugPlan;
    Node * node = currentNode();
    if ( node == 0 || node->parentNode() == 0 ) {
        debugPlan << ( node ?"Task is main project" :"No current task" );
        return ;
    }
    if ( getProject().canUnindentTask( node ) ) {
        NodeUnindentCmd * cmd = new NodeUnindentCmd( *node, kundo2_i18n( "Unindent task" ) );
        getPart() ->addCommand( cmd );
    }
}

void View::slotMoveTaskUp()
{
    //debugPlan;

    Node * task = currentNode();
    if ( 0 == task ) {
        // is always != 0. At least we would get the Project, but you never know who might change that
        // so better be careful
        errorPlan << "No current task" << endl;
        return ;
    }

    if ( Node::Type_Project == task->type() ) {
        debugPlan <<"The root node cannot be moved up";
        return ;
    }
    if ( getProject().canMoveTaskUp( task ) ) {
        NodeMoveUpCmd * cmd = new NodeMoveUpCmd( *task, kundo2_i18n( "Move task up" ) );
        getPart() ->addCommand( cmd );
    }
}

void View::slotMoveTaskDown()
{
    //debugPlan;

    Node * task = currentNode();
    if ( 0 == task ) {
        // is always != 0. At least we would get the Project, but you never know who might change that
        // so better be careful
        return ;
    }

    if ( Node::Type_Project == task->type() ) {
        debugPlan <<"The root node cannot be moved down";
        return ;
    }
    if ( getProject().canMoveTaskDown( task ) ) {
        NodeMoveDownCmd * cmd = new NodeMoveDownCmd( *task, kundo2_i18n( "Move task down" ) );
        getPart() ->addCommand( cmd );
    }
}

void View::slotAddRelation( Node *par, Node *child )
{
    //debugPlan;
    Relation * rel = new Relation( par, child );
    AddRelationDialog *dia = new AddRelationDialog( getProject(), rel, this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotAddRelationFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotAddRelationFinished( int result )
{
    AddRelationDialog *dia = qobject_cast<AddRelationDialog*>(sender() );
    if ( dia == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * m = dia->buildCommand();
        if ( m ) {
            getPart() ->addCommand( m );
        }
    }
    dia->deleteLater();
}

void View::slotAddRelation( Node *par, Node *child, int linkType )
{
    //debugPlan;
    if ( linkType == Relation::FinishStart ||
            linkType == Relation::StartStart ||
            linkType == Relation::FinishFinish ) {
        Relation * rel = new Relation( par, child, static_cast<Relation::Type>( linkType ) );
        getPart() ->addCommand( new AddRelationCmd( getProject(), rel, kundo2_i18n( "Add task dependency" ) ) );
    } else {
        slotAddRelation( par, child );
    }
}

void View::slotModifyRelation( Relation *rel )
{
    //debugPlan;
    ModifyRelationDialog *dia = new ModifyRelationDialog( getProject(), rel, this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotModifyRelationFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotModifyRelationFinished( int result )
{
    ModifyRelationDialog *dia = qobject_cast<ModifyRelationDialog*>( sender() );
    if ( dia == 0 ) {
        return ;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command *cmd = dia->buildCommand();
        if ( cmd ) {
            getPart() ->addCommand( cmd );
        }
    }
    dia->deleteLater();
}

void View::slotModifyRelation( Relation *rel, int linkType )
{
    //debugPlan;
    if ( linkType == Relation::FinishStart ||
            linkType == Relation::StartStart ||
            linkType == Relation::FinishFinish ) {
        getPart() ->addCommand( new ModifyRelationTypeCmd( rel, static_cast<Relation::Type>( linkType ) ) );
    } else {
        slotModifyRelation( rel );
    }
}

void View::slotModifyRelation()
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return;
    }
    Relation *rel = v->currentRelation();
    if ( rel ) {
        slotModifyRelation( rel );
    }
}

void View::slotDeleteRelation()
{
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v == 0 ) {
        return;
    }
    Relation *rel = v->currentRelation();
    if ( rel ) {
        getPart()->addCommand( new DeleteRelationCmd( getProject(), rel, kundo2_i18n( "Delete task dependency" ) ) );
    }
}

void View::slotEditResource()
{
    //debugPlan;
    slotEditResource( currentResource() );
}

void View::slotEditResource( Resource *resource )
{
    if ( resource == 0 ) {
        return ;
    }
    ResourceDialog *dia = new ResourceDialog( getProject(), resource, this );
    connect(dia, SIGNAL(finished(int)), SLOT(slotEditResourceFinished(int)));
    dia->show();
    dia->raise();
    dia->activateWindow();
}

void View::slotEditResourceFinished( int result )
{
    //debugPlan;
    ResourceDialog *dia = qobject_cast<ResourceDialog*>( sender() );
    if ( dia == 0 ) {
        return ;
    }
    if ( result == QDialog::Accepted) {
        KUndo2Command * cmd = dia->buildCommand();
        if ( cmd )
            getPart() ->addCommand( cmd );
    }
    dia->deleteLater();
}

void View::slotDeleteResource( Resource *resource )
{
    getPart()->addCommand( new RemoveResourceCmd( resource->parentGroup(), resource, kundo2_i18n( "Delete resource" ) ) );
}

void View::slotDeleteResourceGroup( ResourceGroup *group )
{
    getPart()->addCommand( new RemoveResourceGroupCmd( group->project(), group, kundo2_i18n( "Delete resourcegroup" ) ) );
}

void View::slotDeleteResourceObjects( QObjectList lst )
{
    //debugPlan;
    foreach ( QObject *o, lst ) {
        Resource *r = qobject_cast<Resource*>( o );
        if ( r && r->isScheduled() ) {
            KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel( this, i18n( "A resource that has been scheduled will be deleted. This will invalidate the schedule." ) );
            if ( res == KMessageBox::Cancel ) {
                return;
            }
            break;
        }
        ResourceGroup *g = qobject_cast<ResourceGroup*>( o );
        if ( g && g->isScheduled() ) {
            KMessageBox::ButtonCode res = KMessageBox::warningContinueCancel( this, i18n( "A resource that has been scheduled will be deleted. This will invalidate the schedule." ) );
            if ( res == KMessageBox::Cancel ) {
                return;
            }
            break;
        }
    }
    if ( lst.count() == 1 ) {
        Resource *r = qobject_cast<Resource*>( lst.first() );
        if ( r ) {
            slotDeleteResource( r );
        } else {
            ResourceGroup *g = qobject_cast<ResourceGroup*>( lst.first() );
            if ( g ) {
                slotDeleteResourceGroup( g );
            }
        }
        return;
    }
//    int num = 0;
    MacroCommand *cmd = 0, *rc = 0, *gc = 0;
    foreach ( QObject *o, lst ) {
        Resource *r = qobject_cast<Resource*>( o );
        if ( r ) {
            if ( rc == 0 )  rc = new MacroCommand( KUndo2MagicString() );
            rc->addCommand( new RemoveResourceCmd( r->parentGroup(), r ) );
            continue;
        }
        ResourceGroup *g = qobject_cast<ResourceGroup*>( o );
        if ( g ) {
            if ( gc == 0 )  gc = new MacroCommand( KUndo2MagicString() );
            gc->addCommand( new RemoveResourceGroupCmd( g->project(), g ) );
        }
    }
    if ( rc || gc ) {
        KUndo2MagicString s;
        if ( rc && gc ) {
            s = kundo2_i18n( "Delete resourcegroups and resources" );
        } else if ( rc ) {
            s = kundo2_i18np( "Delete resource", "Delete resources", lst.count() );
        } else {
            s = kundo2_i18np( "Delete resourcegroup", "Delete resourcegroups", lst.count() );
        }
        cmd = new MacroCommand( s );
    }
    if ( rc )
        cmd->addCommand( rc );
    if ( gc )
        cmd->addCommand( gc );
    if ( cmd )
        getPart()->addCommand( cmd );
}


void View::updateReadWrite( bool readwrite )
{
    m_readWrite = readwrite;
    m_viewlist->setReadWrite( readwrite );
}

MainDocument *View::getPart() const
{
    return ( MainDocument * ) koDocument();
}

KoPart *View::getKoPart() const
{
    return m_partpart;
}

void View::slotConnectNode()
{
    //debugPlan;
    /*    NodeItem *curr = ganttview->currentItem();
        if (curr) {
            debugPlan<<"node="<<curr->getNode().name();
        }*/
}

QMenu * View::popupMenu( const QString& name )
{
    //debugPlan;
    if ( factory() ) {
        return ( ( QMenu* ) factory() ->container( name, this ) );
    }
    debugPlan<<"No factory";
    return 0L;
}

void View::slotUpdate()
{
    //debugPlan<<"calculate="<<calculate;

//    m_updateResourceview = true;
    m_updateResourceAssignmentView = true;
    m_updatePertEditor = true;
    updateView( m_tab->currentWidget() );
}

void View::slotGuiActivated( ViewBase *view, bool activate )
{
    //FIXME: Avoid unplug if possible, it flashes the gui
    // always unplug, in case they already are plugged
    foreach( const QString &name, view->actionListNames() ) {
        unplugActionList( name );
    }
    if ( activate ) {
        foreach( const QString &name, view->actionListNames() ) {
            plugActionList( name, view->actionList( name ) );
        }
        foreach ( DockWidget *ds, view->dockers() ) {
            m_dockers.append( ds );
            ds->activate( mainWindow() );
        }
        if (!m_dockers.isEmpty()) {debugPlan<<"Added dockers:"<<view<<m_dockers;}
    } else {
        if (!m_dockers.isEmpty()) {debugPlan<<"Remove dockers:"<<view<<m_dockers;}
        while ( ! m_dockers.isEmpty() ) {
            m_dockers.takeLast()->deactivate( mainWindow() );
        }
    }
}

void View::guiActivateEvent( bool activated )
{
    if ( activated ) {
        // plug my own actionlists, they may be gone
        slotPlugScheduleActions();
    }
    // propagate to sub-view
    ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
    if ( v ) {
        v->setGuiActive( activated );
    }
}

void View::slotViewListItemRemoved( ViewListItem *item )
{
    getPart()->removeViewListItem( this, item );
}

void View::removeViewListItem( const ViewListItem *item )
{
    if ( item == 0 ) {
        return;
    }
    ViewListItem *itm = m_viewlist->findItem( item->tag() );
    if ( itm == 0 ) {
        return;
    }
    m_viewlist->removeViewListItem( itm );
    return;
}

void View::slotViewListItemInserted( ViewListItem *item, ViewListItem *parent, int index )
{
    getPart()->insertViewListItem( this, item, parent, index );
}

void View::addViewListItem( const ViewListItem *item, const ViewListItem *parent, int index )
{
    if ( item == 0 ) {
        return;
    }
    if ( parent == 0 ) {
        if ( item->type() != ViewListItem::ItemType_Category ) {
            return;
        }
        m_viewlist->blockSignals( true );
        ViewListItem *cat = m_viewlist->addCategory( item->tag(), item->text( 0 ) );
        cat->setToolTip( 0, item->toolTip( 0 ) );
        m_viewlist->blockSignals( false );
        return;
    }
    ViewListItem *cat = m_viewlist->findCategory( parent->tag() );
    if ( cat == 0 ) {
        return;
    }
    m_viewlist->blockSignals( true );
    createView( cat, item->viewType(), item->tag(), item->text( 0 ), item->toolTip( 0 ), index );
    m_viewlist->blockSignals( false );
}

void View::createReportView(const QDomDocument &doc)
{
#ifdef PLAN_USE_KREPORT
    QPointer<ViewListReportsDialog> vd = new ViewListReportsDialog( this, *m_viewlist, doc, this );
    vd->exec(); // FIXME  make non-crash
    delete vd;
#endif
}

void View::slotOpenReportFile()
{
#ifdef PLAN_USE_KREPORT
    QFileDialog *dlg = new QFileDialog(this);
    connect(dlg, SIGNAL(finished(int)), SLOT(slotOpenReportFileFinished(int)));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
#endif
}

void View::slotOpenReportFileFinished( int result )
{
#ifdef PLAN_USE_KREPORT
    QFileDialog *fdlg = qobject_cast<QFileDialog*>( sender() );
    if ( fdlg == 0 || result != QDialog::Accepted ) {
        return;
    }
    QString fn = fdlg->selectedFiles().value(0);
    if ( fn.isEmpty() ) {
        return;
    }
    QFile file( fn );
    if ( ! file.open( QIODevice::ReadOnly | QIODevice::Text ) ) {
        KMessageBox::sorry( this, xi18nc( "@info", "Cannot open file:<br/><filename>%1</filename>", fn ) );
        return;
    }
    QDomDocument doc;
    doc.setContent( &file );
    createReportView(doc);
#endif
}

void View::slotReportDesignFinished( int /*result */)
{
#ifdef PLAN_USE_KREPORT
    if ( sender() ) {
        sender()->deleteLater();
    }
#endif
}

void View::slotCreateView()
{
    ViewListDialog *dlg = new ViewListDialog( this, *m_viewlist, this );
    connect(dlg, SIGNAL(finished(int)), SLOT(slotCreateViewFinished(int)));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void View::slotCreateViewFinished( int )
{
    if ( sender() ) {
        sender()->deleteLater();
    }
}

void View::slotViewActivated( ViewListItem *item, ViewListItem *prev )
{
    QApplication::setOverrideCursor( Qt::WaitCursor );
    if ( prev && prev->type() == ViewListItem::ItemType_Category && m_viewlist->previousViewItem() ) {
        // A view is shown anyway...
        ViewBase *v = qobject_cast<ViewBase*>( m_viewlist->previousViewItem()->view() );
        if ( v ) {
            v->setGuiActive( false );
        }
    } else if ( prev && prev->type() == ViewListItem::ItemType_SubView ) {
        ViewBase *v = qobject_cast<ViewBase*>( prev->view() );
        if ( v ) {
            v->setGuiActive( false );
        }
    }
    if ( item && item->type() == ViewListItem::ItemType_SubView ) {
        //debugPlan<<"Activate:"<<item;
        m_tab->setCurrentWidget( item->view() );
        // Add sub-view specific gui
        ViewBase *v = dynamic_cast<ViewBase*>( m_tab->currentWidget() );
        if ( v ) {
            v->setGuiActive( true );
        }
    }
    QApplication::restoreOverrideCursor();
}

QWidget *View::canvas() const
{
    return m_tab->currentWidget();//KoView::canvas();
}

KoPageLayout View::pageLayout() const
{
    return currentView()->pageLayout();
}

QPrintDialog *View::createPrintDialog( KoPrintJob *printJob, QWidget *parent )
{
    debugPlan<<printJob;
    KoPrintingDialog *job = dynamic_cast<KoPrintingDialog*>( printJob );
    if ( ! job ) {
        return 0;
    }
    QPrintDialog *dia = KoView::createPrintDialog( job, parent );

    PrintingDialog *j = dynamic_cast<PrintingDialog*>( job );
    if ( j ) {
        new PrintingControlPrivate( j, dia );
    }
    return dia;
}

void View::slotCurrentChanged( int view )
{
    m_visitedViews << view;
    ViewListItem *item = m_viewlist->findItem( qobject_cast<ViewBase*>( m_tab->currentWidget() ) );
    m_viewlist->setCurrentItem( item );
}

void View::slotSelectDefaultView()
{
    m_tab->setCurrentIndex(qMin(m_defaultView, m_tab->count()-1));
}

void View::updateView( QWidget * )
{
    QApplication::setOverrideCursor( Qt::WaitCursor );
    //setScheduleActionsEnabled();

    QWidget *widget2;

    widget2 = m_viewlist->findView( "ResourceAssignmentView" );
    if ( widget2 && m_updateResourceAssignmentView )
        static_cast<ViewBase*>( widget2 ) ->draw( getProject() );
    m_updateResourceAssignmentView = false;

    QApplication::restoreOverrideCursor();
}

void View::slotRenameNode( Node *node, const QString& name )
{
    //debugPlan<<name;
    if ( node ) {
        KUndo2MagicString s = kundo2_i18n( "Modify name" );
        switch( node->type() ) {
            case Node::Type_Task: s = kundo2_i18n( "Modify task name" ); break;
            case Node::Type_Milestone: s = kundo2_i18n( "Modify milestone name" ); break;
            case Node::Type_Summarytask: s = kundo2_i18n( "Modify summarytask name" ); break;
            case Node::Type_Project: s = kundo2_i18n( "Modify project name" ); break;
        }
        NodeModifyNameCmd * cmd = new NodeModifyNameCmd( *node, name, s );
        getPart() ->addCommand( cmd );
    }
}

void View::slotPopupMenu( const QString& menuname, const QPoint & pos )
{
    QMenu * menu = this->popupMenu( menuname );
    if ( menu ) {
        //debugPlan<<menu<<":"<<menu->actions().count();
        ViewBase *v = qobject_cast<ViewBase*>( m_tab->currentWidget() );
        //debugPlan<<v<<menuname;
        QList<QAction*> lst;
        if ( v ) {
            lst = v->contextActionList();
            debugPlan<<lst;
            if ( ! lst.isEmpty() ) {
                menu->addSeparator();
                foreach ( QAction *a, lst ) {
                    menu->addAction( a );
                }
            }
        }
        menu->exec( pos );
        foreach ( QAction *a, lst ) {
            menu->removeAction( a );
        }
    }
}

void View::slotPopupMenu( const QString& menuname, const QPoint &pos, ViewListItem *item )
{
    //debugPlan<<menuname;
    m_viewlistItem = item;
    slotPopupMenu( menuname, pos );
}

bool View::loadContext()
{
    Context *ctx = getPart()->context();
    if ( ctx == 0 || ! ctx->isLoaded() ) {
        return false;
    }
    KoXmlElement n = ctx->context();
    QString cv = n.attribute( "current-view" );
    if ( ! cv.isEmpty() ) {
        m_viewlist->setSelected( m_viewlist->findItem( cv ) );
    } else debugPlan<<"No current view";

    long id = n.attribute( "current-schedule", "-1" ).toLong();
    if ( id != -1 ) {
        setActiveSchedule( id );
    } else debugPlan<<"No current schedule";

    return true;
}

void View::saveContext( QDomElement &me ) const
{
    //debugPlan;
    long id = activeScheduleId();
    if ( id != -1 ) {
        me.setAttribute( "current-schedule", QString::number((qlonglong)id) );
    }
    ViewListItem *item = m_viewlist->findItem( qobject_cast<ViewBase*>( m_tab->currentWidget() ) );
    if ( item ) {
        me.setAttribute("current-view", item->tag() );
    }
    m_viewlist->save( me );
}

bool View::loadWorkPackage( Project &project, const QUrl &url )
{
    return getPart()->loadWorkPackage( project, url );
}

void View::setLabel( ScheduleManager *sm )
{
    //debugPlan;
    Schedule *s = sm == 0 ? 0 : sm->expected();
    if ( s && !s->isDeleted() && s->isScheduled() ) {
        m_estlabel->setText( sm->name() );
        return;
    }
    m_estlabel->setText( xi18nc( "@info:status", "Not scheduled" ) );
}

void View::slotWorkPackageLoaded()
{
    debugPlan<<getPart()->workPackages();
}

void View::slotMailWorkpackage( Node *node, Resource *resource )
{
    debugPlan;
    QTemporaryFile tmpfile(QDir::tempPath() + QLatin1String("/calligraplanwork_XXXXXX") + QLatin1String( ".planwork" ));
    tmpfile.setAutoRemove( false );
    if ( ! tmpfile.open() ) {
        debugPlan<<"Failed to open file";
        KMessageBox::error(0, i18n("Failed to open temporary file" ) );
        return;
    }
    QUrl url = QUrl::fromLocalFile( tmpfile.fileName() );
    if ( ! getPart()->saveWorkPackageUrl( url, node, activeScheduleId(), resource ) ) {
        debugPlan<<"Failed to save to file";
        KMessageBox::error(0, xi18nc( "@info", "Failed to save to temporary file:<br/> <filename>%1</filename>", url.url() ) );
        return;
    }
    QStringList attachURLs;
    attachURLs << url.url();
    QString to = resource == 0 ? node->leader() : ( resource->name() + " <" + resource->email() + '>' );
    QString cc;
    QString bcc;
    QString subject = i18n( "Work Package: %1", node->name() );
    QString body = i18nc( "1=project name, 2=task name", "%1\n%2", getProject().name(), node->name() );
    QString messageFile;

    KToolInvocation::invokeMailer( to, cc, bcc, subject, body, messageFile, attachURLs );
}

void View::slotMailWorkpackages( const QList<Node*> &nodes, Resource *resource )
{
    debugPlan;
    if ( resource == 0 ) {
        warnPlan<<"No resource, we don't handle node->leader() yet";
        return;
    }
    QString to = resource->name() + " <" + resource->email() + '>';
    QString subject = i18n( "Work Package for project: %1", getProject().name() );
    QString body;
    QStringList attachURLs;

    foreach ( Node *n, nodes ) {
        QTemporaryFile tmpfile(QDir::tempPath() + QLatin1String("/calligraplanwork_XXXXXX") + QLatin1String( ".planwork" ));
        tmpfile.setAutoRemove( false );
        if ( ! tmpfile.open() ) {
            debugPlan<<"Failed to open file";
            KMessageBox::error(0, i18n("Failed to open temporary file" ) );
            return;
        }
        QUrl url = QUrl::fromLocalFile( tmpfile.fileName() );
        if ( ! getPart()->saveWorkPackageUrl( url, n, activeScheduleId(), resource ) ) {
            debugPlan<<"Failed to save to file";
            KMessageBox::error(0, xi18nc( "@info", "Failed to save to temporary file:<br/><filename>%1</filename>", url.url() ) );
            return;
        }
        attachURLs << url.url();
        body += n->name() + '\n';
    }

    QString cc;
    QString bcc;
    QString messageFile;

    KToolInvocation::invokeMailer( to, cc, bcc, subject, body, messageFile, attachURLs );
}

void View::slotCurrencyConfig()
{
    LocaleConfigMoneyDialog *dlg = new LocaleConfigMoneyDialog( getProject().locale(), this );
    connect(dlg, SIGNAL(finished(int)), SLOT(slotCurrencyConfigFinished(int)));
    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}

void View::slotCurrencyConfigFinished( int result )
{
    LocaleConfigMoneyDialog *dlg = qobject_cast<LocaleConfigMoneyDialog*>( sender() );
    if ( dlg == 0 ) {
        return;
    }
    if ( result == QDialog::Accepted ) {
        KUndo2Command *c = dlg->buildCommand( getProject() );
        if ( c ) {
            getPart()->addCommand( c );
        }
    }
    dlg->deleteLater();
}

void View::saveTaskModule( const QUrl &url, Project *project )
{
    // NOTE: workaround: KoResourcePaths::saveLocation( "calligraplan_taskmodules" ); does not work
    const QString dir = KoResourcePaths::saveLocation( "appdata", "taskmodules/" );
    debugPlan<<"dir="<<dir;
    if ( ! dir.isEmpty() ) {
        Part *part = new Part( this );
        MainDocument *doc = new MainDocument( part );
        part->setDocument( doc );
        doc->disconnect(); // doc shall not handle feedback from openUrl()
        doc->setAutoSave( 0 ); //disable
        doc->insertProject( *project, 0, 0 ); // FIXME: destroys project, find better way
        doc->getProject().setName( project->name() );
        doc->getProject().setLeader( project->leader() );
        doc->getProject().setDescription( project->description() );
        doc->saveNativeFormat( dir + url.fileName() );
        part->deleteLater(); // also deletes document
        debugPlan<<dir + url.fileName();
    } else {
        debugPlan<<"Could not find a location";
    }
}

void View::removeTaskModule( const QUrl &url )
{
    debugPlan<<url;
}

QString View::standardTaskStatusReport() const
{
    QString s;
#ifdef PLAN_USE_KREPORT
    s = QString::fromLatin1(
        "<planreportdefinition version=\"1.0\" mime=\"application/x-vnd.kde.plan.report.definition\" editor=\"Plan\" >"
        "<data-source select-from=\"taskstatus\" ></data-source>"
        "<report:content xmlns:report=\"http://kexi-project.org/report/2.0\" xmlns:fo=\"urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0\" xmlns:svg=\"urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0\" >"
        "<report:title>%1</report:title>"
        "<report:script report:script-interpreter=\"javascript\" ></report:script>"
        "<report:grid report:grid-divisions=\"4\" report:grid-snap=\"1\" report:page-unit=\"cm\" report:grid-visible=\"1\" />"
        "<report:page-style report:print-orientation=\"portrait\" fo:margin-bottom=\"1cm\" fo:margin-top=\"1cm\" fo:margin-left=\"1cm\" fo:margin-right=\"1cm\" report:page-size=\"A4\" >predefined</report:page-style>"
        "<report:body>"
        "<report:section svg:height=\"1.75cm\" fo:background-color=\"#ffffff\" report:section-type=\"header-page-any\" >"
        "<report:field report:name=\"field16\" report:horizontal-align=\"left\" report:item-data-source=\"#project.manager\" svg:x=\"13cm\" svg:width=\"5.9714cm\" svg:y=\"0.4cm\" report:vertical-align=\"bottom\" svg:height=\"0.6cm\" report:z-index=\"0\" >"
        "<report:text-style fo:font-weight=\"bold\" fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"10\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "<report:label report:name=\"label16\" report:horizontal-align=\"left\" svg:x=\"13cm\" svg:width=\"5.9714cm\" svg:y=\"0cm\" report:caption=\"%2\" report:vertical-align=\"center\" svg:height=\"0.4cm\" report:z-index=\"1\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:font-style=\"italic\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "<report:field report:name=\"field17\" report:horizontal-align=\"left\" report:item-data-source=\"#project.name\" svg:x=\"0cm\" svg:width=\"13cm\" svg:y=\"0.4cm\" report:vertical-align=\"bottom\" svg:height=\"0.6cm\" report:z-index=\"1\" >"
        "<report:text-style fo:font-weight=\"bold\" fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"10\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"0%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "<report:label report:name=\"label18\" report:horizontal-align=\"left\" svg:x=\"0cm\" svg:width=\"13cm\" svg:y=\"0cm\" report:caption=\"%3\" report:vertical-align=\"center\" svg:height=\"0.4cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:font-style=\"italic\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "<report:line report:name=\"line15\" svg:y1=\"1.2229cm\" svg:x1=\"0cm\" svg:y2=\"1.2229cm\" svg:x2=\"18.9715cm\" report:z-index=\"0\" >"
        "<report:line-style report:line-style=\"solid\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:line>"
        "</report:section>"
        "<report:section svg:height=\"1.50cm\" fo:background-color=\"#ffffff\" report:section-type=\"header-report\" >"
        "<report:label report:name=\"label17\" report:horizontal-align=\"left\" svg:x=\"0cm\" svg:width=\"18.97cm\" svg:y=\"0cm\" report:caption=\"%4\" report:vertical-align=\"center\" svg:height=\"1.25cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"10\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "</report:section>"
        "<report:section svg:height=\"2.50cm\" fo:background-color=\"#ffffff\" report:section-type=\"footer-page-any\" >"
        "<report:field report:name=\"field10\" report:horizontal-align=\"right\" report:item-data-source=\"=constants.PageNumber()\" svg:x=\"6.75cm\" svg:width=\"0.75cm\" svg:y=\"0.25cm\" report:vertical-align=\"center\" svg:height=\"0.75cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "<report:field report:name=\"field11\" report:horizontal-align=\"left\" report:item-data-source=\"=constants.PageTotal()\" svg:x=\"8.25cm\" svg:width=\"3cm\" svg:y=\"0.25cm\" report:vertical-align=\"center\" svg:height=\"0.75cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "<report:label report:name=\"label12\" report:horizontal-align=\"center\" svg:x=\"7.5cm\" svg:width=\"0.75cm\" svg:y=\"0.25cm\" report:caption=\"%5\" report:vertical-align=\"center\" svg:height=\"0.75cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:font-style=\"italic\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "<report:label report:name=\"label13\" report:horizontal-align=\"right\" svg:x=\"5.75cm\" svg:width=\"1cm\" svg:y=\"0.25cm\" report:caption=\"%6\" report:vertical-align=\"center\" svg:height=\"0.75cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:font-style=\"italic\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "<report:line report:name=\"line14\" svg:y1=\"0.2195cm\" svg:x1=\"0cm\" svg:y2=\"0.2195cm\" svg:x2=\"18.9715cm\" report:z-index=\"0\" >"
        "<report:line-style report:line-style=\"solid\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:line>"
        "</report:section>"
        "<report:detail>"
        "<report:group report:group-sort=\"ascending\" report:group-column=\"Parent\" >"
        "<report:section svg:height=\"2.50cm\" fo:background-color=\"#ffffff\" report:section-type=\"group-header\" >"
        "<report:label report:name=\"label6\" report:horizontal-align=\"left\" svg:x=\"0.5cm\" svg:width=\"3.75cm\" svg:y=\"1.75cm\" report:caption=\"%7\" report:vertical-align=\"center\" svg:height=\"0.75cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "<report:field report:name=\"field8\" report:horizontal-align=\"left\" report:item-data-source=\"Parent\" svg:x=\"0.5cm\" svg:width=\"8cm\" svg:y=\"1cm\" report:vertical-align=\"center\" svg:height=\"0.689cm\" report:z-index=\"0\" >"
        "<report:text-style fo:font-weight=\"bold\" fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "<report:label report:name=\"label8\" report:horizontal-align=\"center\" svg:x=\"4.25cm\" svg:width=\"4.25cm\" svg:y=\"1.75cm\" report:caption=\"%8\" report:vertical-align=\"center\" svg:height=\"0.75cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:label>"
        "</report:section>"
        "</report:group>"
        "<report:section svg:height=\"0.50cm\" fo:background-color=\"#ffffff\" report:section-type=\"detail\" >"
        "<report:field report:name=\"field7\" report:horizontal-align=\"left\" report:item-data-source=\"NodeName\" svg:x=\"0.5cm\" svg:width=\"3.75cm\" svg:y=\"0cm\" report:vertical-align=\"center\" svg:height=\"0.5cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "<report:field report:name=\"field9\" report:horizontal-align=\"center\" report:item-data-source=\"NodeCompleted\" svg:x=\"4.25cm\" svg:width=\"4.25cm\" svg:y=\"0cm\" report:vertical-align=\"center\" svg:height=\"0.5cm\" report:z-index=\"0\" >"
        "<report:text-style fo:letter-spacing=\"0%\" style:letter-kerning=\"true\" fo:font-size=\"8\" fo:foreground-color=\"#000000\" fo:font-family=\"DejaVu Sans\" fo:background-color=\"#ffffff\" fo:background-opacity=\"100%\" />"
        "<report:line-style report:line-style=\"nopen\" report:line-weight=\"1\" report:line-color=\"#000000\" />"
        "</report:field>"
        "</report:section>"
        "</report:detail>"
        "</report:body>"
        "</report:content>"
        "</planreportdefinition>")
        .arg(
            i18n( "Report" ),
            i18nc( "Project manager", "Manager:" ),
            i18n( "Project:" ),
            i18n( "Task Status Report" ),
            i18nc( "As in: Page 1 of 2", "of" ),
            i18n( "Page" ),
            i18nc( "Task name", "Name" ),
            i18nc( "Task completion", "Completion (%)" )
        );
#endif
    return s;
}

}  //KPlato namespace
