/* This file is part of the KDE project
 * Copyright (C) 2009 Dag Andersen <danders@get2net.dk>
 * Copyright (C) 2011 Dag Andersen <danders@get2net.dk>
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

#ifndef PLANTJPSCHEDULER_H
#define PLANTJPSCHEDULER_H

#include "kplatotj_export.h"

#include "kptschedulerplugin.h"

#include "kptdatetime.h"

#include <QThread>
#include <QObject>
#include <QMap>
#include <QList>

class KLocale;
class QDateTime;

namespace TJ
{
    class Project;
    class Task;
    class Resource;
    class Interval;
    class CoreAttributes;

}

namespace KPlato
{
    class Project;
    class ScheduleManager;
    class Schedule;
    class MainSchedule;
    class Resource;
    class ResourceRequest;
    class Task;
    class Node;
    class Relation;
}
using namespace KPlato;

class PlanTJScheduler : public KPlato::SchedulerThread
{
    Q_OBJECT

private:

public:
    PlanTJScheduler( Project *project, ScheduleManager *sm, QObject *parent = 0 );
    ~PlanTJScheduler();

    bool check();
    bool solve();
    int result;

    /// Fill project data into TJ structure
    bool kplatoToTJ();
    /// Fetch project data from TJ structure
    bool kplatoFromTJ();


signals:
    void sigCalculationStarted( Project*, ScheduleManager* );
    void sigCalculationFinished( Project*, ScheduleManager* );
    const char* taskname();

public slots:
    void slotMessage( int type, const QString &msg, TJ::CoreAttributes *object );

protected:
    void run();

    void adjustSummaryTasks( const QList<Node*> &nodes );

    void addResources();
    TJ::Resource *addResource( KPlato::Resource *resource );
    void addTasks();
    TJ::Task *addTask( KPlato::Task *task );
    void addDependencies();
    void addPrecedes( const Relation *rel );
    void addDepends( const Relation *rel );
    void addDependencies( TJ::Task *job, Task *task );
    void setConstraints();
    void setConstraint( TJ::Task *job, KPlato::Task *task );
    void addRequests();
    void addRequest( TJ::Task *job, Task *task );
    void addStartEndJob();
    bool taskFromTJ( TJ::Task *job, Task *task );
    void calcPertValues();
    Duration calcPositiveFloat( Task *task );

    static bool exists( QList<CalendarDay*> &lst, CalendarDay *day );
    static DateTime fromTime_t( time_t );
    AppointmentInterval fromTJInterval( const TJ::Interval &tji );
    static TJ::Interval toTJInterval( const QDateTime &start, const QDateTime &end );
    static TJ::Interval toTJInterval( const QTime &start, const QTime &end );

private:
    KLocale *locale() const;

private:
    MainSchedule *m_schedule;
    bool m_recalculate;
    bool m_usePert;
    bool m_backward;
    TJ::Project *m_tjProject;
//     Task *m_backwardTask;

    QMap<TJ::Task*, Task*> m_taskmap;
    QMap<TJ::Resource*, Resource*> m_resourcemap;
    
};

#endif // PLANTJPSCHEDULER_H
