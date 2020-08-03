/* This file is part of the KDE project
 * Copyright (C) 2009 Dag Andersen <dag.andersen@kdemail.net>
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

#ifndef PLANRCPSPLUGIN_H
#define PLANRCPSPLUGIN_H

#include "kptschedulerplugin.h"

#include <QVariantList>


namespace KPlato
{
    class Project;
    class ScheduleManager;
    class Schedule;
}

using namespace KPlato;

class KPlatoRCPSPlugin : public SchedulerPlugin
{
    Q_OBJECT

public:
    KPlatoRCPSPlugin(QObject * parent,  const QVariantList &);
    ~KPlatoRCPSPlugin();

    virtual QString description() const;
    virtual int capabilities() const;
    /// Calculate the project
    virtual void calculate(Project &project, ScheduleManager *sm, bool nothread = false);

    /// Return the scheduling granularity in milliseconds
    ulong currentGranularity() const;

Q_SIGNALS:
    void sigCalculationStarted(KPlato::Project*, KPlato::ScheduleManager*);
    void sigCalculationFinished(KPlato::Project*, KPlato::ScheduleManager*);

public Q_SLOTS:
    void stopAllCalculations();
    void stopCalculation(KPlato::SchedulerThread *sch);

protected Q_SLOTS:
    void slotStarted(KPlato::SchedulerThread *job);
    void slotFinished(KPlato::SchedulerThread *job);
};


#endif // PLANRCPSPLUGIN_H
