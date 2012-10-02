/* This file is part of the KDE project
 * Copyright (C) 2009, 2010, 2011, 2012 Dag Andersen <danders@get2net.dk>
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
#include "kptdebug.h"

#include "taskjuggler/taskjuggler.h"
#include "taskjuggler/Project.h"
#include "taskjuggler/Scenario.h"
#include "taskjuggler/Resource.h"
#include "taskjuggler/Task.h"
#include "taskjuggler/Interval.h"
#include "taskjuggler/Allocation.h"
#include "taskjuggler/Utility.h"
#include "taskjuggler/UsageLimits.h"
#include "taskjuggler/CoreAttributes.h"
#include "taskjuggler/TjMessageHandler.h"
#include "taskjuggler/debug.h"

#include <QString>
#include <QTime>
#include <QMutexLocker>
#include <QMap>

#include <KGlobal>
#include <KLocale>
#include <KDebug>

#include <iostream>

#define PROGRESS_MAX_VALUE 100


PlanTJScheduler::PlanTJScheduler( Project *project, ScheduleManager *sm, ulong granularity, QObject *parent )
    : SchedulerThread( project, sm, parent ),
    result( -1 ),
    m_schedule( 0 ),
    m_recalculate( false ),
    m_usePert( false ),
    m_backward( false ),
    m_granularity( granularity )
{
    TJ::TJMH.reset();
    connect(&TJ::TJMH, SIGNAL(message(int, const QString&, TJ::CoreAttributes*)), this, SLOT(slotMessage(int, const QString&, TJ::CoreAttributes*)));

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

void PlanTJScheduler::slotMessage( int type, const QString &msg, TJ::CoreAttributes *object )
{
//     kDebug(planDbg())<<"PlanTJScheduler::slotMessage:"<<msg;
    Schedule::Log log;
    if ( object &&  object->getType() == CA_Task && m_taskmap.contains( static_cast<TJ::Task*>( object ) ) ) {
        log = Schedule::Log( static_cast<Node*>( m_taskmap[ static_cast<TJ::Task*>( object ) ] ), type, msg );
    } else if ( object && object->getType() == CA_Resource && m_resourcemap.contains( static_cast<TJ::Resource*>( object ) ) ) {
        log = Schedule::Log( 0, m_resourcemap[ static_cast<TJ::Resource*>( object ) ], type, msg );
    } else if ( object && ! object->getName().isEmpty() ) {
        log = Schedule::Log( static_cast<Node*>( m_project ), type, QString( "%1: %2" ).arg(object->getName() ).arg( msg ) );
    } else {
        log = Schedule::Log( static_cast<Node*>( m_project ), type, msg );
    }
    slotAddLog( log );
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
    setMaxProgress( PROGRESS_MAX_VALUE );
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

        m_schedule->setPhaseName( 0, i18nc( "@info/plain" , "Init" ) );
        if ( ! m_backward && locale() ) {
            logDebug( m_project, 0, QString( "Schedule project using TJ Scheduler, starting at %1, granularity %2" ).arg( QDateTime::currentDateTime().toString() ).arg( locale()->formatDuration( m_granularity ) ), 0 );
            if ( m_recalculate ) {
                logInfo( m_project, 0, i18nc( "@info/plain" , "Re-calculate project from start time: %1", locale()->formatDateTime( m_project->constraintStartTime() ) ), 0 );
            } else {
                logInfo( m_project, 0, i18nc( "@info/plain" , "Schedule project from start time: %1", locale()->formatDateTime( m_project->constraintStartTime() ) ), 0 );
            }
            logInfo( m_project, 0, i18nc( "@info/plain" , "Project target finish time: %1", locale()->formatDateTime( m_project->constraintEndTime() ) ), 0 );
        }
        if ( m_backward && locale() ) {
            logDebug( m_project, 0, QString( "Schedule project backward using TJ Scheduler, starting at %1, granularity %2" ).arg( locale()->formatDateTime( QDateTime::currentDateTime() ) ).arg( locale()->formatDuration( m_granularity ) ), 0 );
            logInfo( m_project, 0, i18nc( "@info/plain" , "Schedule project from end time: %1", locale()->formatDateTime( m_project->constraintEndTime() ) ), 0 );
        }

        m_managerMutex.unlock();
        m_projectMutex.unlock();
    } // <--- mutex
    setProgress( 2 );
    if ( ! kplatoToTJ() ) {
        result = 1;
        setProgress( PROGRESS_MAX_VALUE );
        return;
    }
    setMaxProgress( PROGRESS_MAX_VALUE );
    connect(m_tjProject, SIGNAL(updateProgressBar(int, int)), this, SLOT(setProgress(int)));

    m_schedule->setPhaseName( 1, i18nc( "@info/plain" , "Schedule" ) );
    logInfo( m_project, 0, "Start scheduling", 1 );
    bool r = solve();
    if ( ! r ) {
        kDebug(planDbg())<<"Scheduling failed";
        result = 2;
        logError( m_project, 0, i18nc( "@info/plain" , "Failed to schedule project" ) );
        setProgress( PROGRESS_MAX_VALUE );
        return;
    }
    if ( m_haltScheduling ) {
        kDebug(planDbg())<<"Scheduling halted";
        logInfo( m_project, 0, "Scheduling halted" );
        deleteLater();
        return;
    }
    m_schedule->setPhaseName( 2, i18nc( "@info/plain" , "Update" ) );
    logInfo( m_project, 0, "Scheduling finished, update project", 2 );
    if ( ! kplatoFromTJ() ) {
        logError( m_project, 0, "Project update failed" );
    }
    setProgress( PROGRESS_MAX_VALUE );
    m_schedule->setPhaseName( 3, i18nc( "@info/plain" , "Finish" ) );
}

bool PlanTJScheduler::check()
{
    DebugCtrl.setDebugMode( 0 );
    DebugCtrl.setDebugLevel( 1000 );
    return m_tjProject->pass2( true );
}

bool PlanTJScheduler::solve()
{
    kDebug(planDbg())<<"PlanTJScheduler::solve()";
    TJ::Scenario *sc = m_tjProject->getScenario( 0 );
    if ( ! sc ) {
        if ( locale() ) {
            logError( m_project, 0, i18nc( "@info/plain" , "Failed to find scenario to schedule" ) );
        }
        return false;
    }
    DebugCtrl.setDebugLevel(0);
    DebugCtrl.setDebugMode(PSDEBUG+TSDEBUG);

    return m_tjProject->scheduleScenario( sc );
}

bool PlanTJScheduler::kplatoToTJ()
{
    m_tjProject = new TJ::Project();
    m_tjProject->setScheduleGranularity( m_granularity / 1000 );
    m_tjProject->setNow( m_project->constraintStartTime().toTime_t() );
    m_tjProject->setStart( m_project->constraintStartTime().toTime_t() );
    m_tjProject->setEnd( m_project->constraintEndTime().toTime_t() );

    m_tjProject->setDailyWorkingHours( m_project->standardWorktime()->day() );

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
    if ( ! m_backward ) {
        start->setSpecifiedStart( 0, m_tjProject->getStart() );
        start->setPriority( 999 );
    } else {
        // bacwards: insert a new ms nefore start and make start an ALAP to push all other jobs ALAP
        TJ::Task *bs = new TJ::Task( m_tjProject, "TJ::StartJob-B", "TJ::StartJob-B", 0, QString(), 0);
        bs->setMilestone( true );
        bs->setSpecifiedStart( 0, m_tjProject->getStart() );
        bs->setPriority( 999 );
        bs->addPrecedes( start->getId() );
        start->addDepends( bs->getId() );
        start->setScheduling( TJ::Task::ALAP );
    }
    TJ::Task *end = new TJ::Task( m_tjProject, "TJ::EndJob", "TJ::EndJob", 0, QString(), 0);
    end->setMilestone( true );
    if ( m_backward ) {
        end->setSpecifiedEnd( 0, m_tjProject->getEnd() - 1 );
        end->setScheduling( TJ::Task::ALAP );
    }
    for ( QMap<TJ::Task*, Task*>::ConstIterator it = m_taskmap.constBegin(); it != m_taskmap.constEnd(); ++it ) {
        if ( it.value()->isStartNode() ) {
            it.key()->addDepends( start->getId() );
//             logDebug( m_project, 0, QString( "'%1' depends on: '%2'" ).arg( it.key()->getName() ).arg( start->getName() ) );
            if ( start->getScheduling() == TJ::Task::ALAP ) {
                start->addPrecedes( it.key()->getId() );
//                 logDebug( m_project, 0, QString( "'%1' precedes: '%2'" ).arg( start->getName() ).arg( it.key()->getName() ) );
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
TJ::Interval PlanTJScheduler::toTJInterval( const QDateTime &start, const QDateTime &end, ulong granularity ) {
    int secs = QTime( 0, 0, 0 ).secsTo( start.time() );
    secs -= secs % granularity;
    QDateTime s( start.date(), QTime( 0, 0, 0 ).addSecs( secs ) );
    secs = QTime( 0, 0, 0 ).secsTo( end.time() );
    secs -= secs % granularity;
    QDateTime e( end.date(), QTime( 0, 0, 0 ).addSecs( secs ) );
    TJ::Interval ti( s.toTime_t(), e.addSecs( -1 ).toTime_t() );
    return ti;
}

// static
TJ::Interval PlanTJScheduler::toTJInterval( const QTime &start, const QTime &end, ulong granularity ) {
    int secs = QTime( 0, 0, 0 ).secsTo( start );
    time_t s =  secs - ( secs % granularity );
    secs = ( end == QTime( 0, 0, 0 ) ) ? 86399 : QTime( 0, 0, 0 ).secsTo( end );
    time_t e = secs - ( secs % granularity ) - 1;
    TJ::Interval ti( s, e );
    return ti;
}

ulong PlanTJScheduler::tjGranularity() const
{
    return m_tjProject->getScheduleGranularity();
}

bool PlanTJScheduler::kplatoFromTJ()
{
    MainSchedule *cs = static_cast<MainSchedule*>( m_project->currentSchedule() );

    QDateTime start;
    QDateTime end;
    for ( QMap<TJ::Task*, Task*>::ConstIterator it = m_taskmap.constBegin(); it != m_taskmap.constEnd(); ++it ) {
        if ( ! taskFromTJ( it.key(), it.value() ) ) {
            return false;
        }
        if ( ! start.isValid() || it.value()->startTime() < start ) {
            start = it.value()->startTime();
        }
        if ( ! end.isValid() || it.value()->endTime() > end ) {
            end = it.value()->endTime();
        }
    }
    m_project->setStartTime( start.isValid() ? start : m_project->constraintStartTime() );
    m_project->setEndTime( end.isValid() ? end : m_project->constraintEndTime() );

    adjustSummaryTasks( m_schedule->summaryTasks() );

    m_project->calcCriticalPathList( m_schedule );
    // calculate positive float
    foreach ( Task* t, m_taskmap ) {
        if ( ! t->inCriticalPath() && t->isStartNode() ) {
            calcPositiveFloat( t );
        }
    }

    if ( locale() ) {
        logInfo( m_project, 0, i18nc( "@info/plain" , "Project scheduled to start at %1 and finish at %2", locale()->formatDateTime( m_project->startTime() ), locale()->formatDateTime( m_project->endTime() ) ) );
    }

    if ( m_manager ) {
        if ( locale() ) logDebug( m_project, 0, QString( "Project scheduling finished at %1" ).arg( QDateTime::currentDateTime().toString() ) );
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
    kDebug(planDbg())<<"taskFromTJ:"<<task<<task->name()<<cs->id();
    task->setStartTime( DateTime( QDateTime::fromTime_t( job->getStart( 0 ) ) ) );
    task->setEndTime( DateTime( QDateTime::fromTime_t( job->getEnd( 0 ) ).addSecs( 1 ) ) );
    task->setDuration( task->endTime() - task->startTime() );

    if ( ! task->startTime().isValid() ) {
        logError( task, 0, i18nc( "@info/plain", "Invalid start time" ) );
        return false;
    }
    if ( ! task->endTime().isValid() ) {
        logError( task, 0, i18nc( "@info/plain", "Invalid end time" ) );
        return false;
    }
    if ( m_project->startTime() > task->startTime() ) {
        m_project->setStartTime( task->startTime() );
    }
    if ( task->endTime() > m_project->endTime() ) {
        m_project->setEndTime( task->endTime() );
    }
    foreach ( TJ::CoreAttributes *a, job->getBookedResources( 0 ) ) {
        TJ::Resource *r = static_cast<TJ::Resource*>( a );
        Resource *res = m_resourcemap[ r ];
        QList<TJ::Interval> lst = r->getBookedIntervals( 0, job );
        foreach ( const TJ::Interval &tji, lst ) {
            AppointmentInterval ai = fromTJInterval( tji );
            double load = res->type() == Resource::Type_Material ? res->units() : ai.load() * r->getEfficiency();
            res->addAppointment( cs, ai.startTime(), ai.endTime(), load );
            logDebug( task, 0, "'" + res->name() + "' added appointment: " +  ai.startTime().toString( Qt::ISODate ) + " - " + ai.endTime().toString( Qt::ISODate ) );
        }
    }
    cs->setScheduled( true );
    if ( locale() ) {
        if ( task->type() == Node::Type_Milestone ) {
            logInfo( task, 0, i18nc( "@info/plain" , "Scheduled milestone: %1", locale()->formatDateTime( task->startTime() ) ) );
        } else {
            logInfo( task, 0, i18nc( "@info/plain" , "Scheduled task: %1 - %2", locale()->formatDateTime( task->startTime() ), locale()->formatDateTime( task->endTime() ) ) );
        }
    }
    calcPertValues( task );
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

Duration PlanTJScheduler::calcPositiveFloat( Task *task )
{
    Duration x;
    foreach ( const Relation *r, task->dependChildNodes() + task->childProxyRelations() ) {
        if ( ! r->child()->inCriticalPath() ) {
            Duration f = calcPositiveFloat( static_cast<Task*>( r->child() ) );
            if ( x == 0 || f < x ) {
                x = f;
            }
        }
    }
    Duration totfloat = task->freeFloat() + x;
    task->setPositiveFloat( totfloat );
    return totfloat;
}

void PlanTJScheduler::calcPertValues( Task *t )
{
    // NOTE: no need for milliseconds as TJ works with seconds
    kDebug(planDbg())<<t->name()<<t->startTime()<<t->endTime();
    qint64 startfloat = 0, freefloat = 0, negativefloat = 0;
    foreach ( const Relation *r, t->dependParentNodes() + t->parentProxyRelations() ) {
        qint64 f = (qint64)(r->parent()->endTime().secsTo( t->startTime() ) - r->lag().seconds());
        if ( f < negativefloat ) {
            negativefloat = f;
        }
        if ( f > 0 && ( startfloat == 0 || startfloat > f ) ) {
            startfloat = f;
        }
    }
    foreach ( const Relation *r, t->dependChildNodes() + t->childProxyRelations() ) {
        qint64 f = t->endTime().secsTo( r->child()->startTime() ) - r->lag().seconds();
        if ( f > 0 && ( freefloat == 0 || freefloat > f ) ) {
            freefloat = f;
        }
    }
    t->setFreeFloat( Duration( freefloat, Duration::Unit_s ) );
    t->setNegativeFloat( Duration( negativefloat, Duration::Unit_s ) );
    // TODO calculate real values dependent on resources
    t->setEarlyStart( t->startTime().addSecs( -startfloat ) );
    t->setLateStart( t->startTime().addSecs( freefloat ) );
    t->setEarlyFinish( t->endTime().addSecs( -startfloat ) );
    t->setLateFinish( t->endTime().addSecs( freefloat ) );

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
        kDebug(planDbg())<<r->name()<<"already exist";
        return m_resourcemap.key( r );
    }
    TJ::Resource *res = new TJ::Resource( m_tjProject, r->id(), r->name(), 0 );
    if ( r->type() == Resource::Type_Material ) {
        res->setEfficiency( 0.0 );
    } else {
        res->setEfficiency( (double)(r->units()) / 100. );
    }
    Calendar *cal = r->calendar();
    int days[ 7 ] = { Qt::Sunday, Qt::Monday, Qt::Tuesday, Qt::Wednesday, Qt::Thursday, Qt::Friday, Qt::Saturday };
    for ( int i = 0; i < 7; ++i ) {
        CalendarDay *d = 0;
        for ( Calendar *c = cal; c; c = c->parentCal() ) {
            QTime t; t.start();
            d = c->weekday( days[ i ] );
            Q_ASSERT( d );
            if ( d == 0 || d->state() != CalendarDay::Undefined ) {
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
            res->addVacation( new TJ::Interval( toTJInterval( QDateTime( day->date() ), QDateTime( day->date().addDays( 1 ) ), tjGranularity() ) ) );
        } else if ( day->state() == CalendarDay::Working ) {
            TJ::Shift *shift = new TJ::Shift( m_tjProject, r->id() + day->date().toString( Qt::ISODate ), r->name(), 0, QString(), 0 );
            foreach ( TimeInterval *ti, day->timeIntervals() ) {
                QList<TJ::Interval*> ivs;
                ivs << new TJ::Interval( toTJInterval( ti->startTime(), ti->endTime(), tjGranularity() ) );
                shift->setWorkingHours( day->date().dayOfWeek() - 1, ivs );
            }
            TJ::Interval period = toTJInterval( day->start(), day->end(), tjGranularity() );
            TJ::ShiftSelection *sl = new TJ::ShiftSelection( period, shift );
            res->addShift( sl );
        }
    }
    if ( m_project->startTime() < r->availableFrom() ) {
        res->addVacation( new TJ::Interval( toTJInterval( m_project->startTime(), r->availableFrom(), tjGranularity() ) ) );
    }
    if ( m_project->endTime() > r->availableUntil() ) {
        res->addVacation( new TJ::Interval( toTJInterval( r->availableUntil(), m_project->startTime(), tjGranularity() ) ) );
    }
    m_resourcemap[res] = r;
//     if ( locale() ) { logDebug( m_project, 0, "Added resource: " + r->name() ); }
    return res;
}

TJ::Task *PlanTJScheduler::addTask( KPlato::Task *task )
{
/*    if ( m_backward && task->isStartNode() ) {
        Relation *r = new Relation( m_backwardTask, task );
        m_project->addRelation( r );
    }*/
    TJ::Task *t = new TJ::Task(m_tjProject, task->id(), task->name(), 0, QString(), 0);
    m_taskmap[ t ] = task;
