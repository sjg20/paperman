/***************************************************************************
                          splashwidget.h  -  description
                             -------------------
    begin                : Sat Nov 23 2002
    copyright            : (C) 2002 by Michael Herder
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

#ifndef SPLASHWIDGET_H
#define SPLASHWIDGET_H

#include <qwidget.h>

/**
  *@author Michael Herder
  */

class SplashWidget : public QWidget
{
   Q_OBJECT
public: 
  SplashWidget(QWidget *parent=0, const char *name=0);
  ~SplashWidget();
  /** No descriptions */
  void show();
};

#endif
