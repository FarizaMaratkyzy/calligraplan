/* This file is part of the KDE project
 * Copyright (C) 2001 Thomas zander <zander@kde.org>
 * Copyright (C) 2004 - 2007 Dag Andersen <dag.andersen@kdemail.net>
 * Copyright (C) 2007 Florian Piquemal <flotueur@yahoo.fr>
 * Copyright (C) 2007 Alexis Ménard <darktears31@gmail.com>
 * Copyright (C) 2019 Dag Andersen <dag.andersen@kdemail.net>
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
#include "kpttask.h"
#include "kptappointment.h"
#include "kptproject.h"
#include "kptduration.h"
#include "kptrelation.h"
#include "kptdatetime.h"
#include "kptcalendar.h"
#include "kpteffortcostmap.h"
#include "kptschedule.h"
#include "kptxmlloaderobject.h"
#include "XmlSaveContext.h"
#include <kptdebug.h>

#include <KoXmlReader.h>

#include <KLocalizedString>


namespace KPlato
{

Task::Task(Node *parent)
    : Node(parent),
      m_resource(),
      m_workPackage(this)
{
    //debugPlan<<"("<<this<<')';
    m_requests.setTask(this);
    Duration d(1, 0, 0);
    m_estimate = new Estimate();
    m_estimate->setOptimisticRatio(-10);
    m_estimate->setPessimisticRatio(20);
    m_estimate->setParentNode(this);

    if (m_parent)
        m_leader = m_parent->leader();
}

Task::Task(const Task &task, Node *parent)
    : Node(task, parent),
      m_resource(),
      m_workPackage(this)
{
    //debugPlan<<"("<<this<<')';
    m_requests.setTask(this);
    delete m_estimate;
    if (task.estimate()) {
        m_estimate = new Estimate(*(task.estimate()));
    } else {
        m_estimate = new Estimate();
    }
    m_estimate->setParentNode(this);
}


Task::~Task() {
    while (!m_resource.isEmpty()) {
        delete m_resource.takeFirst();
    }
    while (!m_parentProxyRelations.isEmpty()) {
        delete m_parentProxyRelations.takeFirst();
    }
    while (!m_childProxyRelations.isEmpty()) {
        delete m_childProxyRelations.takeFirst();
    }
}

int Task::type() const {
    if (numChildren() > 0) {
        return Node::Type_Summarytask;
    } else if (m_constraint == Node::FixedInterval) {
        if (m_constraintEndTime == m_constraintStartTime) {
            return Node::Type_Milestone;
        }
    } else if (m_estimate->expectedEstimate() == 0.0) {
        return Node::Type_Milestone;
    }
    return Node::Type_Task;
}

Duration *Task::getRandomDuration() {
    return 0L;
}

// void Task::clearResourceRequests() {
//     m_requests.clear();
//     changed(this, ResourceRequestProperty);
// }

QStringList Task::requestNameList() const {
    return m_requests.requestNameList();
}

QList<Resource*> Task::requestedResources() const {
    return m_requests.requestedResources();
}

bool Task::containsRequest(const QString &identity) const {
    return m_requests.contains(identity);
}

ResourceRequest *Task::resourceRequest(const QString &name) const {
    return m_requests.resourceRequest(name);
}

QStringList Task::assignedNameList(long id) const {
    Schedule *s = schedule(id);
    if (s == 0) {
        return QStringList();
    }
    return s->resourceNameList();
}

void Task::makeAppointments() {
    if (m_currentSchedule == 0)
        return;
    if (type() == Node::Type_Task) {
        //debugPlan<<m_name<<":"<<m_currentSchedule->startTime<<","<<m_currentSchedule->endTime<<";"<<m_currentSchedule->duration.toString();
        m_requests.makeAppointments(m_currentSchedule);
        //debugPlan<<m_name<<":"<<m_currentSchedule->startTime<<","<<m_currentSchedule->endTime<<";"<<m_currentSchedule->duration.toString();
    } else if (type() == Node::Type_Summarytask) {
        foreach (Node *n, m_nodes) {
            n->makeAppointments();
        }
    } else if (type() == Node::Type_Milestone) {
        //debugPlan<<"Milestone not implemented";
        // Well, shouldn't have resources anyway...
    }
}

void Task::copySchedule()
{
    if (m_currentSchedule == 0 || type() != Node::Type_Task) {
        return;
    }
    int id = m_currentSchedule->parentScheduleId();
    NodeSchedule *ns = static_cast<NodeSchedule*>(findSchedule(id));
    if (ns == 0) {
        return;
    }
    if (type() == Node::Type_Task) {
        copyAppointments(ns->startTime, ns->endTime);
    }
    m_currentSchedule->startTime = ns->startTime;
    m_currentSchedule->earlyStart = ns->earlyStart;
    m_currentSchedule->endTime = ns->endTime;
    m_currentSchedule->lateFinish = ns->lateFinish;
    m_currentSchedule->duration = ns->duration;
    // TODO: status flags, etc
    //debugPlan;
}

void Task::copyAppointments()
{
    copyAppointments(DateTime(), m_currentSchedule->startTime);
}

void Task::copyAppointments(const DateTime &start, const DateTime &end)
{
    if (m_currentSchedule == 0 || type() != Node::Type_Task) {
        return;
    }
    int id = m_currentSchedule->parentScheduleId();
    NodeSchedule *ns = static_cast<NodeSchedule*>(findSchedule(id));
    if (ns == 0) {
        return;
    }
    DateTime st = start.isValid() ? start : ns->startTime;
    DateTime et = end.isValid() ? end : ns->endTime;
    //debugPlan<<m_name<<st.toString()<<et.toString()<<m_currentSchedule->calculationMode();
    foreach (const Appointment *a, ns->appointments()) {
        Resource *r = a->resource() == 0 ? 0 : a->resource()->resource();
        if (r == 0) {
            errorPlan<<"No resource";
            continue;
        }
        AppointmentIntervalList lst = a->intervals(st, et);
        if (lst.isEmpty()) {
            //debugPlan<<"No intervals to copy from"<<a;
            continue;
        }
        Appointment *curr = 0;
        foreach (Appointment *c, m_currentSchedule->appointments()) {
            if (c->resource()->resource() == r) {
                //debugPlan<<"Found current appointment to"<<a->resource()->resource()->name()<<c;
                curr = c;
                break;
            }
        }
        if (curr == 0) {
            curr = new Appointment();
            m_currentSchedule->add(curr);
            curr->setNode(m_currentSchedule);
            //debugPlan<<"Created new appointment"<<curr;
        }
        ResourceSchedule *rs = static_cast<ResourceSchedule*>(r->findSchedule(m_currentSchedule->id()));
        if (rs == 0) {
            rs = r->createSchedule(m_currentSchedule->parent());
            rs->setId(m_currentSchedule->id());
            rs->setName(m_currentSchedule->name());
            rs->setType(m_currentSchedule->type());
            //debugPlan<<"Resource schedule not found, id="<<m_currentSchedule->id();
        }
        rs->setCalculationMode(m_currentSchedule->calculationMode());
        if (! rs->appointments().contains(curr)) {
            //debugPlan<<"add to resource"<<rs<<curr;
            rs->add(curr);
            curr->setResource(rs);
        }
        Appointment app;
        app.setIntervals(lst);
        //foreach (AppointmentInterval *i, curr->intervals()) { debugPlan<<i->startTime().toString()<<i->endTime().toString(); }
        curr->merge(app);
        //debugPlan<<"Appointments added";
    }
    m_currentSchedule->startTime = ns->startTime;
    m_currentSchedule->earlyStart = ns->earlyStart;
}

void Task::calcResourceOverbooked() {
    if (m_currentSchedule)
        m_currentSchedule->calcResourceOverbooked();
}

bool Task::load(KoXmlElement &element, XMLLoaderObject &status) {
    QString s;
    bool ok = false;
    m_id = element.attribute(QStringLiteral("id"));
    m_priority = element.attribute(QStringLiteral("priority"), "0").toInt();

    setName(element.attribute(QStringLiteral("name")));
    m_leader = element.attribute(QStringLiteral("leader"));
    m_description = element.attribute(QStringLiteral("description"));
    //debugPlan<<m_name<<": id="<<m_id;

    // Allow for both numeric and text
    QString constraint = element.attribute(QStringLiteral("scheduling"),QStringLiteral("0"));
    m_constraint = (Node::ConstraintType)constraint.toInt(&ok);
    if (!ok)
        Node::setConstraint(constraint); // hmmm, why do I need Node::?

    s = element.attribute(QStringLiteral("constraint-starttime"));
    if (!s.isEmpty())
        m_constraintStartTime = DateTime::fromString(s, status.projectTimeZone());
    s = element.attribute(QStringLiteral("constraint-endtime"));
    if (!s.isEmpty())
        m_constraintEndTime = DateTime::fromString(s, status.projectTimeZone());

    m_startupCost = element.attribute(QStringLiteral("startup-cost"), QStringLiteral("0.0")).toDouble();
    m_shutdownCost = element.attribute(QStringLiteral("shutdown-cost"), QStringLiteral("0.0")).toDouble();

    // Load the task children
    KoXmlNode n = element.firstChild();
    for (; ! n.isNull(); n = n.nextSibling()) {
        if (! n.isElement()) {
            continue;
        }
        KoXmlElement e = n.toElement();
        if (e.tagName() == QLatin1String("project")) {
            // Load the subproject
/*              Project *child = new Project(this, status);
            if (child->load(e)) {
                if (!project.addSubTask(child, this)) {
                    delete child;  // TODO: Complain about this
                }
            } else {
                // TODO: Complain about this
                delete child;
            }*/
        } else if (e.tagName() == QLatin1String("task")) {
            if (status.loadTaskChildren()) {
                // Load the task
                Task *child = new Task(this);
                if (child->load(e, status)) {
                    if (!status.project().addSubTask(child, this)) {
                        delete child;  // TODO: Complain about this
                    }
                } else {
                    // TODO: Complain about this
                    delete child;
                }
            }
        } else if (e.tagName() == QLatin1String("resource")) {
            // TODO: Load the resource (projects don't have resources yet)
        } else if (e.tagName() == QLatin1String("estimate") ||
                   (/*status.version() < "0.6" &&*/ e.tagName() == QLatin1String("effort"))) {
            //  Load the estimate
            m_estimate->load(e, status);
        } else if (e.tagName() == QLatin1String("workpackage")) {
            m_workPackage.loadXML(e, status);
        } else if (e.tagName() == QLatin1String("progress")) {
            completion().loadXML(e, status);
        } else if (e.tagName() == QLatin1String("task-schedules") || (status.version() < "0.7.0" && e.tagName() == QLatin1String("schedules"))) {
            KoXmlNode n = e.firstChild();
            for (; ! n.isNull(); n = n.nextSibling()) {
                if (! n.isElement()) {
                    continue;
                }
                KoXmlElement el = n.toElement();
                if (el.tagName() == QLatin1String("schedule")) {
                    NodeSchedule *sch = new NodeSchedule();
                    if (sch->loadXML(el, status)) {
                        sch->setNode(this);
                        addSchedule(sch);
                    } else {
                        errorPlan<<"Failed to load schedule";
                        delete sch;
                    }
                }
            }
        } else if (e.tagName() == QLatin1String("resourcegroup-request")) {
            Q_ASSERT(status.version() < "0.7.0");
            KoXmlElement re;
            forEachElement(re, e) {
                if (re.tagName() == "resource-request") {
                    ResourceRequest *r = new ResourceRequest();
                    if (r->load(re, status.project())) {
                        m_requests.addResourceRequest(r);
                    } else {
                        errorPlan<<"Failed to load resource request";
                        delete r;
                    }
                }
            }
            ResourceGroup *group = status.project().group(e.attribute("group-id"));
            if (!group) {
                errorPlanXml<<"Could not find resourcegroup"<<e.attribute("group-id");
            } else {
                QList<ResourceRequest*> groupRequests;
                int numRequests = e.attribute("units").toInt();
                for (int i = 0; i < numRequests; ++i) {
                    for (Resource *r : group->resources()) {
                        if (!m_requests.find(r)) {
                            groupRequests << new ResourceRequest(r, 100);
                            m_requests.addResourceRequest(groupRequests.last());
                        }
                    }
                }
                for (ResourceRequest *rr : groupRequests) {
                    for (Resource *r : group->resources()) {
                        if (!m_requests.find(r)) {
                            rr->addAlternativeRequest(new ResourceRequest(r));
                        }
                    }
                }
            }
        } else if (e.tagName() == QLatin1String("documents")) {
            m_documents.load(e, status);
        } else if (e.tagName() == QLatin1String("workpackage-log")) {
            KoXmlNode n = e.firstChild();
            for (; ! n.isNull(); n = n.nextSibling()) {
                if (! n.isElement()) {
                    continue;
                }
                KoXmlElement el = n.toElement();
                if (el.tagName() == QLatin1String("workpackage")) {
                    WorkPackage *wp = new WorkPackage(this);
                    if (wp->loadLoggedXML(el, status)) {
                        m_packageLog << wp;
                    } else {
                        errorPlan<<"Failed to load logged workpackage";
                        delete wp;
                    }
                }
            }
        }
    }
    //debugPlan<<m_name<<" loaded";
    return true;
}

void Task::save(QDomElement &element, const XmlSaveContext &context)  const
{
    if (!context.saveNode(this)) {
        return;
    }
    QDomElement me = element.ownerDocument().createElement(QStringLiteral("task"));
    element.appendChild(me);

    me.setAttribute(QStringLiteral("id"), m_id);
    me.setAttribute("priority", QString::number(m_priority));
    me.setAttribute(QStringLiteral("name"), m_name);
    me.setAttribute(QStringLiteral("leader"), m_leader);
    me.setAttribute(QStringLiteral("description"), m_description);

    me.setAttribute(QStringLiteral("scheduling"),constraintToString());
    me.setAttribute(QStringLiteral("constraint-starttime"),m_constraintStartTime.toString(Qt::ISODate));
    me.setAttribute(QStringLiteral("constraint-endtime"),m_constraintEndTime.toString(Qt::ISODate));

    me.setAttribute(QStringLiteral("startup-cost"), QString::number(m_startupCost));
    me.setAttribute(QStringLiteral("shutdown-cost"), QString::number(m_shutdownCost));

    me.setAttribute(QStringLiteral("wbs"), wbsCode()); //NOTE: included for information

    m_estimate->save(me);

    m_documents.save(me);

    if (context.saveAll(this)) {
        if (!m_schedules.isEmpty()) {
            QDomElement schs = me.ownerDocument().createElement(QStringLiteral("task-schedules"));
            me.appendChild(schs);
            foreach (const Schedule *s, m_schedules) {
                if (!s->isDeleted()) {
                    s->saveXML(schs);
                }
            }
        }
        completion().saveXML(me);

        m_workPackage.saveXML(me);
        // The workpackage log
        if (!m_packageLog.isEmpty()) {
            QDomElement log = me.ownerDocument().createElement(QStringLiteral("workpackage-log"));
            me.appendChild(log);
            foreach (const WorkPackage *wp, m_packageLog) {
                wp->saveLoggedXML(log);
            }
        }
    }
    if (context.saveChildren(this)) {
        for (int i=0; i<numChildren(); i++) {
            childNode(i)->save(me, context);
        }
    }
}

void Task::saveAppointments(QDomElement &element, long id) const {
    //debugPlan<<m_name<<" id="<<id;
    Schedule *sch = findSchedule(id);
    if (sch) {
        sch->saveAppointments(element);
    }
    foreach (const Node *n, m_nodes) {
        n->saveAppointments(element, id);
    }
}

void Task::saveWorkPackageXML(QDomElement &element, long id)  const
{
    QDomElement me = element.ownerDocument().createElement(QStringLiteral("task"));
    element.appendChild(me);

    me.setAttribute(QStringLiteral("id"), m_id);
    me.setAttribute(QStringLiteral("name"), m_name);
    me.setAttribute(QStringLiteral("leader"), m_leader);
    me.setAttribute(QStringLiteral("description"), m_description);

    me.setAttribute(QStringLiteral("scheduling"),constraintToString());
    me.setAttribute(QStringLiteral("constraint-starttime"),m_constraintStartTime.toString(Qt::ISODate));
    me.setAttribute(QStringLiteral("constraint-endtime"),m_constraintEndTime.toString(Qt::ISODate));

    me.setAttribute(QStringLiteral("startup-cost"), QString::number(m_startupCost));
    me.setAttribute(QStringLiteral("shutdown-cost"), QString::number(m_shutdownCost));

    me.setAttribute(QStringLiteral("wbs"), wbsCode()); // NOTE: included for information

    m_estimate->save(me);

    completion().saveXML(me);

    if (m_schedules.contains(id) && ! m_schedules[ id ]->isDeleted()) {
        QDomElement schs = me.ownerDocument().createElement(QStringLiteral("task-schedules"));
        me.appendChild(schs);
        m_schedules[ id ]->saveXML(schs);
    }
    m_documents.save(me); // TODO: copying documents
}

