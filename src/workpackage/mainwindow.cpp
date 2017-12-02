/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
   Copyright (C) 1999 Simon Hausmann <hausmann@kde.org>
   Copyright (C) 2000-2005 David Faure <faure@kde.org>
   Copyright (C) 2005, 2006 Sven Lüppken <sven@kde.org>
   Copyright (C) 2008 - 2009, 2012 Dag Andersen <danders@get2net.dk>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "mainwindow.h"
#include "part.h"
#include "view.h"

#include "kptdocuments.h"

#include <QSplitter>
#include <QLabel>
#include <QWidget>
#include <QFileDialog>
#include <QApplication>

#include <kundo2qstack.h>

#include <assert.h>
//#include "koshellsettings.h"

#include <KoDocument.h>

#include <KLocalizedString>
#include <kmessagebox.h>
#include <kactioncollection.h>

#include <ktoolinvocation.h>
#include <KIO/StatJob>
#include <kxmlguiwindow.h>

#include <KoDocumentInfo.h>
#include <KoView.h>
#include <KoFilterManager.h>

#include "debugarea.h"

KPlatoWork_MainWindow::KPlatoWork_MainWindow()
    : KParts::MainWindow()
{
    debugPlanWork<<this;

    m_part = new KPlatoWork::Part( this, this );

    KStandardAction::quit(qApp, SLOT(quit()), actionCollection());
 
    KStandardAction::open(this, SLOT(slotFileOpen()), actionCollection());

//     KStandardAction::save(this, SLOT(slotFileSave()), actionCollection());

    QAction *a = KStandardAction::undo(m_part->undoStack(), SLOT(undo()), actionCollection());
    a->setEnabled( false );
    connect( m_part->undoStack(), SIGNAL(canUndoChanged(bool)), a, SLOT(setEnabled(bool)) );

    a = KStandardAction::redo(m_part->undoStack(), SLOT(redo()), actionCollection());
    a->setEnabled( false );
    connect( m_part->undoStack(), SIGNAL(canRedoChanged(bool)), a, SLOT(setEnabled(bool)) );
    
    setCentralWidget( m_part->widget() );
    setupGUI( KXmlGuiWindow::ToolBar | KXmlGuiWindow::Keys | KXmlGuiWindow::StatusBar | KXmlGuiWindow::Save);
    createGUI( m_part );
    connect( m_part, SIGNAL(captionChanged(QString,bool)), SLOT(setCaption(QString,bool)) );
}


KPlatoWork_MainWindow::~KPlatoWork_MainWindow()
{
    debugPlanWork;
}

void KPlatoWork_MainWindow::setCaption( const QString & )
{
    KParts::MainWindow::setCaption( QString() );
}

void KPlatoWork_MainWindow::setCaption( const QString &, bool modified )
{
    KParts::MainWindow::setCaption( QString(), modified );
}

bool KPlatoWork_MainWindow::openDocument(const QUrl & url)
{
    // TODO: m_part->openUrl will find out about this as well, no?
    KIO::StatJob* statJob = KIO::stat( url );
    statJob->setSide(  KIO::StatJob::SourceSide );

    const bool isUrlReadable = statJob->exec();

    if (! isUrlReadable) {
        KMessageBox::error(0L, i18n("The file %1 does not exist.", url.url()));
//        d->recent->removeUrl(url); //remove the file from the recent-opened-file-list
//        saveRecentFiles();
        return false;
    }
    return m_part->openUrl( url );
}

QString KPlatoWork_MainWindow::configFile() const
{
  //return readConfigFile( QStandardPaths::locate(QStandardPaths::GenericDataLocation "koshell/koshell_shell.rc" ) );
  return QString(); // use UI standards only for now
}

//called from slotFileSave(), slotFileSaveAs(), queryClose(), slotEmailFile()
bool KPlatoWork_MainWindow::saveDocument( bool saveas, bool silent )
{
    debugPlanWork<<saveas<<silent;
    KPlatoWork::Part *doc = rootDocument();
    if ( doc == 0 ) {
        return true;
    }
    return doc->saveWorkPackages( silent );
}


bool KPlatoWork_MainWindow::queryClose()
{
    KPlatoWork::Part *part = rootDocument();
    if ( part == 0 ) {
        return true;
    }
    return part->queryClose();
}

void KPlatoWork_MainWindow::slotFileClose()
{
    if (queryClose()) {
    }
}

void KPlatoWork_MainWindow::slotFileSave()
{
    saveDocument();
}

void KPlatoWork_MainWindow::slotFileOpen()
{
    const QUrl file = QFileDialog::getOpenFileUrl( 0, QString(), QUrl(), "*.planwork" );
    if ( ! file.isEmpty() ) {
        openDocument( file );
    }
}
