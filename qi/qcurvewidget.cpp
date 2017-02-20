/***************************************************************************
                          qcurvewidget.cpp  -  description
                             -------------------
    begin                : Mon Oct 30 2000
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

#include "qcurvewidget.h"
#include <QPolygon>
#include <QMouseEvent>
#include <QPaintEvent>
#include <math.h>
#include <qevent.h>
#include <qpainter.h>
#include <qrect.h>
#include <qpoint.h>
#include <qsize.h>
#include <qcursor.h>
#include <qcolor.h>
#include <qmatrix.h>

QCurveWidget::QCurveWidget(QWidget *parent, const char *name )
             : QWidget(parent)
{
  setObjectName(name);
  mGamma = 1.0;
  setMouseTracking(TRUE);
  setFocusProxy(0);
  mLmbPressed = false;
  mCurveType = CurveType_Free;
  //no control point
  mSplineArray.resize(0);//
  mRectArray.resize(0);
  calcRectArray();
  mNewDataArray.resize(256); //always !
  mOldDataArray.resize(256); //always !
}
QCurveWidget::~QCurveWidget()
{
}
/**  */
void QCurveWidget::mouseMoveEvent(QMouseEvent* me)
{
  if (mCurveType == CurveType_Gamma) return;//do nothing
  if ((mCurveType == CurveType_Free) && (me->buttons() != Qt::LeftButton)) return;
  QPolygon qpa;
  int ax;
  int bx;
  int ay;
  int by;
  int y;

  QMatrix qwm(1.0,0.0,0.0,-1.0,-5.0,260.0);//to get 0,0 at bottom/left
  int i;
  bool cflag;
  cflag = false;
  mNewPoint = qwm.map(me->pos());//(mapFromGlobal(QCursor::pos()));
  if ((mCurveType == CurveType_Free) && (me->buttons() == Qt::LeftButton))//(mLmbPressed == true))
  {
    if(mOldPoint.x()<=mNewPoint.x())
    {
      ax = mOldPoint.x();
      bx = mNewPoint.x();
      ay = mOldPoint.y();
      by = mNewPoint.y();
    }
    else
    {
      ax = mNewPoint.x();
      bx = mOldPoint.x();
      ay = mNewPoint.y();
      by = mOldPoint.y();
    }
    if(ax < 0) ax = 0;
    if(bx > 255) bx = 255;
    for(i=ax;i<=bx;i++)
    {
      if(ax == bx)
        y = by;
      else
        y = ay + int(double(by-ay)*(double(i-ax)/double(bx-ax)));
      if(y<0) y = 0;
      if(y>255) y = 255;
      mNewDataArray.setPoint(i,i,y);
    }
    mOldPoint = mNewPoint;
    repaint();
    return;
  }
  for(i=0;i<(int)mRectArray.size();i++)
  {
    if(mRectArray[i]->contains(mNewPoint) == true)
    {
      //
      setCursor(Qt::CrossCursor);
      cflag=true;
      break;
    }
  }
  if(cflag==false) unsetCursor();
  if((mLmbPressed == true)&&(mRectIndex != -1))
  {
    //check whether the mouspointer is inside one of the rects
    //move rect and set control point accordingly
    if(mCurveType != CurveType_Free)
    {
      if(mRectIndex == 0)
      {
        if(mNewPoint.x()<0)mNewPoint.setX(0);
        if(mNewPoint.x()>= mSplineArray.point(1).x())
           mNewPoint.setX(mSplineArray.point(1).x()-1);
      }
      if(mRectIndex == int(mSplineArray.size())-1)
      {
        if(mNewPoint.x()>255) mNewPoint.setX(255);
        if(mNewPoint.x()<= mSplineArray.point(mSplineArray.size()-2).x())
           mNewPoint.setX(mSplineArray.point(mSplineArray.size()-2).x()+1);
      }
      if((mRectIndex > 0) && (mRectIndex < int(mSplineArray.size())-1))
      {
        if(mNewPoint.x()<= mSplineArray.point(mRectIndex-1).x())
           mNewPoint.setX(mSplineArray.point(mRectIndex-1).x()+1);
        if(mNewPoint.x()>= mSplineArray.point(mRectIndex+1).x())
           mNewPoint.setX(mSplineArray.point(mRectIndex+1).x()-1);
      }
      if(mNewPoint.y()<0)
         mNewPoint.setY(0);
      if(mNewPoint.y()>255)
         mNewPoint.setY(255);
      mRectArray[mRectIndex]->moveCenter(mNewPoint);
      mSplineArray.setPoint(mRectIndex,mNewPoint);
//      repaint();
//    }
//    else
//    {
//      if(mCurveType == CurveType_Interpolated)
//        qpa = mSplineArray.spline();
//      else if(mCurveType == CurveType_LineSegments)
//        qpa = mSplineArray.line();
//    //check array
//      for(i=0;i<int(qpa.size());i++)
//      {
//        if(qpa.point(i).y()<0) qpa.setPoint(i,qpa.point(i).x(),0);
//        if(qpa.point(i).y()>=height()) qpa.setPoint(i,qpa.point(i).x(),height()-1);
//      }
//    //copy Data in DataArray
//      int cnt;
//      cnt = 0;
//      for(i=0;i<qpa.point(0).x();i++)
//         if(i<256)mNewDataArray.setPoint(i,i,mSplineArray.point(0).y());
//      for(i=i;i<qpa.point(qpa.size()-1).x();i++)
//      {
//         if(i<256)mNewDataArray.setPoint(i,i,qpa.point(cnt).y());
//         cnt++;
//      }
//      for(i=i;i<256;i++)
//         mNewDataArray.setPoint(i,i,mSplineArray.point(mSplineArray.size()-1).y());
      calcDataArray();
      repaint();
    }
  }
}
/**  */
void QCurveWidget::mousePressEvent(QMouseEvent* me)
{
  if(mCurveType == CurveType_Gamma) return;
  QMatrix qwm(1.0,0.0,0.0,-1.0,-5.0,260.0);//to get 0,0 at bottom/left
  if((qwm.map(me->pos()).x() < 0) || (qwm.map(me->pos()).x() > 255) ||
     (qwm.map(me->pos()).y() < 0) || (qwm.map(me->pos()).y() > 255))
  {
    if(mCurveType != CurveType_Free)
    {
      return;
    }
    else
    {
      if(qwm.map(me->pos()).x() < 0) mOldPoint.setX(0);
      if(qwm.map(me->pos()).x() > 255) mOldPoint.setX(255);
      if(qwm.map(me->pos()).y() < 0) mOldPoint.setY(0);
      if(qwm.map(me->pos()).y() > 255) mOldPoint.setY(255);
      return;
    }
  }
  mOldPoint = qwm.map(me->pos());
  int i;
  int i2;
  mRectIndex = -1;
  for(i=0;i<(int)mRectArray.size();i++)
  {
    if(mRectArray[i]->contains(mOldPoint) == true)
    {
      //
      mRectIndex = i;
      break;
    }
  }

  if(me->button() == Qt::LeftButton)
  {
    mLmbPressed = true;
    //in free curve mode we can return here
    if(mCurveType == CurveType_Free) return;
    //check whether the user presses the LMB inside
    //a control point rect
    //if the mouse press event didn't occur inside the control point
    //rect, we have to add a new control point
    if(mRectIndex == -1)
    {
      //search the correct spline array index to add this new control point
      i2 = -3;
      if(mOldPoint.x()<mSplineArray.point(0).x())
        i2 = 0;
      else if(mOldPoint.x()>mSplineArray.point(mSplineArray.size()-1).x())
        i2 = -1;
      else
      {
        for(i=0;i<(int)mSplineArray.size()-1;i++)
        {
          if((mSplineArray.point(i).x()<mOldPoint.x()) &&
             (mSplineArray.point(i+1).x()>mOldPoint.x()))
          {
            i2 = i+1;
            break;
          }
        }
      }
      if(i2 == -1)//we have to add a new point
      {
        mSplineArray.resize(mSplineArray.size()+1);
        mSplineArray.setPoint(mSplineArray.size()-1,mOldPoint);
        calcRectArray();
        repaint();
      }
      if(i2 > -1)//we have to add a new point
      {
        mSplineArray.resize(mSplineArray.size()+1);
        for(i=mSplineArray.size()-1;i>i2;i--)
          mSplineArray.setPoint(i,mSplineArray.point(i-1));
        mSplineArray.setPoint(i2,mOldPoint);
        calcRectArray();
        calcDataArray();
        repaint();
      }
    }
  }
  if(me->button() == Qt::RightButton)
  {
    if((mCurveType == CurveType_Free) || (mCurveType == CurveType_Gamma)) return;
    for(i=0;i<(int)mRectArray.size();i++)
    {
      //if click occurred inside a control point rect
      //delete control point/rect if number >=3
      if((mRectArray[i]->contains(mOldPoint) == true) &&
         (mRectArray.size()>2))
      {
        delete mRectArray[i];
        for(i2=i;i2<int(mRectArray.size())-1;i2++) mRectArray[i2] = mRectArray[i2+1];
        mRectArray.resize(mRectArray.size()-1);
        for(i2=i;i2<int(mSplineArray.size())-1;i2++) mSplineArray[i2] = mSplineArray[i2+1];
        mSplineArray.resize(mSplineArray.size()-1);
        //
        break;
      }
    }
    calcDataArray();
    repaint();
  }
}
/**  */
void QCurveWidget::mouseReleaseEvent(QMouseEvent*)
{
  mLmbPressed = false;
  mRectIndex = -1;
}
/**  */
void QCurveWidget::paintEvent(QPaintEvent*)
{
  QString qs;
  int i;
  i = 0;
  QPainter p;
  p.begin(this);
  p.setClipRect(5,5,256,256);

  p.setWindow(-5,-5,266,266);
  QMatrix qwm(1.0,0.0,0.0,-1.0,0.0,255.0);//to get 0,0 at bottom/left
  p.setWorldMatrix(qwm);
//drawing

  QRect border(0,0,256,256);
  p.setPen(QColor(Qt::lightGray));
  p.drawRect(border);
  for(i=64;i<=3*64;i+=62)
  {
    p.drawLine(0,i,256,i);
    p.drawLine(i,0,i,256);
  }
  p.setPen(QColor(Qt::gray));
  p.drawPolyline(mOldDataArray);
  p.setPen(QColor(Qt::black));
  p.drawPolyline(mNewDataArray);
  if((mCurveType != CurveType_Free) && (mCurveType != CurveType_Gamma))
  {
    //draw rectangles
    for(i=0;i<int(mRectArray.size());i++)
    {
      p.drawRect(*mRectArray[i]);
    }
  }
  p.end();
}
/**  */
void QCurveWidget::calcRectArray()
{
  int i;
  QRect* qr;

  for(i=0;i<int(mRectArray.size());i++)
    delete mRectArray[i];
  mRectArray.resize(0);
  for(i=0;i<int(mSplineArray.size());i++)
  {
    qr = new QRect(QPoint(mSplineArray[i].x()-5,mSplineArray[i].y()-5),
                   QSize(10,10));
    mRectArray.resize(mRectArray.size()+1);
    mRectArray[mRectArray.size()-1] = qr;
  }
}
/**  */
void QCurveWidget::slotChangeCurveType(int index)
{
  switch(index)
  {
    case 0:
      mCurveType = CurveType_Gamma;
      setGamma(mGamma);
      break;
    case 1:
      mCurveType = CurveType_Free;
      break;
    case 2:
      mCurveType = CurveType_LineSegments;
      //if there are no control point, we set two
      if(mSplineArray.size()==0)
        mSplineArray.setPoints(2,mNewDataArray.point(0).x(),
                                mNewDataArray.point(0).y(),
                                mNewDataArray.point(mNewDataArray.size()-1).x(),
                                mNewDataArray.point(mNewDataArray.size()-1).y());
      calcRectArray();
      calcDataArray();
      break;
    case 3:
      mCurveType = CurveType_Interpolated;
      //if there are no control point, we set two
      if(mSplineArray.size()==0)
        mSplineArray.setPoints(2,mNewDataArray.point(0).x(),
                                mNewDataArray.point(0).y(),
                                mNewDataArray.point(mNewDataArray.size()-1).x(),
                                mNewDataArray.point(mNewDataArray.size()-1).y());
      calcRectArray();
      calcDataArray();
      break;
    default:
      mCurveType = CurveType_Free;
  }
  if(mCurveType == CurveType_Gamma)
    emit signalGamma(true);
  else
    emit signalGamma(false);
  repaint();
}
/**  */
void QCurveWidget::setDataArray(QPolygon qpa)
{
  int i;
  for(i=0;i<int(qpa.size());i++)
  {
    mOldDataArray.setPoint(i,qpa.point(i));
    mNewDataArray.setPoint(i,qpa.point(i));
  }
  repaint();
}
/**  */
void QCurveWidget::reset()
{
//the curve is reset to the previous curve (drawn in gray)
  int i;
  for(i=0;i<int(mOldDataArray.size());i++)
  {
    mNewDataArray[i] = mOldDataArray[i];
  }
//set number of control point to 2
  mSplineArray.resize(2);
  mSplineArray.setPoints(2,mNewDataArray.point(0).x(),
                          mNewDataArray.point(0).y(),
                          mNewDataArray.point(mNewDataArray.size()-1).x(),
                          mNewDataArray.point(mNewDataArray.size()-1).y());
  slotChangeCurveType(CurveType_Free); //display as free curve
}
/**  */
void QCurveWidget::set()
{
//the previous curve is set to the new curve (drawn in black)
  int i;
  for(i=0;i<int(mOldDataArray.size());i++)
  {
    mOldDataArray[i] = mNewDataArray[i];
  }
  repaint();
}
/**  */
void QCurveWidget::calcDataArray()
{
  QPolygon qpa;
  int cnt;
  int i;
  cnt = 0;
  i = 0;
  if(mCurveType == CurveType_Free) return;
  if(mCurveType == CurveType_Interpolated)
    qpa = mSplineArray.spline();
  else if(mCurveType == CurveType_LineSegments)
    qpa = mSplineArray.line();
  //check array
  for(i=0;i<int(qpa.size());i++)
  {
    if(qpa.point(i).y()<0) qpa.setPoint(i,qpa.point(i).x(),0);
    if(qpa.point(i).y()>=height()) qpa.setPoint(i,qpa.point(i).x(),height()-1);
  }
  //copy Data in DataArray
  for(i=0;i<qpa.point(0).x();i++)
     if(i<256)mNewDataArray.setPoint(i,i,mSplineArray.point(0).y());
  for(i=i;i<qpa.point(qpa.size()-1).x();i++)
  {
     if(i<256)mNewDataArray.setPoint(i,i,qpa.point(cnt).y());
     cnt++;
  }
  for(i=i;i<256;i++)
      mNewDataArray.setPoint(i,i,mSplineArray.point(mSplineArray.size()-1).y());
}
/**  */
QPolygon QCurveWidget::pointArray()
{
  return mOldDataArray;
}
/**  */
void QCurveWidget::setGamma(double gamma)
{
  double topval;
  double s;
  int gval;
  int i;

  mGamma = gamma;
  topval = double(mNewDataArray.size());
  s = topval/pow(topval,1.0/mGamma);
  for(i=0;i<mNewDataArray.size();i++)
  {
    gval = int(pow(double(i),1.0/mGamma)*s);
    mNewDataArray.setPoint(i,i,gval);
  }
  repaint();
}
