/* This file is part of the KDE project
   Copyright (C) 1998, 1999, 2000 Torben Weis <weis@kde.org>

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
 * Boston, MA 02110-1301, USA.
*/

#include "kptfactory.h"
#include "kptmaindocument.h"
#include "kptpart.h"
#include "kptaboutdata.h"

#include <KoResourcePaths.h>
#include <KoDockRegistry.h>
#include <KoComponentData.h>

#include <kiconloader.h>


namespace KPlato
{

KoComponentData* Factory::s_global = 0L;
KAboutData* Factory::s_aboutData = 0L;

Factory::Factory()
    : KPluginFactory()
{
    global();
}

Factory::~Factory()
{
    delete s_aboutData;
    s_aboutData = 0L;
    delete s_global;
    s_global = 0L;
}

QObject* Factory::create( const char* /*iface*/, QWidget* /*parentWidget*/, QObject *parent,
                             const QVariantList& args, const QString& keyword )
{
    Q_UNUSED( args );
    Q_UNUSED( keyword );

    Part *part = new Part(parent);
    MainDocument *doc = new MainDocument(part);
    part->setDocument(doc);

    return part;
}

KAboutData* Factory::aboutData()
{
    if ( !s_aboutData )
        s_aboutData = newAboutData();
    return s_aboutData;
}

const KoComponentData &Factory::global()
{
    if ( !s_global )
    {
        debugPlan;
        s_global = new KoComponentData( *aboutData() );

        // Add any application-specific resource directories here
        KoResourcePaths::addResourceType("calligraplan_taskmodules", "data", "calligraplan/taskmodules/");

        // Tell the iconloader about share/apps/calligra/icons
        KIconLoader::global()->addAppDir("calligra");

        KoDockRegistry *dockRegistry = KoDockRegistry::instance();
        dockRegistry->remove("StencilBox"); //don't want this in plan
    }
    return *s_global;
}

} // KPlato namespace
