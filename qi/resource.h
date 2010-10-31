/***************************************************************************
                          resource.h  -  description
                             -------------------
    begin                : Don Jul 13 20:27:22 CEST 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef RESOURCE_H
#define RESOURCE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <qglobal.h>

#if QT_VERSION >= 300
#define USE_QT3
#endif

#ifdef KDEAPP
#ifndef QIS_NO_STYLES
#define QIS_NO_STYLES 1
#endif
#endif

///////////////////////////////////////////////////////////////////
// General application values
#define IDS_APP_ABOUT               "QuiteInsane\nVersion " VERSION \
                                    "\n(w) 2000 by M. Herder"
#define IDS_STATUS_DEFAULT          "Ready."

#endif // RESOURCE_H
