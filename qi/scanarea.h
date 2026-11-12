/***************************************************************************
                          scanarea.h  -  description
                             -------------------
    begin                : Mon Jul 3 2000
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

#ifndef SCANAREA_H
#define SCANAREA_H


/**
  *@author M. Herder
  */
#include <qstring.h>

class ScanArea
{
public:
	ScanArea(QString name="",double tlx=0.0,double tly=0.0,double brx=1.0,double bry=1.0, double width = -1.0, double height = -1.0);
	~ScanArea();
private: // Private attributes
  /**  */
  int mNumber;
  /**  */
  double mTlx;
  /**  */
  double mTly;
  /**  */
  double mBrx;
  /**  */
  double mBry;

  /** page size */
  double mWidth, mHeight;

  /** true if the size is the scanner's own reach rather than a page */
  bool mScannerSized;
  /**  */
  QString mName;
public:
  void setRange(double tlx=0.0,double tly=0.0,double brx=1.0,double bry=1.0, double width = -1.0, double height = -1.0);
  /**  */
  double tlx();
  /**  */
  double tly();
  /**  */
  double brx();
  /**  */
  double bry();
  /**  */

  double width ();
  double height ();

  /** say that the size is what the scanner takes, as Full size and Long
      are, rather than a sheet of paper. A sheet fed sideways is turned
      round to go through the scanner, but the scanner itself is not */
  void setScannerSized (bool sized);

  /** \returns true if the size is what the scanner takes, not a page */
  bool scannerSized ();

  void setTlx(double dvalue);
  /**  */
  void setTly(double dvalue);
  /**  */
  void setBrx(double dvalue);
  /**  */
  void setBry(double dvalue);
  /**  */
  void setName(QString qs);
  /**  */
  QString getName();
  /**  */
  bool isValid();
  /** No descriptions */
  int number();
  /** No descriptions */
  void setNumber(int num);
};

#endif
