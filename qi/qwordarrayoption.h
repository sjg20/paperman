/***************************************************************************
                          qwordarrayoption.h  -  description
                             -------------------
    begin                : Mon Oct 30 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2        *
 *   as published by the Free Software Foundation.                          *
 *                                                                         *
 ***************************************************************************/

#ifndef QWORDARRAYOPTION_H
#define QWORDARRAYOPTION_H

#include "qsaneoption.h"
#include <qstring.h>
#include <QLabel>
extern "C"
{
#include <sane/sane.h>
}
/**
  *@author M. Herder
  */

class QDouble100SpinBox;
class QPushButton;
class QLabel;
class QCurveWidget;
class QComboBox;

class QWordArrayOption : public QSaneOption
{
Q_OBJECT
public:
	QWordArrayOption(QString title,QWidget* parent,
                   SANE_Value_Type type=SANE_TYPE_INT,const char* name=0);
	~QWordArrayOption();
  /**  */
  QVector<SANE_Word> getValue();
  /**  */
  SANE_Value_Type getSaneType();
  /**  */
  void setRange(int min, int max);
  /**  */
  void setQuant(int min);
  /**  */
  void setValue(QVector<SANE_Word> array);
  /**  */
  void calcDataArray();
  QPolygon pointArray();
  /**  */
  void closeCurveWidget();
private:
  /**  */
  QCurveWidget* mpCurveWidget;
  /**  */
  QPushButton* mpSetButton;
  /**  */
  QPushButton* mpResetButton;
  /**  */
  QPushButton* mpCloseButton;
  /**  */
  QDouble100SpinBox* mpGammaSpin;
  /**  */
  QComboBox* mpCurveCombo;
  /**  */
  QPolygon mPointArray;
  /**  */
  QLabel* mpTitleLabel;

  QPushButton* mpShowButton;
  /**  */
  SANE_Word mMaxVal;
  /**  */
  SANE_Word mMinVal;
  /**  */
  SANE_Word mQuant;
  /**  */
  QVector<SANE_Word> mDataArray;
  /**  */
  QWidget* mpArrayWidget;
  /** */
  SANE_Value_Type mValueType;
  /** */
  double mGamma;
  /**  */
  double mOldGamma;
private: // Private methods
  /**  */
  void setCurve();
  /**  */
  void initWidget();
  /** */
  void setPointArray(QPolygon qpa);
  /**  */
  void createCurveWidget();

private slots: // Private slots
  /**  */
  void slotShowOption();
  /**  */
  void slotGammaValue(int value);
  /**  */
  void slotCurveCombo(int index);
public slots:
  /**  */
  void slotReset();
  /**  */
  void slotSet();
};

#endif
