/***************************************************************************
                          sanefixedoption.h  -  description
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

#ifndef SANEFIXEDOPTION_H
#define SANEFIXEDOPTION_H

#include "qsaneoption.h"
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
class SaneFixedSpinBox;
class QLabel;
class QString;

class SaneFixedOption : public QSaneOption
{
Q_OBJECT
public:
/**
  *@author Michael Herder
  */
	SaneFixedOption(QString title,QWidget * parent,
                  SANE_Value_Type type=SANE_TYPE_INT,const char * name=0);
	~SaneFixedOption();
  /**  */
  void setUnit(SANE_Unit unit);
  /**  */
  void setValue(int val);
  /**  */
  int value();
  /**  */
  SANE_Value_Type getSaneType();
  /** No descriptions */
  int maxValue();
  /** No descriptions */
  int minValue();
  /** No descriptions */
  void setValueExt(int value);
  /** No descriptions */
  double getPercentValue();
private: // Private attributes
  /**  */
  SANE_Unit mSaneUnit;
  /**  */
  SaneFixedSpinBox* mpValueSpinBox;
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  QString mUnitString;
  /**  */
  SANE_Value_Type mSaneValueType;
private: // Private methods
  /**  */
  void initWidget();
  /**  */
  void redrawValueLabel();
private slots: // Private slots
  /**  */
  void slotValueChanged(int val);
signals: // Signals
  /**  */
  void valueChanged(int);
};

#endif
