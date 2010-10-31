/***************************************************************************
                          qsplinearray.h  -  description
                             -------------------
    begin                : Tue Oct 31 2000
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

#ifndef QSPLINEARRAY_H
#define QSPLINEARRAY_H
#define MAX_POINTS 200
#include <q3pointarray.h>
//s #include <qarray.h>

/**
  *@author M. Herder
  */

class QSplineArray : public Q3PointArray
{
public: 
	QSplineArray();
	~QSplineArray();
  /**  */
  Q3PointArray spline();
  /**  */
  Q3PointArray line();
private: // Private attributes
  /**  */
  bool mSorted;
private: // Private methods
  void sortCoords();
};

#endif
