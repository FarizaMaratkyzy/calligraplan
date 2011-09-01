/* This file is part of the KDE project
 * Copyright (C) 2009, 2010, 2011 Dag Andersen <danders@get2net.dk>
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

#include "PlanTJScheduler.h"

#include "kptproject.h"
#include "kptschedule.h"
#include "kptresource.h"
#include "kpttask.h"
#include "kptrelation.h"
#include "kptduration.h"
#include "kptcalendar.h"

#include "taskjuggler/Project.h"
#include "taskjuggler/Resource.h"
#include "taskjuggler/Task.h"
#include "taskjuggler/Interval.h"
#include "taskjuggler/Allocation.h"
#include "taskjuggler/Utility.h"
#include "taskjuggler/TjMessageHandler.h"

#include <QString>
#include <QTimer>
#include <QMutexLocker>

#include <KGlobal>
#include <KLocale>
#include <KDebug>

#include <iostream>

#define GENERATION_MIN_LIMIT 5000

#define PROGRESS_CALLBACK_FREQUENCY 100
#define PROGRESS_MAX_VALUE 120000
#define PROGRESS_INIT_VALUE 12000
#define PROGRESS_INIT_STEP 2000

/* low weight == late, high weight == early */
#define WEIGHT_ASAP         50
#define WEIGHT_ALAP         1
#define WEIGHT_CONSTRAINT   1000
#define WEIGHT_FINISH       1000

#define GROUP_TARGETTIME    1
#define GROUP_CONSTRAINT    2


PlanTJScheduler::PlanTJScheduler( Project *project, ScheduleManager *sm, QObject *parent )
    : SchedulerThread( project, sm, parent ),
    result( -1 ),
    m_schedule( 0 ),
    m_recalculate( false ),
    m_usePert( false ),
    m_backward( false )/*,
    m_backwardTask( 0 )*/
{
    TJ::TJMH.reset();
    connect(this, SIGNAL(sigCalculationStarted( Project*, ScheduleManager*)), project, SIGNAL(sigCalculationStarted( Project*, ScheduleManager*)));
    emit sigCalculationStarted( project, sm );

    connect( this, SIGNAL( sigCalculationFinished( Project*, ScheduleManager* ) ), project, SIGNAL( sigCalculationFinished( Project*, ScheduleManager* ) ) );
}

PlanTJScheduler::~PlanTJScheduler()
{
    delete m_tjProject;
}

KLocale *PlanTJScheduler::locale() const
{
    return KGlobal::locale();
}

