/***************************************************************************
                          sanefixedspinbox.h  -  description
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

#ifndef SANEFIXEDSPINBOX_H
#define SANEFIXEDSPINBOX_H

#include "quiteinsanenamespace.h"

extern "C"
{
#include <sane/sane.h>
}
#include <qspinbox.h>
#include <qstring.h>
/**
  *@author M. Herder
  */
class QDoubleValidator;

class SaneFixedSpinBox : public QSpinBox
{
Q_OBJECT
public:
	SaneFixedSpinBox(QWidget* parent=0,const char* name=0);
	SaneFixedSpinBox(int min=0,int max=0,int step=0,QWidget* parent=0,const char* name=0);
	~SaneFixedSpinBox();
  /**  */
  void selectAll();
  /** No descriptions */
  void setSaneUnit(SANE_Unit unit);
  /** No descriptions */
  void setUnit(SANE_Unit unit);
private:
  /**  */
  QDoubleValidator* mpValidator;
  /**  */
  SANE_Unit mSaneUnit;
  /**  */
  QIN::MetricSystem mMetricSystem;
protected: // Protected methods
  /**  */
  virtual int mapTextToValue(bool* ok);
  /**  */
  virtual QString mapValueToText(int value);
};

#endif
