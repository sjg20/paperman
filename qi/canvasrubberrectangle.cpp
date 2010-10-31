/***************************************************************************
                          canvasrubberrectangle.cpp  -  description
                             -------------------
    begin                : Thu Jan 24 2002
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

#include "canvasrubberrectangle.h"

#include <qpainter.h>

CanvasRubberRectangle::CanvasRubberRectangle(Q3Canvas* canvas)
                   :Q3CanvasRectangle(canvas)
{
  initRect();
}
CanvasRubberRectangle::CanvasRubberRectangle(const QRect& rect,Q3Canvas* canvas)
                   :Q3CanvasRectangle(rect,canvas)
{
  initRect();
}
CanvasRubberRectangle::CanvasRubberRectangle(int x,int y,int width,int height,Q3Canvas* canvas)
                   :Q3CanvasRectangle(x,y,width,height,canvas)
{
  initRect();
}
CanvasRubberRectangle::~CanvasRubberRectangle()
{
}
/** No descriptions */
void CanvasRubberRectangle::initRect()
{
  mUserSelected = false;
  mLineOffset = 0;
  setFgColor(qRgb(255,255,255));
  setBgColor(qRgb(0,0,0));
  setZ(0.0);
  setAnimated(true);
  mTlx = 0.0;
  mTly = 0.0;
  mBrx = 1.0;
  mBry = 1.0;
}
/** No descriptions */
void CanvasRubberRectangle::advance(int stage)
{
  if(!visible())
    return;
  if(stage == 1)
  {
    canvas()->setAllChanged();
  }
  Q3CanvasRectangle::advance(stage);
}
/** No descriptions */
void CanvasRubberRectangle::setFgColor(QRgb rgb)
{
  mFgColor = rgb;
  mFgPen = QPen(QColor(mFgColor),0,Qt::DotLine);
}
/** No descriptions */
void CanvasRubberRectangle::setBgColor(QRgb rgb)
{
  setPen(QPen(QColor(rgb)));
}
/** No descriptions */
void CanvasRubberRectangle::drawShape(QPainter& p)
{
  Q3CanvasRectangle::drawShape(p);
  p.setPen(mFgPen);
  mLineOffset += 1;
  if(mLineOffset > 5)
    mLineOffset = 0;
  int l,t,r,b;
  l=rect().left();
  t=rect().top();
  r=rect().right();
  b=rect().bottom();

  if(animated())
  {
    if((r - l) < 6)
    {
      p.drawLine(l,t,r,t);
      p.drawLine(r,b,l,b);
    }
    else
    {
      p.drawLine(l+mLineOffset,t,r,t);
      p.drawLine(r-mLineOffset,b,l,b);
    }
    if((b - t) < 6)
    {
      p.drawLine(r,t,r,b);
      p.drawLine(l,b,l,t);
    }
    else
    {
      p.drawLine(r,t+mLineOffset,r,b);
      p.drawLine(l,b-mLineOffset,l,t);
    }
  }
  else
  {
    p.drawLine(l,t,r,t);
    p.drawLine(r,b,l,b);
    p.drawLine(r,t,r,b);
    p.drawLine(l,b,l,t);
  }
}
/** No descriptions */
bool CanvasRubberRectangle::userSelected()
{
  return mUserSelected;
}
/** No descriptions */
void CanvasRubberRectangle::setUserSelected(bool state)
{
  mUserSelected = state;
}
/** No descriptions */
void CanvasRubberRectangle::setTlx(double d)
{
  mTlx = d;
}
/** No descriptions */
void CanvasRubberRectangle::setTly(double d)
{
  mTly = d;
}
/** No descriptions */
void CanvasRubberRectangle::setBrx(double d)
{
  mBrx = d;
}
/** No descriptions */
void CanvasRubberRectangle::setBry(double d)
{
  mBry = d;
}
/** No descriptions */
double CanvasRubberRectangle::tlx()
{
  return mTlx;
}
/** No descriptions */
double CanvasRubberRectangle::tly()
{
  return mTly;
}
/** No descriptions */
double CanvasRubberRectangle::brx()
{
  return mBrx;
}
/** No descriptions */
double CanvasRubberRectangle::bry()
{
  return mBry;
}
