/* This file is part of the KDE project
   Copyright (C) 2009, 2011 Dag Andersen <danders@get2net.dk>

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
   Boston, MA 02110-1301, USA.
*/

#include "taskcompletiondialog.h"
#include "workpackage.h"

#include "kptusedefforteditor.h"
#include "kptcommand.h"
#include "kptitemmodelbase.h"

#include <QComboBox>

#include <kdebug.h>
#include <klocale.h>

using namespace KPlato;

namespace KPlatoWork
{

TaskCompletionDialog::TaskCompletionDialog(WorkPackage &p, ScheduleManager *sm, QWidget *parent)
    : KDialog(parent)
{
    setCaption( i18n("Task Progress") );
    setButtons( Ok|Cancel );
    setDefaultButton( Ok );
    showButtonSeparator( true );
    m_panel = new TaskCompletionPanel( p, sm, this);

    setMainWidget(m_panel);

    enableButtonOk(false);

    connect(m_panel, SIGNAL( changed( bool ) ), SLOT(slotChanged( bool )));
}

void TaskCompletionDialog::slotChanged( bool )
{
    enableButtonOk( true );
}

KUndo2Command *TaskCompletionDialog::buildCommand()
{
    //kDebug();
    return m_panel->buildCommand();
}


TaskCompletionPanel::TaskCompletionPanel(WorkPackage &p, ScheduleManager *sm, QWidget *parent)
    : QWidget(parent),
      m_package( &p )
{
    //kDebug();
    setupUi(this);

    CompletionEntryItemModel *m = new CompletionEntryItemModel( this );
    entryTable->setItemDelegateForColumn ( 1, new ProgressBarDelegate( this ) );
    entryTable->setItemDelegateForColumn ( 2, new DurationSpinBoxDelegate( this ) );
    entryTable->setItemDelegateForColumn ( 3, new DurationSpinBoxDelegate( this ) );
    entryTable->setCompletionModel( m );

    Task *task = qobject_cast<Task*>( p.node() );
    m_completion = task->completion();
    started->setChecked(m_completion.isStarted());
    finished->setChecked(m_completion.isFinished());
    startTime->setDateTime(m_completion.startTime());
    finishTime->setDateTime(m_completion.finishTime());
    finishTime->setMinimumDateTime( qMax( startTime->dateTime(), QDateTime(m_completion.entryDate(), QTime() ) ) );
    
    scheduledEffort = p.node()->estimate()->expectedValue();
    
    if ( m_completion.usedEffortMap().isEmpty() || task->requests().isEmpty() ) {
        foreach ( ResourceGroupRequest *g, task->requests().requests() ) {
            foreach ( ResourceRequest *r, g->resourceRequests() ) {
                m_completion.addUsedEffort( r->resource() );
            }
        }
    }
    int mode = m_completion.entrymode();
    mode = mode > 1 ? 1 : 0;
    editmode->setCurrentIndex( mode );

    enableWidgets();
    started->setFocus();
    
    entryTable->model()->setManager( sm );
    entryTable->model()->setTask( task );
    entryTable->setCompletion( &m_completion );

    connect( editmode, SIGNAL( currentIndexChanged( int ) ), SLOT( slotEditmodeChanged( int ) ) );
    connect( editmode, SIGNAL( activated( int ) ), SLOT( slotChanged() ) );

    connect( addEntryBtn, SIGNAL( clicked() ), entryTable, SLOT( addEntry() ) );
    connect( removeEntryBtn, SIGNAL( clicked() ), entryTable, SLOT( removeEntry() ) );

    connect( entryTable, SIGNAL( rowInserted( const QDate ) ), SLOT( slotEntryAdded( const QDate ) ) );
    connect(entryTable, SIGNAL(changed() ), SLOT( slotChanged() ) );
    connect(entryTable, SIGNAL(changed() ), SLOT( slotEntryChanged() ) );
    connect(entryTable, SIGNAL(rowInserted( const QDate ) ), SLOT( slotChanged() ) );
    connect(entryTable, SIGNAL(rowInserted( const QDate ) ), SLOT( slotEntryChanged() ) );
    connect(entryTable, SIGNAL(rowRemoved( const QDate ) ), SLOT( slotEntryChanged() ) );
    connect(entryTable, SIGNAL(selectionChanged( const QItemSelection&, const QItemSelection& ) ), SLOT( slotSelectionChanged( const QItemSelection& ) ) );
    

    connect(started, SIGNAL(toggled(bool)), SLOT(slotStartedChanged(bool)));
    connect(started, SIGNAL(toggled(bool)), SLOT(slotChanged()));
    connect(finished, SIGNAL(toggled(bool)), SLOT(slotFinishedChanged(bool)));
    connect(finished, SIGNAL(toggled(bool)), SLOT(slotChanged()));

    connect(startTime, SIGNAL(dateTimeChanged(const QDateTime &)), SLOT(slotChanged()));
    connect(startTime, SIGNAL(dateTimeChanged(const QDateTime &)), SLOT(slotStartTimeChanged( const QDateTime& )));
    connect(finishTime, SIGNAL(dateTimeChanged(const QDateTime &)), SLOT(slotChanged()));
    connect(finishTime, SIGNAL(dateTimeChanged(const QDateTime &)), SLOT(slotFinishTimeChanged( const QDateTime& )));

    removeEntryBtn->setEnabled( false );

    // TODO edit mode

}

KUndo2Command *TaskCompletionPanel::buildCommand()
{
    MacroCommand *cmd = new MacroCommand( i18nc("(qtundo-format)", "Modify task completion") );
    Completion &org = m_package->task()->completion();
    if ( org.entrymode() != m_completion.entrymode() ) {
        cmd->addCommand( new ModifyCompletionEntrymodeCmd(org, m_completion.entrymode() ) );
    }
    if ( org.isStarted() != m_completion.isStarted() ) {
        cmd->addCommand( new ModifyCompletionStartedCmd(org, m_completion.isStarted() ) );
    }
    if ( org.isFinished() != m_completion.isFinished() ) {
        cmd->addCommand( new ModifyCompletionFinishedCmd(org, m_completion.isFinished() ) );
    }
    if ( org.startTime() != m_completion.startTime() ) {
        cmd->addCommand( new ModifyCompletionStartTimeCmd(org, m_completion.startTime() ) );
    }
    if ( org.finishTime() != m_completion.finishTime() ) {
        cmd->addCommand( new ModifyCompletionFinishTimeCmd(org, m_completion.finishTime() ) );
    }
    QList<QDate> orgdates = org.entries().keys();
    QList<QDate> m_completiondates = m_completion.entries().keys();
    foreach ( const QDate &d, orgdates ) {
        if ( m_completiondates.contains( d ) ) {
            if ( m_completion.entry( d ) == org.entry( d ) ) {
                continue;
            }
            Completion::Entry *e = new Completion::Entry( *( m_completion.entry( d ) ) );
            cmd->addCommand( new ModifyCompletionEntryCmd(org, d, e ) );
        } else {
            cmd->addCommand( new RemoveCompletionEntryCmd(org, d ) );
        }
    }
    foreach ( const QDate &d, m_completiondates ) {
        if ( ! orgdates.contains( d ) ) {
            Completion::Entry *e = new Completion::Entry( * ( m_completion.entry( d ) ) );
            cmd->addCommand( new AddCompletionEntryCmd(org, d, e ) );
        }
    }

    if ( cmd->isEmpty() ) {
        delete cmd;
        return 0;
    }
    return cmd;
}

void TaskCompletionPanel::slotChanged()
{
    emit changed( true ); //FIXME
}

void TaskCompletionPanel::optionChanged( int id )
{
    m_completion.setEntrymode( static_cast<Completion::Entrymode>( id ) );
    entryTable->model()->slotDataChanged();
    enableWidgets();
}

void TaskCompletionPanel::slotStartedChanged(bool state) {
    m_completion.setStarted( state );
    if (state) {
        m_completion.setStartTime( KDateTime::currentLocalDateTime() );
        startTime->setDateTime( m_completion.startTime() );
        slotCalculateEffort();
    }
    enableWidgets();
}

void TaskCompletionPanel::setFinished() {
    finishTime->setDateTime( QDateTime::currentDateTime() );
    slotFinishTimeChanged( finishTime->dateTime() );
}

void TaskCompletionPanel::slotFinishedChanged(bool state) {
    kDebug()<<state;
    m_completion.setFinished( state );
    if (state) {
        kDebug()<<state;
        setFinished();
        kDebug()<<finishTime->dateTime();
        slotCalculateEffort();
    }   
    enableWidgets();
}

void TaskCompletionPanel::slotFinishTimeChanged( const QDateTime &dt )
{
    if ( ! m_completion.isFinished() ) {
        return;
    }
    m_completion.setFinishTime( KDateTime( dt, KDateTime::Spec(KDateTime::LocalZone) ) );
    if ( m_completion.percentFinished() == 100 ) {
        m_completion.takeEntry( m_completion.entryDate() );
    }
    if ( m_completion.percentFinished() < 100 ) {
        m_completion.setPercentFinished( dt.date(), 100 );
    }
    entryTable->setCompletion( &m_completion ); // for refresh
}

void TaskCompletionPanel::slotStartTimeChanged( const QDateTime &dt )
{
    m_completion.setStartTime( KDateTime( dt, KDateTime::Spec(KDateTime::LocalZone) ) );
    finishTime->setMinimumDateTime( qMax( startTime->dateTime(), QDateTime(m_completion.entryDate(), QTime() ) ) );
    
}

void TaskCompletionPanel::slotEntryChanged()
{
    finishTime->setMinimumDateTime( qMax( startTime->dateTime(), QDateTime(m_completion.entryDate(), QTime() ) ) );
}

void TaskCompletionPanel::enableWidgets() {
    started->setEnabled(!finished->isChecked());
    finished->setEnabled(started->isChecked());
    finishTime->setEnabled(finished->isChecked());
    startTime->setEnabled(started->isChecked() && !finished->isChecked());
}


void TaskCompletionPanel::slotPercentFinishedChanged( int ) {
    slotCalculateEffort();
}

void TaskCompletionPanel::slotCalculateEffort()
{
}

void TaskCompletionPanel::slotEntryAdded( const QDate date )
{
    kDebug()<<date;
}

void TaskCompletionPanel::slotSelectionChanged( const QItemSelection &sel )
{
    removeEntryBtn->setEnabled( !sel.isEmpty() );
}

void TaskCompletionPanel::slotEditmodeChanged( int index )
{
    m_completion.setEntrymode( index == 0 ? Completion::EnterCompleted : Completion::EnterEffortPerTask );
    emit changed( true );
}

//-------------------
CompletionEntryItemModel::CompletionEntryItemModel( QObject *parent )
    : KPlato::CompletionEntryItemModel( parent )
{
    m_headers << "Accumulated";
}

int CompletionEntryItemModel::columnCount( const QModelIndex& ) const
{
    return 6;
}

QVariant CompletionEntryItemModel::actualEffort ( int row, int role ) const
{
    Completion::Entry *e = m_completion->entry( date( row ).toDate() );
    if ( e == 0 ) {
        return QVariant();
    }
    switch ( role ) {
        case Qt::DisplayRole:
        case Qt::ToolTipRole: {
            Duration v = e->totalPerformed;
            if ( row > 0 ) {
                v -= m_completion->entry( date( row - 1 ).toDate() )->totalPerformed;
            }
            //kDebug()<<m_node->name()<<": "<<v<<" "<<unit<<" : "<<scales<<endl;
            return v.format();
        }
        case Qt::EditRole: {
            Duration v = e->totalPerformed;
            if ( row > 0 ) {
                v -= m_completion->entry( date( row - 1 ).toDate() )->totalPerformed;
            }
            //kDebug()<<m_node->name()<<": "<<v<<" "<<unit<<" : "<<scales<<endl;
            return v.toDouble( Duration::Unit_h );
        }
        case Role::DurationScales: {
            QVariantList lst;
            lst << 24 << 60 << 60 << 1000;
            return lst;
        }
        case Role::DurationUnit:
            return static_cast<int>( Duration::Unit_h );
        case Role::Minimum:
            return static_cast<int>( Duration::Unit_h );
        case Role::Maximum:
            return static_cast<int>( Duration::Unit_h );
        case Qt::StatusTipRole:
        case Qt::WhatsThisRole:
            return QVariant();
        default:
            break;
    }
    return QVariant();
}

QVariant CompletionEntryItemModel::data( const QModelIndex &idx, int role ) const
{
    if ( idx.column() == Property_ActualAccumulated ) {
        switch ( role ) {
            case Qt::DisplayRole: {
                    Duration v;
                    Completion::Entry *e = m_completion->entry( date( idx.row() ).toDate() );
                    if ( e ) {
                        v = e->totalPerformed;
                    }
                    return v.format();
                }
            default:
                return QVariant();
        }
    }
    return KPlato::CompletionEntryItemModel::data( idx, role );
}

bool CompletionEntryItemModel::setData( const QModelIndex &idx, const QVariant &value, int role )
{
    //kDebug();
    switch ( role ) {
        case Qt::EditRole: {
            if ( idx.column() == Property_Date ) {
                QDate od = date( idx.row() ).toDate();
                removeEntry( od );
                addEntry( value.toDate() );
                // emit dataChanged( idx, idx );
                return true;
            }
            if ( idx.column() == Property_Completion ) {
                Completion::Entry *e = m_completion->entry( date( idx.row() ).toDate() );
                if ( e == 0 ) {
                    return false;
                }
                e->percentFinished = value.toInt();
                if ( m_completion->entrymode() == Completion::EnterCompleted && m_node ) {
                    // calculate used/remaining
                    Duration est = m_node->plannedEffort( id(), ECCT_EffortWork );
                    e->totalPerformed = est * e->percentFinished / 100;
                    e->remainingEffort = est - e->totalPerformed;
                }
                emit dataChanged( idx, createIndex( idx.row(), 3 ) );
                return true;
            }
            if ( idx.column() == Property_ActualEffort ) {
                Completion::Entry *e = m_completion->entry( date( idx.row() ).toDate() );
                if ( e == 0 ) {
                    return false;
                }
                Duration prev;
                if ( idx.row() > 0 ) {
                    Completion::Entry *pe = m_completion->entry( date( idx.row() - 1 ).toDate() );
                    if ( pe ) {
                        prev = pe->totalPerformed;
                    }
                }
                double v( value.toList()[0].toDouble() );
                Duration::Unit unit = static_cast<Duration::Unit>( value.toList()[1].toInt() );
                Duration d = Estimate::scale( v, unit, scales() );
                if ( d + prev == e->totalPerformed ) {
                    return false;
                }
                e->totalPerformed = d + prev;
                emit dataChanged( idx, idx );
                return true;
            }
            if ( idx.column() == Property_RemainigEffort ) {
                Completion::Entry *e = m_completion->entry( date( idx.row() ).toDate() );
                if ( e == 0 ) {
                    return false;
                }
                double v( value.toList()[0].toDouble() );
                Duration::Unit unit = static_cast<Duration::Unit>( value.toList()[1].toInt() );
                Duration d = Estimate::scale( v, unit, scales() );
                if ( d == e->remainingEffort ) {
                    return false;
                }
                e->remainingEffort = d;
                kDebug()<<value<<d.format()<<e->remainingEffort.format();
                emit dataChanged( idx, idx );
                return true;
            }
            break;
        }
        default: break;
    }
    return false;
}

}  //KPlatoWork namespace

#include "taskcompletiondialog.moc"
