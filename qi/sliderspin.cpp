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

#include "err.h"
#include "sliderspin.h"

#include "resource.h"

#include <QHBoxLayout>
#include <qlabel.h>
#include <qslider.h>
#include <qspinbox.h>

SliderSpin::SliderSpin(QWidget* parent,const char* name,Qt::WFlags f,bool allowLines)
           :QWidget(parent)
{
  UNUSED(allowLines);
  UNUSED(f);
  setObjectName(name);
  initWidget();
}

SliderSpin::SliderSpin(int minval,int maxval,int val,QString title,QWidget* parent,
                       const char* name,Qt::WFlags f,bool allowLines)
           :QWidget(parent)
{
  UNUSED(allowLines);
  UNUSED(f);
  setObjectName(name);
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
  QHBoxLayout* hb = new QHBoxLayout(this);
  mpTitleLabel = new QLabel();
  hb->addWidget(mpTitleLabel);
  mpSlider = new QSlider(Qt::Horizontal);
  hb->addWidget(mpSlider);
  mpSlider->setMinimum(0);
  mpSlider->setMaximum(10);
  mpSlider->setSingleStep(1);
  mpSlider->setPageStep(10);
  mpSpinBox = new QSpinBox();
  hb->addWidget(mpSpinBox);
  mpSpinBox->setMinimum(0);
  mpSpinBox->setMaximum(10);
  mpSpinBox->setSingleStep(1);
  hb->setSpacing(5);
  hb->setStretchFactor(mpSlider,1);
  setLayout(hb);
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




