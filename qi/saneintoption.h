/***************************************************************************
                          saneintoption.h  -  description
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

#ifndef SANEINTOPTION_H
#define SANEINTOPTION_H

#include "qsaneoption.h"
#include "quiteinsanenamespace.h"
#include <QLabel>

extern "C"
{
#include <sane/sane.h>
}
/**
  *@author M. Herder
  */
//forward declarations
class QSpinBox;
class QLabel;
class QString;

class SaneIntOption : public QSaneOption
{
Q_OBJECT

public:
/**
  *@author Michael Herder
  */
	SaneIntOption(QString title,QWidget * parent,
                   SANE_Value_Type type=SANE_TYPE_INT,const char * name=0);
	~SaneIntOption();
  /**  */
  void setUnit(SANE_Unit unit);
  /**  */
  void setValue(int val);
  /**  */
  int value();
  /**  */
  SANE_Value_Type getSaneType();
private: // Private attributes
  /**  */
  SANE_Unit mSaneUnit;
  /**  */
  QSpinBox* mpValueSpinBox;
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  SANE_Value_Type mSaneValueType;
  /**  */
  QIN::MetricSystem mMetricSystem;
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
