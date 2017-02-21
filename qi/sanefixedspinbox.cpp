/***************************************************************************
                          sanefixedspinbox.cpp  -  description
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

#include "sanefixedspinbox.h"
#include <qlineedit.h>
#include <qvalidator.h>

SaneFixedSpinBox::SaneFixedSpinBox(QWidget * parent,const char * name)
                 :QSpinBox(parent)
{
  setObjectName(name);
  mSaneUnit = SANE_UNIT_NONE;
  mpValidator = new QDoubleValidator(-32768.0,32767.9999,4,this);
//s  setValidator(mpValidator);
}
SaneFixedSpinBox::SaneFixedSpinBox(int min,int max,int step,QWidget* parent,const char * name)
                 :QSpinBox(parent)
{
    setMinimum(min);
    setMaximum(max);
    setSingleStep(step);
    setObjectName(name);
  mSaneUnit = SANE_UNIT_NONE;
  mpValidator = new QDoubleValidator(-32768.0,32767.9999,4,this);
//s  setValidator(mpValidator);
}
SaneFixedSpinBox::~SaneFixedSpinBox()
{
}
/**  */
QString SaneFixedSpinBox::mapValueToText(int value)
{
  QString qs;
  double val;
  val = SANE_UNFIX(value);
  qs.setNum(val,'f',4);
  return qs;
}
/**  */
int SaneFixedSpinBox::mapTextToValue(bool* ok)
{
  ok = ok; //s unused
  int i;
  double val;
  val = text().toDouble();
  i = SANE_FIX(val);
  return i;
}
/**  */
void SaneFixedSpinBox::selectAll()
{
  selectAll();
}
/** No descriptions */
void SaneFixedSpinBox::setUnit(SANE_Unit unit)
{
  unit = unit; //s unused
  switch(mSaneUnit)
  {
    case SANE_UNIT_PIXEL:
      setSuffix(tr("Pixel"));
      break;
    case SANE_UNIT_BIT:
      setSuffix(tr("Bit"));
      break;
    case SANE_UNIT_MM:
      setSuffix(tr("mm"));
      break;
    case SANE_UNIT_DPI:
      setSuffix(tr("dpi"));
      break;
    case SANE_UNIT_PERCENT:
      setSuffix(tr("%"));
      break;
    case SANE_UNIT_MICROSECOND:
      setSuffix(tr("�s"));
      break;
    default:
      setSuffix("");
      break;
  }
}
