/***************************************************************************
                          imagedetection.cpp  -  description
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

#include "imagedetection.h"

#include <math.h>

#include <qimage.h>

ImageDetection::ImageDetection(QImage* image,bool multiple_images,QRgb rgb,
                               double factor,double min_size)
{
  mBlackBg = true;
  mAvgMinMax = 100;
  mMultipleImages = multiple_images;
  mRgb = rgb;
  mFactor = factor;
  mMinSize = min_size;
  mpImage = 0;
  if(image)
    setImage(image);
}
ImageDetection::~ImageDetection()
{
}
/**  */
void ImageDetection::setImage(QImage* image)
{
  mpImage = image;
}
/** Find the first line, beginning at the bottom of the image, which
    is not filled with color rgb.
 */
int ImageDetection::lastValidLine()
{
  int y;
  for(y = mpImage->height() - 1;y >= 0; y--)
  {
    for(int x = 0;x < mpImage->width();x++)
    {
      if(mpImage->pixel(x,y) != mRgb)
        return y;
    }
  }
  return y;
}
/** No descriptions */
QVector <int> ImageDetection::findVerticalLines(double std_dev)
{
  QVector <int> result;
  QVector <bool> v_array;
  QVector <double> v_stddev_array;
  result.resize(0);
  if(mpImage->isNull())
    return result;

  int ll = lastValidLine();

  v_array.resize(ll+1);
  v_stddev_array.resize(ll+1);
  int i;
  int avg = 0;
  for(i = 0;i <= ll ;i++)
  {
    int val = 0;
    int x;
    double sum = 0.0;
    int n;
    for(x = 0;x < mpImage->width();x++)
    {
      val += qGray(mpImage->pixel(x,i));
    }
    n = mpImage->width();
    avg = int(double(val)/double(n));
    for(x = 0;x < mpImage->width();x++)
    {
      sum += double(pow((qGray(mpImage->pixel(x,i)) - avg),2.0));
    }
    v_stddev_array[i] = sqrt(sum/double(n-1));
    if(mBlackBg == true)
    {
      if((avg < mAvgMinMax) && (v_stddev_array[i] < std_dev))
        v_array[i] = true;
      else
        v_array[i] = false;
    }
    else
    {
      if((avg > mAvgMinMax) && (v_stddev_array[i] < std_dev))
        v_array[i] = true;
      else
        v_array[i] = false;
    }
  }
  if(mMultipleImages)
  {
    bool find_bg = false;
    for(i=0;i<int(v_array.size());i++)
    {
      if(v_array[i] == find_bg)
      {
        result.resize(result.size()+1);
        result[result.size()-1] = i;
        find_bg = (find_bg == false) ? true : false;
      }
    }
    if((result.size() == 1) && (result[0] < int(v_array.size()-1)) &&
       (v_array.size() > 1))
    {
      result.resize(2);
      result[1] = v_array.size()-1;
    }
    else if((result.size() == 1) && (result[0] == int(v_array.size()-1)) &&
       (v_array.size() > 1))
    {
      result.resize(2);
      result[0] = 0;
      result[1] = v_array.size()-1;
    }
    if(((result.size() % 2) != 0) && (find_bg == true))
    {
      result.resize(result.size() + 1);
      result[result.size()-1] = v_array.size()-1;
    }
  }
  else
  {
    for(i=0;i<int(v_array.size());i++)
    {
      if(v_array[i] == false)
      {
        result.resize(result.size()+1);
        result[result.size()-1] = i;
        break;
      }
    }
    if(result.size() == 0)
    {
      result.resize(1);
      result[0] = 0;
    }
    for(i=v_array.size()-1;i>=0;i--)
    {
      if(v_array[i] == false)
      {
        result.resize(result.size()+1);
        result[result.size()-1] = i;
        break;
      }
    }
    if(result.size() == 1)
    {
      result.resize(2);
      result[1] = v_array.size()-1;
    }
  }
  return result;
}
/** No descriptions */
QVector <int> ImageDetection::findHorizontalLines(int top,int bottom,double std_dev)
{
  QVector <int> result;
  QVector <bool> h_array;
  QVector <double> h_stddev_array;
  result.resize(0);
  if(mpImage->isNull())
    return result;

  int i;
  int avg;
  int n;

  h_array.resize(mpImage->width());
  h_stddev_array.resize(mpImage->width());
  for(i = 0;i < mpImage->width();i++)
  {
    int val = 0;
    double sum = 0.0;
    for(int y = top;y <= bottom;y++)
    {
      val += qGray(mpImage->pixel(i,y));
    }
    n = bottom-top+1;
    avg = int(double(val)/double(n));
    for(int y = top;y < bottom;y++)
    {
      sum += double(pow((qGray(mpImage->pixel(i,y)) - avg),2.0));
    }
    h_stddev_array[i] = sqrt(sum/double(n-1));
    if(mBlackBg == true)
    {
      if((avg < mAvgMinMax) && (h_stddev_array[i] < std_dev))
        h_array[i] = true;
      else
        h_array[i] = false;
    }
    else
    {
      if((avg > mAvgMinMax) && (h_stddev_array[i] < std_dev))
        h_array[i] = true;
      else
        h_array[i] = false;
    }
  }
  for(i=0;i<int(h_array.size());i++)
  {
    if(h_array[i] == false)
    {
      result.resize(result.size()+1);
      result[result.size()-1] = i;
      break;
    }
  }
  for(i=h_array.size()-1;i>=0;i--)
  {
    if(h_array[i] == false)
    {
      result.resize(result.size()+1);
      result[result.size()-1] = i;
      break;
    }
  }
  return result;
}
/** No descriptions */
QVector<double> ImageDetection::autoSelect()
{
  double std_dev_max;
  double std_dev_min;
  QVector <int> lines;
  QVector <int> hlines;
  QVector <bool> h_array;
  QVector <bool> v_array;
  QVector <double> h_stddev_array;
  QVector <double> v_stddev_array;
  QVector <double> rects;
  if(mpImage->isNull())
    return rects;
  double factor = mFactor;
  double sizefactor = mMinSize;

  int ll = lastValidLine();
  if(ll <= int(double(mpImage->height())*sizefactor))
    return rects;
  rects.resize(0);
  h_array.resize(mpImage->width());
  v_array.resize(ll);
  h_stddev_array.resize(mpImage->width());
  v_stddev_array.resize(ll);

  int i;
  int avg = 0;
  double std_dev = 0.0;
  for(i = 0;i < ll ;i++)
  {
    int val = 0;
    int x;
    double sum = 0.0;
    int n;
    n = mpImage->width();
    for(x = 0;x < mpImage->width();x++)
    {
      val += qGray(mpImage->pixel(x,i));
    }
    avg = int(double(val)/double(n));
    for(x = 0;x < mpImage->width();x++)
    {
      sum += double(pow((qGray(mpImage->pixel(x,i)) - avg),2.0));
    }
    v_stddev_array[i] = sqrt(sum/double(n-1));
  }
  std_dev_max = 0.0;
  std_dev_min = 20000.0;
  for(i = 0;i < int(v_stddev_array.size()) ;i++)
  {
    if(v_stddev_array[i] > std_dev_max)
      std_dev_max = v_stddev_array[i];
    if(v_stddev_array[i] < std_dev_min)
      std_dev_min = v_stddev_array[i];
  }
//determine horizontal lines
  for(i = 0;i < mpImage->width();i++)
  {
    int val = 0;
    double sum = 0.0;
    for(int y = 0;y < ll;y++)
    {
      val += qGray(mpImage->pixel(i,y));
    }
    avg = int(double(val)/double(ll));
    for(int y = 0;y < ll;y++)
    {
      sum += double(pow((qGray(mpImage->pixel(i,y)) - avg),2.0));
    }
    h_stddev_array[i] = sqrt(sum/double(ll-1));
  }
  for(i = 0;i < int(h_stddev_array.size()) ;i++)
  {
    if(h_stddev_array[i] > std_dev_max)
      std_dev_max = h_stddev_array[i];
    if(h_stddev_array[i] < std_dev_min)
      std_dev_min = h_stddev_array[i];
  }
  std_dev = std_dev_min + factor*(std_dev_max - std_dev_min);
  qDebug("std_dev_max: %.2f",std_dev_max);
  qDebug("std_dev_min: %.2f",std_dev_min);
  qDebug("std_dev: %.2f",std_dev);

  lines = findVerticalLines(std_dev);
  for(int c=0;c<int(lines.size())-1;c+=2)
  {
    hlines = findHorizontalLines(lines[c],lines[c+1],std_dev);
    if(hlines.size() >= 2)
    {
       rects.resize(rects.size() + 4);
       rects[rects.size() - 4] = double(hlines[0])/double(mpImage->width());
       rects[rects.size() - 3] = double(hlines[1])/double(mpImage->width());
       rects[rects.size() - 2] = double(lines[c])/double(mpImage->height());
       rects[rects.size() - 1] = double(lines[c+1])/double(mpImage->height());
    }
  }
  return rects;
}
/** No descriptions */
void ImageDetection::setGrayLimit(int min_or_max,bool black_bg)
{
  mBlackBg = black_bg;
  mAvgMinMax = min_or_max;
}
