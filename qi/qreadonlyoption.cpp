/***************************************************************************
                          qreadonlyoption.cpp  -  description
                             -------------------
    begin                : Sat Feb 17 2001
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

#include "qreadonlyoption.h"
#include <qlabel.h>
#include <qstring.h>
#include <qlayout.h>
#include <QGridLayout>

QReadOnlyOption::QReadOnlyOption(QString title,QWidget *parent, const char *name )
                :QSaneOption(title,parent,name)
{
  mTitleText = title;
  initWidget();
}
QReadOnlyOption::~QReadOnlyOption()
{
}
/**  */
void QReadOnlyOption::initWidget()
{
    QGridLayout* qgl = new QGridLayout(this);
  qgl->setMargin(4);
  qgl->setSpacing(4);
  mpTitleLabel = new QLabel(mTitleText,this);
  mpValueLabel = new QLabel(this);
//create pixmap
  assignPixmap();
	qgl->addWidget(pixmapWidget(),0,0);
	qgl->addWidget(mpTitleLabel,0,1);
	qgl->addWidget(mpValueLabel,0,2);
    qgl->setColumnStretch(1,1);
	qgl->activate();
}
/**  */
void QReadOnlyOption::setText(QString text)
{
  mpValueLabel->setText(text);
}