bool Task::isStarted() const
{
    return completion().isStarted();
}

EffortCostMap Task::plannedEffortCostPrDay(QDate start, QDate end, long id, EffortCostCalculationType typ) const {
    //debugPlan<<m_name;
    if (type() == Node::Type_Summarytask) {
        EffortCostMap ec;
        QListIterator<Node*> it(childNodeIterator());
        while (it.hasNext()) {
            ec += it.next() ->plannedEffortCostPrDay(start, end, id, typ);
        }
        return ec;
    }
    Schedule *s = schedule(id);
    if (s) {
        return s->plannedEffortCostPrDay(start, end, typ);
    }
    return EffortCostMap();
}

EffortCostMap Task::plannedEffortCostPrDay(const Resource *resource, QDate start, QDate end, long id, EffortCostCalculationType typ) const {
    //debugPlan<<m_name;
    if (type() == Node::Type_Summarytask) {
        EffortCostMap ec;
        QListIterator<Node*> it(childNodeIterator());
        while (it.hasNext()) {
            ec += it.next() ->plannedEffortCostPrDay(resource, start, end, id, typ);
        }
        return ec;
    }
    Schedule *s = schedule(id);
    if (s) {
        return s->plannedEffortCostPrDay(resource, start, end, typ);
    }
    return EffortCostMap();
}

EffortCostMap Task::actualEffortCostPrDay(QDate start, QDate end, long id, EffortCostCalculationType typ) const {
    //debugPlan<<m_name;
    if (type() == Node::Type_Summarytask) {
        EffortCostMap ec;
        QListIterator<Node*> it(childNodeIterator());
        while (it.hasNext()) {
            ec += it.next() ->actualEffortCostPrDay(start, end, id, typ);
        }
        return ec;
    }
    switch (completion().entrymode()) {
        case Completion::FollowPlan:
            return plannedEffortCostPrDay(start, end, id, typ);
        default:
            return completion().effortCostPrDay(start, end, id);
    }
    return EffortCostMap();
}

EffortCostMap Task::actualEffortCostPrDay(const Resource *resource, QDate start, QDate end, long id, EffortCostCalculationType typ) const {
    //debugPlan<<m_name;
    if (type() == Node::Type_Summarytask) {
        EffortCostMap ec;
        QListIterator<Node*> it(childNodeIterator());
        while (it.hasNext()) {
            ec += it.next() ->actualEffortCostPrDay(resource, start, end, id, typ);
        }
        return ec;
    }
    switch (completion().entrymode()) {
        case Completion::FollowPlan:
            return plannedEffortCostPrDay(resource, start, end, id, typ);
        default:
            return completion().effortCostPrDay(resource, start, end);
    }
    return EffortCostMap();
}

// Returns the total planned effort for this task (or subtasks)
Duration Task::plannedEffort(const Resource *resource, long id, EffortCostCalculationType typ) const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->plannedEffort(resource, id, typ);
        }
        return eff;
    }
    Schedule *s = schedule(id);
    if (s) {
        eff = s->plannedEffort(resource, typ);
    }
    return eff;
}

// Returns the total planned effort for this task (or subtasks)
Duration Task::plannedEffort(long id, EffortCostCalculationType typ) const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->plannedEffort(id, typ);
        }
        return eff;
    }
    Schedule *s = schedule(id);
    if (s) {
        eff = s->plannedEffort(typ);
    }
    return eff;
}

// Returns the total planned effort for this task (or subtasks) on date
Duration Task::plannedEffort(const Resource *resource, QDate date, long id, EffortCostCalculationType typ) const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->plannedEffort(resource, date, id, typ);
        }
        return eff;
    }
    Schedule *s = schedule(id);
    if (s) {
        eff = s->plannedEffort(resource, date, typ);
    }
    return eff;
}

// Returns the total planned effort for this task (or subtasks) on date
Duration Task::plannedEffort(QDate date, long id, EffortCostCalculationType typ) const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->plannedEffort(date, id, typ);
        }
        return eff;
    }
    Schedule *s = schedule(id);
    if (s) {
        eff = s->plannedEffort(date, typ);
    }
    return eff;
}

// Returns the total planned effort for this task (or subtasks) upto and including date
Duration Task::plannedEffortTo(QDate date, long id, EffortCostCalculationType typ) const {
    //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->plannedEffortTo(date, id, typ);
        }
        return eff;
    }
    Schedule *s = schedule(id);
    if (s) {
        eff = s->plannedEffortTo(date, typ);
    }
    return eff;
}

// Returns the total planned effort for this task (or subtasks) upto and including date
Duration Task::plannedEffortTo(const Resource *resource, QDate date, long id, EffortCostCalculationType typ) const {
    //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->plannedEffortTo(resource, date, id, typ);
        }
        return eff;
    }
    Schedule *s = schedule(id);
    if (s) {
        eff = s->plannedEffortTo(resource, date, typ);
    }
    return eff;
}

// Returns the total actual effort for this task (or subtasks)
Duration Task::actualEffort() const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->actualEffort();
        }
    }
    return completion().actualEffort();
}

// Returns the total actual effort for this task (or subtasks) on date
Duration Task::actualEffort(QDate date) const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->actualEffort(date);
        }
        return eff;
    }
    return completion().actualEffort(date);
}

// Returns the total actual effort for this task (or subtasks) to date
Duration Task::actualEffortTo(QDate date) const {
   //debugPlan;
    Duration eff;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            eff += n->actualEffortTo(date);
        }
        return eff;
    }
    return completion().actualEffortTo(date);
}

EffortCost Task::plannedCost(long id, EffortCostCalculationType typ) const {
    //debugPlan;
    if (type() == Node::Type_Summarytask) {
        return Node::plannedCost(id, typ);
    }
    EffortCost c;
    Schedule *s = schedule(id);
    if (s) {
        c = s->plannedCost(typ);
    }
    return c;
}

double Task::plannedCostTo(QDate date, long id, EffortCostCalculationType typ) const {
    //debugPlan;
    double c = 0;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            c += n->plannedCostTo(date, id, typ);
        }
        return c;
    }
    Schedule *s = schedule(id);
    if (s == 0) {
        return c;
    }
    c = s->plannedCostTo(date, typ);
    if (date >= s->startTime.date()) {
        c += m_startupCost;
    }
    if (date >= s->endTime.date()) {
        c += m_shutdownCost;
    }
    return c;
}

EffortCost Task::actualCostTo(long int id, QDate date) const {
    //debugPlan;
    EffortCostMap ecm = acwp(id);
    return EffortCost(ecm.effortTo(date), ecm.costTo(date));
}

double Task::bcws(QDate date, long id) const
{
    //debugPlan;
    double c = plannedCostTo(date, id);
    //debugPlan<<c;
    return c;
}

EffortCostMap Task::bcwsPrDay(long int id, EffortCostCalculationType typ)
{
    //debugPlan;
    if (type() == Node::Type_Summarytask) {
        return Node::bcwsPrDay(id);
    }
    Schedule *s = schedule(id);
    if (s == 0) {
        return EffortCostMap();
    }
    EffortCostCache &cache = s->bcwsPrDayCache(typ);
    if (! cache.cached) {
        EffortCostMap ec = s->bcwsPrDay(typ);
        if (typ != ECCT_Work) {
            if (m_startupCost > 0.0) {
                ec.add(s->startTime.date(), Duration::zeroDuration, m_startupCost);
            }
            if (m_shutdownCost > 0.0) {
                ec.add(s->endTime.date(), Duration::zeroDuration, m_shutdownCost);
            }
            cache.effortcostmap = ec;
            cache.cached = true;
        }
    }
    return cache.effortcostmap;
}

EffortCostMap Task::bcwpPrDay(long int id, EffortCostCalculationType typ)
{
    //debugPlan;
    if (type() == Node::Type_Summarytask) {
        return Node::bcwpPrDay(id, typ);
    }
    Schedule *s = schedule(id);
    if (s == 0) {
        return EffortCostMap();
    }
    EffortCostCache cache = s->bcwpPrDayCache(typ);
    if (! cache.cached) {
        // do not use bcws cache, it includes startup/shutdown cost
        EffortCostMap e = s->plannedEffortCostPrDay(s->appointmentStartTime().date(), s->appointmentEndTime().date(), typ);
        if (completion().isStarted() && ! e.isEmpty()) {
            // calculate bcwp on bases of bcws *without* startup/shutdown cost
            double totEff = e.totalEffort().toDouble(Duration::Unit_h);
            double totCost = e.totalCost();
            QDate sd = completion().entries().keys().value(0);
            if (! sd.isValid() || e.startDate() < sd) {
                sd = e.startDate();
            }
            QDate ed = qMax(e.endDate(), completion().entryDate());
            for (QDate d = sd; d <= ed; d = d.addDays(1)) {
                double p = (double)(completion().percentFinished(d)) / 100.0;
                EffortCost ec = e.days()[ d ];
                ec.setBcwpEffort(totEff  * p);
                ec.setBcwpCost(totCost  * p);
                e.insert(d, ec);
            }
        }
        if (typ != ECCT_Work) {
            // add bcws startup/shutdown cost
            if (m_startupCost > 0.0) {
                e.add(s->startTime.date(), Duration::zeroDuration, m_startupCost);
            }
            if (m_shutdownCost > 0.0) {
                e.add(s->endTime.date(), Duration::zeroDuration, m_shutdownCost);
            }
            // add bcwp startup/shutdown cost
            if (m_shutdownCost > 0.0 && completion().finishIsValid()) {
                QDate finish = completion().finishTime().date();
                e.addBcwpCost(finish, m_shutdownCost);
                debugPlan<<"addBcwpCost:"<<finish<<m_shutdownCost;
                // bcwp is cumulative so add to all entries after finish (in case task finished early)
                for (EffortCostDayMap::const_iterator it = e.days().constBegin(); it != e.days().constEnd(); ++it) {
                    const QDate date = it.key();
                    if (date > finish) {
                        e.addBcwpCost(date, m_shutdownCost);
                        debugPlan<<"addBcwpCost:"<<date<<m_shutdownCost;
                    }
                }
            }
            if (m_startupCost > 0.0 && completion().startIsValid()) {
                QDate start = completion().startTime().date();
                e.addBcwpCost(start, m_startupCost);
                // bcwp is cumulative so add to all entries after start
                for (EffortCostDayMap::const_iterator it = e.days().constBegin(); it != e.days().constEnd(); ++it) {
                    const QDate date = it.key();
                    if (date > start) {
                        e.addBcwpCost(date, m_startupCost);
                    }
                }
            }
        }
        cache.effortcostmap = e;
        cache.cached = true;
    }
    return cache.effortcostmap;
}

Duration Task::budgetedWorkPerformed(QDate date, long id) const
{
    //debugPlan;
    Duration e;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            e += n->budgetedWorkPerformed(date, id);
        }
        return e;
    }

    e = plannedEffort(id) * (double)completion().percentFinished(date) / 100.0;
    //debugPlan<<m_name<<"("<<id<<")"<<date<<"="<<e.toString();
    return e;
}

double Task::budgetedCostPerformed(QDate date, long id) const
{
    //debugPlan;
    double c = 0.0;
    if (type() == Node::Type_Summarytask) {
        foreach (const Node *n, childNodeIterator()) {
            c += n->budgetedCostPerformed(date, id);
        }
        return c;
    }

    c = plannedCost(id).cost() * (double)completion().percentFinished(date) / 100.0;
    if (completion().isStarted() && date >= completion().startTime().date()) {
        c += m_startupCost;
    }
    if (completion().isFinished() && date >= completion().finishTime().date()) {
        c += m_shutdownCost;
    }
    //debugPlan<<m_name<<"("<<id<<")"<<date<<"="<<e.toString();
    return c;
}

double Task::bcwp(long id) const
{
    return bcwp(QDate::currentDate(), id);
}

double Task::bcwp(QDate date, long id) const
{
    return budgetedCostPerformed(date, id);
}

EffortCostMap Task::acwp(long int id, KPlato::EffortCostCalculationType typ)
{
    if (type() == Node::Type_Summarytask) {
        return Node::acwp(id, typ);
    }
    Schedule *s = schedule(id);
    if (s == 0) {
        return EffortCostMap();
    }
    EffortCostCache ec = s->acwpCache(typ);
    if (! ec.cached) {
        //debugPlan<<m_name<<completion().entrymode();
        EffortCostMap m;
        switch (completion().entrymode()) {
            case Completion::FollowPlan:
                //TODO
                break;
            case Completion::EnterCompleted:
                //hmmm
            default: {
                m = completion().actualEffortCost(id);
                if (completion().isStarted()) {
                    EffortCost e;
                    e.setCost(m_startupCost);
                    m.add(completion().startTime().date(), e);
                }
                if (completion().isFinished()) {
                    EffortCost e;
                    e.setCost(m_shutdownCost);
                    m.add(completion().finishTime().date(), e);
                }
            }
        }
        ec.effortcostmap = m;
        ec.cached = true;
    }
    return ec.effortcostmap;
}

EffortCost Task::acwp(QDate date, long id) const
{
    //debugPlan;
    if (type() == Node::Type_Summarytask) {
        return Node::acwp(date, id);
    }
    EffortCost c;
    c = completion().actualCostTo(id, date);
    if (completion().isStarted() && date >= completion().startTime().date()) {
        c.add(Duration::zeroDuration, m_startupCost);
    }
    if (completion().isFinished() && date >= completion().finishTime().date()) {
        c.add(Duration::zeroDuration, m_shutdownCost);
    }
    return c;
}

double Task::schedulePerformanceIndex(QDate date, long id) const {
    //debugPlan;
    double r = 1.0;
    double s = bcws(date, id);
    double p = bcwp(date, id);
    if (s > 0.0) {
        r = p / s;
    }
    return r;
}

double Task::effortPerformanceIndex(QDate date, long id) const {
    //debugPlan;
    double r = 1.0;
    Duration a, b;
    if (m_estimate->type() == Estimate::Type_Effort) {
        Duration b = budgetedWorkPerformed(date, id);
        if (b == Duration::zeroDuration) {
            return r;
        }
        Duration a = actualEffortTo(date);
        if (b == Duration::zeroDuration) {
            return 1.0;
        }
        r = b.toDouble() / a.toDouble();
    } else if (m_estimate->type() == Estimate::Type_Duration) {
        //TODO
    }
    return r;
}


//FIXME Handle summarytasks
double Task::costPerformanceIndex(long int id, QDate date, bool *error) const
{
    double res = 0.0;
    double ac = actualCostTo(id, date).cost();

    bool e = (ac == 0.0 || completion().percentFinished() == 0);
    if (error) {
        *error = e;
    }
    if (!e) {
        res = (plannedCostTo(date, id) * completion().percentFinished()) / (100 * ac);
    }
    return res;
}

void Task::initiateCalculation(MainSchedule &sch) {
    //debugPlan<<m_name<<" schedule:"<<sch.name()<<" id="<<sch.id();
    m_currentSchedule = createSchedule(&sch);
    m_currentSchedule->initiateCalculation();
    clearProxyRelations();
    Node::initiateCalculation(sch);
    m_calculateForwardRun = false;
    m_calculateBackwardRun = false;
    m_scheduleForwardRun = false;
    m_scheduleBackwardRun = false;
}


void Task::initiateCalculationLists(MainSchedule &sch) {
    //debugPlan<<m_name;
    if (type() == Node::Type_Summarytask) {
        sch.insertSummaryTask(this);
        // propagate my relations to my children and dependent nodes
        foreach (Node *n, m_nodes) {
            if (!dependParentNodes().isEmpty()) {
                n->addParentProxyRelations(dependParentNodes());
            }
            if (!dependChildNodes().isEmpty()) {
                n->addChildProxyRelations(dependChildNodes());
            }
            n->initiateCalculationLists(sch);
        }
    } else {
        if (isEndNode()) {
            sch.insertEndNode(this);
            //debugPlan<<"endnodes append:"<<m_name;
        }
        if (isStartNode()) {
            sch.insertStartNode(this);
            //debugPlan<<"startnodes append:"<<m_name;
        }
        if ((m_constraint == Node::MustStartOn) ||
            (m_constraint == Node::MustFinishOn) ||
            (m_constraint == Node::FixedInterval))
        {
            sch.insertHardConstraint(this);
        }
        else if ((m_constraint == Node::StartNotEarlier) ||
                  (m_constraint == Node::FinishNotLater))
        {
            sch.insertSoftConstraint(this);
        }
    }
}

