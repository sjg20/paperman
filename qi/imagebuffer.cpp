/***************************************************************************
                          imagebuffer.cpp  -  description
                             -------------------
    begin                : Tue Jan 22 2002
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

#include "imagebuffer.h"

#include <qimage.h>

ImageBuffer::ImageBuffer()
{
  mPath = QString();
  mpImage = 0;
  mTlxPercent = 0.0;
  mTlyPercent = 0.0;
  mBrxPercent = 1.0;
  mBryPercent = 1.0;
  mTlxOrig = 0.0;
  mTlyOrig = 0.0;
  mBrxOrig = 1.0;
  mBryOrig = 1.0;
  mAspectRatio = 1.0;
  mZoomString = QString();
}
ImageBuffer::~ImageBuffer()
{
//qDebug("~ImageBuffer()");
  if(mpImage)
  {
    delete mpImage;
  }
}
/** No descriptions */
void ImageBuffer::setImage(QImage* image,bool saved)
{
  if(mpImage)
  {
    delete mpImage;
  }
  mpImage = image;
  mSaved = saved;
}
/** No descriptions */
QImage* ImageBuffer::image()
{
  return mpImage;
}
/** No descriptions */
void ImageBuffer::setZoomString(QString string)
{
  mZoomString = string;
}
/** No descriptions */
QString ImageBuffer::zoomString()
{
  return mZoomString;
}
/** No descriptions */
void ImageBuffer::setSaved(bool status)
{
  mSaved = status;
}
/** No descriptions */
bool ImageBuffer::saved()
{
  return mSaved;
}
/** No descriptions */
void ImageBuffer::setPercentSize(double tlx,double tly,double brx,double bry)
{
  mTlxPercent = tlx;
  mTlyPercent = tly;
  mBrxPercent = brx;
  mBryPercent = bry;
}
/** No descriptions */
void ImageBuffer::setTlx(double tlx)
{
  mTlxPercent = tlx;
}
/** No descriptions */
void ImageBuffer::setTly(double tly)
{
  mTlyPercent = tly;
}
/** No descriptions */
void ImageBuffer::setBrx(double brx)
{
  mBrxPercent = brx;
}
/** No descriptions */
void ImageBuffer::setBry(double bry)
{
  mBryPercent = bry;
}
/** No descriptions */
double ImageBuffer::tlx()
{
  return mTlxPercent;
}
/** No descriptions */
double ImageBuffer::tly()
{
  return mTlyPercent;
}
/** No descriptions */
double ImageBuffer::brx()
{
  return mBrxPercent;
}
/** No descriptions */
double ImageBuffer::bry()
{
  return mBryPercent;
}
/** No descriptions */
void ImageBuffer::setTlxOrig(double value)
{
  mTlxOrig = value;
}
/** No descriptions */
void ImageBuffer::setTlyOrig(double value)
{
  mTlyOrig = value;
}
/** No descriptions */
void ImageBuffer::setBrxOrig(double value)
{
  mBrxOrig = value;
}
/** No descriptions */
void ImageBuffer::setBryOrig(double value)
{
  mBryOrig = value;
}
/** No descriptions */
double ImageBuffer::tlxOrig()
{
  return mTlxOrig;
}
/** No descriptions */
double ImageBuffer::tlyOrig()
{
  return mTlyOrig;
}
/** No descriptions */
double ImageBuffer::brxOrig()
{
  return mBrxOrig;
}
/** No descriptions */
double ImageBuffer::bryOrig()
{
  return mBryOrig;
}
/** No descriptions */
double ImageBuffer::tlxMapped()
{
  //map to device range
  double d = (mBrxOrig - mTlxOrig) * mTlxPercent + mTlxOrig;
//  d = d / mWidthMM;
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  return d;
}
/** No descriptions */
double ImageBuffer::tlyMapped()
{
  //map to device range
  double d = (mBryOrig - mTlyOrig) * mTlyPercent + mTlyOrig;
//  d = d / mWidthMM;
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  return d;
}
/** No descriptions */
double ImageBuffer::brxMapped()
{
  //map to device range
  double d = (mBrxOrig - mTlxOrig) * mBrxPercent + mTlxOrig;
//  d = d / mWidthMM;
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  return d;
}
/** No descriptions */
double ImageBuffer::bryMapped()
{
  //map to device range
  double d = (mBryOrig - mTlyOrig) * mBryPercent + mTlyOrig;
//  d = d / mWidthMM;
  if (d < 0.0)
    d = 0.0;
  if (d > 1.0)
    d = 1.0;
  return d;
}
/** No descriptions */
void ImageBuffer::setPath(QString path)
{
  mPath = path;
}
/** No descriptions */
QString ImageBuffer::path()
{
  return mPath;
}
/** No descriptions */
QImage ImageBuffer::selectedImage()
{
  QImage image;
  if(!mpImage)
    return image;
  int x,y,w,h;
  x = int(double(mpImage->width())*tlx());
  y = int(double(mpImage->height())*tly());
  w = int(double(mpImage->width())*brx()) - x;
  h = int(double(mpImage->height())*bry()) - y;

  image = mpImage->copy(x,y,w,h);
  return image;
}
/** No descriptions */
void ImageBuffer::setAspectRatio(double aspect)
{
  mAspectRatio = aspect;
}
/** No descriptions */
double ImageBuffer::aspectRatio()
{
  return mAspectRatio;
}
