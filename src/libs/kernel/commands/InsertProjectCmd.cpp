/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2004-2007 Dag Andersen <dag.andersen@kdemail.net>
 * SPDX-FileCopyrightText: 2011 Dag Andersen <dag.andersen@kdemail.net>
 * SPDX-FileCopyrightText: 2016 Dag Andersen <dag.andersen@kdemail.net>
 * SPDX-FileCopyrightText: 2019 Dag Andersen <dag.andersen@kdemail.net>
 * 
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

// clazy:excludeall=qstring-arg
#include "InsertProjectCmd.h"

#include "kptcommand.h"
#include "AddResourceCmd.h"
#include "AddParentGroupCmd.h"
#include "RemoveParentGroupCmd.h"
#include "RemoveResourceCmd.h"
// #include "kptaccount.h"
// #include "kptappointment.h"
// #include "kptproject.h"
// #include "kpttask.h"
// #include "kptcalendar.h"
// #include "kptrelation.h"
// #include "kptresource.h"
// #include "kptdocuments.h"
// #include "kptlocale.h"
#include "kptdebug.h"

#include <QApplication>

using namespace KPlato;


//----------------
InsertProjectCmd::InsertProjectCmd(Project &fromProject, Node *parent, Node *after, const KUndo2MagicString& name)
    : MacroCommand(name),
    m_project(static_cast<Project*>(parent->projectNode())),
    m_parent(parent)
{
    Q_ASSERT(&fromProject != m_project);

    if (m_project->defaultCalendar()) {
        fromProject.setDefaultCalendar(nullptr); // or else m_project default calendar may be overwritten
    }
    QString defaultAccount;
    if (! m_project->accounts().defaultAccount() && fromProject.accounts().defaultAccount()) {
        defaultAccount = fromProject.accounts().defaultAccount()->name();
    }

    QHash<Node*, QString> startupaccountmap;
    QHash<Node*, QString> shutdownaccountmap;
    QHash<Node*, QString> runningaccountmap;
    QHash<Node*, QString> nodecalendarmap;

    // remove unhandled info in tasks and get accounts and calendars
    const auto nodes = fromProject.allNodes();
    for (Node *n : nodes ) {
        if (n->type() == Node::Type_Task) {
            Task *t = static_cast<Task*>(n);
            t->workPackage().clear();
            while (! t->workPackageLog().isEmpty()) {
                WorkPackage *wp = t->workPackageLog().at(0);
                t->removeWorkPackage(wp);
                delete wp;
            }
        }
        if (n->startupAccount()) {
            startupaccountmap.insert(n, n->startupAccount()->name());
            n->setStartupAccount(nullptr);
        }
        if (n->shutdownAccount()) {
            shutdownaccountmap.insert(n, n->shutdownAccount()->name());
            n->setShutdownAccount(nullptr);
        }
        if (n->runningAccount()) {
            runningaccountmap.insert(n, n->runningAccount()->name());
            n->setRunningAccount(nullptr);
        }
        if (n->estimate()->calendar()) {
            nodecalendarmap.insert(n, n->estimate()->calendar()->id());
            n->estimate()->setCalendar(nullptr);
        }
    }
    // get resources pointing to calendars, accounts and parentgroups
    QHash<Resource*, QString> resaccountmap;
    QHash<Resource*, QString> rescalendarmap;
    QHash<Resource*, QList<AddParentGroupCmd*> > resparentgroupcmds;
    const auto resources = fromProject.resourceList();
    for (Resource *r : resources) {
        if (r->account()) {
            resaccountmap.insert(r, r->account()->name());
            r->setAccount(nullptr);
        }
        if (r->calendar()) {
            rescalendarmap.insert(r, r->calendar()->id());
            r->setCalendar(nullptr);
        }
        if (!m_project->findResource(r->id())) {
            // Need to move parentgroups to m_project groups
            // to avoid dangling pointers
            const auto groups = r->parentGroups();
            if (!groups.isEmpty()) {
                QList<AddParentGroupCmd*> cmds;
                for (const auto g : groups) {
                    auto group = m_project->findResourceGroup(g->id());
                    if (group) {
                        r->removeParentGroup(g);
                        cmds << new AddParentGroupCmd(r, group);
                    }
                }
                if (!cmds.isEmpty()) {
                    resparentgroupcmds.insert(r, cmds);
                }
            }
        }
    }
    // create add account commands and keep track of used and unused accounts
    QList<Account*> unusedAccounts;
    QMap<QString, Account*> accountsmap;
    const auto accounts = m_project->accounts().allAccounts();
    for (Account *a : accounts) {
        accountsmap.insert(a->name(), a);
    }
    const auto accounts2 = fromProject.accounts().accountList();
    for (Account *a : accounts2) {
        addAccounts(a, nullptr, unusedAccounts, accountsmap);
    }
    // create add calendar commands and keep track of used and unused calendars
    QList<Calendar*> unusedCalendars;
    QMap<QString, Calendar*> calendarsmap;
    const auto calendars = m_project->allCalendars();
    for (Calendar *c : calendars) {
        calendarsmap.insert(c->id(), c);
    }
    const auto calendars2 = fromProject.calendars();
    for (Calendar *c : calendars2) {
        addCalendars(c, nullptr, unusedCalendars, calendarsmap);
    }
    // get all requests from fromProject before resources are merged
    QMultiHash<Node*, QPair<ResourceRequest*, Resource*> > rreqs;
    const auto nodes2 = fromProject.allNodes();
    for (Node *n : nodes2 ) {
        if (n->type() != (int)Node::Type_Task || n->requests().isEmpty()) {
            continue;
        }
        while (ResourceRequest *rr = n->requests().resourceRequests(false).value(0)) {
            debugPlanInsertProject<<"Get resource request:"<<rr;
            rreqs.insert(n, QPair<ResourceRequest*, Resource*>(rr, rr->resource()));
            // all resource requests shall be reinserted
            rr->unregisterRequest();
            n->requests().removeResourceRequest(rr);
        }
    }
    debugPlanInsertProject<<"fromProject resource requests:"<<rreqs;
    QList<ResourceGroup*> allGroups;
    QList<ResourceGroup*> newGroups;
    QHash<ResourceGroup*, ResourceGroup*> existingGroups; // QHash<fromProject group, toProject group>
    const auto groups = fromProject.resourceGroups();
    for (ResourceGroup *g : groups) {
        ResourceGroup *gr = m_project->findResourceGroup(g->id());
        if (gr == nullptr) {
            gr = g;
            newGroups << gr;
        } else {
            existingGroups[ gr ] = g;
        }
        allGroups << gr;
    }
    debugPlanInsertProject<<"fromProject resources:"<<fromProject.resourceList();
    QList<Resource*> allResources;
    QList<Resource*> newResources; // resource in fromProject that does not exist in toProject
    QHash<Resource*, Resource*> existingResources; // hash[toProject resource, fromProject resource]
    const auto resources2 = fromProject.resourceList();
    for (Resource *r : resources2) {
        while (Schedule *s = r->schedules().values().value(0)) {
            r->deleteSchedule(s); // schedules not handled
        }
        Resource *res = m_project->resource(r->id());
        if (res == nullptr) {
            newResources << r;
            allResources << r;
        } else {
            existingResources[ res ] = r;
            allResources << res;
        }
    }
    for (auto r1 : qAsConst(allResources)) {
        for (auto r2 : qAsConst(allResources)) {
            if (r1 != r2 && r1->name() == r2->name()) {
                warnPlanInsertProject<<"Two resources with same name!"<<r1<<r2;
            }
        }
    }
    // Add new groups
    for (ResourceGroup *g : qAsConst(newGroups)) {
        debugPlanInsertProject<<"AddResourceGroupCmd:"<<g->name()<<g->resources();
        addCommand(new AddResourceGroupCmd(m_project, g, kundo2_noi18n(QStringLiteral("ResourceGroup"))));
        const auto resources = g->resources();
        for (Resource *r : resources) {
            addCommand(new AddParentGroupCmd(r, g));
        }
    }
    // Add new resources
    for (Resource *r : qAsConst(newResources)) {
        addCommand(new AddResourceCmd(m_project, r, kundo2_noi18n(QStringLiteral("Resource"))));
        // add any moved parent groups
        auto cmds = resparentgroupcmds.take(r);
        while (!cmds.isEmpty()) {
            addCommand(cmds.takeFirst());
        }
        debugPlanInsertProject<<"AddResourceCmd:"<<r->name()<<r->parentGroups();
    }
    // Update resource account
    {QHash<Resource*, QString>::const_iterator it = resaccountmap.constBegin();
    QHash<Resource*, QString>::const_iterator end = resaccountmap.constEnd();
    for (; it != end; ++it) {
        Resource *r = it.key();
        if (newResources.contains(r)) {
            Q_ASSERT(allResources.contains(r));
            addCommand(new ResourceModifyAccountCmd(*r, nullptr, accountsmap.value(it.value())));
        }
    }}
    // Update resource calendar
    {QHash<Resource*, QString>::const_iterator it = rescalendarmap.constBegin();
    QHash<Resource*, QString>::const_iterator end = rescalendarmap.constEnd();
    for (; it != end; ++it) {
        Resource *r = it.key();
        if (newResources.contains(r)) {
            Q_ASSERT(allResources.contains(r));
            addCommand(new ModifyResourceCalendarCmd(r, calendarsmap.value(it.value())));
        }
    }}
    debugPlanInsertProject<<"Requests: clean up requests to resources already in m_project";
    debugPlanInsertProject<<"All groups:"<<allGroups;
    debugPlanInsertProject<<"New groups:"<<newGroups;
    debugPlanInsertProject<<"Existing groups:"<<existingGroups;
    debugPlanInsertProject<<"All resources:"<<allResources;
    debugPlanInsertProject<<"New resources:"<<newResources;
    debugPlanInsertProject<<"Existing resources:"<<existingResources;
    debugPlanInsertProject<<"Resource requests:"<<rreqs;

    QHash<Node*, QPair<ResourceRequest*, Resource*> >::const_iterator i;
    for (i = rreqs.constBegin(); i != rreqs.constEnd(); ++i) {
        Node *n = i.key();
        ResourceRequest *rr = i.value().first;
        rr->setId(0);
        rr->setCollection(nullptr);
        Resource *newRes = i.value().second;
        debugPlanInsertProject<<"Resource exists:"<<newRes<<newRes->id()<<(void*)newRes<<':'<<m_project->resource(newRes->id());
        if (Resource *nr = existingResources.key(newRes)) {
            newRes = nr;
            debugPlanInsertProject<<"Resource existed:"<<newRes<<(void*)newRes;
        }
        debugPlanInsertProject<<"Add resource request:"<<"task:"<<n->wbsCode()<<n->name()<<"resource:"<<newRes<<"requests:"<<rr;
        if (!rr->requiredResources().isEmpty()) {
            // the resource request may have required resources that needs mapping
            QList<Resource*> required;
            const auto resources = rr->requiredResources();
            for (Resource *r : resources) {
                if (newResources.contains(r)) {
                    required << r;
                    debugPlanInsertProject<<"Request: required (new)"<<r->name();
                    continue;
                }
                Resource *r2 = existingResources.key(r);
                Q_ASSERT(allResources.contains(r2));
                if (r2) {
                    debugPlanInsertProject<<"Request: required (existing)"<<r2->name();
                    required << r2;
                }
            }
            rr->setRequiredResources(required);
        }
        if (!rr->alternativeRequests().isEmpty()) {
            const auto alts = rr->alternativeRequests();
            for (auto alt : alts) {
                auto resource = m_project->findResource(alt->resource()->id());
                if (resource) {
                    debugPlanInsertProject<<"swap alternative"<<alt->resource()<<"->"<<resource;
                    alt->setResource(resource);
                }
            }
        }
        Q_ASSERT(allResources.contains(newRes));
        // all resource requests shall be reinserted
        rr->setResource(newRes);
        addCommand(new AddResourceRequestCmd(&n->requests(), rr));
    }
    // Add nodes (ids are unique, no need to check)
    Node *node_after = after;
    for (int i = 0; i < fromProject.numChildren(); ++i) {
        Node *n = fromProject.childNode(i);
        Q_ASSERT(n);
        while (Schedule *s = n->schedules().values().value(0)) {
            n->takeSchedule(s); // schedules not handled
            delete s;
        }
        n->setParentNode(nullptr);
        if (node_after) {
            addCommand(new TaskAddCmd(m_project, n, node_after, kundo2_noi18n(QStringLiteral("Task"))));
            node_after = n;
        } else {
            addCommand(new SubtaskAddCmd(m_project, n, parent, kundo2_noi18n(QStringLiteral("Subtask"))));
        }
        addChildNodes(n);
    }
    // Dependencies:
    const auto nodes3 = fromProject.allNodes();
    for (Node *n : nodes3) {
        while (n->numDependChildNodes() > 0) {
            Relation *r = n->dependChildNodes().at(0);
            n->takeDependChildNode(r);
            r->child()->takeDependParentNode(r);
            addCommand(new AddRelationCmd(*m_project, r));
        }
    }
    // node calendar
    {QHash<Node*, QString>::const_iterator it = nodecalendarmap.constBegin();
    QHash<Node*, QString>::const_iterator end = nodecalendarmap.constEnd();
    for (; it != end; ++it) {
        addCommand(new ModifyEstimateCalendarCmd(*(it.key()), nullptr, calendarsmap.value(it.value())));
    }}
    // node startup account
    {QHash<Node*, QString>::const_iterator it = startupaccountmap.constBegin();
    QHash<Node*, QString>::const_iterator end = startupaccountmap.constEnd();
    for (; it != end; ++it) {
        addCommand(new NodeModifyStartupAccountCmd(*(it.key()), nullptr, accountsmap.value(it.value())));
    }}
    // node shutdown account
    {QHash<Node*, QString>::const_iterator it = shutdownaccountmap.constBegin();
    QHash<Node*, QString>::const_iterator end = shutdownaccountmap.constEnd();
    for (; it != end; ++it) {
        addCommand(new NodeModifyShutdownAccountCmd(*(it.key()), nullptr, accountsmap.value(it.value())));
    }}
    // node running account
    {QHash<Node*, QString>::const_iterator it = runningaccountmap.constBegin();
    QHash<Node*, QString>::const_iterator end = runningaccountmap.constEnd();
    for (; it != end; ++it) {
        addCommand(new NodeModifyRunningAccountCmd(*(it.key()), nullptr, accountsmap.value(it.value())));
    }}

    if (! defaultAccount.isEmpty()) {
        Account *a = accountsmap.value(defaultAccount);
        if (a && a->list()) {
            addCommand(new ModifyDefaultAccountCmd(m_project->accounts(), nullptr, a));
        }
    }
    debugPlanInsertProject<<"Cleanup unused stuff from inserted project:"<<&fromProject;
    // Cleanup
    // Remove nodes from fromProject so they are not deleted
    while (Node *ch = fromProject.childNode(0)) {
        fromProject.takeChildNode(ch);
    }
    const auto nodes4 = fromProject.allNodes();
    for (Node *n : nodes4) {
        fromProject.removeId(n->id());
    }

    // Remove calendars from fromProject
    while (fromProject.calendarCount() > 0) {
        fromProject.takeCalendar(fromProject.calendarAt(0));
    }
    qDeleteAll(unusedCalendars);

    // Remove accounts from fromProject
    while (fromProject.accounts().accountCount() > 0) {
        fromProject.accounts().take(fromProject.accounts().accountAt(0));
    }
    qDeleteAll(unusedAccounts);

    for (Resource *r = fromProject.resourceList().value(0); r; r = fromProject.resourceList().value(0)) {
        debugPlanInsertProject<<"remove all resources from fromProject:"<<r;
        if (!fromProject.takeResource(r)) {
            errorPlanInsertProject<<"Internal error, failed to remove resource";
            Q_ASSERT(false);
            break;
        }
    }
    while (fromProject.numResourceGroups() > 0) {
        ResourceGroup *g = fromProject.resourceGroupAt(0);
        debugPlanInsertProject<<"Take used group:"<<g;
        fromProject.takeResourceGroup(g);
    }
    debugPlanInsertProject<<"Delete unused resources:"<<existingResources;
    qDeleteAll(existingResources); // deletes unused resources
    debugPlanInsertProject<<"Delete unused groups:"<<existingGroups;
    qDeleteAll(existingGroups); // deletes unused resource groups
}