void PlanTJScheduler::run()
{
    if ( m_haltScheduling ) {
        deleteLater();
        return;
    }
    if ( m_stopScheduling ) {
        return;
    }
    { // mutex -->
        m_projectMutex.lock();
        m_managerMutex.lock();

        m_project = new Project();
        loadProject( m_project, m_pdoc );
        m_project->setName( "Schedule: " + m_project->name() ); //Debug
        m_project->stopcalculation = false;
        m_manager = m_project->scheduleManager( m_mainmanagerId );
        Q_CHECK_PTR( m_manager );
        Q_ASSERT( m_manager->expected() );
        Q_ASSERT( m_manager != m_mainmanager );
        Q_ASSERT( m_manager->scheduleId() == m_mainmanager->scheduleId() );
        Q_ASSERT( m_manager->expected() != m_mainmanager->expected() );
        m_manager->setName( "Schedule: " + m_manager->name() ); //Debug
        m_schedule = m_manager->expected();

        bool x = connect(m_manager, SIGNAL(sigLogAdded(Schedule::Log)), this, SLOT(slotAddLog(Schedule::Log)));
        Q_ASSERT( x );

        m_project->initiateCalculation( *m_schedule );
        m_project->initiateCalculationLists( *m_schedule );

        m_usePert = m_manager->usePert();
        m_recalculate = m_manager->recalculate();
        if ( m_recalculate ) {
            m_backward = false;
        } else {
            m_backward = m_manager->schedulingDirection();
        }
        m_project->setCurrentSchedule( m_manager->expected()->id() );

        m_schedule->setPhaseName( 0, i18n( "Init" ) );
        if ( m_backward ) {
            m_schedule->logWarning( i18n( "Scheduling backeards from project end time is not supported" ), 0 );
            m_backward = false;
        }
        if ( ! m_backward && locale() ) {
            m_schedule->logDebug( QString( "Schedule project using TJ Scheduler, starting at %1" ).arg( QDateTime::currentDateTime().toString() ), 0 );
            if ( m_recalculate ) {
                m_schedule->logInfo( i18n( "Re-calculate project from start time: %1", locale()->formatDateTime( m_project->constraintStartTime() ) ), 0 );
            } else {
                m_schedule->logInfo( i18n( "Schedule project from start time: %1", locale()->formatDateTime( m_project->constraintStartTime() ) ), 0 );
            }
            m_schedule->logInfo( i18n( "Project target finish time: %1", locale()->formatDateTime( m_project->constraintEndTime() ) ), 0 );
        }
        if ( m_backward && locale() ) {
            m_schedule->logDebug( QString( "Schedule project backward using TJ Scheduler, starting at %1" ).arg( locale()->formatDateTime( QDateTime::currentDateTime() ) ), 0 );
            m_schedule->logInfo( i18n( "Schedule project from end time: %1", locale()->formatDateTime( m_project->constraintEndTime() ) ), 0 );
        }

        m_managerMutex.unlock();
        m_projectMutex.unlock();
    } // <--- mutex
 
    if ( ! kplatoToTJ() ) {
        result = 1;
        if ( locale() ) {
            m_schedule->logError( i18n( "Failed to build a valid TJ project" ) );
            for ( int i = 0; i < TJ::TJMH.getMessageCount(); ++i ) {
                if ( TJ::TJMH.isError( i ) ) {
                    m_schedule->logError( TJ::TJMH.getMessage( i ) );
                } else if ( TJ::TJMH.isWarning( i ) ) {
                    m_schedule->logWarning( TJ::TJMH.getMessage( i ) );
                } else if ( TJ::TJMH.isInfo( i ) ) {
                    m_schedule->logInfo( TJ::TJMH.getMessage( i ) );
                } else if ( TJ::TJMH.isDebug( i ) ) {
                    m_schedule->logDebug( TJ::TJMH.getMessage( i ) );
                }
            }
        }
        setProgress( PROGRESS_MAX_VALUE );
        return;
    }
    setMaxProgress( PROGRESS_MAX_VALUE );
    
    m_schedule->setPhaseName( 1, i18n( "Schedule" ) );
    m_schedule->logInfo( "Start scheduling", 1 );
    bool r = solve();
    for ( int i = 0; i < TJ::TJMH.getMessageCount(); ++i ) {
        if ( TJ::TJMH.isError( i ) ) {
            m_schedule->logError( TJ::TJMH.getMessage( i ) );
        } else if ( TJ::TJMH.isWarning( i ) ) {
            m_schedule->logWarning( TJ::TJMH.getMessage( i ) );
        } else if ( TJ::TJMH.isInfo( i ) ) {
            m_schedule->logInfo( TJ::TJMH.getMessage( i ) );
        } else if ( TJ::TJMH.isDebug( i ) ) {
            m_schedule->logDebug( TJ::TJMH.getMessage( i ) );
        }
    }
    if ( ! r ) {
        qDebug()<<"Scheduling failed";
        result = 2;
        m_schedule->logError( i18n( "Failed to schedule project" ) );
        setProgress( PROGRESS_MAX_VALUE );
        return;
    }
    if ( m_haltScheduling ) {
        qDebug()<<"Scheduling halted";
        m_schedule->logInfo( "Scheduling halted" );
        deleteLater();
        return;
    }
    m_schedule->setPhaseName( 2, i18n( "Update" ) );
    m_schedule->logInfo( "Scheduling finished, update project", 2 );
    if ( ! kplatoFromTJ() ) {
        m_schedule->logError( "Project update failed" );
    }
    setProgress( PROGRESS_MAX_VALUE );
    m_schedule->setPhaseName( 3, i18n( "Finish" ) );
}

bool PlanTJScheduler::check()
{
    return m_tjProject->pass2( true );
}

bool PlanTJScheduler::solve()
{
    qDebug()<<"PlanTJScheduler::solve()";
    TJ::Scenario *sc = m_tjProject->getScenario( 0 );
    if ( ! sc ) {
        if ( locale() ) {
            m_schedule->logError( i18n( "Failed to find scenario to schedule" ) );
        }
        return false;
    }
    return m_tjProject->scheduleScenario( sc );
}

