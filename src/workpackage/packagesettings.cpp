/* This file is part of the KDE project
   Copyright (C) 2009 Dag Andersen <danders@get2net.dk>

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

// clazy:excludeall=qstring-arg
#include "packagesettings.h"
#include "workpackage.h"

#include <QComboBox>

#include <KLocalizedString>


namespace KPlatoWork
{

PackageSettingsDialog::PackageSettingsDialog(WorkPackage &p, QWidget *parent, bool enableok)
    : KoDialog(parent)
{
    setCaption( i18n("Work Package Settings") );
    setButtons( Ok|Cancel );
    setDefaultButton( Ok );
    showButtonSeparator( true );
    //debugPlanWork<<&p;

    dia = new PackageSettingsPanel(p, this);

    setMainWidget(dia);
    enableButtonOk(false);

    if (!enableok) {
        connect(dia, &PackageSettingsPanel::changed, this, &KoDialog::enableButtonOk);
    }
}

KUndo2Command *PackageSettingsDialog::buildCommand()
{
    //debugPlanWork;
    return dia->buildCommand();
}


PackageSettingsPanel::PackageSettingsPanel(WorkPackage &p, QWidget *parent)
    : QWidget(parent),
      m_package( p )
{
    setupUi(this);

    setSettings( p.settings() );

    connect( ui_usedEffort, &QCheckBox::stateChanged, this, &PackageSettingsPanel::slotChanged );
    connect( ui_progress, &QCheckBox::stateChanged, this, &PackageSettingsPanel::slotChanged );
    connect( ui_documents, &QCheckBox::stateChanged, this, &PackageSettingsPanel::slotChanged );

    // FIXME: Use sematic markup
    ui_explain->setText(i18n("Package: %1\nThese settings indicates to the receiver of the package which information is relevant.", p.name()));
}

KUndo2Command *PackageSettingsPanel::buildCommand()
{
    //debugPlanWork;
    WorkPackageSettings s = settings();
    if ( s == m_package.settings() ) {
        return 0;
    }
    return new ModifyPackageSettingsCmd( &m_package, s, kundo2_i18n( "Modify package settings" ) );
}

void PackageSettingsPanel::slotChanged() {
    emit changed( settings() != m_package.settings() );
}

WorkPackageSettings PackageSettingsPanel::settings() const
{
    WorkPackageSettings s;
    s.usedEffort = ui_usedEffort->checkState() == Qt::Checked;
    s.progress = ui_progress->checkState() == Qt::Checked;
    s.documents = ui_documents->checkState() == Qt::Checked;
    return s;
}

void PackageSettingsPanel::setSettings( const WorkPackageSettings &s )
{
    ui_usedEffort->setCheckState( s.usedEffort ? Qt::Checked : Qt::Unchecked );
    ui_progress->setCheckState( s.progress ? Qt::Checked : Qt::Unchecked );
    ui_documents->setCheckState( s.documents ? Qt::Checked : Qt::Unchecked );
}

}  //KPlatoWork namespace
