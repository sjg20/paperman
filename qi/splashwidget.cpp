/***************************************************************************
                          splashwidget.cpp  -  description
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

#include "splashwidget.h"
#include "quiteinsane_logo.xpm"

#include <qapplication.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qpixmap.h>

SplashWidget::SplashWidget(QWidget *parent, const char *name )
             : QWidget(parent,name,WType_TopLevel|WStyle_Tool|WStyle_Customize|
                                   WStyle_NoBorder)
{
  QGridLayout* grid = new QGridLayout(this,1,2);
  grid->setMargin(10);
  grid->setSpacing(8);
  QLabel* label1 = new QLabel(this);
  QPixmap qp((const char **)quiteinsane_logo_xpm);
  label1->setPixmap(qp);
  QLabel* label2 = new QLabel(tr("<qt><h2><center><b>QuiteInsane</b></center></h2><br>"
                                 "<center><nobr>Searching available devices</nobr></center><br>"
                                 "<center><nobr>...please wait...</nobr></center></qt>"),this);
  grid->addWidget(label1,0,0);
  grid->addWidget(label2,0,1);
}
SplashWidget::~SplashWidget()
{
}
/** No descriptions */
void SplashWidget::show()
{
  int x,y;
  x = (qApp->desktop()->width() - sizeHint().width())/2;
  y = (qApp->desktop()->height() - sizeHint().height())/2;
  move(x,y);
  QWidget::show();
}
