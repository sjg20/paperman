/***************************************************************************
                          sliderspin.cpp  -  description
                             -------------------
    begin                : Fri Mar 8 2002
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

#include "sliderspin.h"

#include "resource.h"

#include <q3hbox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qspinbox.h>

#ifdef USE_QT3
SliderSpin::SliderSpin(QWidget* parent,const char* name,Qt::WFlags f,bool allowLines)
           :Q3VBox(parent,name,f)
#else
SliderSpin::SliderSpin(QWidget* parent,const char* name,Qt::WFlags f,bool allowLines)
           :Q3VBox(parent,name,f,allowLines)
#endif
{
  initWidget();
}

#ifdef USE_QT3
SliderSpin::SliderSpin(int minval,int maxval,int val,QString title,QWidget* parent,
                       const char* name,Qt::WFlags f,bool allowLines)
           :Q3VBox(parent,name,f)
#else
SliderSpin::SliderSpin(int minval,int maxval,int val,QString title,QWidget* parent,
                       const char* name,Qt::WFlags f,bool allowLines)
           :Q3VBox(parent,name,f,allowLines)
#endif
{
  initWidget();
  setTitle(title);
  setRange(minval,maxval);
  setValue(val);
}

SliderSpin::~SliderSpin()
{
}
/** No descriptions */
void SliderSpin::initWidget()
{
  Q3HBox* hb = new Q3HBox(this);
  mpTitleLabel = new QLabel(hb);
  mpSlider = new QSlider(0,10,1,1,Qt::Horizontal,hb);
  mpSpinBox = new QSpinBox(0,10,1,hb);
  hb->setSpacing(5);
  hb->setStretchFactor(mpSlider,1);
  mpSlider->setTracking(false);
  connect(mpSlider,SIGNAL(valueChanged(int)),
          this,SLOT(slotValueChanged(int)));
  connect(mpSlider,SIGNAL(sliderMoved(int)),
          this,SLOT(slotSliderMoved(int)));
  connect(mpSpinBox,SIGNAL(valueChanged(int)),
          this,SLOT(slotSpinValueChanged(int)));
}
/** No descriptions */
void SliderSpin::setTitle(QString title)
{
  mpTitleLabel->setText(title);
}
/** No descriptions */
void SliderSpin::setRange(int min,int max)
{
  mpSlider->setRange(min,max);
  mpSpinBox->setRange(min,max);
  mpSlider->setPageStep (32);
}
/** No descriptions */
void SliderSpin::slotValueChanged(int value)
{
  mpSpinBox->blockSignals(true);
  mpSpinBox->setValue(value);
  mpSpinBox->blockSignals(false);
  emit signalValueChanged(value);
}
/** No descriptions */
void SliderSpin::slotSliderMoved(int value)
{
  mpSpinBox->blockSignals(true);
  mpSpinBox->setValue(value);
  mpSpinBox->blockSignals(false);
}
/** No descriptions */
void SliderSpin::slotSpinValueChanged(int value)
{
  mpSlider->blockSignals(true);
  mpSlider->setValue(value);
  mpSlider->blockSignals(false);
  emit signalValueChanged(value);
}
/** No descriptions */
int SliderSpin::value()
{
  return mpSlider->value();
}
/** No descriptions */
void SliderSpin::setValue(int value)
{
  return mpSlider->setValue(value);
}


void SliderSpin::setEnabled ( bool enable)
{
  mpSlider->setEnabled (enable);
  mpSpinBox->setEnabled (enable);
  mpTitleLabel->setEnabled (enable);
}




