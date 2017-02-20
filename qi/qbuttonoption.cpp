/***************************************************************************
                          qbuttonoption.cpp  -  description
                             -------------------
    begin                : Thu Sep 7 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#include "qbuttonoption.h"
#include <qpixmap.h>
#include <qlabel.h>
#include <qstring.h>
#include <qlayout.h>
#include <qpushbutton.h>
//Added by qt3to4:
#include <QGridLayout>
#include <sane/saneopts.h>

QButtonOption::QButtonOption(QString title,QWidget *parent, const char *name )
              :QSaneOption(title,parent,name)
{
  initWidget();
}
QButtonOption::~QButtonOption()
{
}
/**  */
void QButtonOption::initWidget()
{
    QGridLayout* qgl = new QGridLayout(this);
  mpTitleLabel = new QLabel(optionTitle(),this);
	mpOptionButton = new QPushButton(tr("Set Option"),this);
  connect(mpOptionButton,SIGNAL(clicked()),
          this,SLOT(slotEmitOptionChanged()));
//create pixmap
  assignPixmap();
  if(pixmapWidget())
	  qgl->addWidget(pixmapWidget(),0,0);
	qgl->addWidget(mpTitleLabel,0,1);
	qgl->addWidget(mpOptionButton,0,2);
  qgl->setSpacing(5);
    qgl->setColumnStretch(1,1);
	qgl->activate();
}
