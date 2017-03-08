/***************************************************************************
                          qscrollbaroption.cpp  -  description
                             -------------------
    begin                : Tue Jul 4 2000
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

#include "qscrollbaroption.h"
#include <qsizepolicy.h>
#include <qlayout.h>
#include <qfontmetrics.h>
#include <qlabel.h>
#include <qstring.h>
#include <qpixmap.h>
#include <qimage.h>
#include <QGridLayout>
#include <sane/sane.h>
#include <sane/saneopts.h>
#include <qslider.h>

QScrollBarOption::QScrollBarOption(QString title,QWidget* parent,
                                   SANE_Value_Type type,const char* name)
                 :QSaneOption(title,parent,name)
{
  mMetricSystem = QIN::NoMetricSystem;
  mSaneValueType = type;
  mSignalResize = true;
  mQuant = 0;
  mHasQuant = false;
  mBusy = false;
	initWidget();
/*
  if(mSaneValueType == SANE_TYPE_FIXED)
   qDebug("constructed QScrollbarOption - TYPE_FIXED - %s",title.latin1());
  else
   qDebug("constructed QScrollbarOption - TYPE_INT - %s",title.latin1());
*/
}

QScrollBarOption::~QScrollBarOption()
{
}
/**  */
void QScrollBarOption::initWidget()
{
  QGridLayout* qgl = new QGridLayout(this);
  qgl->setContentsMargins(0, 0, 0, 0);
  qgl->setSpacing(0);
	mpTitleLabel = new QLabel(optionTitle(),this);
	mpValueSlider = new QSlider(Qt::Horizontal,this);
  mpValueSlider->setTracking(false);
  mpValueSlider->setFocusPolicy(Qt::StrongFocus);//should get focus after clicking
	mpValueLabel = new QLabel(this);
  mpValueLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
//create pixmap
  assignPixmap();
    qgl->addWidget(pixmapWidget(),0,0,3,1);

	qgl->addWidget(mpTitleLabel,0,2);
	qgl->addWidget(mpValueSlider,2,2);
	qgl->addWidget(mpValueLabel,2,3);
  qgl->setColumnMinimumWidth(1,5);
  qgl->setColumnStretch(2,1);
	connect(mpValueSlider,SIGNAL(sliderMoved(int)),this,SLOT(slotSliderMoved(int)));
	connect(mpValueSlider,SIGNAL(valueChanged(int)),this,SLOT(slotValueChanged(int)));
  qgl->activate();
}
/**  */
void QScrollBarOption::setRange(int min,int max,int quant)
{
//if we have a quant we can calculate the number of steps like
//this: k = (maxval - minval)/quant; steps = k +1(because we start with 0)
  mQuant = quant;
  mMinVal = min;
  mMaxVal = max;
//  mpValueSlider->blockSignals(true);
	if(mSaneValueType == SANE_TYPE_FIXED)
  {
    mMinVal = int(SANE_UNFIX(min) * 100.0);
    mMaxVal = int(SANE_UNFIX(max) * 100.0);
    mQuant = int(SANE_UNFIX(mQuant) * 100.0);
  }
	mpValueSlider->setRange(mMinVal,mMaxVal);
  if(mQuant > 0)
  {
    int k;
    k= (mMaxVal - mMinVal)/ mQuant;
    mpValueSlider->setRange(0,k);
    mpValueSlider->setSingleStep(1);
    mpValueSlider->setPageStep(10);
    mHasQuant = true;
  }
  if(mSaneValueType == SANE_TYPE_FIXED)
  {
    switch(mMetricSystem)
    {
      case QIN::Millimetre:
        mpValueSlider->setSingleStep(1);
        mpValueSlider->setPageStep(10);
        break;
      case QIN::Centimetre:
        mpValueSlider->setSingleStep(10);
        mpValueSlider->setPageStep(100);
        break;
      case QIN::Inch:
        mpValueSlider->setSingleStep(25);
        mpValueSlider->setPageStep(254);
        break;
      default:;
    }
  }
  else
  {
    mpValueSlider->setSingleStep(1);
    mpValueSlider->setPageStep(10);
  }
  //find useful size for textlabel
	QFontMetrics fm(font());
  QString qs;
  qs.setNum(mMaxVal);
  int wwidth  = fm.width(qs);
  qs.setNum(mMinVal);
  if(wwidth  < fm.width(qs))
    wwidth = fm.width(qs);
  wwidth += fm.width(mUnitString + "00");
  mpValueLabel->setFixedWidth(wwidth);
//  mpValueSlider->blockSignals(false);
}
/**  */
void QScrollBarOption::slotValueChanged(int value)
{
  double pval;
  mCurrentValue = value;
//  printf ("   slotValueChanged\n");

  redrawValueLabel();
	slotEmitOptionChanged();
  //only emit this signal if the value was changed by
  //dragging the slider
  if(mSignalResize == true)
  {
//	  printf ("   mSignalResize\n");
    emit signalResizeScanRect();
    if(mHasQuant)
      pval = double(mCurrentValue*mQuant + mMinVal)/double(mMaxVal-mMinVal);
    else
      pval = double(mCurrentValue)/double(mMaxVal-mMinVal);
    if(pval < 0.0) pval = 0.0;
    if(pval > 1.0) pval = 1.0;
    emit signalValuePercent(pval);
  }
}
/**  */
void QScrollBarOption::setUnit(SANE_Unit unit)
{
	switch(unit)
	{
		case SANE_UNIT_NONE:
			mUnitString = "";
			break;
		case SANE_UNIT_PIXEL:
			mUnitString = "Pixel";
			break;
		case SANE_UNIT_BIT:
			mUnitString = "bit";
			break;
		case SANE_UNIT_MM:
			mUnitString = "mm";
    //displayed value depends on the metric system choosen by the user
      mMetricSystem = QIN::Millimetre;
			break;
		case SANE_UNIT_DPI:
			mUnitString = "dpi";
			break;
		case SANE_UNIT_PERCENT:
			mUnitString = "%";
			break;
		case SANE_UNIT_MICROSECOND:
			mUnitString = "�s";
  }
}
/**  */
void QScrollBarOption::setValue(int val)
{
  if (mBusy)
	  return;
  if(mSaneValueType == SANE_TYPE_FIXED)
  {
    val = int(SANE_UNFIX(val) * 100.0 + 0.5);
//    printf ("val = %d\n", val);
  }
  if(mHasQuant)
    mCurrentValue = (val - mMinVal)/mQuant;
  else
    mCurrentValue = val;
//  printf ("mCurrentValue = %d\n", mCurrentValue);
  redrawValueLabel();
//  printf ("calling setValue\n");
  mBusy = true;
  mpValueSlider->setValue(mCurrentValue);
  mBusy = false;
//  printf ("calling setValue - done\n\n");
}
/**  */
SANE_Value_Type QScrollBarOption::getSaneType()
{
	return mSaneValueType;
}
/**  */
int QScrollBarOption::getValue()
{
  int val;
  if(mHasQuant)
    val = mCurrentValue*mQuant + mMinVal;
  else
    val = mCurrentValue ;
	if(mSaneValueType == SANE_TYPE_FIXED)
    val = SANE_FIX(double(val)/100.0);
  return val;
//	return mpValueSlider->value();
}
/**  */
void QScrollBarOption::slotValueChangedExt(int value)
{
  mpValueSlider->blockSignals(true);
  mbExtCall = true;
	slotValueChanged(value);
  mpValueSlider->blockSignals(false);
}
/**  */
void QScrollBarOption::slotEmitSignal()
{
  mCurrentValue = mpValueSlider->value();
  slotEmitOptionChanged();
}
/**  */
void QScrollBarOption::slotChangeMetricSystem(QIN::MetricSystem ms)
{
  mMetricSystem = ms;
  switch(ms)
	{
		case QIN::Millimetre:
      mUnitString = tr("mm");
      if(mSaneValueType == SANE_TYPE_FIXED)
      {
          mpValueSlider->setSingleStep(1);
          mpValueSlider->setPageStep(10);
      }
			break;
		case QIN::Centimetre:
      mUnitString = tr("cm");
      if(mSaneValueType == SANE_TYPE_FIXED) {
          mpValueSlider->setSingleStep(1);
          mpValueSlider->setPageStep(100);
      }
			break;
		case  QIN::Inch:
      mUnitString = tr("inch");
      if(mSaneValueType == SANE_TYPE_FIXED) {
          mpValueSlider->setSingleStep(25);
          mpValueSlider->setPageStep(254);
        }
			break;
    default:;//do nothing--shouldn't happen
	}
  slotValueChanged(mpValueSlider->value()); //trigger update
}
/**  */
void QScrollBarOption::slotSliderMoved(int val)
{
  double pval;
  mCurrentValue = val;
  redrawValueLabel();
  if(mHasQuant)
    pval = double(val*mQuant + mMinVal)/double(mMaxVal-mMinVal);
  else
    pval = double(val)/double(mMaxVal-mMinVal);
  if(pval < 0.0) pval = 0.0;
  if(pval > 1.0) pval = 1.0;
  emit signalValuePercent(pval);
}
/**Draw the current scrollbar value  */
void QScrollBarOption::redrawValueLabel()
{
	QString qs;
  int ival;
  int val;
  double dval;
  if(mHasQuant)
    val = mCurrentValue*mQuant + mMinVal;
  else
    val = mCurrentValue;
  if(mSaneValueType == SANE_TYPE_INT)
  {
    ival = val;
		if(mMetricSystem == QIN::Inch)
			ival = int(double(ival)/25.4);
		if(mMetricSystem == QIN::Centimetre)
			ival = int(double(ival)/10.0);
    qs.setNum(ival);
    qs += " %1";
		qs = qs.arg(mUnitString);
  }
  if(mSaneValueType == SANE_TYPE_FIXED)
  {
    dval = double(val)/100.0;//SANE_UNFIX(SANE_Fixed(val))*100.0;
		if(mMetricSystem == QIN::Inch)
			dval = dval/25.4;
		if(mMetricSystem == QIN::Centimetre)
			dval = dval/10.0;
    qs.setNum(dval,'f',0);
    qs += " %1";
		qs = qs.arg(mUnitString);
  }
	mpValueLabel->clear();
	mpValueLabel->setText(qs);
}
/** Set a new scrollbar value without emitting
a signal that causes a resize of the
scan rect in the preview widget */
void QScrollBarOption::setValueExt(int value)
{
  mSignalResize = false;
  int k;
	if(mSaneValueType == SANE_TYPE_FIXED)
    value = int(SANE_UNFIX(value) * 100.0);
  if(mHasQuant)
    k = (value - mMinVal)/mQuant;
  else
    k = value;
  mCurrentValue = k;
  mpValueSlider->setValue(k);
  mSignalResize = true;
}
/**  */
void QScrollBarOption::setMinimumValue()
{
  mpValueSlider->setValue(mpValueSlider->minimum());
}
/**  */
void QScrollBarOption::setMaximumValue()
{
  mpValueSlider->setValue(mpValueSlider->maximum());
}
/**  */
int QScrollBarOption::maxValue()
{
  int i;
  i = mpValueSlider->maximum();
  if(mHasQuant)
    i = i*mQuant + mMinVal;
  if(mSaneValueType == SANE_TYPE_FIXED)
  {
    i = SANE_FIX(double(i)/100.0);
  }
  return i;
}

