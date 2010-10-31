/***************************************************************************
                          scanarea.cpp  -  description
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

#include "scanarea.h"

ScanArea::ScanArea(QString name,double tlx,double tly,double brx,double bry,
		  double width, double height)
{
  mTlx = tlx;
  mTly = tly;
  mBrx = brx;
  mBry = bry;
  mWidth = width;
  mHeight = height;
  mName = name;
  mNumber = -1;
}
ScanArea::~ScanArea()
{
}


void ScanArea::setRange(double tlx,double tly,double brx,double bry,
          double width, double height)
{
  mTlx = tlx;
  mTly = tly;
  mBrx = brx;
  mBry = bry;
  mWidth = width;
  mHeight = height;
}


/**  */
double ScanArea::tly()
{
	return mTly;
}
/**  */
double ScanArea::tlx()
{
	return mTlx;
}
/**  */
double ScanArea::bry()
{
	return mBry;
}
/**  */
double ScanArea::brx()
{
	return mBrx;
}
/**  */
double ScanArea::width()
{
	return mWidth;
}
/**  */
double ScanArea::height()
{
	return mHeight;
}
/**  */
void ScanArea::setTly(double dvalue)
{
	mTly = dvalue;
}
/**  */
void ScanArea::setTlx(double dvalue)
{
	mTlx = dvalue;
}
/**  */
void ScanArea::setBry(double dvalue)
{
	mBry = dvalue;
}
/**  */
void ScanArea::setBrx(double dvalue)
{
	mBrx = dvalue;
}
/**  */
QString ScanArea::getName()
{
	return mName;
}
/**  */
void ScanArea::setName(QString qs)
{
  mName = qs;
}
/**  */
bool ScanArea::isValid()
{
  bool valid = true;
  if((mTlx < 0.0) || (mTlx > 1.0)) valid = false;
  if((mTly < 0.0) || (mTly > 1.0)) valid = false;
  if((mBrx < 0.0) || (mBrx > 1.0)) valid = false;
  if((mBry < 0.0) || (mBry > 1.0)) valid = false;
  return valid;
}
/** No descriptions */
void ScanArea::setNumber(int num)
{
  mNumber = num;
}
/** No descriptions */
int ScanArea::number()
{
  return mNumber;
}