DateTime Task::calculatePredeccessors(const QList<Relation*> &list_, int use) {
    DateTime time;
    // do them forward
    QMultiMap<int, Relation*> lst;
    for (Relation* r : list_) {
        lst.insert(-r->parent()->priority(), r);
    }
    const QList<Relation*> list = lst.values();
    foreach (Relation *r, list) {
        if (r->parent()->type() == Type_Summarytask) {
            //debugPlan<<"Skip summarytask:"<<it.current()->parent()->name();
            continue; // skip summarytasks
        }
        DateTime t = r->parent()->calculateForward(use); // early finish
        switch (r->type()) {
            case Relation::StartStart:
                // I can't start earlier than my predesseccor
                t = r->parent()->earlyStart() + r->lag();
                break;
            case Relation::FinishFinish: {
                // I can't finish earlier than my predeccessor, so
                // I can't start earlier than it's (earlyfinish+lag)- my duration
                t += r->lag();
                Schedule::OBState obs = m_currentSchedule->allowOverbookingState();
                m_currentSchedule->setAllowOverbookingState(Schedule::OBS_Allow);
#ifndef PLAN_NLOGDEBUG
                m_currentSchedule->logDebug(QStringLiteral("FinishFinish: get duration to calculate early finish"));
#endif
                t -= duration(t, use, true);
                m_currentSchedule->setAllowOverbookingState(obs);
                break;
            }
            default:
                t += r->lag();
                break;
        }
        if (!time.isValid() || t > time)
            time = t;
    }
    //debugPlan<<time.toString()<<""<<m_name<<" calculatePredeccessors() ("<<list.count()<<")";
    return time;
}

DateTime Task::calculateForward(int use)
{
    if (m_calculateForwardRun) {
        return m_currentSchedule->earlyFinish;
    }
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    cs->setCalculationMode(Schedule::CalculateForward);
    //cs->logDebug("calculateForward: earlyStart=" + cs->earlyStart.toString());
    // calculate all predecessors
    if (!dependParentNodes().isEmpty()) {
        DateTime time = calculatePredeccessors(dependParentNodes(), use);
        if (time.isValid() && time > cs->earlyStart) {
            cs->earlyStart = time;
            //cs->logDebug(QString("calculate forward: early start moved to: %1").arg(cs->earlyStart.toString()));
        }
    }
    if (!m_parentProxyRelations.isEmpty()) {
        DateTime time = calculatePredeccessors(m_parentProxyRelations, use);
        if (time.isValid() && time > cs->earlyStart) {
            cs->earlyStart = time;
            //cs->logDebug(QString("calculate forward: early start moved to: %1").arg(cs->earlyStart.toString()));
        }
    }
    m_calculateForwardRun = true;
    //cs->logDebug("calculateForward: earlyStart=" + cs->earlyStart.toString());
    return calculateEarlyFinish(use);
}