bool PlanTJScheduler::kplatoToTJ()
{
    m_tjProject = new TJ::Project();
    m_tjProject->setNow( m_project->constraintStartTime().toTime_t() );
    m_tjProject->setStart( m_project->constraintStartTime().toTime_t() );
    m_tjProject->setEnd( m_project->constraintEndTime().toTime_t() );
    m_tjProject->setScheduleGranularity( 300 ); //5 minutes

//     if ( m_backward ) {
//         // Add an ALAP milestone after start to push everything back
//         m_backwardTask = m_project->createTask();
//         m_backwardTask->estimate()->setExpectedEstimate( 0 );
//         m_backwardTask->setConstraint( Node::ALAP );
//         m_project->addTask( m_backwardTask, 0 );
//     }

    addResources();
    addTasks();
    setConstraints();
    addDependencies();
    addRequests();
    addStartEndJob();

    return check();
}

void PlanTJScheduler::addStartEndJob()
{
    TJ::Task *start = new TJ::Task( m_tjProject, "TJ::StartJob", "TJ::StartJob", 0, QString(), 0);
    start->setMilestone( true );
    start->setSpecifiedStart( 0, m_tjProject->getStart() );
    start->setPriority( 999 );
    TJ::Task *end = new TJ::Task( m_tjProject, "TJ::EndJob", "TJ::EndJob", 0, QString(), 0);
    end->setMilestone( true );
    end->setSpecifiedEnd( 0, m_tjProject->getEnd() - 1 );
    end->setScheduling( TJ::Task::ALAP );

    for ( QMap<TJ::Task*, Task*>::ConstIterator it = m_taskmap.constBegin(); it != m_taskmap.constEnd(); ++it ) {
        if ( it.value()->isStartNode() ) {
            it.key()->addDepends( start->getId() );
            m_schedule->logDebug( QString( "'%1' depends on: '%2'" ).arg( it.key()->getName() ).arg( start->getName() ) );
            if ( start->getScheduling() == TJ::Task::ALAP ) {
                start->addPrecedes( it.key()->getId() );
                m_schedule->logDebug( QString( "'%1' precedes: '%2'" ).arg( start->getName() ).arg( it.key()->getName() ) );
            }
        }
        if ( it.value()->isEndNode() ) {
            end->addDepends( it.key()->getId() );
            if ( it.key()->getScheduling() == TJ::Task::ALAP ) {
                it.key()->addPrecedes( end->getId() );
            }
        }
    }
}

// static
DateTime PlanTJScheduler::fromTime_t( time_t t ) {
    return DateTime ( QDateTime::fromTime_t( t ) );
}

// static
AppointmentInterval PlanTJScheduler::fromTJInterval( const TJ::Interval &tji ) {
    AppointmentInterval a( fromTime_t( tji.getStart() ), fromTime_t( tji.getEnd() ).addSecs( 1 ) );
    return a;
}

// static
TJ::Interval PlanTJScheduler::toTJInterval( const QDateTime &start, const QDateTime &end ) {
    TJ::Interval ti( start.toTime_t(), end.addSecs( - 1).toTime_t() );
    return ti;
}

// static
TJ::Interval PlanTJScheduler::toTJInterval( const QTime &start, const QTime &end ) {
    time_t s = QTime( 0, 0, 0 ).secsTo( start );
    time_t e = ( end == QTime( 0, 0, 0 ) ) ? 86399 : QTime( 0, 0, 0 ).secsTo( end );
    TJ::Interval ti( s, e );
    return ti;
}

