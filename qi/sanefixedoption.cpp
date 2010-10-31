/***************************************************************************
                          sanefixedoption.cpp  -  description
                             -------------------
    begin                : Mon Apr 8 2002
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

#include "sanefixedoption.h"
#include "sanefixedspinbox.h"

#include <limits.h>
#include <qfontmetrics.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qnamespace.h>
#include <qpixmap.h>
#include <qstring.h>
//Added by qt3to4:
#include <Q3GridLayout>
#include <sane/sane.h>
#include <sane/saneopts.h>

SaneFixedOption::SaneFixedOption(QString title,QWidget* parent,
                             SANE_Value_Type type,const char* name)
                :QSaneOption(title,parent,name)
{
  mSaneUnit = SANE_UNIT_NONE;
  mSaneValueType = type;
	initWidget();
}

SaneFixedOption::~SaneFixedOption()
{
}
/**  */
void SaneFixedOption::initWidget()
{
  Q3GridLayout* qgl = new Q3GridLayout(this,1,3);
	mpTitleLabel = new QLabel(optionTitle(),this);
	mpValueSpinBox = new SaneFixedSpinBox(INT_MIN,INT_MAX,65536,this);
  mpValueSpinBox->setFocusPolicy(Qt::StrongFocus);//should get focus after clicking
//create pixmap
  assignPixmap();
	qgl->addWidget(pixmapWidget(),0,0);
	qgl->addWidget(mpTitleLabel,0,1);
	qgl->addWidget(mpValueSpinBox,0,2);
  qgl->setSpacing(5);
  qgl->setColStretch(1,1);
	connect(mpValueSpinBox,SIGNAL(valueChanged(int)),this,SLOT(slotValueChanged(int)));
  qgl->activate();
}
/**  */
void SaneFixedOption::slotValueChanged(int value)
{
   value = value; //s unused
	slotEmitOptionChanged();
}
/**  */
void SaneFixedOption::setUnit(SANE_Unit unit)
{
  mSaneUnit = unit;
  mpValueSpinBox->setUnit(unit);
}
/**  */
void SaneFixedOption::setValue(int val)
{
  mpValueSpinBox->setValue(val);
}
/**  */
SANE_Value_Type SaneFixedOption::getSaneType()
{
	return mSaneValueType;
}
/**  */
int SaneFixedOption::value()
{
	return mpValueSpinBox->value();
}
