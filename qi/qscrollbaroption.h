/***************************************************************************
                          qscrollbaroption.h  -  description
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

#ifndef QSCROLLBAROPTION_H
#define QSCROLLBAROPTION_H

#include "qsaneoption.h"
#include "quiteinsanenamespace.h"
//Added by qt3to4:
#include <QLabel>

extern "C"
{
#include <sane/sane.h>
}
/**
  *@author M. Herder
  */
//forward declarations
class QSlider;
class QLabel;
class QString;

class QScrollBarOption : public QSaneOption
{
Q_OBJECT

public:
	QScrollBarOption(QString title,QWidget * parent,
                   SANE_Value_Type type=SANE_TYPE_INT,const char * name=0);
	~QScrollBarOption();
  /**  */
  void setRange(int min,int max,int quant);
  /**  */
  void setUnit(SANE_Unit unit);
  /**  */
  void setValue(int val);
  /**  */
  int getValue();
  /**  */
  SANE_Value_Type getSaneType();
  /** Set a new scrollbar value without emitting
a signal that causes a resize of the
scan rect in the preview widget */
  void setValueExt(int value);
  /**  */
  void setMaximumValue();
  /**  */
  void setMinimumValue();
  /** */
  int maxIntValue();
  /**  */
  int maxValue();
  /**  */
  int minValue();
  /**  */
  double getPercentValue();
private: // Private attributes
  /** */
  bool mSignalResize;
  /** */
  int mMaxVal;
  /** */
  int mMinVal;
  /** */
  int mQuant;
  /** */
  bool mHasQuant;
  /**  */
  QSlider* mpValueSlider;
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  QLabel* mpValueLabel;
  /**  */
  QString mUnitString;
  /**  */
  SANE_Value_Type mSaneValueType;
  /**  */
  bool mbExtCall;
  /**  */
  int mCurrentValue;
  /**  */
  QIN::MetricSystem mMetricSystem;
  
  int mBusy;  // lock out nested calls
private: // Private methods
  /**  */
  void initWidget();
  /**  */
  void redrawValueLabel();
private slots: // Private slots
  /**  */
  void slotValueChanged(int value);
  /**  */
  void slotEmitSignal();
  /**  */
  void slotSliderMoved(int val);
signals: // Signals
  /**  */
  void valueChanged(int);
public slots: // Public slots
  /**  */
  void slotValueChangedExt(int value);
  /**  */
  void slotChangeMetricSystem(QIN::MetricSystem);
  /**  */
  void slotSetPercentValue(double pval);
signals: // Signals
  /**  */
  void signalResizeScanRect();
  /**  */
  void signalValuePercent(double pval);
};

#endif