bool PlanTJScheduler::kplatoFromTJ()
{
    MainSchedule *cs = static_cast<MainSchedule*>( m_project->currentSchedule() );
    // FIXME calculate real start/end
    m_project->setStartTime( m_project->constraintStartTime() );
    m_project->setEndTime( m_project->constraintEndTime() );

/*    if ( m_backward ) {
        TJ::Task *key = m_taskmap.key( m_backwardTask );
        if ( key ) {
            m_taskmap.remove( key );
        }
        delete m_backwardTask;
    }*/
    for ( QMap<TJ::Task*, Task*>::ConstIterator it = m_taskmap.constBegin(); it != m_taskmap.constEnd(); ++it ) {
        if ( ! taskFromTJ( it.key(), it.value() ) ) {
            return false;
        }
    }
    adjustSummaryTasks( m_schedule->summaryTasks() );

    cs->logInfo( i18n( "Project scheduled to start at %1 and finish at %2", locale()->formatDateTime( fromTime_t( m_tjProject->getStart() ) ), locale()->formatDateTime( fromTime_t( m_tjProject->getEnd() ) ) ) );

    if ( m_manager ) {
        if ( locale() ) cs->logDebug( QString( "Project scheduling finished at %1" ).arg( QDateTime::currentDateTime().toString() ) );
        m_project->finishCalculation( *m_manager );
        m_manager->scheduleChanged( cs );
    }
    return true;
}

bool PlanTJScheduler::taskFromTJ( TJ::Task *job, Task *task )
{
    if ( m_haltScheduling || m_manager == 0 ) {
        return true;
    }
    Schedule *cs = task->currentSchedule();
    Q_ASSERT( cs );
    qDebug()<<"taskFromTJ:"<<task<<task->name()<<cs->id();
    task->setStartTime( DateTime( QDateTime::fromTime_t( job->getStart( 0 ) ) ) );
    task->setEndTime( DateTime( QDateTime::fromTime_t( job->getEnd( 0 ) ).addSecs( 1 ) ) );
    task->setDuration( task->endTime() - task->startTime() );

    if ( ! task->startTime().isValid() ) {
        cs->logError( i18n( "Task has not a valid start time" ) );
        return false;
    }
    if ( ! task->endTime().isValid() ) {
        cs->logError( i18n( "Task has not a valid end time" ) );
        return false;
    }
    if ( m_project->startTime() > task->startTime() ) {
        m_project->setStartTime( task->startTime() );
    }
    if ( task->endTime() > m_project->endTime() ) {
        m_project->setEndTime( task->endTime() );
    }
    if ( locale() ) cs->logDebug( "TJ project scheduled: " + TJ::time2ISO( job->getStart( 0 ) ) + " - " + TJ::time2ISO( job->getEnd( 0 ) ) );

    foreach ( TJ::CoreAttributes *a, job->getBookedResources( 0 ) ) {
        TJ::Resource *r = static_cast<TJ::Resource*>( a );
        Resource *res = m_resourcemap[ r ];
        QList<TJ::Interval> lst = r->getBookedIntervals( 0, job );
        foreach ( const TJ::Interval &tji, lst ) {
            AppointmentInterval ai = fromTJInterval( tji );
            res->addAppointment( cs, ai.startTime(), ai.endTime(), ai.load() );
            if ( locale() ) cs->logDebug( "'" + res->name() + "' added appointment: " +  ai.startTime().toString( Qt::ISODate ) + " - " + ai.endTime().toString( Qt::ISODate ) );
        }
    }
    cs->setScheduled( true );
    if ( locale() ) {
        cs->logInfo( i18n( "Scheduled task to start at %1 and finish at %2", locale()->formatDateTime( task->startTime() ), locale()->formatDateTime( task->endTime() ) ) );
    }
    return true;
}



void PlanTJScheduler::adjustSummaryTasks( const QList<Node*> &nodes )
{
    foreach ( Node *n, nodes ) {
        adjustSummaryTasks( n->childNodeIterator() );
        if ( n->parentNode()->type() == Node::Type_Summarytask ) {
            DateTime pt = n->parentNode()->startTime();
            DateTime nt = n->startTime();
            if ( ! pt.isValid() || pt > nt ) {
                n->parentNode()->setStartTime( nt );
            }
            pt = n->parentNode()->endTime();
            nt = n->endTime();
            if ( ! pt.isValid() || pt < nt ) {
                n->parentNode()->setEndTime( nt );
            }
        }
    }
}

bool PlanTJScheduler::exists( QList<CalendarDay*> &lst, CalendarDay *day )
{
    foreach ( CalendarDay *d, lst ) {
        if ( d->date() == day->date() && day->state() != CalendarDay::Undefined && d->state() != CalendarDay::Undefined ) {
            return true;
        }
    }
    return false;
}

