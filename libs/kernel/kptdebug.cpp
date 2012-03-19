/* This file is part of the KDE project
  Copyright (C) 2012 Dag Andersen <danders@get2net.dk>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version..

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.
*/

#include "kptdebug.h"

#include <kdeversion.h>

int planDbg() {
    #if KDE_IS_VERSION( 4, 3, 80 )
    static int s_area = KDebug::registerArea( "plan" );
    #else
    static int s_area = 0;
    #endif
    return s_area;
}

int planDependencyEditorDbg() {
    #if KDE_IS_VERSION( 4, 3, 80 )
    static int s_area = KDebug::registerArea( "plan (Dependency Editor)" );
    #else
    static int s_area = 0;
    #endif
    return s_area;
}
