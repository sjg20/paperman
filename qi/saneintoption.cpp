/***************************************************************************
                          saneintoption.cpp  -  description
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

#include "saneintoption.h"

#include <limits.h>
#include <qfontmetrics.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qsizepolicy.h>
#include <qspinbox.h>
#include <qstring.h>
#include <QGridLayout>
#include <sane/sane.h>
#include <sane/saneopts.h>

SaneIntOption::SaneIntOption(QString title,QWidget* parent,
                             SANE_Value_Type type,const char* name)
              :QSaneOption(title,parent,name)
{
  mMetricSystem = QIN::NoMetricSystem;
  mSaneValueType = type;
  mSaneUnit = SANE_UNIT_NONE;
	initWidget();
}

SaneIntOption::~SaneIntOption()
{
}
/**  */
void SaneIntOption::initWidget()
{
  QGridLayout* qgl = new QGridLayout(this);
	mpTitleLabel = new QLabel(optionTitle(),this);
    mpValueSpinBox = new QSpinBox(this);
    mpValueSpinBox->setMinimum(INT_MIN);
    mpValueSpinBox->setMaximum(INT_MAX);
  mpValueSpinBox->setFocusPolicy(Qt::StrongFocus);//should get focus after clicking
//create pixmap
  assignPixmap();
    qgl->addWidget(pixmapWidget(),0,2,0,0);

	qgl->addWidget(mpTitleLabel,1,1);
	qgl->addWidget(mpValueSpinBox,1,2);
  qgl->setSpacing(5);
  qgl->setColumnStretch(1,1);
	connect(mpValueSpinBox,SIGNAL(valueChanged(int)),this,SLOT(slotValueChanged(int)));
  qgl->activate();
}
/**  */
void SaneIntOption::slotValueChanged(int value)
{
   value = value;  //s unused
	slotEmitOptionChanged();
}
/**  */
void SaneIntOption::setUnit(SANE_Unit unit)
{
  mSaneUnit = unit;
  switch(mSaneUnit)
  {
    case SANE_UNIT_PIXEL:
      mpValueSpinBox->setSuffix(tr("Pixel"));
      break;
    case SANE_UNIT_BIT:
      mpValueSpinBox->setSuffix(tr("Bit"));
      break;
    case SANE_UNIT_MM:
      mpValueSpinBox->setSuffix(tr("mm"));
      break;
    case SANE_UNIT_DPI:
      mpValueSpinBox->setSuffix(tr("dpi"));
      break;
    case SANE_UNIT_PERCENT:
      mpValueSpinBox->setSuffix(tr("%"));
      break;
    case SANE_UNIT_MICROSECOND:
      mpValueSpinBox->setSuffix(tr("�s"));
      break;
    default:
      mpValueSpinBox->setSuffix("");
      break;
  }
}
/**  */
void SaneIntOption::setValue(int val)
{
  mpValueSpinBox->setValue(val);
}
/**  */
SANE_Value_Type SaneIntOption::getSaneType()
{
	return mSaneValueType;
}
/**  */
int SaneIntOption::value()
{
	return mpValueSpinBox->value();
}