TJ::Resource *PlanTJScheduler::addResource( KPlato::Resource *r)
{
    if ( m_resourcemap.values().contains( r ) ) {
        kWarning()<<r->name()<<"already exist";
        return 0;
    }

    TJ::Resource *res = new TJ::Resource( m_tjProject, r->id(), r->name(), 0 );
    res->setEfficiency( (double)(r->units()) / 100. );
    Calendar *cal = r->calendar();
    int days[ 7 ] = { Qt::Sunday, Qt::Monday, Qt::Tuesday, Qt::Wednesday, Qt::Thursday, Qt::Friday, Qt::Saturday };
    for ( int i = 0; i < 7; ++i ) {
        CalendarDay *d = 0;
        for ( Calendar *c = cal; c; c = c->parentCal() ) {
            d = c->weekday( days[ i ] );
            if ( d->state() != CalendarDay::Undefined ) {
                break;
            }
        }
        if ( d && d->state() == CalendarDay::Working ) {
            QList<TJ::Interval*> lst;
            foreach ( const TimeInterval *ti, d->timeIntervals() ) {
                time_t s = QTime( 0, 0, 0 ).secsTo( ti->startTime() );
                time_t e = s + ( ti->second / 1000 ) - 1; 
                TJ::Interval *tji = new TJ::Interval( s, e );
                lst << tji;
            }
            res->setWorkingHours( i, lst );
            qDeleteAll( lst );
        }
    }
    QList<CalendarDay*> lst;
    for ( Calendar *c = cal; c; c = c->parentCal() ) {
        foreach ( CalendarDay *day, c->days() ) {
            if ( ! exists( lst, day ) ) {
                lst << day;
            }
        }
    }
    foreach ( CalendarDay *day, lst ) {
        if ( day->state() == CalendarDay::NonWorking ) {
            res->addVacation( new TJ::Interval( toTJInterval( QDateTime( day->date() ), QDateTime( day->date().addDays( 1 ) ) ) ) );
        } else if ( day->state() == CalendarDay::Working ) {
            TJ::Shift *shift = new TJ::Shift( m_tjProject, r->id() + day->date().toString( Qt::ISODate ), r->name(), 0, QString(), 0 );
            foreach ( TimeInterval *ti, day->timeIntervals() ) {
                QList<TJ::Interval*> ivs;
                ivs << new TJ::Interval( toTJInterval( ti->startTime(), ti->endTime() ) );
                shift->setWorkingHours( day->date().dayOfWeek() - 1, ivs );
            }
            TJ::Interval period = toTJInterval( day->start(), day->end() );
            TJ::ShiftSelection *sl = new TJ::ShiftSelection( period, shift );
            res->addShift( sl );
        }
    }

    m_resourcemap[res] = r;
    if ( locale() ) { m_schedule->logDebug( "Added resource: " + r->name() ); }
    return res;
}

void PlanTJScheduler::addResources()
{
    kDebug();
    QList<Resource*> list = m_project->resourceList();
    for (int i = 0; i < list.count(); ++i) {
        addResource( list.at(i) );
    }
}

TJ::Task *PlanTJScheduler::addTask( KPlato::Task *task )
{
/*    if ( m_backward && task->isStartNode() ) {
        Relation *r = new Relation( m_backwardTask, task );
        m_project->addRelation( r );
    }*/
    TJ::Task *t = new TJ::Task(m_tjProject, task->id(), task->name(), 0, QString(), 0);
    m_taskmap[ t ] = task;
    if ( locale() ) { m_schedule->logDebug( "Added task: " + task->name() ); }
    return t;
}

void PlanTJScheduler::addTasks()
{
    kDebug();
    QList<Node*> list = m_project->allNodes();
    for (int i = 0; i < list.count(); ++i) {
        Node *n = list.at(i);
        switch ( n->type() ) {
            case Node::Type_Summarytask:
                m_schedule->insertSummaryTask( n );
                break;
            case Node::Type_Task:
            case Node::Type_Milestone:
                addTask( static_cast<Task*>( n ) );
                break;
            default:
                break;
        }
    }
}