DateTime Task::calculateEarlyFinish(int use) {
    //debugPlan<<m_name;
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    if (m_visitedForward) {
        //debugPlan<<earliestStart.toString()<<" +"<<m_durationBackward.toString()<<""<<m_name<<" calculateForward() (visited)";
        return m_earlyFinish;
    }
    bool pert = cs->usePert();
    cs->setCalculationMode(Schedule::CalculateForward);
#ifndef PLAN_NLOGDEBUG
    QTime timer;
    timer.start();
    cs->logDebug(QStringLiteral("Start calculate forward: %1 ").arg(constraintToString(true)));
#endif
    QLocale locale;
    cs->logInfo(i18n("Calculate early finish "));
    //debugPlan<<"------>"<<m_name<<""<<cs->earlyStart;
    if (type() == Node::Type_Task) {
        m_durationForward = m_estimate->value(use, pert);
        switch (constraint()) {
            case Node::ASAP:
            case Node::ALAP:
            {
                //debugPlan<<m_name<<" ASAP/ALAP:"<<cs->earlyStart;
                cs->earlyStart = workTimeAfter(cs->earlyStart);
                m_durationForward = duration(cs->earlyStart, use, false);
                m_earlyFinish = cs->earlyStart + m_durationForward;
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ASAP/ALAP: " + cs->earlyStart.toString() + '+' + m_durationForward.toString() + '=' + m_earlyFinish.toString());
#endif
                if (!cs->allowOverbooking()) {
                    cs->startTime = cs->earlyStart;
                    cs->endTime = m_earlyFinish;
                    makeAppointments();

                    // calculate duration wo checking booking = the earliest finish possible
                    Schedule::OBState obs = cs->allowOverbookingState();
                    cs->setAllowOverbookingState(Schedule::OBS_Allow);
                    m_durationForward = duration(cs->earlyStart, use, false);
                    cs->setAllowOverbookingState(obs);
#ifndef PLAN_NLOGDEBUG
                    cs->logDebug("ASAP/ALAP earliest possible: " + cs->earlyStart.toString() + '+' + m_durationForward.toString() + '=' + (cs->earlyStart+m_durationForward).toString());
#endif
                }
                break;
            }
            case Node::MustFinishOn:
            {
                cs->earlyStart = workTimeAfter(cs->earlyStart);
                m_durationForward = duration(cs->earlyStart, use, false);
                cs->earlyFinish = cs->earlyStart + m_durationForward;
                //debugPlan<<"MustFinishOn:"<<m_constraintEndTime<<cs->earlyStart<<cs->earlyFinish;
                if (cs->earlyFinish > m_constraintEndTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                cs->earlyFinish = qMax(cs->earlyFinish, m_constraintEndTime);
                if (!cs->allowOverbooking()) {
                    cs->endTime = cs->earlyFinish;
                    cs->startTime = cs->earlyFinish - duration(cs->earlyFinish, use, true);
                    makeAppointments();
                }
                m_earlyFinish = cs->earlyFinish;
                m_durationForward = m_earlyFinish - cs->earlyStart;
                break;
            }
            case Node::FinishNotLater:
            {
                m_durationForward = duration(cs->earlyStart, use, false);
                cs->earlyFinish = cs->earlyStart + m_durationForward;
                //debugPlan<<"FinishNotLater:"<<m_constraintEndTime<<cs->earlyStart<<cs->earlyFinish;
                if (cs->earlyFinish > m_constraintEndTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                if (!cs->allowOverbooking()) {
                    cs->startTime = cs->earlyStart;
                    cs->endTime = cs->earlyFinish;
                    makeAppointments();
                }
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
            }
            case Node::MustStartOn:
            case Node::StartNotEarlier:
            {
                //debugPlan<<"MSO/SNE:"<<m_constraintStartTime<<cs->earlyStart;
                cs->logDebug(constraintToString() + ": " + m_constraintStartTime.toString() + ' ' + cs->earlyStart.toString());
                cs->earlyStart = workTimeAfter(qMax(cs->earlyStart, m_constraintStartTime));
                if (cs->earlyStart < m_constraintStartTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                m_durationForward = duration(cs->earlyStart, use, false);
                m_earlyFinish = cs->earlyStart + m_durationForward;
                if (!cs->allowOverbooking()) {
                    cs->startTime = cs->earlyStart;
                    cs->endTime = m_earlyFinish;
                    makeAppointments();

                    // calculate duration wo checking booking = the earliest finish possible
                    Schedule::OBState obs = cs->allowOverbookingState();
                    cs->setAllowOverbookingState(Schedule::OBS_Allow);
                    m_durationForward = duration(cs->startTime, use, false);
                    cs->setAllowOverbookingState(obs);
                    m_earlyFinish = cs->earlyStart + m_durationForward;
#ifndef PLAN_NLOGDEBUG
                    cs->logDebug("MSO/SNE earliest possible: " + cs->earlyStart.toString() + '+' + m_durationForward.toString() + '=' + (cs->earlyStart+m_durationForward).toString());
#endif
                }
                break;
            }
            case Node::FixedInterval: {
                if (cs->earlyStart > m_constraintStartTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                //cs->earlyStart = m_constraintStartTime;
                m_durationForward = m_constraintEndTime - m_constraintStartTime;
                if (cs->earlyStart < m_constraintStartTime) {
                    m_durationForward = m_constraintEndTime - cs->earlyStart;
                }
                if (!cs->allowOverbooking()) {
                    cs->startTime = m_constraintStartTime;
                    cs->endTime = m_constraintEndTime;
                    makeAppointments();
                }
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
            }
        }
    } else if (type() == Node::Type_Milestone) {
        m_durationForward = Duration::zeroDuration;
        switch (constraint()) {
            case Node::MustFinishOn:
                //debugPlan<<"MustFinishOn:"<<m_constraintEndTime<<cs->earlyStart;
                //cs->logDebug(QString("%1: %2, early start: %3").arg(constraintToString()).arg(m_constraintEndTime.toString()).arg(cs->earlyStart.toString()));
                if (cs->earlyStart < m_constraintEndTime) {
                    m_durationForward = m_constraintEndTime - cs->earlyStart;
                }
                if (cs->earlyStart > m_constraintEndTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
            case Node::FinishNotLater:
                //debugPlan<<"FinishNotLater:"<<m_constraintEndTime<<cs->earlyStart;
                if (cs->earlyStart > m_constraintEndTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                m_earlyFinish = cs->earlyStart;
                break;
            case Node::MustStartOn:
                //debugPlan<<"MustStartOn:"<<m_constraintStartTime<<cs->earlyStart;
                if (cs->earlyStart < m_constraintStartTime) {
                    m_durationForward = m_constraintStartTime - cs->earlyStart;
                }
                if (cs->earlyStart > m_constraintStartTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
            case Node::StartNotEarlier:
                //debugPlan<<"StartNotEarlier:"<<m_constraintStartTime<<cs->earlyStart;
                if (cs->earlyStart < m_constraintStartTime) {
                    m_durationForward = m_constraintStartTime - cs->earlyStart;
                }
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
            case Node::FixedInterval:
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
            default:
                m_earlyFinish = cs->earlyStart + m_durationForward;
                break;
        }
        //debugPlan<<m_name<<""<<earliestStart.toString();
    } else if (type() == Node::Type_Summarytask) {
        warnPlan<<"Summarytasks should not be calculated here: "<<m_name;
    } else { // ???
        m_durationForward = Duration::zeroDuration;
    }
    m_visitedForward = true;
    cs->insertForwardNode(this);
    cs->earlyFinish = cs->earlyStart + m_durationForward;
    foreach (const Appointment *a, cs->appointments(Schedule::CalculateForward)) {
        cs->logInfo(i18n("Resource %1 booked from %2 to %3", a->resource()->resource()->name(), locale.toString(a->startTime(), QLocale::ShortFormat), locale.toString(a->endTime(), QLocale::ShortFormat)));
    }
    // clean up temporary usage
    cs->startTime = DateTime();
    cs->endTime = DateTime();
    cs->duration = Duration::zeroDuration;
    cs->logInfo(i18n("Early finish calculated: %1", locale.toString(cs->earlyFinish, QLocale::ShortFormat)));
    cs->incProgress();
#ifndef PLAN_NLOGDEBUG
    cs->logDebug(QStringLiteral("Finished calculate forward: %1 ms").arg(timer.elapsed()));
#endif
    return m_earlyFinish;
}

DateTime Task::calculateSuccessors(const QList<Relation*> &list_, int use) {
    DateTime time;
    QMultiMap<int, Relation*> lst;
    for (Relation* r : list_) {
        lst.insert(-r->child()->priority(), r);
    }
    const QList<Relation*> list = lst.values();
    foreach (Relation *r, list) {
        if (r->child()->type() == Type_Summarytask) {
            //debugPlan<<"Skip summarytask:"<<r->parent()->name();
            continue; // skip summarytasks
        }
        DateTime t = r->child()->calculateBackward(use);
        switch (r->type()) {
            case Relation::StartStart: {
                // I must start before my successor, so
                // I can't finish later than it's (starttime-lag) + my duration
                t -= r->lag();
                Schedule::OBState obs = m_currentSchedule->allowOverbookingState();
                m_currentSchedule->setAllowOverbookingState(Schedule::OBS_Allow);
#ifndef PLAN_NLOGDEBUG
                m_currentSchedule->logDebug(QStringLiteral("StartStart: get duration to calculate late start"));
#endif
                t += duration(t, use, false);
                m_currentSchedule->setAllowOverbookingState(obs);
                break;
            }
            case Relation::FinishFinish:
                // My successor cannot finish before me, so
                // I can't finish later than it's latest finish - lag
                t = r->child()->lateFinish() -  r->lag();
                break;
            default:
                t -= r->lag();
                break;
        }
        if (!time.isValid() || t < time)
            time = t;
    }
    //debugPlan<<time.toString()<<""<<m_name<<" calculateSuccessors() ("<<list.count()<<")";
    return time;
}

DateTime Task::calculateBackward(int use) {
    //debugPlan<<m_name;
    if (m_calculateBackwardRun) {
        return m_currentSchedule->lateStart;
    }
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    cs->setCalculationMode(Schedule::CalculateBackward);
    //cs->lateFinish = projectNode()->constraintEndTime();
    // calculate all successors
    if (!dependChildNodes().isEmpty()) {
        DateTime time = calculateSuccessors(dependChildNodes(), use);
        if (time.isValid() && time < cs->lateFinish) {
            cs->lateFinish = time;
        }
    }
    if (!m_childProxyRelations.isEmpty()) {
        DateTime time = calculateSuccessors(m_childProxyRelations, use);
        if (time.isValid() && time < cs->lateFinish) {
            cs->lateFinish = time;
        }
    }
    m_calculateBackwardRun = true;
    return calculateLateStart(use);
}

DateTime Task::calculateLateStart(int use) {
    //debugPlan<<m_name;
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    if (m_visitedBackward) {
        //debugPlan<<latestFinish.toString()<<" -"<<m_durationBackward.toString()<<""<<m_name<<" calculateBackward() (visited)";
        return cs->lateStart;
    }
    bool pert = cs->usePert();
    cs->setCalculationMode(Schedule::CalculateBackward);
#ifndef PLAN_NLOGDEBUG
    QTime timer;
    timer.start();
    cs->logDebug(QStringLiteral("Start calculate backward: %1 ").arg(constraintToString(true)));
#endif
    QLocale locale;
    cs->logInfo(i18n("Calculate late start"));
    cs->logDebug(QStringLiteral("%1: late finish= %2").arg(constraintToString()).arg(cs->lateFinish.toString()));
    //debugPlan<<m_name<<" id="<<cs->id()<<" mode="<<cs->calculationMode()<<": latestFinish="<<cs->lateFinish;
    if (type() == Node::Type_Task) {
        m_durationBackward = m_estimate->value(use, pert);
        switch (constraint()) {
            case Node::ASAP:
            case Node::ALAP:
                //debugPlan<<m_name<<" ASAP/ALAP:"<<cs->lateFinish;
                cs->lateFinish = workTimeBefore(cs->lateFinish);
                m_durationBackward = duration(cs->lateFinish, use, true);
                cs->lateStart = cs->lateFinish - m_durationBackward;
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ASAP/ALAP: " + cs->lateFinish.toString() + '-' + m_durationBackward.toString() + '=' + cs->lateStart.toString());
#endif
                if (!cs->allowOverbooking()) {
                    cs->startTime = cs->lateStart;
                    cs->endTime = cs->lateFinish;
                    makeAppointments();

                    // calculate wo checking bookings = latest start possible
                    Schedule::OBState obs = cs->allowOverbookingState();
                    cs->setAllowOverbookingState(Schedule::OBS_Allow);
                    m_durationBackward = duration(cs->lateFinish, use, true);
                    cs->setAllowOverbookingState(obs);
#ifndef PLAN_NLOGDEBUG
                    cs->logDebug("ASAP/ALAP latest start possible: " + cs->lateFinish.toString() + '-' + m_durationBackward.toString() + '=' + (cs->lateFinish-m_durationBackward).toString());
#endif
                }
                break;
            case Node::MustStartOn:
            case Node::StartNotEarlier:
            {
                //debugPlan<<"MustStartOn:"<<m_constraintStartTime<<cs->lateFinish;
                cs->lateFinish = workTimeBefore(cs->lateFinish);
                m_durationBackward = duration(cs->lateFinish, use, true);
                cs->lateStart = cs->lateFinish - m_durationBackward;
                if (cs->lateStart < m_constraintStartTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                } else {
                    cs->lateStart = qMax(cs->earlyStart, m_constraintStartTime);
                }
                if (!cs->allowOverbooking()) {
                    if (constraint() == MustStartOn) {
                        cs->startTime = m_constraintStartTime;
                        cs->endTime = m_constraintStartTime + duration(m_constraintStartTime, use, false);
                    } else {
                        cs->startTime = qMax(cs->lateStart, m_constraintStartTime);
                        cs->endTime = qMax(cs->lateFinish, cs->startTime); // safety
                    }
                    makeAppointments();
                }
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
            }
            case Node::MustFinishOn:
            case Node::FinishNotLater:
                //debugPlan<<"MustFinishOn:"<<m_constraintEndTime<<cs->lateFinish;
                cs->lateFinish = workTimeBefore(cs->lateFinish);
                cs->endTime = cs->lateFinish;
                if (cs->lateFinish < m_constraintEndTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                } else {
                    cs->endTime = qMax(cs->earlyFinish, m_constraintEndTime);
                }
                m_durationBackward = duration(cs->endTime, use, true);
                cs->startTime = cs->endTime - m_durationBackward;
                if (!cs->allowOverbooking()) {
                    makeAppointments();
                }
                m_durationBackward = cs->lateFinish - cs->startTime;
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
            case Node::FixedInterval: {
                //cs->lateFinish = m_constraintEndTime;
                if (cs->lateFinish < m_constraintEndTime) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                m_durationBackward = m_constraintEndTime - m_constraintStartTime;
                if (cs->lateFinish > m_constraintEndTime) {
                    m_durationBackward = cs->lateFinish - m_constraintStartTime;
                }
                if (!cs->allowOverbooking()) {
                    cs->startTime = m_constraintStartTime;
                    cs->endTime = m_constraintEndTime;
                    makeAppointments();
                }
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
            }
        }
    } else if (type() == Node::Type_Milestone) {
        m_durationBackward = Duration::zeroDuration;
        switch (constraint()) {
            case Node::MustFinishOn:
                //debugPlan<<"MustFinishOn:"<<m_constraintEndTime<<cs->lateFinish;
                if (m_constraintEndTime < cs->lateFinish) {
                    m_durationBackward = cs->lateFinish - m_constraintEndTime;
                } else if (m_constraintEndTime > cs->lateFinish) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
            case Node::FinishNotLater:
                //debugPlan<<"FinishNotLater:"<<m_constraintEndTime<<cs->lateFinish;
                if (m_constraintEndTime < cs->lateFinish) {
                    m_durationBackward = cs->lateFinish - m_constraintEndTime;
                } else if (m_constraintEndTime > cs->lateFinish) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
            case Node::MustStartOn:
                //debugPlan<<"MustStartOn:"<<m_constraintStartTime<<cs->lateFinish;
                if (m_constraintStartTime < cs->lateFinish) {
                    m_durationBackward = cs->lateFinish - m_constraintStartTime;
                } else if (m_constraintStartTime > cs->lateFinish) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                cs->lateStart = cs->lateFinish - m_durationBackward;
                //cs->logDebug(QString("%1: constraint:%2, start=%3, finish=%4").arg(constraintToString()).arg(m_constraintStartTime.toString()).arg(cs->lateStart.toString()).arg(cs->lateFinish.toString()));
                break;
            case Node::StartNotEarlier:
                //debugPlan<<"MustStartOn:"<<m_constraintStartTime<<cs->lateFinish;
                if (m_constraintStartTime > cs->lateFinish) {
                    cs->logWarning(i18nc("1=type of constraint", "%1: Failed to meet constraint", constraintToString(true)));
                }
                cs->lateStart = cs->lateFinish;
                break;
            case Node::FixedInterval:
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
            default:
                cs->lateStart = cs->lateFinish - m_durationBackward;
                break;
        }
        //debugPlan<<m_name<<""<<cs->lateFinish;
    } else if (type() == Node::Type_Summarytask) {
        warnPlan<<"Summarytasks should not be calculated here: "<<m_name;
    } else { // ???
        m_durationBackward = Duration::zeroDuration;
    }
    m_visitedBackward = true;
    cs->insertBackwardNode(this);
    cs->lateStart = cs->lateFinish - m_durationBackward;
    foreach (const Appointment *a, cs->appointments(Schedule::CalculateBackward)) {
        cs->logInfo(i18n("Resource %1 booked from %2 to %3", a->resource()->resource()->name(), locale.toString(a->startTime(), QLocale::ShortFormat), locale.toString(a->endTime(), QLocale::ShortFormat)));
    }
    // clean up temporary usage
    cs->startTime = DateTime();
    cs->endTime = DateTime();
    cs->duration = Duration::zeroDuration;
    cs->logInfo(i18n("Late start calculated: %1", locale.toString(cs->lateStart, QLocale::ShortFormat)));
    cs->incProgress();
#ifndef PLAN_NLOGDEBUG
    cs->logDebug(QStringLiteral("Finished calculate backward: %1 ms").arg(timer.elapsed()));
#endif
    return cs->lateStart;
}

DateTime Task::schedulePredeccessors(const QList<Relation*> &list_, int use) {
    DateTime time;
    QMultiMap<int, Relation*> lst;
    for (Relation* r : list_) {
        lst.insert(-r->parent()->priority(), r);
    }
    const QList<Relation*> list = lst.values();
    foreach (Relation *r, list) {
        if (r->parent()->type() == Type_Summarytask) {
            //debugPlan<<"Skip summarytask:"<<r->parent()->name();
            continue; // skip summarytasks
        }
        // schedule the predecessors
        DateTime earliest = r->parent()->earlyStart();
        DateTime t = r->parent()->scheduleForward(earliest, use);
        switch (r->type()) {
            case Relation::StartStart:
                // I can't start before my predesseccor
                t = r->parent()->startTime() + r->lag();
                break;
            case Relation::FinishFinish:
                // I can't end before my predecessor, so
                // I can't start before it's endtime - my duration
#ifndef PLAN_NLOGDEBUG
                m_currentSchedule->logDebug(QStringLiteral("FinishFinish: get duration to calculate earliest start"));
#endif
                t -= duration(t + r->lag(), use, true);
                break;
            default:
                t += r->lag();
                break;
        }
        if (!time.isValid() || t > time)
            time = t;
    }
    //debugPlan<<time.toString()<<""<<m_name<<" schedulePredeccessors()";
    return time;
}

DateTime Task::scheduleForward(const DateTime &earliest, int use) {
    if (m_scheduleForwardRun) {
        return m_currentSchedule->endTime;
    }
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    //cs->logDebug(QString("Schedule forward (early start: %1)").arg(cs->earlyStart.toString()));
    cs->setCalculationMode(Schedule::Scheduling);
    DateTime startTime = earliest > cs->earlyStart ? earliest : cs->earlyStart;
    // First, calculate all my own predecessors
    DateTime time = schedulePredeccessors(dependParentNodes(), use);
    if (time > startTime) {
        startTime = time;
        //debugPlan<<m_name<<" new startime="<<cs->startTime;
    }
    // Then my parents
    time = schedulePredeccessors(m_parentProxyRelations, use);
    if (time > startTime) {
        startTime = time;
    }
    if (! m_visitedForward) {
        cs->startTime = startTime;
    }
    m_scheduleForwardRun = true;
    return scheduleFromStartTime(use);
}

DateTime Task::scheduleFromStartTime(int use) {
    //debugPlan<<m_name;
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    cs->setCalculationMode(Schedule::Scheduling);
    bool pert = cs->usePert();
    if (m_visitedForward) {
        return cs->endTime;
    }
    cs->notScheduled = false;
    if (!cs->startTime.isValid()) {
        //cs->logDebug(QString("Schedule from start time: no start time use early start: %1").arg(cs->earlyStart.toString()));
        cs->startTime = cs->earlyStart;
    }
    QTime timer;
    timer.start();
    cs->logInfo(i18n("Start schedule forward: %1 ", constraintToString(true)));
    QLocale locale;
    cs->logInfo(i18n("Schedule from start %1", locale.toString(cs->startTime, QLocale::ShortFormat)));
    //debugPlan<<m_name<<" startTime="<<cs->startTime;
    if(type() == Node::Type_Task) {
        if (cs->recalculate() && completion().isFinished()) {
            copySchedule();
            m_visitedForward = true;
            return cs->endTime;
        }
        cs->duration = m_estimate->value(use, pert);
        switch (m_constraint) {
        case Node::ASAP:
            // cs->startTime calculated above
            //debugPlan<<m_name<<"ASAP:"<<cs->startTime<<"earliest:"<<cs->earlyStart;
            if (false/*useCalculateForwardAppointments*/
                    && m_estimate->type() == Estimate::Type_Effort
                    && ! cs->allowOverbooking()
                    && cs->hasAppointments(Schedule::CalculateForward)
                ) {
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ASAP: " + cs->startTime.toString() + " earliest: " + cs->earlyStart.toString());
#endif
                cs->copyAppointments(Schedule::CalculateForward, Schedule::Scheduling);
                if (cs->recalculate() && completion().isStarted()) {
                    // copy start times + appointments from parent schedule
                    copyAppointments();
                }
                cs->startTime = cs->appointmentStartTime();
                cs->endTime = cs->appointmentEndTime();
                Q_ASSERT(cs->startTime.isValid());
                Q_ASSERT(cs->endTime.isValid());
                cs->duration = cs->endTime - cs->startTime;
                if (cs->lateFinish > cs->endTime) {
                    cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
                } else {
                    cs->positiveFloat = Duration::zeroDuration;
                }
                cs->logInfo(i18n("Scheduled: %1 to %2", locale.toString(cs->startTime, QLocale::ShortFormat), locale.toString(cs->endTime, QLocale::ShortFormat)));
                return cs->endTime;
            }
            cs->startTime = workTimeAfter(cs->startTime, cs);
#ifndef PLAN_NLOGDEBUG
            cs->logDebug("ASAP: " + cs->startTime.toString() + " earliest: " + cs->earlyStart.toString());
#endif
            cs->duration = duration(cs->startTime, use, false);
            cs->endTime = cs->startTime + cs->duration;
            makeAppointments();
            if (cs->recalculate() && completion().isStarted()) {
                // copy start times + appointments from parent schedule
                copyAppointments();
                cs->duration = cs->endTime - cs->startTime;
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            break;
        case Node::ALAP:
            // cs->startTime calculated above
            //debugPlan<<m_name<<"ALAP:"<<cs->startTime<<cs->endTime<<" latest="<<cs->lateFinish;
            cs->endTime = workTimeBefore(cs->lateFinish, cs);
            cs->duration = duration(cs->endTime, use, true);
            cs->startTime = cs->endTime - cs->duration;
            //debugPlan<<m_name<<" endTime="<<cs->endTime<<" latest="<<cs->lateFinish;
            makeAppointments();
            if (cs->plannedEffort() == 0 && cs->lateFinish < cs->earlyFinish) {
                // the backward pass failed to calculate sane values, try to handle it
                //TODO add an error indication
                cs->logWarning(i18n("%1: Scheduling failed using late finish, trying early finish instead.", constraintToString()));
                cs->endTime = workTimeBefore(cs->earlyFinish, cs);
                cs->duration = duration(cs->endTime, use, true);
                cs->startTime = cs->endTime - cs->duration;
                makeAppointments();
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            if (cs->recalculate() && completion().isStarted()) {
                cs->earlyStart = cs->startTime = completion().startTime();
            }
            break;
        case Node::StartNotEarlier:
            // cs->startTime calculated above
            //debugPlan<<"StartNotEarlier:"<<m_constraintStartTime<<cs->startTime<<cs->lateStart;
            cs->startTime = workTimeAfter(qMax(cs->startTime, m_constraintStartTime), cs);
            cs->duration = duration(cs->startTime, use, false);
            cs->endTime = cs->startTime + cs->duration;
            makeAppointments();
            if (cs->recalculate() && completion().isStarted()) {
                // copy start times + appointments from parent schedule
                copyAppointments();
                cs->duration = cs->endTime - cs->startTime;
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            if (cs->startTime < m_constraintStartTime) {
                cs->constraintError = true;
                cs->negativeFloat = cs->startTime - m_constraintStartTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            break;
        case Node::FinishNotLater:
            // cs->startTime calculated above
            //debugPlan<<"FinishNotLater:"<<m_constraintEndTime<<cs->startTime;
            cs->startTime = workTimeAfter(cs->startTime, cs);
            cs->duration = duration(cs->startTime, use, false);
            cs->endTime = cs->startTime + cs->duration;
            makeAppointments();
            if (cs->recalculate() && completion().isStarted()) {
                // copy start times + appointments from parent schedule
                copyAppointments();
                cs->duration = cs->endTime - cs->startTime;
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            if (cs->endTime > m_constraintEndTime) {
                //warnPlan<<"cs->endTime > m_constraintEndTime";
                cs->constraintError = true;
                cs->negativeFloat = cs->endTime - m_constraintEndTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            break;
        case Node::MustStartOn:
            // Always try to put it on time
            cs->startTime = workTimeAfter(m_constraintStartTime, cs);
            //debugPlan<<"MustStartOn="<<m_constraintStartTime<<"<"<<cs->startTime;
            cs->duration = duration(cs->startTime, use, false);
            cs->endTime = cs->startTime + cs->duration;
#ifndef PLAN_NLOGDEBUG
            cs->logDebug(QStringLiteral("%1: Schedule from %2 to %3").arg(constraintToString()).arg(cs->startTime.toString()).arg(cs->endTime.toString()));
#endif
            makeAppointments();
            if (cs->recalculate() && completion().isStarted()) {
                // copy start times + appointments from parent schedule
                copyAppointments();
                cs->duration = cs->endTime - cs->startTime;
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            if (m_constraintStartTime < cs->startTime) {
                cs->constraintError = true;
                cs->negativeFloat = cs->startTime - m_constraintStartTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            break;
        case Node::MustFinishOn:
            // Just try to schedule on time
            cs->endTime = workTimeBefore(m_constraintEndTime, cs);
            cs->duration = duration(cs->endTime, use, true);
            cs->startTime = cs->endTime - cs->duration;

            //debugPlan<<"MustFinishOn:"<<m_constraintEndTime<<","<<cs->lateFinish<<":"<<cs->startTime<<cs->endTime;
            makeAppointments();
            if (cs->recalculate() && completion().isStarted()) {
                // copy start times + appointments from parent schedule
                copyAppointments();
                cs->duration = cs->endTime - cs->startTime;
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            if (cs->endTime != m_constraintEndTime) {
                cs->constraintError = true;
                cs->negativeFloat = cs->endTime - m_constraintEndTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            break;
        case Node::FixedInterval: {
            // cs->startTime calculated above
            //debugPlan<<"FixedInterval="<<m_constraintStartTime<<""<<cs->startTime;
            cs->duration = m_constraintEndTime - m_constraintStartTime;
            if (m_constraintStartTime >= cs->earlyStart) {
                cs->startTime = m_constraintStartTime;
                cs->endTime = m_constraintEndTime;
            } else {
                cs->startTime = cs->earlyStart;
                cs->endTime = cs->startTime + cs->duration;
                cs->constraintError = true;
            }
            if (m_constraintStartTime < cs->startTime) {
                cs->constraintError = true;
                cs->negativeFloat = cs->startTime - m_constraintStartTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            } else {
                cs->positiveFloat = Duration::zeroDuration;
            }
            cs->workStartTime = m_constraintStartTime;
            cs->workEndTime = m_constraintEndTime;
            //debugPlan<<"FixedInterval="<<cs->startTime<<","<<cs->endTime;
            makeAppointments();
            break;
        }
        default:
            break;
        }
        if (m_estimate->type() == Estimate::Type_Effort) {
            // HACK scheduling may accept deviation less than 5 mins to improve performance
            cs->effortNotMet = (m_estimate->value(use, cs->usePert()) - cs->plannedEffort()) > (5 * 60000);
            if (cs->effortNotMet) {
                cs->logError(i18n("Effort not met. Estimate: %1, planned: %2", estimate()->value(use, cs->usePert()).toHours(), cs->plannedEffort().toHours()));
            }
        }
    } else if (type() == Node::Type_Milestone) {
        if (cs->recalculate() && completion().isFinished()) {
            cs->startTime = completion().startTime();
            cs->endTime = completion().finishTime();
            m_visitedForward = true;
            return cs->endTime;
        }
        switch (m_constraint) {
        case Node::ASAP: {
            cs->endTime = cs->startTime;
            // TODO check, do we need to check successors earliestStart?
            cs->positiveFloat = cs->lateFinish - cs->endTime;
            break;
        }
        case Node::ALAP: {
            cs->startTime = qMax(cs->lateFinish, cs->earlyFinish);
            cs->endTime = cs->startTime;
            cs->positiveFloat = Duration::zeroDuration;
            break;
        }
        case Node::MustStartOn:
        case Node::MustFinishOn:
        case Node::FixedInterval: {
            //debugPlan<<"MustStartOn:"<<m_constraintStartTime<<cs->startTime;
            DateTime contime = m_constraint == Node::MustFinishOn ? m_constraintEndTime : m_constraintStartTime;
#ifndef PLAN_NLOGDEBUG
            cs->logDebug(QStringLiteral("%1: constraint time=%2, start time=%3").arg(constraintToString()).arg(contime.toString()).arg(cs->startTime.toString()));
#endif
            if (cs->startTime < contime) {
                if (contime <= cs->lateFinish || contime <= cs->earlyFinish) {
                    cs->startTime = contime;
                }
            }
            cs->negativeFloat = cs->startTime > contime ? cs->startTime - contime :  contime - cs->startTime;
            if (cs->negativeFloat != 0) {
                cs->constraintError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            cs->endTime = cs->startTime;
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            break;
        }
        case Node::StartNotEarlier:
            if (cs->startTime < m_constraintStartTime) {
                if (m_constraintStartTime <= cs->lateFinish || m_constraintStartTime <= cs->earlyFinish) {
                    cs->startTime = m_constraintStartTime;
                }
            }
            if (cs->startTime < m_constraintStartTime) {
                cs->constraintError = true;
                cs->negativeFloat = m_constraintStartTime - cs->startTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            cs->endTime = cs->startTime;
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            break;
        case Node::FinishNotLater:
            //debugPlan<<m_constraintEndTime<<cs->startTime;
            if (cs->startTime > m_constraintEndTime) {
                cs->constraintError = true;
                cs->negativeFloat = cs->startTime - m_constraintEndTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            cs->endTime = cs->startTime;
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            break;
        default:
            break;
        }
        cs->duration = Duration::zeroDuration;
        //debugPlan<<m_name<<":"<<cs->startTime<<","<<cs->endTime;
    } else if (type() == Node::Type_Summarytask) {
        //shouldn't come here
        cs->endTime = cs->startTime;
        cs->duration = cs->endTime - cs->startTime;
        warnPlan<<"Summarytasks should not be calculated here: "<<m_name;
    }
    //debugPlan<<cs->startTime<<" :"<<cs->endTime<<""<<m_name<<" scheduleForward()";
    if (cs->startTime < projectNode()->constraintStartTime() || cs->endTime > projectNode()->constraintEndTime()) {
        cs->logError(i18n("Failed to schedule within project target time"));
    }
    foreach (const Appointment *a, cs->appointments()) {
        cs->logInfo(i18n("Resource %1 booked from %2 to %3", a->resource()->resource()->name(), locale.toString(a->startTime(), QLocale::ShortFormat), locale.toString(a->endTime(), QLocale::ShortFormat)));
    }
    if (cs->startTime < cs->earlyStart) {
        cs->logWarning(i18n("Starting earlier than early start"));
    }
    if (cs->endTime > cs->lateFinish) {
        cs->logWarning(i18n("Finishing later than late finish"));
    }
    cs->logInfo(i18n("Scheduled: %1 to %2", locale.toString(cs->startTime, QLocale::ShortFormat), locale.toString(cs->endTime, QLocale::ShortFormat)));
    m_visitedForward = true;
    cs->incProgress();
    m_requests.resetDynamicAllocations();
    cs->logInfo(i18n("Finished schedule forward: %1 ms", timer.elapsed()));
    return cs->endTime;
}

DateTime Task::scheduleSuccessors(const QList<Relation*> &list_, int use) {
    DateTime time;
    QMultiMap<int, Relation*> lst;
    for (Relation* r : list_) {
        lst.insert(-r->child()->priority(), r);
    }
    const QList<Relation*> list = lst.values();
    foreach (Relation *r, list) {
        if (r->child()->type() == Type_Summarytask) {
            //debugPlan<<"Skip summarytask:"<<r->child()->name();
            continue;
        }
        // get the successors starttime
        DateTime latest = r->child()->lateFinish();
        DateTime t = r->child()->scheduleBackward(latest, use);
        switch (r->type()) {
            case Relation::StartStart:
                // I can't start before my successor, so
                // I can't finish later than it's starttime + my duration
#ifndef PLAN_NLOGDEBUG
                m_currentSchedule->logDebug(QStringLiteral("StartStart: get duration to calculate late finish"));
#endif
                t += duration(t - r->lag(), use, false);
                break;
            case Relation::FinishFinish:
                t = r->child()->endTime() - r->lag();
                break;
            default:
                t -= r->lag();
                break;
        }
        if (!time.isValid() || t < time)
            time = t;
   }
   return time;
}

DateTime Task::scheduleBackward(const DateTime &latest, int use) {
    if (m_scheduleBackwardRun) {
        return m_currentSchedule->startTime;
    }
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    cs->setCalculationMode(Schedule::Scheduling);

    DateTime endTime = latest < cs->lateFinish ? latest : cs->lateFinish;
    // First, calculate all my own successors
    DateTime time = scheduleSuccessors(dependChildNodes(), use);
    if (time.isValid() && time < endTime) {
        endTime = time;
    }
    // Then my parents
    time = scheduleSuccessors(m_childProxyRelations, use);
    if (time.isValid() && time < endTime) {
        endTime = time;
    }
    if (! m_visitedBackward) {
        cs->endTime = endTime;
    }
    m_scheduleBackwardRun = true;
    return scheduleFromEndTime(use);
}

DateTime Task::scheduleFromEndTime(int use) {
    //debugPlan<<m_name;
    if (m_currentSchedule == 0) {
        return DateTime();
    }
    Schedule *cs = m_currentSchedule;
    cs->setCalculationMode(Schedule::Scheduling);
    bool pert = cs->usePert();
    if (m_visitedBackward) {
        return cs->startTime;
    }
    cs->notScheduled = false;
    if (!cs->endTime.isValid()) {
        cs->endTime = cs->lateFinish;
    }
#ifndef PLAN_NLOGDEBUG
    QTime timer;
    timer.start();
    cs->logDebug(QStringLiteral("Start schedule backward: %1 ").arg(constraintToString(true)));
#endif
    QLocale locale;
    cs->logInfo(i18n("Schedule from end time: %1", cs->endTime.toString()));
    if (type() == Node::Type_Task) {
        cs->duration = m_estimate->value(use, pert);
        switch (m_constraint) {
        case Node::ASAP: {
            // cs->endTime calculated above
            //debugPlan<<m_name<<": end="<<cs->endTime<<"  early="<<cs->earlyStart;
            //TODO: try to keep within projects constraint times
            cs->endTime = workTimeBefore(cs->endTime, cs);
            cs->startTime = workTimeAfter(cs->earlyStart, cs);
            DateTime e;
            if (cs->startTime < cs->endTime) {
                cs->duration = duration(cs->startTime, use, false);
                e = cs->startTime + cs->duration;
            } else {
#ifndef PLAN_NLOGDEBUG
                cs->logDebug(QStringLiteral("%1: Latest allowed end time earlier than early start").arg(constraintToString()));
#endif
                cs->duration = duration(cs->endTime, use, true);
                e = cs->endTime;
                cs->startTime = e - cs->duration;
            }
            if (e > cs->lateFinish) {
                cs->schedulingError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to schedule within late finish.", constraintToString()));
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ASAP: late finish=" + cs->lateFinish.toString() + " end time=" + e.toString());
#endif
            } else if (e > cs->endTime) {
                cs->schedulingError = true;
                cs->logWarning(i18nc("1=type of constraint", "%1: Failed to schedule within successors start time",  constraintToString()));
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ASAP: succ. start=" + cs->endTime.toString() + " end time=" + e.toString());
#endif
            }
            if (cs->lateFinish > e) {
                DateTime w = workTimeBefore(cs->lateFinish);
                if (w > e) {
                    cs->positiveFloat = w - e;
                }
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ASAP: positiveFloat=" + cs->positiveFloat.toString());
#endif
            }
            cs->endTime = e;
            makeAppointments();
            break;
        }
        case Node::ALAP:
        {
            // cs->endTime calculated above
            //debugPlan<<m_name<<": end="<<cs->endTime<<"  late="<<cs->lateFinish;
            cs->endTime = workTimeBefore(cs->endTime, cs);
            cs->duration = duration(cs->endTime, use, true);
            cs->startTime = cs->endTime - cs->duration;
            if (cs->startTime < cs->earlyStart) {
                cs->schedulingError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to schedule after early start.", constraintToString()));
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ALAP: earlyStart=" + cs->earlyStart.toString() + " cs->startTime=" + cs->startTime.toString());
#endif
            } else if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
#ifndef PLAN_NLOGDEBUG
                cs->logDebug("ALAP: positiveFloat=" + cs->positiveFloat.toString());
#endif
            }
            //debugPlan<<m_name<<": lateStart="<<cs->startTime;
            makeAppointments();
            break;
        }
        case Node::StartNotEarlier:
            // cs->endTime calculated above
            //debugPlan<<"StartNotEarlier:"<<m_constraintStartTime<<cs->endTime;
            cs->endTime = workTimeBefore(cs->endTime, cs);
            cs->duration = duration(cs->endTime, use, true);
            cs->startTime = cs->endTime - cs->duration;
            if (cs->startTime < m_constraintStartTime) {
                //warnPlan<<"m_constraintStartTime > cs->lateStart";
                cs->constraintError = true;
                cs->negativeFloat = m_constraintStartTime - cs->startTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            makeAppointments();
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            }
            break;
        case Node::FinishNotLater:
            // cs->endTime calculated above
            //debugPlan<<"FinishNotLater:"<<m_constraintEndTime<<cs->endTime;
            if (cs->endTime > m_constraintEndTime) {
                cs->endTime = qMax(qMin(m_constraintEndTime, cs->lateFinish), cs->earlyFinish);
            }
            cs->endTime = workTimeBefore(cs->endTime, cs);
            cs->duration = duration(cs->endTime, use, true);
            cs->startTime = cs->endTime - cs->duration;
            if (cs->endTime > m_constraintEndTime) {
                cs->negativeFloat = cs->endTime - m_constraintEndTime;
                cs->constraintError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            makeAppointments();
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            }
            break;
        case Node::MustStartOn:
            // Just try to schedule on time
            //debugPlan<<"MustStartOn="<<m_constraintStartTime.toString()<<""<<cs->startTime.toString();
            cs->startTime = workTimeAfter(m_constraintStartTime, cs);
            cs->duration = duration(cs->startTime, use, false);
            if (cs->endTime >= cs->startTime + cs->duration) {
                cs->endTime = cs->startTime + cs->duration;
            } else {
                cs->endTime = workTimeBefore(cs->endTime);
                cs->duration = duration(cs->endTime, use, true);
                cs->startTime = cs->endTime - cs->duration;
            }
            if (m_constraintStartTime != cs->startTime) {
                cs->constraintError = true;
                cs->negativeFloat = m_constraintStartTime - cs->startTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            makeAppointments();
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            }
            break;
        case Node::MustFinishOn:
            // Just try to schedule on time
            //debugPlan<<m_name<<"MustFinishOn:"<<m_constraintEndTime<<cs->endTime<<cs->earlyFinish;
            cs->endTime = workTimeBefore(m_constraintEndTime, cs);
            cs->duration = duration(cs->endTime, use, true);
            cs->startTime = cs->endTime - cs->duration;
            if (m_constraintEndTime != cs->endTime) {
                cs->negativeFloat = m_constraintEndTime - cs->endTime;
                cs->constraintError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
                //warnPlan<<"m_constraintEndTime > cs->endTime";
            }
            makeAppointments();
            if (cs->lateFinish > cs->endTime) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            }
            break;
        case Node::FixedInterval: {
            // cs->endTime calculated above
            //debugPlan<<m_name<<"FixedInterval="<<m_constraintEndTime<<""<<cs->endTime;
            cs->duration = m_constraintEndTime - m_constraintStartTime;
            if (cs->endTime > m_constraintEndTime) {
                cs->endTime = qMax(m_constraintEndTime, cs->earlyFinish);
            }
            cs->startTime = cs->endTime - cs->duration;
            if (m_constraintEndTime != cs->endTime) {
                cs->negativeFloat = m_constraintEndTime - cs->endTime;
                cs->constraintError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            cs->workStartTime = workTimeAfter(cs->startTime);
            cs->workEndTime = workTimeBefore(cs->endTime);
            makeAppointments();
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = workTimeBefore(cs->lateFinish) - cs->endTime;
            }
            break;
        }
        default:
            break;
        }
        m_requests.reserve(cs->startTime, cs->duration);
        if (m_estimate->type() == Estimate::Type_Effort) {
            // HACK scheduling may accept deviation less than 5 mins to improve performance
            cs->effortNotMet = (m_estimate->value(use, cs->usePert()) - cs->plannedEffort()) > (5 * 60000);
            if (cs->effortNotMet) {
                cs->logError(i18n("Effort not met. Estimate: %1, planned: %2", estimate()->value(use, cs->usePert()).toHours(), cs->plannedEffort().toHours()));
            }
        }
    } else if (type() == Node::Type_Milestone) {
        switch (m_constraint) {
        case Node::ASAP:
            if (cs->endTime < cs->earlyStart) {
                cs->schedulingError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to schedule after early start.", constraintToString()));
                cs->endTime = cs->earlyStart;
            } else {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            //cs->endTime = cs->earlyStart; FIXME need to follow predeccessors. Defer scheduling?
            cs->startTime = cs->endTime;
            break;
        case Node::ALAP:
            cs->startTime = cs->endTime;
            cs->positiveFloat = cs->lateFinish - cs->endTime;
            break;
        case Node::MustStartOn:
        case Node::MustFinishOn:
        case Node::FixedInterval: {
            DateTime contime = m_constraint == Node::MustFinishOn ? m_constraintEndTime : m_constraintStartTime;
            if (contime < cs->earlyStart) {
                if (cs->earlyStart < cs->endTime) {
                    cs->endTime = cs->earlyStart;
                }
            } else if (contime < cs->endTime) {
                cs->endTime = contime;
            }
            cs->negativeFloat = cs->endTime > contime ? cs->endTime - contime : contime - cs->endTime;
            if (cs->negativeFloat != 0) {
                cs->constraintError = true;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            cs->startTime = cs->endTime;
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            break;
        }
        case Node::StartNotEarlier:
            cs->startTime = cs->endTime;
            if (m_constraintStartTime > cs->startTime) {
                cs->constraintError = true;
                cs->negativeFloat = m_constraintStartTime - cs->startTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            break;
        case Node::FinishNotLater:
            if (m_constraintEndTime < cs->earlyStart) {
                if (cs->earlyStart < cs->endTime) {
                    cs->endTime = cs->earlyStart;
                }
            } else if (m_constraintEndTime < cs->endTime) {
                cs->endTime = m_constraintEndTime;
            }
            if (m_constraintEndTime > cs->endTime) {
                cs->constraintError = true;
                cs->negativeFloat = cs->endTime - m_constraintEndTime;
                cs->logError(i18nc("1=type of constraint", "%1: Failed to meet constraint. Negative float=%2", constraintToString(true), cs->negativeFloat.toString(Duration::Format_i18nHour)));
            }
            cs->startTime = cs->endTime;
            if (cs->negativeFloat == Duration::zeroDuration) {
                cs->positiveFloat = cs->lateFinish - cs->endTime;
            }
            break;
        default:
            break;
        }
        cs->duration = Duration::zeroDuration;
    } else if (type() == Node::Type_Summarytask) {
        //shouldn't come here
        cs->startTime = cs->endTime;
        cs->duration = cs->endTime - cs->startTime;
        warnPlan<<"Summarytasks should not be calculated here: "<<m_name;
    }
    if (cs->startTime < projectNode()->constraintStartTime() || cs->endTime > projectNode()->constraintEndTime()) {
        cs->logError(i18n("Failed to schedule within project target time"));
    }
    foreach (const Appointment *a, cs->appointments()) {
        cs->logInfo(i18n("Resource %1 booked from %2 to %3", a->resource()->resource()->name(), locale.toString(a->startTime(), QLocale::ShortFormat), locale.toString(a->endTime(), QLocale::ShortFormat)));
    }
    if (cs->startTime < cs->earlyStart) {
        cs->logWarning(i18n("Starting earlier than early start"));
    }
    if (cs->endTime > cs->lateFinish) {
        cs->logWarning(i18n("Finishing later than late finish"));
    }
    cs->logInfo(i18n("Scheduled: %1 to %2", locale.toString(cs->startTime, QLocale::ShortFormat), locale.toString(cs->endTime, QLocale::ShortFormat)));
    m_visitedBackward = true;
    cs->incProgress();
    m_requests.resetDynamicAllocations();
#ifndef PLAN_NLOGDEBUG
    cs->logDebug(QStringLiteral("Finished schedule backward: %1 ms").arg(timer.elapsed()));
#endif
    return cs->startTime;
}

void Task::adjustSummarytask() {
    if (m_currentSchedule == 0)
        return;
    if (type() == Type_Summarytask) {
        DateTime start = m_currentSchedule->lateFinish;
        DateTime end = m_currentSchedule->earlyStart;
        foreach (Node *n, m_nodes) {
            n->adjustSummarytask();
            if (n->startTime() < start)
                start = n->startTime();
            if (n->endTime() > end)
                end = n->endTime();
        }
        m_currentSchedule->startTime = start;
        m_currentSchedule->endTime = end;
        m_currentSchedule->duration = end - start;
        m_currentSchedule->notScheduled = false;
        //debugPlan<<cs->name<<":"<<m_currentSchedule->startTime.toString()<<" :"<<m_currentSchedule->endTime.toString();
    }
}

Duration Task::duration(const DateTime &time, int use, bool backward) {
    //debugPlan;
    // TODO: handle risc
    if (m_currentSchedule == 0) {
        errorPlan<<"No current schedule";
        return Duration::zeroDuration;
    }
    if (!time.isValid()) {
#ifndef PLAN_NLOGDEBUG
        m_currentSchedule->logDebug(QStringLiteral("Calculate duration: Start time is not valid"));
#endif
        return Duration::zeroDuration;
    }
    //debugPlan<<m_name<<": Use="<<use;
    Duration eff;
    if (m_currentSchedule->recalculate() && completion().isStarted()) {
        eff = completion().remainingEffort();
        //debugPlan<<m_name<<": recalculate, effort="<<eff.toDouble(Duration::Unit_h);
        if (eff == 0 || completion().isFinished()) {
            return eff;
        }
    } else {
        eff = m_estimate->value(use, m_currentSchedule->usePert());
    }
    return calcDuration(time, eff, backward);
}


Duration Task::calcDuration(const DateTime &time, KPlato::Duration effort, bool backward) {
    //debugPlan<<"--------> calcDuration"<<(backward?"(B)":"(F)")<<m_name<<" time="<<time<<" effort="<<effort.toString(Duration::Format_Day);

    // Already checked: m_currentSchedule and time.
    Duration dur = effort; // use effort as default duration
    if (m_estimate->type() == Estimate::Type_Effort) {
        if (m_requests.isEmpty()) {
            m_currentSchedule->resourceError = true;
            m_currentSchedule->logError(i18n("No resource has been allocated"));
            return effort;
        }
        dur = m_requests.duration(time, effort, m_currentSchedule, backward);
        if (dur == Duration::zeroDuration) {
            warnPlan<<"zero duration: Resource not available";
            m_currentSchedule->resourceNotAvailable = true;
            dur = effort; //???
        }
        return dur;
    }
    if (m_estimate->type() == Estimate::Type_Duration) {
        return length(time, dur, backward);
    }
    errorPlan<<"Unsupported estimate type: "<<m_estimate->type();
    return dur;
}

Duration Task::length(const DateTime &time, KPlato::Duration duration, bool backward)
{
    return length(time, duration, m_currentSchedule, backward);
}

Duration Task::length(const DateTime &time, KPlato::Duration duration, Schedule *sch, bool backward) {
    //debugPlan<<"--->"<<(backward?"(B)":"(F)")<<m_name<<""<<time.toString()<<": duration:"<<duration.toString(Duration::Format_Day)<<" ("<<duration.milliseconds()<<")";

    Duration l;
    if (duration == Duration::zeroDuration) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug(QStringLiteral("Calculate length: estimate == 0"));
#else
        Q_UNUSED(sch)
#endif
        return l;
    }
    Calendar *cal = m_estimate->calendar();
    if (cal == 0) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Calculate length: No calendar, return estimate " + duration.toString());
#endif
        return duration;
    }
#ifndef PLAN_NLOGDEBUG
    if (sch) sch->logDebug("Calculate length from: " + time.toString());
#endif
    DateTime logtime = time;
    bool sts=true;
    bool match = false;
    DateTime start = time;
    int inc = backward ? -1 : 1;
    DateTime end = start;
    Duration l1;
    int nDays = backward ? projectNode()->constraintStartTime().daysTo(time) : time.daysTo(projectNode()->constraintEndTime());
    for (int i=0; !match && i <= nDays; ++i) {
        // days
        end = end.addDays(inc);
        l1 = backward ? cal->effort(end, start) : cal->effort(start, end);
        //debugPlan<<"["<<i<<"of"<<nDays<<"]"<<(backward?"(B)":"(F):")<<"  start="<<start<<" l+l1="<<(l+l1).toString()<<" match"<<duration.toString();
        if (l + l1 < duration) {
            l += l1;
            start = end;
        } else if (l + l1 == duration) {
            l += l1;
            match = true;
        } else {
            end = start;
            break;
        }
    }
    if (! match) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Days: duration " + logtime.toString() + " - " + end.toString() + " = " + l.toString() + " (" + (duration - l).toString() + ')');
#endif
        logtime = start;
        for (int i=0; !match && i < 24; ++i) {
            // hours
            end = end.addSecs(inc*60*60);
            l1 = backward ? cal->effort(end, start) : cal->effort(start, end);
            if (l + l1 < duration) {
                l += l1;
                start = end;
            } else if (l + l1 == duration) {
                l += l1;
                match = true;
            } else {
                end = start;
                break;
            }
            //debugPlan<<"duration(h)["<<i<<"]"<<(backward?"backward":"forward:")<<" time="<<start.time()<<" l="<<l.toString()<<" ("<<l.milliseconds()<<')';
        }
        //debugPlan<<"duration"<<(backward?"backward":"forward:")<<start.toString()<<" l="<<l.toString()<<" ("<<l.milliseconds()<<")  match="<<match<<" sts="<<sts;
    }
    if (! match) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Hours: duration " + logtime.toString() + " - " + end.toString() + " = " + l.toString() + " (" + (duration - l).toString() + ')');
#endif
        logtime = start;
        for (int i=0; !match && i < 60; ++i) {
            //minutes
            end = end.addSecs(inc*60);
            l1 = backward ? cal->effort(end, start) : cal->effort(start, end);
            if (l + l1 < duration) {
                l += l1;
                start = end;
            } else if (l + l1 == duration) {
                l += l1;
                match = true;
            } else if (l + l1 > duration) {
                end = start;
                break;
            }
            //debugPlan<<"duration(m)"<<(backward?"backward":"forward:")<<"  time="<<start.time().toString()<<" l="<<l.toString()<<" ("<<l.milliseconds()<<')';
        }
        //debugPlan<<"duration"<<(backward?"backward":"forward:")<<"  start="<<start.toString()<<" l="<<l.toString()<<" match="<<match<<" sts="<<sts;
    }
    if (! match) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Minutes: duration " + logtime.toString() + " - " + end.toString() + " = " + l.toString() + " (" + (duration - l).toString() + ')');
#endif
        logtime = start;
        for (int i=0; !match && i < 60 && sts; ++i) {
            //seconds
            end = end.addSecs(inc);
            l1 = backward ? cal->effort(end, start) : cal->effort(start, end);
            if (l + l1 < duration) {
                l += l1;
                start = end;
            } else if (l + l1 == duration) {
                l += l1;
                match = true;
            } else if (l + l1 > duration) {
                end = start;
                break;
            }
            //debugPlan<<"duration(s)["<<i<<"]"<<(backward?"backward":"forward:")<<" time="<<start.time().toString()<<" l="<<l.toString()<<" ("<<l.milliseconds()<<')';
        }
    }
    if (! match) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Seconds: duration " + logtime.toString() + " - " + end.toString() + " l " + l.toString() + " (" + (duration - l).toString() + ')');
#endif
        for (int i=0; !match && i < 1000; ++i) {
            //milliseconds
            end.setTime(end.time().addMSecs(inc));
            l1 = backward ? cal->effort(end, start) : cal->effort(start, end);
            if (l + l1 < duration) {
                l += l1;
                start = end;
            } else if (l + l1 == duration) {
                l += l1;
                match = true;
            } else {
#ifndef PLAN_NLOGDEBUG
                if (sch) sch->logDebug("Got more than asked for, should not happen! Want: " + duration.toString(Duration::Format_Hour) + " got: " + l.toString(Duration::Format_Hour));
#endif
                break;
            }
            //debugPlan<<"duration(ms)["<<i<<"]"<<(backward?"backward":"forward:")<<" time="<<start.time().toString()<<" l="<<l.toString()<<" ("<<l.milliseconds()<<')';
        }
    }
    if (!match) {
        m_currentSchedule->logError(i18n("Could not match work duration. Want: %1 got: %2",  l.toString(Duration::Format_i18nHour), duration.toString(Duration::Format_i18nHour)));
    }
    DateTime t = end;
    if (l != Duration::zeroDuration) {
        if (backward) {
            if (end < projectNode()->constraintEndTime()) {
                t = cal->firstAvailableAfter(end, projectNode()->constraintEndTime());
            }
        } else {
            if (end > projectNode()->constraintStartTime()) {
                t = cal->firstAvailableBefore(end, projectNode()->constraintStartTime());
            }
        }
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Moved end to work: " + end.toString() + " -> " + t.toString());
#endif
    }
    end = t.isValid() ? t : time;
    //debugPlan<<"<---"<<(backward?"(B)":"(F)")<<m_name<<":"<<end.toString()<<"-"<<time.toString()<<"="<<(end - time).toString()<<" duration:"<<duration.toString(Duration::Format_Day);
    l = end>time ? end-time : time-end;
    if (match) {
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug("Calculated length: " + time.toString() + " - " + end.toString() + " = " + l.toString());
#endif
    }
    return l;
}

void Task::clearProxyRelations() {
    m_parentProxyRelations.clear();
    m_childProxyRelations.clear();
}

void Task::addParentProxyRelations(const QList<Relation*> &list)
{
    //debugPlan<<m_name;
    if (type() == Type_Summarytask) {
        // propagate to my children
        //debugPlan<<m_name<<" is summary task";
        foreach (Node *n, m_nodes) {
            n->addParentProxyRelations(list);
            n->addParentProxyRelations(dependParentNodes());
        }
    } else {
        // add 'this' as child relation to the relations parent
        //debugPlan<<m_name<<" is not summary task";
        foreach (Relation *r, list) {
            r->parent()->addChildProxyRelation(this, r);
            // add a parent relation to myself
            addParentProxyRelation(r->parent(), r);
        }
    }
}

void Task::addChildProxyRelations(const QList<Relation*> &list) {
    //debugPlan<<m_name;
    if (type() == Type_Summarytask) {
        // propagate to my children
        //debugPlan<<m_name<<" is summary task";
        foreach (Node *n, m_nodes) {
            n->addChildProxyRelations(list);
            n->addChildProxyRelations(dependChildNodes());
        }
    } else {
        // add 'this' as parent relation to the relations child
        //debugPlan<<m_name<<" is not summary task";
        foreach (Relation *r, list) {
            r->child()->addParentProxyRelation(this, r);
            // add a child relation to myself
            addChildProxyRelation(r->child(), r);
        }
    }
}

void Task::addParentProxyRelation(Node *node, const Relation *rel) {
    if (node->type() != Type_Summarytask) {
        if (type() == Type_Summarytask) {
            //debugPlan<<"Add parent proxy from my children"<<m_name<<" to"<<node->name();
            foreach (Node *n, m_nodes) {
                n->addParentProxyRelation(node, rel);
            }
        } else {
            //debugPlan<<"Add parent proxy from"<<node->name()<<" to (me)"<<m_name;
            m_parentProxyRelations.append(new ProxyRelation(node, this, rel->type(), rel->lag()));
        }
    }
}

void Task::addChildProxyRelation(Node *node, const Relation *rel) {
    if (node->type() != Type_Summarytask) {
        if (type() == Type_Summarytask) {
            //debugPlan<<"Add child proxy from my children"<<m_name<<" to"<<node->name();
            foreach (Node *n, m_nodes) {
                n->addChildProxyRelation(node, rel);
            }
        } else {
            //debugPlan<<"Add child proxy from (me)"<<m_name<<" to"<<node->name();
            m_childProxyRelations.append(new ProxyRelation(this, node, rel->type(), rel->lag()));
        }
    }
}

bool Task::isEndNode() const
{
    return m_dependChildNodes.isEmpty() && m_childProxyRelations.isEmpty();
}
bool Task::isStartNode() const
{
    return m_dependParentNodes.isEmpty() && m_parentProxyRelations.isEmpty();
}

DateTime Task::workTimeAfter(const DateTime &dt, Schedule *sch) const {
    DateTime t;
    if (m_estimate->type() == Estimate::Type_Duration) {
        if (m_estimate->calendar()) {
            t = m_estimate->calendar()->firstAvailableAfter(dt, projectNode()->constraintEndTime());
        }
    } else {
        t = m_requests.workTimeAfter(dt, sch);
#ifndef PLAN_NLOGDEBUG
        if (sch) sch->logDebug(QStringLiteral("workTimeAfter: %1 = %2").arg(dt.toString()).arg(t.toString()));
#endif
    }
    return t.isValid() ? t : dt;
}

DateTime Task::workTimeBefore(const DateTime &dt, Schedule *sch) const {
    DateTime t;
    if (m_estimate->type() == Estimate::Type_Duration) {
        if (m_estimate->calendar()) {
            t = m_estimate->calendar()->firstAvailableBefore(dt, projectNode()->constraintStartTime());
        }
    } else {
        t = m_requests.workTimeBefore(dt, sch);
    }
    return t.isValid() ? t : dt;
}

Duration Task::positiveFloat(long id) const
{
    Schedule *s = schedule(id);
    return s == 0 ? Duration::zeroDuration : s->positiveFloat;
}

void Task::setPositiveFloat(KPlato::Duration fl, long id) const
{
    Schedule *s = schedule(id);
    if (s)
        s->positiveFloat = fl;
}

Duration Task::negativeFloat(long id) const
{
    Schedule *s = schedule(id);
    return s == 0 ? Duration::zeroDuration : s->negativeFloat;
}

void Task::setNegativeFloat(KPlato::Duration fl, long id) const
{
    Schedule *s = schedule(id);
    if (s)
        s->negativeFloat = fl;
}

Duration Task::freeFloat(long id) const
{
    Schedule *s = schedule(id);
    return s == 0 ? Duration::zeroDuration : s->freeFloat;
}

void Task::setFreeFloat(KPlato::Duration fl, long id) const
{
    Schedule *s = schedule(id);
    if (s)
        s->freeFloat = fl;
}

Duration Task::startFloat(long id) const
{
    Schedule *s = schedule(id);
    return s == 0 || s->earlyStart > s->lateStart ? Duration::zeroDuration : (s->earlyStart - s->lateStart);
}

Duration Task::finishFloat(long id) const
{
    Schedule *s = schedule(id);
    return s == 0 || s->lateFinish < s->earlyFinish ? Duration::zeroDuration : (s->lateFinish - s->earlyFinish);
}

bool Task::isCritical(long id) const
{
    Schedule *s = schedule(id);
    return s == 0 ? false : s->isCritical();
}

bool Task::calcCriticalPath(bool fromEnd)
{
    if (m_currentSchedule == 0)
        return false;
    //debugPlan<<m_name<<" fromEnd="<<fromEnd<<" cp="<<m_currentSchedule->inCriticalPath;
    if (m_currentSchedule->inCriticalPath) {
        return true; // path already calculated
    }
    if (!isCritical()) {
        return false;
    }
    if (fromEnd) {
        if (isEndNode() && startFloat() == 0 && finishFloat() == 0) {
            m_currentSchedule->inCriticalPath = true;
            //debugPlan<<m_name<<" end node";
            return true;
        }
        foreach (Relation *r, m_childProxyRelations) {
            if (r->child()->calcCriticalPath(fromEnd)) {
                m_currentSchedule->inCriticalPath = true;
            }
        }
        foreach (Relation *r, m_dependChildNodes) {
            if (r->child()->calcCriticalPath(fromEnd)) {
                m_currentSchedule->inCriticalPath = true;
            }
        }
    } else {
        if (isStartNode() && startFloat() == 0 && finishFloat() == 0) {
            m_currentSchedule->inCriticalPath = true;
            //debugPlan<<m_name<<" start node";
            return true;
        }
        foreach (Relation *r, m_parentProxyRelations) {
            if (r->parent()->calcCriticalPath(fromEnd)) {
                m_currentSchedule->inCriticalPath = true;
            }
        }
        foreach (Relation *r, m_dependParentNodes) {
            if (r->parent()->calcCriticalPath(fromEnd)) {
                m_currentSchedule->inCriticalPath = true;
            }
        }
    }
    //debugPlan<<m_name<<" return cp="<<m_currentSchedule->inCriticalPath;
    return m_currentSchedule->inCriticalPath;
}

void Task::calcFreeFloat()
{
    //debugPlan<<m_name;
    if (type() == Node::Type_Summarytask) {
        Node::calcFreeFloat();
        return;
    }
    Schedule *cs = m_currentSchedule;
    if (cs == 0) {
        return;
    }
    DateTime t;
    foreach (Relation *r, m_dependChildNodes) {
        DateTime c = r->child()->startTime();
        if (!t.isValid() || c < t) {
            t = c;
        }
    }
    foreach (Relation *r, m_childProxyRelations) {
        DateTime c = r->child()->startTime();
        if (!t.isValid() || c < t) {
            t = c;
        }
    }
    if (t.isValid() && t > cs->endTime) {
        cs->freeFloat = t - cs->endTime;
        //debugPlan<<m_name<<": "<<cs->freeFloat.toString();
    }
}

void Task::setCurrentSchedule(long id)
{
    setCurrentSchedulePtr(findSchedule(id));
    Node::setCurrentSchedule(id);
}

bool Task::effortMetError(long id) const
{
    Schedule *s = schedule(id);
    if (s == 0 || s->notScheduled || m_estimate->type() != Estimate::Type_Effort) {
        return false;
    }
    return s->effortNotMet;
}

uint Task::state(long id) const
{
    int st = Node::State_None;
    if (! isScheduled(id)) {
        st |= State_NotScheduled;
    }
    if (completion().isFinished()) {
        st |= Node::State_Finished;
        if (completion().finishTime() > endTime(id)) {
            st |= State_FinishedLate;
        }
        if (completion().finishTime() < endTime(id)) {
            st |= State_FinishedEarly;
        }
    } else if (completion().isStarted()) {
        st |= Node::State_Started;
        if (completion().startTime() > startTime(id)) {
            st |= State_StartedLate;
        }
        if (completion().startTime() < startTime(id)) {
            st |= State_StartedEarly;
        }
        if (completion().percentFinished() > 0) {
            st |= State_Running;
        }
        if (endTime(id) < QDateTime::currentDateTime()) {
            st |= State_Late;
        }
    } else if (isScheduled(id)) {
        if (startTime(id) < QDateTime::currentDateTime()) {
            st |= State_Late;
        }
    }
    st |= State_ReadyToStart;
    //TODO: check proxy relations
    foreach (const Relation *r, m_dependParentNodes) {
        if (! static_cast<Task*>(r->parent())->completion().isFinished()) {
            st &= ~Node::State_ReadyToStart;
            st |= Node::State_NotReadyToStart;
            break;
        }
    }
    return st;
}

void Task::addWorkPackage(WorkPackage *wp)
{
    emit workPackageToBeAdded(this, m_packageLog.count());
    m_packageLog.append(wp);
    emit workPackageAdded(this);
}

void Task::removeWorkPackage(WorkPackage *wp)
{
    int index = m_packageLog.indexOf(wp);
    if (index < 0) {
        return;
    }
    emit workPackageToBeRemoved(this, index);
    m_packageLog.removeAt(index);
    emit workPackageRemoved(this);
}

WorkPackage *Task::workPackageAt(int index) const
{
    Q_ASSERT (index >= 0 && index < m_packageLog.count());
    return m_packageLog.at(index);
}

QString Task::wpOwnerName() const
{
    if (m_packageLog.isEmpty()) {
        return m_workPackage.ownerName();
    }
    return m_packageLog.last()->ownerName();
}

WorkPackage::WPTransmitionStatus Task::wpTransmitionStatus() const
{
    if (m_packageLog.isEmpty()) {
        return m_workPackage.transmitionStatus();
    }
    return m_packageLog.last()->transmitionStatus();
}

DateTime Task::wpTransmitionTime() const
{
    if (m_packageLog.isEmpty()) {
        return m_workPackage.transmitionTime();
    }
    return m_packageLog.last()->transmitionTime();
}

//------------------------------------------

Completion::Completion(Node *node)
    : m_node(node),
      m_started(false),
      m_finished(false),
      m_entrymode(EnterEffortPerResource)
{}

Completion::Completion(const Completion &c)
{
    copy(c);
}

Completion::~Completion()
{
    qDeleteAll(m_entries);
    qDeleteAll(m_usedEffort);
}

void Completion::copy(const Completion &p)
{
    m_node = 0; //NOTE
    m_started = p.isStarted(); m_finished = p.isFinished();
    m_startTime = p.startTime(); m_finishTime = p.finishTime();
    m_entrymode = p.entrymode();

    qDeleteAll(m_entries);
    m_entries.clear();
    Completion::EntryList::ConstIterator entriesIt = p.entries().constBegin();
    const Completion::EntryList::ConstIterator entriesEnd = p.entries().constEnd();
    for (; entriesIt != entriesEnd; ++entriesIt) {
        addEntry(entriesIt.key(), new Entry(*entriesIt.value()));
    }

    qDeleteAll(m_usedEffort);
    m_usedEffort.clear();
    Completion::ResourceUsedEffortMap::ConstIterator usedEffortMapIt = p.usedEffortMap().constBegin();
    const Completion::ResourceUsedEffortMap::ConstIterator usedEffortMapEnd = p.usedEffortMap().constEnd();
    for (; usedEffortMapIt != usedEffortMapEnd; ++usedEffortMapIt) {
        addUsedEffort(usedEffortMapIt.key(), new UsedEffort(*usedEffortMapIt.value()));
    }
}

bool Completion::operator==(const Completion &p)
{
    return m_started == p.isStarted() && m_finished == p.isFinished() &&
            m_startTime == p.startTime() && m_finishTime == p.finishTime() &&
            m_entries == p.entries() &&
            m_usedEffort == p.usedEffortMap();
}
Completion &Completion::operator=(const Completion &p)
{
    copy(p);
    return *this;
}

void Completion::changed(int property)
{
    if (m_node) {
        m_node->changed(property);
    }
}

void Completion::setStarted(bool on)
{
     m_started = on;
     changed(Node::CompletionStartedProperty);
}

void Completion::setFinished(bool on)
{
     m_finished = on;
     changed(Node::CompletionFinishedProperty);
}

void Completion::setStartTime(const DateTime &dt)
{
     m_startTime = dt;
     changed(Node::CompletionStartTimeProperty);
}

void Completion::setFinishTime(const DateTime &dt)
{
     m_finishTime = dt;
     changed(Node::CompletionFinishTimeProperty);
}

void Completion::setPercentFinished(QDate date, int value)
{
    Entry *e = 0;
    if (m_entries.contains(date)) {
        e = m_entries[ date ];
    } else {
        e = new Entry();
        m_entries[ date ] = e;
    }
    e->percentFinished = value;
    changed(Node::CompletionPercentageProperty);
}

void Completion::setRemainingEffort(QDate date, KPlato::Duration value)
{
    Entry *e = 0;
    if (m_entries.contains(date)) {
        e = m_entries[ date ];
    } else {
        e = new Entry();
        m_entries[ date ] = e;
    }
    e->remainingEffort = value;
    changed(Node::CompletionRemainingEffortProperty);
}

void Completion::setActualEffort(QDate date, KPlato::Duration value)
{
    Entry *e = 0;
    if (m_entries.contains(date)) {
        e = m_entries[ date ];
    } else {
        e = new Entry();
        m_entries[ date ] = e;
    }
    e->totalPerformed = value;
    changed(Node::CompletionActualEffortProperty);
}

void Completion::addEntry(QDate date, Entry *entry)
{
     m_entries.insert(date, entry);
     //debugPlan<<m_entries.count()<<" added:"<<date;
     changed(Node::CompletionEntryProperty);
}

QDate Completion::entryDate() const
{
    return m_entries.isEmpty() ? QDate() : m_entries.lastKey();
}

int Completion::percentFinished() const
{
    return m_entries.isEmpty() ? 0 : m_entries.last()->percentFinished;
}

int Completion::percentFinished(QDate date) const
{
    int x = 0;
    EntryList::const_iterator it;
    for (it = m_entries.constBegin(); it != m_entries.constEnd() && it.key() <= date; ++it) {
        x = it.value()->percentFinished;
    }
    return x;
}

Duration Completion::remainingEffort() const
{
    return m_entries.isEmpty() ? Duration::zeroDuration : m_entries.last()->remainingEffort;
}

Duration Completion::remainingEffort(QDate date) const
{
    Duration x;
    EntryList::const_iterator it;
    for (it = m_entries.constBegin(); it != m_entries.constEnd() && it.key() <= date; ++it) {
        x = it.value()->remainingEffort;
    }
    return x;
}

Duration Completion::actualEffort() const
{
    Duration eff;
    if (m_entrymode == EnterEffortPerResource) {
        foreach(const UsedEffort *ue, m_usedEffort) {
            const QMap<QDate, UsedEffort::ActualEffort> map = ue->actualEffortMap();
            QMap<QDate, UsedEffort::ActualEffort>::const_iterator it;
            for (it = map.constBegin(); it != map.constEnd(); ++it) {
                eff += it.value().effort();
            }
        }
    } else if (! m_entries.isEmpty()) {
        eff = m_entries.last()->totalPerformed;
    }
    return eff;
}

Duration Completion::actualEffort(const Resource *resource, QDate date) const
{
    UsedEffort *ue = usedEffort(resource);
    if (ue == 0) {
        return Duration::zeroDuration;
    }
    UsedEffort::ActualEffort ae = ue->effort(date);
    return ae.effort();
}

Duration Completion::actualEffort(QDate date) const
{
    Duration eff;
    if (m_entrymode == EnterEffortPerResource) {
        foreach(const UsedEffort *ue, m_usedEffort) {
            if (ue && ue->actualEffortMap().contains(date)) {
                eff += ue->actualEffortMap().value(date).effort();
            }
        }
    } else {
        // Hmmm: How to really know a specific date?
        if (m_entries.contains(date)) {
            eff = m_entries[ date ]->totalPerformed;
        }
    }
    return eff;
}

Duration Completion::actualEffortTo(QDate date) const
{
    //debugPlan<<date;
    Duration eff;
    if (m_entrymode == EnterEffortPerResource) {
        foreach(const UsedEffort *ue, m_usedEffort) {
            eff += ue->effortTo(date);
        }
    } else {
        QListIterator<QDate> it(m_entries.uniqueKeys());
        it.toBack();
        while (it.hasPrevious()) {
            QDate d = it.previous();
            if (d <= date) {
                eff = m_entries[ d ]->totalPerformed;
                break;
            }
        }
    }
    return eff;
}

double Completion::averageCostPrHour(QDate date, long id) const
{
    Schedule *s = m_node->schedule(id);
    if (s == 0) {
        return 0.0;
    }
    double cost = 0.0;
    double eff = 0.0;
    QList<double> cl;
    foreach (const Appointment *a, s->appointments()) {
        cl << a->resource()->resource()->normalRate();
        double e = a->plannedEffort(date).toDouble(Duration::Unit_h);
        if (e > 0.0) {
            eff += e;
            cost += e * cl.last();
        }
    }
    if (eff > 0.0) {
        cost /= eff;
    } else {
        foreach (double c, cl) {
            cost += c;
        }
        cost /= cl.count();
    }
    return cost;
}

EffortCostMap Completion::effortCostPrDay(QDate start, QDate end, long id) const
{
    //debugPlan<<m_node->name()<<start<<end;
    EffortCostMap ec;
    if (! isStarted()) {
        return ec;
    }
    switch (m_entrymode) {
        case FollowPlan:
            break;
        case EnterCompleted:
        case EnterEffortPerTask: {
            QDate st = start.isValid() ? start : m_startTime.date();
            QDate et = end.isValid() ? end : m_finishTime.date();
            Duration last;
            foreach (const QDate &d, m_entries.uniqueKeys()) {
                if (d < st) {
                    continue;
                }
                if (et.isValid() && d > et) {
                    break;
                }
                Duration e = m_entries[ d ]->totalPerformed;
                if (e != Duration::zeroDuration && e != last) {
                    Duration eff = e - last;
                    ec.insert(d, eff, eff.toDouble(Duration::Unit_h) * averageCostPrHour(d, id));
                    last = e;
                }
            }
            break;
        }
        case EnterEffortPerResource: {
            std::pair<QDate, QDate> dates = actualStartEndDates();
            if (! dates.first.isValid()) {
                // no data, so just break
                break;
            }
            QDate st = start.isValid() ? start : dates.first;
            QDate et = end.isValid() ? end : dates.second;
            for (QDate d = st; d <= et; d = d.addDays(1)) {
                ec.add(d, actualEffort(d), actualCost(d));
            }
            break;
        }
    }
    return ec;
}

EffortCostMap Completion::effortCostPrDay(const Resource *resource, QDate start, QDate end, long id) const
{
    Q_UNUSED(id);
    //debugPlan<<m_node->name()<<start<<end;
    EffortCostMap ec;
    if (! isStarted()) {
        return ec;
    }
    switch (m_entrymode) {
        case FollowPlan:
            break;
        case EnterCompleted:
        case EnterEffortPerTask: {
            //TODO but what todo?
            break;
        }
        case EnterEffortPerResource: {
            std::pair<QDate, QDate> dates = actualStartEndDates();
            if (! dates.first.isValid()) {
                // no data, so just break
                break;
            }
            QDate st = start.isValid() ? start : dates.first;
            QDate et = end.isValid() ? end : dates.second;
            for (QDate d = st; d <= et; d = d.addDays(1)) {
                ec.add(d, actualEffort(resource, d), actualCost(resource, d));
            }
            break;
        }
    }
    return ec;
}

void Completion::addUsedEffort(const Resource *resource, Completion::UsedEffort *value)
{
    UsedEffort *v = value == 0 ? new UsedEffort() : value;
    if (m_usedEffort.contains(resource)) {
        m_usedEffort[ resource ]->mergeEffort(*v);
        delete v;
    } else {
        m_usedEffort.insert(resource, v);
    }
    changed(Node::CompletionUsedEffortProperty);
}

void Completion::setActualEffort(Resource *resource, const QDate &date, const Completion::UsedEffort::ActualEffort &value)
{
    if (value.isNull()) {
        if (!m_usedEffort.contains(resource)) {
            return;
        }
        UsedEffort *ue = m_usedEffort.value(resource);
        if (!ue) {
            return;
        }
        ue->takeEffort(date);
    } else {
        UsedEffort *ue = m_usedEffort[resource];
        if (!ue) {
            ue = new UsedEffort();
            m_usedEffort.insert(resource, ue);
        }
        ue->setEffort(date, value);
    }
    changed(Node::CompletionActualEffortProperty);
}

Completion::UsedEffort::ActualEffort Completion::getActualEffort(Resource *resource, const QDate &date) const
{
    UsedEffort::ActualEffort value;
    UsedEffort *ue = m_usedEffort.value(resource);
    if (ue) {
        value = ue->effort(date);
    }
    return value;
}

QString Completion::note() const
{
    return m_entries.isEmpty() ? QString() : m_entries.last()->note;
}

void Completion::setNote(const QString &str)
{
    if (! m_entries.isEmpty()) {
        m_entries.last()->note = str;
        changed(Node::CompletionNoteProperty);
    }
}

std::pair<QDate, QDate> Completion::actualStartEndDates() const
{
    std::pair<QDate, QDate> p;
    ResourceUsedEffortMap::const_iterator it;
    for (it = m_usedEffort.constBegin(); it != m_usedEffort.constEnd(); ++it) {
        if (!it.value()->actualEffortMap().isEmpty()) {
            QDate d = it.value()->firstDate();
            if (!p.first.isValid() || d < p.first) {
                p.first = d;
            }
            d = it.value()->lastDate();
            if (!p.second.isValid() || d > p.second) {
                p.second = d;
            }
        }
    }
    return p;
}

double Completion::actualCost(QDate date) const
{
    //debugPlan<<date;
    double c = 0.0;
    ResourceUsedEffortMap::const_iterator it;
    for (it = m_usedEffort.constBegin(); it != m_usedEffort.constEnd(); ++it) {
        double nc = it.key()->normalRate();
        double oc = it.key()->overtimeRate();
        if (it.value()->actualEffortMap().contains(date)) {
            UsedEffort::ActualEffort a = it.value()->effort(date);
            c += a.normalEffort().toDouble(Duration::Unit_h) * nc;
            c += a.overtimeEffort().toDouble(Duration::Unit_h) * oc;
        }
    }
    return c;
}

double Completion::actualCost(const Resource *resource) const
{
    UsedEffort *ue = usedEffort(resource);
    if (ue == 0) {
        return 0.0;
    }
    double c = 0.0;
    double nc = resource->normalRate();
    double oc = resource->overtimeRate();
    foreach (const UsedEffort::ActualEffort &a, ue->actualEffortMap()) {
        c += a.normalEffort().toDouble(Duration::Unit_h) * nc;
        c += a.overtimeEffort().toDouble(Duration::Unit_h) * oc;
    }
    return c;
}

double Completion::actualCost() const
{
    double c = 0.0;
    ResourceUsedEffortMap::const_iterator it;
    for (it = m_usedEffort.constBegin(); it != m_usedEffort.constEnd(); ++it) {
        c += actualCost(it.key());
    }
    return c;
}

double Completion::actualCost(const Resource *resource, QDate date) const
{
    UsedEffort *ue = usedEffort(resource);
    if (ue == 0) {
        return 0.0;
    }
    UsedEffort::ActualEffort a = ue->actualEffortMap().value(date);
    double c = a.normalEffort().toDouble(Duration::Unit_h) * resource->normalRate();
    c += a.overtimeEffort().toDouble(Duration::Unit_h) * resource->overtimeRate();
    return c;
}

EffortCostMap Completion::actualEffortCost(long int id, KPlato::EffortCostCalculationType type) const
{
    //debugPlan;
    EffortCostMap map;
    if (! isStarted()) {
        return map;
    }
    QList< QMap<QDate, UsedEffort::ActualEffort> > lst;
    QList< double > rate;
    QDate start, end;
    ResourceUsedEffortMap::const_iterator it;
    for (it = m_usedEffort.constBegin(); it != m_usedEffort.constEnd(); ++it) {
        const Resource *r = it.key();
        //debugPlan<<m_node->name()<<r->name();
        lst << usedEffort(r)->actualEffortMap();
        if (lst.last().isEmpty()) {
            lst.takeLast();
            continue;
        }
        if (r->type() == Resource::Type_Material) {
            if (type == ECCT_All) {
                rate.append(r->normalRate());
            } else if (type == ECCT_EffortWork) {
                rate.append(0.0);
            } else {
                lst.takeLast();
                continue;
            }
        } else {
            rate.append(r->normalRate());
        }
        if (! start.isValid() || start > lst.last().firstKey()) {
            start = lst.last().firstKey();
        }
        if (! end.isValid() || end < lst.last().lastKey()) {
            end = lst.last().lastKey();
        }
    }
    if (! lst.isEmpty() && start.isValid() && end.isValid()) {
        for (QDate d = start; d <= end; d = d.addDays(1)) {
            EffortCost c;
            for (int i = 0; i < lst.count(); ++i) {
                UsedEffort::ActualEffort a = lst.at(i).value(d);
                double nc = rate.value(i);
                Duration eff = a.normalEffort();
                double cost = eff.toDouble(Duration::Unit_h) * nc;
                c.add(eff, cost);
            }
            if (c.effort() != Duration::zeroDuration || c.cost() != 0.0) {
                map.add(d, c);
            }
        }
    } else if (! m_entries.isEmpty()) {
        QDate st = start.isValid() ? start : m_startTime.date();
        QDate et = end.isValid() ? end : m_finishTime.date();
        Duration last;
        foreach (const QDate &d, m_entries.uniqueKeys()) {
            if (d < st) {
                continue;
            }
            Duration e = m_entries[ d ]->totalPerformed;
            if (e != Duration::zeroDuration && e != last) {
                //debugPlan<<m_node->name()<<d<<(e - last).toDouble(Duration::Unit_h);
                double eff = (e - last).toDouble(Duration::Unit_h);
                map.insert(d, e - last, eff * averageCostPrHour(d, id)); // try to guess cost
                last = e;
            }
            if (et.isValid() && d > et) {
                break;
            }
        }
    }
    return map;
}

EffortCost Completion::actualCostTo(long int id, QDate date) const
{
    //debugPlan<<date;
    EffortCostMap ecm = actualEffortCost(id);
    return EffortCost(ecm.effortTo(date), ecm.costTo(date));
}

QStringList Completion::entrymodeList() const
{
    return QStringList()
            << QStringLiteral("FollowPlan")
            << QStringLiteral("EnterCompleted")
            << QStringLiteral("EnterEffortPerTask")
            << QStringLiteral("EnterEffortPerResource");

}

void Completion::setEntrymode(const QString &mode)
{
    int m = entrymodeList().indexOf(mode);
    if (m == -1) {
        m = EnterCompleted;
    }
    m_entrymode = static_cast<Entrymode>(m);
}
QString Completion::entryModeToString() const
{
    return entrymodeList().value(m_entrymode);
}

bool Completion::loadXML(KoXmlElement &element, XMLLoaderObject &status)
{
    //debugPlan;
    QString s;
    m_started = (bool)element.attribute(QStringLiteral("started"), QStringLiteral("0")).toInt();
    m_finished = (bool)element.attribute(QStringLiteral("finished"), QStringLiteral("0")).toInt();
    s = element.attribute(QStringLiteral("startTime"));
    if (!s.isEmpty()) {
        m_startTime = DateTime::fromString(s, status.projectTimeZone());
    }
    s = element.attribute(QStringLiteral("finishTime"));
    if (!s.isEmpty()) {
        m_finishTime = DateTime::fromString(s, status.projectTimeZone());
    }
    setEntrymode(element.attribute(QStringLiteral("entrymode")));
    if (status.version() < QLatin1String("0.6")) {
        if (m_started) {
            Entry *entry = new Entry(element.attribute(QStringLiteral("percent-finished"), QStringLiteral("0")).toInt(), Duration::fromString(element.attribute(QStringLiteral("remaining-effort"))),  Duration::fromString(element.attribute(QStringLiteral("performed-effort"))));
            entry->note = element.attribute(QStringLiteral("note"));
            QDate date = m_startTime.date();
            if (m_finished) {
                date = m_finishTime.date();
            }
            // almost the best we can do ;)
            addEntry(date, entry);
        }
    } else {
        KoXmlElement e;
        forEachElement(e, element) {
                if (e.tagName() == QLatin1String("completion-entry")) {
                    QDate date;
                    s = e.attribute(QStringLiteral("date"));
                    if (!s.isEmpty()) {
                        date = QDate::fromString(s, Qt::ISODate);
                    }
                    if (!date.isValid()) {
                        warnPlan<<"Invalid date: "<<date<<s;
                        continue;
                    }
                    Entry *entry = new Entry(e.attribute(QStringLiteral("percent-finished"), QStringLiteral("0")).toInt(), Duration::fromString(e.attribute(QStringLiteral("remaining-effort"))),  Duration::fromString(e.attribute(QStringLiteral("performed-effort"))));
                    addEntry(date, entry);
                } else if (e.tagName() == QLatin1String("used-effort")) {
                    KoXmlElement el;
                    forEachElement(el, e) {
                            if (el.tagName() == QLatin1String("resource")) {
                                QString id = el.attribute(QStringLiteral("id"));
                                Resource *r = status.project().resource(id);
                                if (r == 0) {
                                    warnPlan<<"Cannot find resource, id="<<id;
                                    continue;
                                }
                                UsedEffort *ue = new UsedEffort();
                                addUsedEffort(r, ue);
                                ue->loadXML(el, status);
                            }
                    }
                }
        }
    }
    return true;
}

void Completion::saveXML(QDomElement &element)  const
{
    QDomElement el = element.ownerDocument().createElement(QStringLiteral("progress"));
    element.appendChild(el);
    el.setAttribute(QStringLiteral("started"), QString::number(m_started));
    el.setAttribute(QStringLiteral("finished"), QString::number(m_finished));
    el.setAttribute(QStringLiteral("startTime"), m_startTime.toString(Qt::ISODate));
    el.setAttribute(QStringLiteral("finishTime"), m_finishTime.toString(Qt::ISODate));
    el.setAttribute(QStringLiteral("entrymode"), entryModeToString());
    foreach(const QDate &date, m_entries.uniqueKeys()) {
        QDomElement elm = el.ownerDocument().createElement(QStringLiteral("completion-entry"));
        el.appendChild(elm);
        Entry *e = m_entries[ date ];
        elm.setAttribute(QStringLiteral("date"), date.toString(Qt::ISODate));
        elm.setAttribute(QStringLiteral("percent-finished"), e->percentFinished);
        elm.setAttribute(QStringLiteral("remaining-effort"), e->remainingEffort.toString());
        elm.setAttribute(QStringLiteral("performed-effort"), e->totalPerformed.toString());
        elm.setAttribute(QStringLiteral("note"), e->note);
    }
    if (! m_usedEffort.isEmpty()) {
        QDomElement elm = el.ownerDocument().createElement(QStringLiteral("used-effort"));
        el.appendChild(elm);
        ResourceUsedEffortMap::ConstIterator i = m_usedEffort.constBegin();
        for (; i != m_usedEffort.constEnd(); ++i) {
            if (i.value() == 0) {
                continue;
            }
            QDomElement e = elm.ownerDocument().createElement(QStringLiteral("resource"));
            elm.appendChild(e);
            e.setAttribute(QStringLiteral("id"), i.key()->id());
            i.value()->saveXML(e);
        }
    }
}

//--------------
Completion::UsedEffort::UsedEffort()
{
}

Completion::UsedEffort::UsedEffort(const UsedEffort &e)
{
    mergeEffort(e);
}

Completion::UsedEffort::~UsedEffort()
{
}

void Completion::UsedEffort::mergeEffort(const Completion::UsedEffort &value)
{
    const QMap<QDate, ActualEffort> map = value.actualEffortMap();
    QMap<QDate, ActualEffort>::const_iterator it;
    for (it = map.constBegin(); it != map.constEnd(); ++it) {
        setEffort(it.key(), it.value());
    }
}

void Completion::UsedEffort::setEffort(QDate date, const ActualEffort &value)
{
    m_actual.insert(date, value);
}

Duration Completion::UsedEffort::effortTo(QDate date) const
{
    Duration eff;
    QMap<QDate, ActualEffort>::const_iterator it;
    for (it = m_actual.constBegin(); it != m_actual.constEnd() && it.key() <= date; ++it) {
        eff += it.value().effort();
    }
    return eff;
}

Duration Completion::UsedEffort::effort() const
{
    Duration eff;
    foreach (const ActualEffort &e, m_actual) {
        eff += e.effort();
    }
    return eff;
}

bool Completion::UsedEffort::operator==(const Completion::UsedEffort &e) const
{
    return m_actual == e.actualEffortMap();
}

bool Completion::UsedEffort::loadXML(KoXmlElement &element, XMLLoaderObject &)
{
    //debugPlan;
    KoXmlElement e;
    forEachElement(e, element) {
            if (e.tagName() == QLatin1String("actual-effort")) {
                QDate date = QDate::fromString(e.attribute(QStringLiteral("date")), Qt::ISODate);
                if (date.isValid()) {
                    ActualEffort a;
                    a.setNormalEffort(Duration::fromString(e.attribute(QStringLiteral("normal-effort"))));
                    a.setOvertimeEffort(Duration::fromString(e.attribute(QStringLiteral("overtime-effort"))));
                    setEffort(date, a);
                }
            }
    }
    return true;
}

void Completion::UsedEffort::saveXML(QDomElement &element) const
{
    if (m_actual.isEmpty()) {
        return;
    }
    DateUsedEffortMap::ConstIterator i = m_actual.constBegin();
    for (; i != m_actual.constEnd(); ++i) {
        QDomElement el = element.ownerDocument().createElement(QStringLiteral("actual-effort"));
        element.appendChild(el);
        el.setAttribute(QStringLiteral("overtime-effort"), i.value().overtimeEffort().toString());
        el.setAttribute(QStringLiteral("normal-effort"), i.value().normalEffort().toString());
        el.setAttribute(QStringLiteral("date"), i.key().toString(Qt::ISODate));
    }
}

//----------------------------------
WorkPackage::WorkPackage(Task *task)
    : m_task(task),
    m_manager(0),
    m_transmitionStatus(TS_None)
{
    m_completion.setNode(task);
}

WorkPackage::WorkPackage(const WorkPackage &wp)
    : m_task(0),
    m_manager(0),
    m_completion(wp.m_completion),
    m_ownerName(wp.m_ownerName),
    m_ownerId(wp.m_ownerId),
    m_transmitionStatus(wp.m_transmitionStatus),
    m_transmitionTime(wp.m_transmitionTime)
{
}

WorkPackage::~WorkPackage()
{
}

bool WorkPackage::loadXML(KoXmlElement &element, XMLLoaderObject &status)
{
    Q_UNUSED(status);
    m_ownerName = element.attribute(QStringLiteral("owner"));
    m_ownerId = element.attribute(QStringLiteral("owner-id"));
    return true;
}

void WorkPackage::saveXML(QDomElement &element) const
{
    QDomElement el = element.ownerDocument().createElement(QStringLiteral("workpackage"));
    element.appendChild(el);
    el.setAttribute(QStringLiteral("owner"), m_ownerName);
    el.setAttribute(QStringLiteral("owner-id"), m_ownerId);
}

bool WorkPackage::loadLoggedXML(KoXmlElement &element, XMLLoaderObject &status)
{
    m_ownerName = element.attribute(QStringLiteral("owner"));
    m_ownerId = element.attribute(QStringLiteral("owner-id"));
    m_transmitionStatus = transmitionStatusFromString(element.attribute(QStringLiteral("status")));
    m_transmitionTime = DateTime(QDateTime::fromString(element.attribute(QStringLiteral("time")), Qt::ISODate));
    return m_completion.loadXML(element, status);
}

void WorkPackage::saveLoggedXML(QDomElement &element) const
{
    QDomElement el = element.ownerDocument().createElement(QStringLiteral("workpackage"));
    element.appendChild(el);
    el.setAttribute(QStringLiteral("owner"), m_ownerName);
    el.setAttribute(QStringLiteral("owner-id"), m_ownerId);
    el.setAttribute(QStringLiteral("status"), transmitionStatusToString(m_transmitionStatus));
    el.setAttribute(QStringLiteral("time"), m_transmitionTime.toString(Qt::ISODate));
    m_completion.saveXML(el);
}

QList<Resource*> WorkPackage::fetchResources()
{
    return fetchResources(id());
}

QList<Resource*> WorkPackage::fetchResources(long id)
{
    //debugPlan<<m_task.name();
    QList<Resource*> lst;
    if (id == NOTSCHEDULED) {
        if (m_task) {
            lst << m_task->requestedResources();
        }
    } else {
        if (m_task) lst = m_task->assignedResources(id);
        foreach (const Resource *r, m_completion.resources()) {
            if (! lst.contains(const_cast<Resource*>(r))) {
                lst << const_cast<Resource*>(r);
            }
        }
    }
    return lst;
}

Completion &WorkPackage::completion()
{
    return m_completion;
}

const Completion &WorkPackage::completion() const
{
    return m_completion;
}


void WorkPackage::setScheduleManager(ScheduleManager *sm)
{
    m_manager = sm;
}


QString WorkPackage::transmitionStatusToString(WorkPackage::WPTransmitionStatus sts, bool trans)
{
    QString s = trans ? i18n("None") : QStringLiteral("None");
    switch (sts) {
        case TS_Send:
            s = trans ? i18n("Send") : QStringLiteral("Send");
            break;
        case TS_Receive:
            s = trans ? i18n("Receive") : QStringLiteral("Receive");
            break;
        case TS_Rejected:
            s = trans ? i18n("Rejected") : QStringLiteral("Rejected");
            break;
        default:
            break;
    }
    return s;
}

WorkPackage::WPTransmitionStatus WorkPackage::transmitionStatusFromString(const QString &sts)
{
    QStringList lst;
    lst << QStringLiteral("None") << QStringLiteral("Send") << QStringLiteral("Receive");
    int s = lst.indexOf(sts);
    return s < 0 ? TS_None : static_cast<WPTransmitionStatus>(s);
}

void WorkPackage::clear()
{
    //m_task = 0;
    m_manager = 0;
    m_ownerName.clear();
    m_ownerId.clear();
    m_transmitionStatus = TS_None;
    m_transmitionTime = DateTime();
    m_log.clear();

    m_completion = Completion();
    m_completion.setNode(m_task);

}

//--------------------------------
WorkPackageSettings::WorkPackageSettings()
    : usedEffort(true),
    progress(true),
    documents(true),
    remainingEffort(true)
{
}

void WorkPackageSettings::saveXML(QDomElement &element) const
{
    QDomElement el = element.ownerDocument().createElement(QStringLiteral("settings"));
    element.appendChild(el);
    el.setAttribute(QStringLiteral("used-effort"), QString::number(usedEffort));
    el.setAttribute(QStringLiteral("progress"), QString::number(progress));
    el.setAttribute(QStringLiteral("documents"), QString::number(documents));
    el.setAttribute(QStringLiteral("remaining-effort"), QString::number(remainingEffort));
}

bool WorkPackageSettings::loadXML(const KoXmlElement &element)
{
    usedEffort = (bool)element.attribute(QStringLiteral("used-effort")).toInt();
    progress = (bool)element.attribute(QStringLiteral("progress")).toInt();
    documents = (bool)element.attribute(QStringLiteral("documents")).toInt();
    remainingEffort = (bool)element.attribute(QStringLiteral("remaining-effort")).toInt();
    return true;
}

bool WorkPackageSettings::operator==(KPlato::WorkPackageSettings s) const
{
    return usedEffort == s.usedEffort &&
            progress == s.progress &&
            documents == s.documents &&
            remainingEffort == s.remainingEffort;
}

bool WorkPackageSettings::operator!=(KPlato::WorkPackageSettings s) const
{
    return ! operator==(s);
}


}  //KPlato namespace

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug dbg, const KPlato::Completion::UsedEffort::ActualEffort &ae)
{
    dbg << QStringLiteral("%1").arg(ae.normalEffort().toDouble(KPlato::Duration::Unit_h), 1);
    return dbg;
}
#endif