//     if ( locale() ) { logDebug( m_project, 0, "Added task: " + task->name() ); }
    return t;
}

void PlanTJScheduler::addTasks()
{
    kDebug(planDbg());
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

void PlanTJScheduler::addDepends( const Relation *rel )
{
    TJ::Task *child = m_tjProject->getTask( rel->child()->id() );
    TJ::TaskDependency *d = child->addDepends( rel->parent()->id() );
    d->setGapDuration( 0, rel->lag().seconds() );
}

void PlanTJScheduler::addPrecedes( const Relation *rel )
{
    TJ::Task *parent = m_tjProject->getTask( rel->parent()->id() );
    TJ::TaskDependency *d = parent->addPrecedes( rel->child()->id() );
    d->setGapDuration( 0, rel->lag().seconds() );
}

void PlanTJScheduler::addDependencies( KPlato::Task *task )
{
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
                if ( locale() ) {
                    logWarning( task, 0, i18nc( "@info/plain" , "Dependency type '%1' not handled. Using FinishStart.", task->constraintToString( true ) ) );
                }
                break;
        }
        switch ( task->constraint() ) {
            case Node::ASAP:
            case Node::ALAP:
                addPrecedes( r );
                addDepends( r );
                break;
            case Node::MustStartOn:
            case Node::StartNotEarlier:
                addPrecedes( r );
                break;
            case Node::MustFinishOn:
            case Node::FinishNotLater:
                addDepends( r );
                break;
            case Node::FixedInterval:
                break;
        }
    }
}

