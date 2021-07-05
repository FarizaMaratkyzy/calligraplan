/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Dag Andersen <dag.andersen@kdemail.net>
 * SPDX-FileCopyrightText: 2019 Dag Andersen <dag.andersen@kdemail.net>
 * 
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef ICALENDAREXPORT_H
#define ICALENDAREXPORT_H


#include <KoFilter.h>

#define USE_KCALCORE

#ifdef USE_KCALCORE
#include <kcalcore/calendar.h>
#endif

#include <QObject>
#include <QVariantList>

class QFile;
class QByteArray;


namespace KPlato
{
class Project;
class Node;
}

class ICalendarExport : public KoFilter
{

    Q_OBJECT

public:
    ICalendarExport(QObject* parent, const QVariantList &);
    ~ICalendarExport() override {}

    KoFilter::ConversionStatus convert(const QByteArray& from, const QByteArray& to) override;

protected:
    KoFilter::ConversionStatus convert(const KPlato::Project &project, QFile &file);
#ifdef USE_KCALCORE
    void createTodos(KCalCore::Calendar::Ptr cal, const KPlato::Node *node, long id, KCalCore::Todo::Ptr parent = KCalCore::Todo::Ptr());
#endif

private:
    QString doNode(const KPlato::Node *node, long sid);
    QString createTodo(const KPlato::Node &node, long sid);
    QString doDescription(const QString &description);

private:
    long m_scheduleId;
    bool m_includeProject;
    bool m_includeSummarytasks;
    QList<QString> m_descriptions;
};

#endif // ICALENDAREXPORT_H