int QScrollBarOption::maxIntValue()
{
  int i;
  i = mpValueSlider->maximum();
  if(mHasQuant)
    i = i*mQuant + mMinVal;
  if(mSaneValueType == SANE_TYPE_FIXED)
  {
    i = int(double(i)/100.0);
  }
  return i;
}

/**  */
int QScrollBarOption::minValue()
{
  int i;
  i = mpValueSlider->minimum();
  if(mHasQuant)
    i = i*mQuant + mMinVal;
  if(mSaneValueType == SANE_TYPE_FIXED)
  {
    i = SANE_FIX(double(i)/100.0);
  }
  return i;
}
/**  */
double QScrollBarOption::getPercentValue()
{
  double pval;
  if(mHasQuant)
    pval = double(mCurrentValue*mQuant)/double(mMaxVal-mMinVal);
  else
    pval = double(mCurrentValue)/double(mMaxVal-mMinVal);
  if(pval<0.0) pval = 0.0;
  if(pval>1.0) pval = 1.0;
  return pval;
}
/**  */
void QScrollBarOption::slotSetPercentValue(double pval)
{
  mSignalResize = false;
  int value;

  if(mHasQuant)
    value = int(pval*double(mMaxVal-mMinVal)/double(mQuant));
  else
    value = int(pval*double(mMaxVal-mMinVal));
  mpValueSlider->setValue(value+mMinVal);
  mSignalResize = true;
}
