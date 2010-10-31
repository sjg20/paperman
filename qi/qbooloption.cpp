/***************************************************************************
                          qbooloption.cpp  -  description
                             -------------------
    begin                : Thu Sep 14 2000
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

#include "qbooloption.h"

#include <qcheckbox.h>
#include <q3hbox.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qstring.h>
//Added by qt3to4:
#include <Q3GridLayout>

QBoolOption::QBoolOption(QString title,QWidget *parent, const char *name )
            :QSaneOption(title,parent,name)

{
  mAutomatic = false;
  initWidget();
}
QBoolOption::~QBoolOption()
{
}
/**  */
void QBoolOption::initWidget()
{
	Q3GridLayout* qgl = new Q3GridLayout(this,1,2);
  Q3HBox* hbox1 = new Q3HBox(this);
	mpOptionCheckBox = new QCheckBox(optionTitle(),hbox1);
  connect(mpOptionCheckBox,SIGNAL(clicked()),
          this,SLOT(slotEmitOptionChanged()));
  QWidget* dummy = new QWidget(hbox1);
  hbox1->setStretchFactor(dummy,1);
  mpAutoCheckBox = new QCheckBox(tr("Automatic"),hbox1);
  hbox1->setSpacing(6);
  mpAutoCheckBox->hide();
//create pixmap
  assignPixmap();
	qgl->addWidget(pixmapWidget(),0,0);
	qgl->addWidget(hbox1,0,1);
  qgl->setSpacing(5);
	qgl->setColStretch(1,1);
	qgl->activate();
  connect(mpAutoCheckBox,SIGNAL(toggled(bool)),this,SLOT(slotAutoMode(bool)));
}
/**  */
SANE_Bool QBoolOption::state()
{
  SANE_Bool sb;
	sb = (mpOptionCheckBox->isChecked() == true) ?
        SANE_TRUE : SANE_FALSE;
  return sb;
}
/**  */
void QBoolOption::setState(SANE_Bool sb)
{
  if(sb == SANE_TRUE)
    mpOptionCheckBox->setChecked(true);
	else
    mpOptionCheckBox->setChecked(false);
}
/**  */
void QBoolOption::enableAutomatic(bool b)
{
  mAutomatic =b;
  if(b)
    mpAutoCheckBox->show();
  else
    mpAutoCheckBox->hide();
}
/**  */
bool QBoolOption::automatic()
{
  return mAutomatic;
}
/**Turn automatic mode on or off.  */
void QBoolOption::slotAutoMode(bool automode)
{
  mAutomatic = automode;
  if(automode)
    mpOptionCheckBox->setEnabled(false);
  else
    mpOptionCheckBox->setEnabled(true);
  emit signalAutomatic(optionNumber(),automode);
}