void InsertProjectCmd::addCalendars(Calendar *calendar, Calendar *parent, QList<Calendar*> &unused, QMap<QString, Calendar*> &calendarsmap) {
    Calendar *par = nullptr;
    if (parent) {
        par = calendarsmap.value(parent->id());
    }
    if (par == nullptr) {
        par = parent;
    }
    Calendar *cal = calendarsmap.value(calendar->id());
    if (cal == nullptr) {
        calendarsmap.insert(calendar->id(), calendar);
        addCommand(new CalendarAddCmd(m_project, calendar, -1, par));
    } else {
        unused << calendar;
    }
    const auto calendars = calendar->calendars();
    for (Calendar *c : calendars) {
        addCalendars(c, calendar, unused, calendarsmap);
    }
}

void InsertProjectCmd::addAccounts(Account *account, Account *parent, QList<Account*>  &unused, QMap<QString, Account*>  &accountsmap) {
    Account *par = nullptr;
    if (parent) {
        par = accountsmap.value(parent->name());
    }
    if (par == nullptr) {
        par = parent;
    }
    Account *acc = accountsmap.value(account->name());
    if (acc == nullptr) {
        accountsmap.insert(account->name(), account);
        addCommand(new AddAccountCmd(*m_project, account, par, -1, kundo2_noi18n("Add account %1", account->name())));
    } else {
        unused << account;
    }
    while (! account->accountList().isEmpty()) {
        Account *a = account->accountList().first();
        account->list()->take(a);
        addAccounts(a, account, unused, accountsmap);
    }
}

void InsertProjectCmd::addChildNodes(Node *node) {
    // schedules not handled
    while (Schedule *s = node->schedules().values().value(0)) {
        node->takeSchedule(s); // schedules not handled
        delete s;
    }
    const auto nodes = node->childNodeIterator();
    for (Node *n : nodes ) {
        n->setParentNode(nullptr);
        addCommand(new SubtaskAddCmd(m_project, n, node));
        addChildNodes(n);
    }
    // Remove child nodes so they are not added twice
    while (Node *ch = node->childNode(0)) {
        node->takeChildNode(ch);
    }
}

void InsertProjectCmd::execute()
{
    //debugPlanInsertProject<<"before execute:"<<m_project->resourceGroups()<<m_project->resourceList();
    MacroCommand::execute();
    //debugPlanInsertProject<<"after execute:"<<m_project->resourceGroups()<<m_project->resourceList();
}
void InsertProjectCmd::unexecute()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    MacroCommand::unexecute();
    QApplication::restoreOverrideCursor();
}