void PlanTJScheduler::addDependencies( TJ::Task *job, KPlato::Task *task )
{
    Schedule * cs = task->currentSchedule();
    if ( job->getScheduling() == TJ::Task::ASAP ) {
        foreach ( Relation *r, task->dependParentNodes() + task->parentProxyRelations() ) {
            Node *n = r->parent();
            if ( n == 0 || n->type() == Node::Type_Summarytask ) {
                continue;
            }
            switch ( r->type() ) {
                case Relation::FinishStart:
                    break;
                case Relation::FinishFinish:
                case Relation::StartStart:
                    kWarning()<<"Dependency type not handled. Using FinishStart.";
                    if ( cs && locale() ) {
                        cs->logWarning( i18n( "%1: Dependency type not handled. Using FinishStart.", task->constraintToString( true ) ) );
                    }
                    break;
            }
            TJ::Task *parent = m_tjProject->getTask( n->id() );
            Q_ASSERT( parent );
            TJ::TaskDependency *d = job->addDepends( parent->getId() );
            d->setGapDuration( 0, r->lag().seconds() );
            if ( cs && locale() ) { cs->logDebug( QString( "'%1' depends on '%2', lag: %3h (%4s)" ).arg(task->name()).arg( n->name() ).arg( r->lag().toString( Duration::Format_HourFraction ) ).arg( d->getGapDuration( 0 ) ) ); }
            if ( parent->getScheduling() == TJ::Task::ALAP ) {
                // when predeccessor is ALAP, this must be ALAP too
                job->setScheduling( TJ::Task::ALAP );
                if ( cs ) { cs->logDebug( QString( "Parent '%1' is ALAP, set '%2' to ALAP" ).arg(parent->getName()).arg( job->getName() ) ); }
            }
        }
    }
    if ( job->getScheduling() == TJ::Task::ALAP ) {
        foreach ( Relation *r, task->dependChildNodes() + task->childProxyRelations() ) {
            Node *n = r->child();
            if ( n == 0 || n->type() == Node::Type_Summarytask ) {
                continue;
            }
            switch ( r->type() ) {
                case Relation::FinishStart:
                    break;
                case Relation::FinishFinish:
                case Relation::StartStart:
                    kWarning()<<"Dependency type not handled. Using FinishStart.";
                    if ( cs && locale() ) {
                        cs->logWarning( i18n( "%1: Dependency type not handled. Using FinishStart.", task->constraintToString( true ) ) );
                    }
                    break;
            }
            TJ::TaskDependency *d = job->addPrecedes( n->id() );
            d->setGapDuration( 0, r->lag().seconds() );
            if ( cs && locale() ) { cs->logDebug( QString( "'%1' precedes '%2', lag: %3h (%4s)" ).arg(task->name()).arg( n->name() ).arg( r->lag().toString( Duration::Format_HourFraction ) ).arg( d->getGapDuration( 0 ) ) ); }
        }
    }
}

void PlanTJScheduler::addDependencies()
{
    kDebug();
    QMap<TJ::Task*, Task*> ::const_iterator it = m_taskmap.constBegin();
    for ( ; it != m_taskmap.constEnd(); ++it ) {
        addDependencies( it.key(), it.value() );
    }
}

void PlanTJScheduler::setConstraints()
{
    QMap<TJ::Task*, Task*> ::const_iterator it = m_taskmap.constBegin();
    for ( ; it != m_taskmap.constEnd(); ++it ) {
        setConstraint( it.key(), it.value() );
    }
}

