/***************************************************************************
                          imagedetection.h  -  description
                             -------------------
    begin                : Tue Jun 18 2002
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

#ifndef IMAGEDETECTION_H
#define IMAGEDETECTION_H

//s #include <qarray.h>
#include <qcolor.h>
#include <qimage.h>

/**
  *@author Michael Herder
  */

class ImageDetection
{
public: 
  ImageDetection(QImage* image=0,bool multiple_images=false,QRgb rgb=0,
                 double factor=1.2,double min_size=0.02);
  ~ImageDetection();
  /**  */
  void setImage(QImage* image);
  /** No descriptions */
  QVector<double> autoSelect();
  /** No descriptions */
  void setGrayLimit(int min_or_max,bool black_bg);
private:
  /** No descriptions */
  QImage* mpImage;
  /** Find the first line, beginning at the bottom of the image, which
      is not filled with color rgb.
   */
  int lastValidLine();
  /** No descriptions */
  QVector <int> findVerticalLines(double std_dev);
  /** No descriptions */
  QVector <int> findHorizontalLines(int top,int bottom,double std_dev);
  /** No descriptions */
  bool mMultipleImages;
  /** No descriptions */
  QRgb mRgb;
  /** No descriptions */
  double mMinSize;
  /** No descriptions */
  double mFactor;
  bool mBlackBg;
  int mAvgMinMax;
};

#endif
