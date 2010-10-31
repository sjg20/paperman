/***************************************************************************
                           ruler.h  -  description
                             -------------------
    begin                : Mon Jul 17 2000
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

//class name changed from QRuler to Ruler on Mon Jan 28 2002

#ifndef RULER_H
#define RULER_H

#include <q3frame.h>
#include "quiteinsanenamespace.h"
/**
  *@author M. Herder
  */
//forward declarations
class QPainter;

class Ruler : public Q3Frame
{
public:
	Ruler( QWidget * parent=0, const char * name=0,Qt::Orientation o=Qt::Vertical, Qt::WFlags f=0);
	~Ruler();
  void setRulerValues(double rmv,double sm,int ssn);
  /** No descriptions */
  void setMaxRange(double max);
  /** No descriptions */
  void setMinRange(double min);
  /** No descriptions */
  void setRange(double min,double max);
private: // Private attributes
  /**  */
  double mMaxRange;
  /**  */
  double mMinRange;
  /**  */
  Qt::Orientation mOrientation;
protected: // Protected methods
  /**  */
  virtual void drawContents(QPainter* painter);
};

#endif