void PlanTJScheduler::setConstraint( TJ::Task *job, KPlato::Task *task )
{
    Schedule * cs = task->currentSchedule();
    switch ( task->constraint() ) {
        case Node::ASAP:
            job->setScheduling( TJ::Task::ASAP);
            break;
        case Node::ALAP:
            job->setScheduling( TJ::Task::ALAP);
            break;
        case Node::MustStartOn:
            job->setPriority( 600 );
            job->setSpecifiedStart( 0, task->constraintStartTime().toTime_t() );
            if ( cs ) cs->logDebug( QString( "MSO: set specified start: %1").arg( TJ::time2ISO( task->constraintStartTime().toTime_t() ) ) );
            break;
        case Node::StartNotEarlier: {
            job->setPriority( 500 );
            job->setSpecifiedStart( 0, task->constraintStartTime().toTime_t() );
            if ( cs ) cs->logDebug( QString( "SNE: set specified start: %1").arg( TJ::time2ISO( task->constraintStartTime().toTime_t() ) ) );
            break;
        }
        case Node::MustFinishOn:
            job->setPriority( 600 );
            job->setScheduling( TJ::Task::ALAP );
            job->setSpecifiedEnd( 0, task->constraintEndTime().toTime_t() - 1 );
            if ( cs ) cs->logDebug( QString( "MFO: set specified end: %1").arg( TJ::time2ISO( task->constraintEndTime().toTime_t() ) ) );
            break;
        case Node::FinishNotLater: {
            job->setPriority( 500 );
            job->setScheduling( TJ::Task::ALAP );
            job->setSpecifiedEnd( 0, task->constraintEndTime().toTime_t() - 1 );
            if ( cs ) cs->logDebug( QString( "FNL: set specified end: %1").arg( TJ::time2ISO( task->constraintEndTime().toTime_t() ) ) );
            break;
        }
        case Node::FixedInterval:
            job->setPriority( 700 );
            job->setSpecifiedStart( 0, task->constraintStartTime().toTime_t() );
            job->setSpecifiedEnd( 0, task->constraintEndTime().toTime_t() - 1 );
            if ( cs ) cs->logDebug( QString( "FI: set specified: %1 - %2").arg( TJ::time2ISO( task->constraintStartTime().toTime_t() ) ).arg( TJ::time2ISO( task->constraintEndTime().toTime_t() ) ) );
            break;
        default:
            if ( cs && locale() ) cs->logWarning( i18n( "Unhandled time constraint type" ) );
            break;
    }
}

void PlanTJScheduler::addRequests()
{
    kDebug();
    QMap<TJ::Task*, Task*> ::const_iterator it = m_taskmap.constBegin();
    for ( ; it != m_taskmap.constEnd(); ++it ) {
        addRequest( it.key(), it.value() );
    }
}

void PlanTJScheduler::addRequest( TJ::Task *job, Task *task )
{
    kDebug();
    if ( task->constraint() == Node::FixedInterval ) {
        job->setDuration( 0, ( task->constraintEndTime() - task->constraintStartTime() ).toDouble( Duration::Unit_d ) );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": fixed duration set to " + QString::number( job->getDuration( 0 ) ) ); }
        return;
    }
    if ( task->type() == Node::Type_Milestone || task->estimate() == 0 || ( m_recalculate && task->completion().isFinished() ) ) {
        job->setMilestone( true );
        job->setDuration( 0, 0.0 );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": set to milestone, duration set to 0" ); }
        return;
    }
    if ( task->estimate()->type() == Estimate::Type_Duration && task->estimate()->calendar() == 0 ) {
        job->setDuration( 0, task->estimate()->value( Estimate::Use_Expected, m_usePert ).toDouble( Duration::Unit_d ) );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": duration set to " + QString::number( job->getDuration( 0 ) ) ); }
        return;
    }
    if ( task->estimate()->type() == Estimate::Type_Duration && task->estimate()->calendar() != 0 ) {
        job->setLength( 0, task->estimate()->value( Estimate::Use_Expected, m_usePert ).toDouble( Duration::Unit_d ) );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": length set to " + QString::number( job->getLength( 0 ) ) ); }
        return;
    }
    if ( m_recalculate && task->completion().isStarted() ) {
        job->setEffort( 0, task->completion().remainingEffort().toDouble( Duration::Unit_d ) );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": (started) effort set to " + QString::number( job->getEffort( 0 ) ) ); }
    } else {
        Estimate *estimate = task->estimate();
        double e = estimate->scale( estimate->value( Estimate::Use_Expected, m_usePert ), Duration::Unit_d, estimate->scales() );
        job->setEffort( 0, e );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": effort set to " + QString::number( job->getEffort( 0 ) ) ); }
    }
    if ( task->requests().isEmpty() ) {
        return;
    }
    foreach ( ResourceRequest *rr, task->requests().resourceRequests( true /*resolveTeam*/ ) ) {
        TJ::Allocation *a = new TJ::Allocation();
        a->addCandidate( m_tjProject->getResource( rr->resource()->id() ) );
        a->setMandatory( true );
        if ( locale() ) { m_schedule->logDebug( task->name() + ": add candidate " + rr->resource()->name() ); }
        job->addAllocation( a );
    }
}


#include "PlanTJScheduler.moc"
