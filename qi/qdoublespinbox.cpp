/***************************************************************************
                          qdoublespinbox.cpp  -  description
                             -------------------
    begin                : Tue Dec 12 2000
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

#include "qdoublespinbox.h"
#include <qlineedit.h>
#include <qvalidator.h>

QDouble100SpinBox::QDouble100SpinBox(QWidget * parent,const char * name)
               :QSpinBox(parent)
{
  setObjectName(name);
  QDoubleValidator* dv = new QDoubleValidator(this);
  dv->setDecimals(2);
//s  setValidator(dv);
}
QDouble100SpinBox::~QDouble100SpinBox()
{
}
/**  */
QString QDouble100SpinBox::mapValueToText(int value)
{
  QString qs;
  qs.setNum(double(value)/100.0,'f',2);
  return qs;
}
/**  */
int QDouble100SpinBox::mapTextToValue(bool* ok)
{
  ok = ok; //s unused
  return int(text().toDouble()*100.0);
}
/**  */
void QDouble100SpinBox::selectAll()
{
  selectAll();
}

QValidator::State QDouble100SpinBox::validate(QString &input, int &pos) const
{
    if (!mValidator)
        return QValidator::Acceptable;
    return mValidator->validate(input, pos);
}

void QDouble100SpinBox::setValidator(QValidator *validator)
{
    mValidator = validator;
}