void PlanTJScheduler::addDependencies()
{
    foreach ( Task *t, m_taskmap ) {
        addDependencies( t );
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
    switch ( task->constraint() ) {
        case Node::ASAP:
            if ( ! job->isMilestone() ) {
                job->setScheduling( m_backward ? TJ::Task::ALAP : TJ::Task::ASAP );
            }
            break;
        case Node::ALAP:
            job->setScheduling( TJ::Task::ALAP);
            break;
        case Node::MustStartOn:
            job->setPriority( 600 );
            job->setSpecifiedStart( 0, task->constraintStartTime().toTime_t() );
            logDebug( task, 0, QString( "MSO: set specified start: %1").arg( TJ::time2ISO( task->constraintStartTime().toTime_t() ) ) );
            break;
        case Node::StartNotEarlier: {
            job->setPriority( 500 );
            job->setSpecifiedStart( 0, task->constraintStartTime().toTime_t() );
            logDebug( task, 0, QString( "SNE: set specified start: %1").arg( TJ::time2ISO( task->constraintStartTime().toTime_t() ) ) );
            break;
        }
        case Node::MustFinishOn:
            job->setPriority( 600 );
            job->setScheduling( TJ::Task::ALAP );
            job->setSpecifiedEnd( 0, task->constraintEndTime().toTime_t() - 1 );
            logDebug( task, 0, QString( "MFO: set specified end: %1").arg( TJ::time2ISO( task->constraintEndTime().toTime_t() ) ) );
            break;
        case Node::FinishNotLater: {
            job->setPriority( 500 );
            job->setScheduling( TJ::Task::ALAP );
            job->setSpecifiedEnd( 0, task->constraintEndTime().toTime_t() - 1 );
            logDebug( task, 0, QString( "FNL: set specified end: %1").arg( TJ::time2ISO( task->constraintEndTime().toTime_t() ) ) );
            break;
        }
        case Node::FixedInterval: {
            job->setPriority( 700 );
            TJ::Interval i( toTJInterval( task->constraintStartTime(), task->constraintEndTime(), tjGranularity() ) );
            job->setSpecifiedPeriod( 0, i );
            // estimate not allowed
            job->setDuration( 0, 0.0 );
            job->setLength( 0, 0.0 );
            job->setEffort( 0, 0.0 );
            logDebug( task, 0, QString( "FI: set specified: %1 - %2 -> %3 - %4 (%5)")
                      .arg( TJ::time2ISO( task->constraintStartTime().toTime_t() ) )
                      .arg( TJ::time2ISO( task->constraintEndTime().toTime_t() ) )
                      .arg( TJ::time2ISO( i.getStart() ) )
                      .arg( TJ::time2ISO( i.getEnd() ) )
                      .arg( tjGranularity() ) );
            break;
        }
        default:
            if ( locale() ) logWarning( task, 0, i18nc( "@info/plain", "Unhandled time constraint type" ) );
            break;
    }
}

void PlanTJScheduler::addRequests()
{
    kDebug(planDbg());
    QMap<TJ::Task*, Task*> ::const_iterator it = m_taskmap.constBegin();
    for ( ; it != m_taskmap.constEnd(); ++it ) {
        addRequest( it.key(), it.value() );
    }
}

void PlanTJScheduler::addRequest( TJ::Task *job, Task *task )
{
    kDebug(planDbg());
    if ( task->type() == Node::Type_Milestone || task->estimate() == 0 || ( m_recalculate && task->completion().isFinished() ) ) {
        job->setMilestone( true );
        job->setDuration( 0, 0.0 );
        return;
    }
    // Note: FI tasks can never have an estimate set (duration, length or effort)
    if ( task->constraint() != Node::FixedInterval ) {
        if ( task->estimate()->type() == Estimate::Type_Duration && task->estimate()->calendar() == 0 ) {
            job->setDuration( 0, task->estimate()->value( Estimate::Use_Expected, m_usePert ).toDouble( Duration::Unit_d ) );
            return;
        }
        if ( task->estimate()->type() == Estimate::Type_Duration && task->estimate()->calendar() != 0 ) {
            job->setLength( 0, task->estimate()->value( Estimate::Use_Expected, m_usePert ).toDouble( Duration::Unit_d ) );
            return;
        }
        if ( m_recalculate && task->completion().isStarted() ) {
            job->setEffort( 0, task->completion().remainingEffort().toDouble( Duration::Unit_d ) );
        } else {
            Estimate *estimate = task->estimate();
            double e = estimate->scale( estimate->value( Estimate::Use_Expected, m_usePert ), Duration::Unit_d, estimate->scales() );
            job->setEffort( 0, e );
        }
    }
    if ( task->requests().isEmpty() ) {
        return;
    }
    foreach ( ResourceRequest *rr, task->requests().resourceRequests( true /*resolveTeam*/ ) ) {
        TJ::Resource *tjr = addResource( rr->resource() );
        TJ::Allocation *a = new TJ::Allocation();
        a->setSelectionMode( TJ::Allocation::order );
        if ( rr->units() != 100 ) {
            TJ::UsageLimits *l = new TJ::UsageLimits();
            l->setDailyUnits( rr->units() );
            a->setLimits( l );
        }
        a->addCandidate( tjr );
        job->addAllocation( a );
        if ( locale() ) { logDebug( task, 0, "Added resource candidate: " + rr->resource()->name() ); }
        foreach ( Resource *r, rr->requiredResources() ) {
            TJ::Resource *tr = addResource( r );
            a->addRequiredResource( tjr, tr );
            if ( locale() ) { logDebug( task, 0, "Added required resource: " + r->name() ); }
        }
    }
}


#include "PlanTJScheduler.moc"
