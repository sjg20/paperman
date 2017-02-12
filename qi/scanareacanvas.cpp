/***************************************************************************
                          scanareacanvas.cpp  -  description
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

#include "scanareacanvas.h"
#include "qxmlconfig.h"

#include <qapplication.h>
#include <qcolor.h>
#include <qcursor.h>
#include <qpaintdevice.h>
#include <qpen.h>
#include <qmatrix.h>
//Added by qt3to4:
#include <QResizeEvent>
#include <QPixmap>
#include <QMouseEvent>

ScanAreaCanvas::ScanAreaCanvas(QWidget* parent,const char* name,Qt::WFlags f)
               :Q3CanvasView(0,parent,name,f)
{
  mTlxPercentChanged = false;
  mTlyPercentChanged = false;
  mBrxPercentChanged = false;
  mBryPercentChanged = false;
  mSelectionEnabled = false;
  mMode = 0;
  mLineOffset = 0;
  mLineToggle = 0;
  mScaleFactor = 1.0;
  mAspectRatio = 1.0;
  mLmbPressed = false;
  mMultiSelectionMode = false;
  initWidget();
  viewport()->setMouseTracking(true);
}
ScanAreaCanvas::~ScanAreaCanvas()
{
}
/**  */
void ScanAreaCanvas::initWidget()
{
  setFrameStyle(NoFrame);
  //Construct a QCanvas which is viewed through this QCanvasView
  mpCanvas = new Q3Canvas();
//  mpCanvas->setBackgroundColor(QColor(gray));
  setCanvas(mpCanvas);
  mRectVector.clear ();
  for(int i=0;i<20;i++)
  {
    CanvasRubberRectangle *cr = new CanvasRubberRectangle (mpCanvas);
    mRectVector.append(cr);
    cr->setTlx(0.25);
    cr->setTly(0.25);
    cr->setBrx(0.75);
    cr->setBry(0.75);
  }
  mRectVectorIndex = 0;
  mActiveRect = 0;
  mRectVector[0]->show();
}
/**  */
void ScanAreaCanvas::scaleRects()
{
  QPixmap pix = mpCanvas->backgroundPixmap();
  //resize rectangles
  for(int i=0;i<mRectVector.size();i++)
  {
    CanvasRubberRectangle *cr = mRectVector[i];
    int x = int(cr->tlx()*double(pix.width()));
    int y = int(cr->tly()*double(pix.height()));
    int w = int(cr->brx()*double(pix.width())) - x;
    int h = int(cr->bry()*double(pix.height())) -y;
    cr->setX(double(x));
    cr->setY(double(y));
    cr->setSize(w,h);
  }
}
/**  */
void ScanAreaCanvas::contentsMousePressEvent(QMouseEvent* me)
{
	if(me->button()!=Qt::LeftButton) return;
  if(!mSelectionEnabled)
    return;
  mOldMousePoint = me->pos();
  mLmbPressed = true;
  if (mRectVector[mActiveRect]->rect().contains(mOldMousePoint))
  {
    mMustMove  = true;
  }
  else
  {
    for(int i=0;i<int(mRectVector.size());i++)
    {
      CanvasRubberRectangle *cr = mRectVector[i];
      if (cr->rect().contains(mOldMousePoint) &&
          cr->visible())
      {
        setActiveRect(i);
        mMustMove  = true;
    		//near left border ?
    	 	if((mOldMousePoint.x()-cr->x())<5) mCursorState=1;
    	 	//near top border ?
    	 	if((mOldMousePoint.y()-cr->y())<5) mCursorState+=2;
    		//near right border ?
    		if((cr->x()+cr->width()-mOldMousePoint.x())<5)
          mCursorState+=4;
     		//near bottom border ?
    		if((cr->y()+cr->height()-mOldMousePoint.y())<5)
          mCursorState+=8;
     	  if((mCursorState==1)||(mCursorState==4)) viewport()->setCursor(Qt::sizeHorCursor);
     	  if((mCursorState==2)||(mCursorState==8)) viewport()->setCursor(Qt::sizeVerCursor);
     	  if((mCursorState==3)||(mCursorState==12)) viewport()->setCursor(Qt::sizeFDiagCursor);
     	  if((mCursorState==6)||(mCursorState==9)) viewport()->setCursor(Qt::sizeBDiagCursor);
            if(mCursorState==0) {
          if(mMode == 0)
      			viewport()->setCursor(Qt::sizeAllCursor);
          else
      			viewport()->setCursor(Qt::arrowCursor);
            }
        emit signalNewActiveRect(i);
        return;
      }
    }
    mMustMove  = false;
  }
}
/**  */
void ScanAreaCanvas::contentsMouseReleaseEvent(QMouseEvent* me)
{
  if(!mSelectionEnabled)
    return;
  CanvasRubberRectangle *cr = mRectVector[mActiveRect];
	if(me->button()==Qt::LeftButton)
	{
    //emit percent signal, if value has changed
    if(mTlxPercentChanged)
    {
      if(mMultiSelectionMode)
        emit signalTlxPercent(overallTlx());
      else
        emit signalTlxPercent(cr->tlx());
      mTlxPercentChanged = false;
    }
    if(mTlyPercentChanged)
    {
      if(mMultiSelectionMode)
        emit signalTlyPercent(overallTly());
      else
        emit signalTlyPercent(cr->tly());
      mTlyPercentChanged = false;
    }
    if(mBrxPercentChanged)
    {
      if(mMultiSelectionMode)
        emit signalBrxPercent(overallBrx());
      else
        emit signalBrxPercent(cr->brx());
      mBrxPercentChanged = false;
    }
    if(mBryPercentChanged)
    {
      if(mMultiSelectionMode)
        emit signalBryPercent(overallBry());
      else
        emit signalBryPercent(cr->bry());
      mBryPercentChanged = false;
    }
    //Don't emit the signal, if the user moved the rectangle, but didn't
    //change it's size.
    if((mCursorState != 0) || (mMultiSelectionMode == true))
		  emit signalUserSetSize();
	}
  mLmbPressed = false;
  mMustMove = false;
}
/**  */
void ScanAreaCanvas::contentsMouseMoveEvent(QMouseEvent* me)
{
  CanvasRubberRectangle *cur = mRectVector[mActiveRect];

//s  if(!mRectVector[mActiveRect])
//    return;
  if(!mSelectionEnabled)
    return;
  int dx,dy;

  QPoint point;
  point = me->pos();
  if(!mLmbPressed)
  {
    mCursorState = 0;
    if(cur->rect().contains(point))
    {
  		//near left border ?
  	 	if((point.x()-cur->x())<5) mCursorState=1;
  	 	//near top border ?
  	 	if((point.y()-cur->y())<5) mCursorState+=2;
  		//near right border ?
  		if((cur->x()+cur->width()-point.x())<5)
        mCursorState+=4;
   		//near bottom border ?
  		if((cur->y()+cur->height()-point.y())<5)
        mCursorState+=8;
   	  if((mCursorState==1)||(mCursorState==4)) viewport()->setCursor(Qt::sizeHorCursor);
   	  if((mCursorState==2)||(mCursorState==8)) viewport()->setCursor(Qt::sizeVerCursor);
   	  if((mCursorState==3)||(mCursorState==12)) viewport()->setCursor(Qt::sizeFDiagCursor);
   	  if((mCursorState==6)||(mCursorState==9)) viewport()->setCursor(Qt::sizeBDiagCursor);
      if(mCursorState==0) {
        if(mMode == 0)
    			viewport()->setCursor(Qt::sizeAllCursor);
        else
    			viewport()->setCursor(Qt::arrowCursor);
      }
    }
    else
    {
			viewport()->setCursor(Qt::arrowCursor);
    }
  }
  else
  {
    if(mMustMove)
    {
      dx = point.x()-mOldMousePoint.x();
      dy = point.y()-mOldMousePoint.y();
      ensureVisible (point.x(),point.y(),1,1);
      if((mCursorState == 0) && (mMode == 0))
      {
        if(cur->x()+dx < 0)
         dx = int(-1*cur->x());
        if(cur->x()+dx+cur->width() > mpCanvas->width())
         dx = int(mpCanvas->width()-cur->x()-
                  cur->width());
        if(cur->y()+dy < 0)
         dy = int(-1*cur->y());
        if(cur->y()+dy+cur->height() > mpCanvas->height())
         dy = int(mpCanvas->height()-cur->y()-
              cur->height());
        cur->moveBy(dx,dy);
      }
      if(mCursorState & 1)
      {
        if(cur->x()+dx < 0)
          dx = int(-1*cur->x());
        else if(cur->width()-dx < 1)
          dx = -1*cur->width()+1;
        if((cur->x()+dx) <
           (cur->x()+cur->width() - 10))
        {
          cur->setSize(cur->width()-dx,
                                    cur->height());
          cur->setX(cur->x()+dx);
        }
      }
      if(mCursorState & 2)
      {
        if(cur->y()+dy < 0)
          dy = int(-1*cur->y());
        else if(cur->height()-dy < 1)
          dy = cur->height()-1;
        if((cur->y()+dy) <
           (cur->y()+cur->height() - 10))
        {
          cur->setSize(cur->width(),
                                    cur->height()-dy);
          cur->setY(cur->y()+dy);
        }
      }
      if(mCursorState & 4)
      {
        if(cur->x()+dx+cur->width() > mpCanvas->width())
          dx = int(mpCanvas->width()-cur->x()-cur->width());
        else if(cur->width()+dx < 1)
          dx = -1*cur->width()+1;
        cur->setSize(cur->width()+dx,
                                  cur->height());
        if(cur->width() < 10)
        {
          cur->setSize(10,cur->height());
        }
      }
      if(mCursorState & 8)
      {
        if(cur->y()+dy+cur->height() > mpCanvas->height())
          dy = int(mpCanvas->height()-cur->y()-cur->height());
        else if(cur->height()+dy < 1)
          dy = -1*cur->height()+1;
        cur->setSize(cur->width(),
                                  cur->height()+dy);
        if(cur->height() < 10)
        {
          cur->setSize(cur->width(),10);
        }
      }


    if(cur->tlx() != double(cur->rect().left())/
                                          double(mpCanvas->width()-1))
    {
      mTlxPercentChanged = true;
      double tlx;
      tlx = double(cur->rect().left())/
            double(mpCanvas->width()-1);
      if(tlx<0.0) tlx = 0.0;
      if(tlx>1.0) tlx = 1.0;
      cur->setTlx(tlx);
    }
    if(cur->tly() != double(cur->rect().top())/
                                          double(mpCanvas->height()-1))
    {
      mTlyPercentChanged = true;
      double tly;
      tly = double(cur->rect().top())/
            double(mpCanvas->height()-1);
      if(tly<0.0) tly = 0.0;
      if(tly>1.0) tly = 1.0;
      cur->setTly(tly);
    }
    if(cur->brx() != double(cur->rect().right())/
                                          double(mpCanvas->width()-1))
    {
      mBrxPercentChanged = true;
      double brx;
      brx = double(cur->rect().right())/
            double(mpCanvas->width()-1);
      if(brx<0.0) brx = 0.0;
      if(brx>1.0) brx = 1.0;
      cur->setBrx(brx);
    }
    if(cur->bry() != double(cur->rect().bottom())/
                                          double(mpCanvas->height()-1))
    {
      mBryPercentChanged = true;
      double bry;
      bry = double(cur->rect().bottom())/
            double(mpCanvas->height()-1);
      if(bry<0.0) bry = 0.0;
      if(bry>1.0) bry = 1.0;
      cur->setBry(bry);
    }
      mOldMousePoint = point;
    }
    mpCanvas->update();
  }
}
/**  */
void ScanAreaCanvas::setImage(QImage* image)
{
  if(!image)
    return;
  mCanvasImage = image->copy();
  mCanvasPixmap.convertFromImage(*image);
  resizePixmap();
  mpCanvas->setAdvancePeriod(200);
}
/**  */
double ScanAreaCanvas::tlxPercent()
{
  return mRectVector[mActiveRect]->tlx();
}
/**  */
double ScanAreaCanvas::tlyPercent()
{
  return mRectVector[mActiveRect]->tly();
}
/**  */
double ScanAreaCanvas::brxPercent()
{
  return mRectVector[mActiveRect]->brx();
}
/**  */
double ScanAreaCanvas::bryPercent()
{
  return mRectVector[mActiveRect]->bry();
}
/**  */
void ScanAreaCanvas::setMode(int mode)
{
  mMode = mode;
}
/**  */
void ScanAreaCanvas::setRectSize(double left, double top,double right,double bottom)
{
  if(left < 0.0) left = 0.0;
  if(left > 1.0) left = 1.0;
  if(top < 0.0) top = 0.0;
  if(top > 1.0) top = 1.0;
  if(right < 0.0) right = 0.0;
  if(right > 1.0) right = 1.0;
  if(bottom < 0.0) bottom = 0.0;
  if(bottom > 1.0) bottom = 1.0;

  setTlx(left);
  setTly(top);
  setBrx(right);
  setBry(bottom);
  forceOptionUpdate();
}
/** No descriptions */
void ScanAreaCanvas::showRect(int num)
{
  if((num >= 0) && (num < int(mRectVector.size())))
    mRectVector[num]->show();
  forceOptionUpdate();
}
/** No descriptions */
void ScanAreaCanvas::hideRect(int num)
{
  if((num >= 0) && (num < int(mRectVector.size())))
    mRectVector[num]->hide();
  forceOptionUpdate();
}
/** No descriptions */
void ScanAreaCanvas::setActiveRect(int num)
{
  if((num < 0) || (num > (int(mRectVector.size()) - 1)))
    return;
  if(mActiveRect >= 0)
  {
    mRectVector[mActiveRect]->setAnimated(false);
    mRectVector[mActiveRect]->setZ(0.0);
  }
  mActiveRect = num;
  mRectVector[mActiveRect]->setAnimated(true);
  mRectVector[mActiveRect]->setZ(1.0);
}
/** No descriptions */
void ScanAreaCanvas::setRectFgColor(int num,QRgb rgb)
{
  if((num < 0) || (num > int(mRectVector.size())-1))
    return;
  mRectVector[num]->setFgColor(rgb);
}
/** No descriptions */
void ScanAreaCanvas::setRectBgColor(int num,QRgb rgb)
{
  if((num < 0) || (num > int(mRectVector.size())-1))
    return;
  mRectVector[num]->setBgColor(rgb);
}
/** No descriptions */
void ScanAreaCanvas::setMultiSelectionMode(bool on)
{
  int ui;
  mMultiSelectionMode = on;
  if(on)
  {
    for(ui = 0;ui < mRectVector.size();ui++)
    {
      if(mRectVector[ui]->userSelected())
        mRectVector[ui]->show();
    }
    mRectVectorIndex = 0;
  }
  else
  {
    for(ui = 1;ui < mRectVector.size();ui++)
      mRectVector[ui]->hide();
    mRectVectorIndex = 0;
  }
  setActiveRect(0);
  forceOptionUpdate();
}
/** No descriptions */
void ScanAreaCanvas::setUserSelected(int num,bool state)
{
  if((num < 0) || (num > int(mRectVector.size())-1))
    return;
  mRectVector[num]->setUserSelected(state);
}
/** No descriptions */
void ScanAreaCanvas::resizeEvent(QResizeEvent* e)
{
  Q3CanvasView::resizeEvent(e);
  resizePixmap();
  viewport()->update();
  mpCanvas->update();
  scaleRects();
}
/** No descriptions */
void ScanAreaCanvas::setTlx(double value)
{
  mRectVector[mActiveRect]->setTlx(value);
  redrawRect(mActiveRect);
  mpCanvas->update();
}
/** No descriptions */
void ScanAreaCanvas::setTly(double value)
{
  mRectVector[mActiveRect]->setTly(value);
  redrawRect(mActiveRect);
  mpCanvas->update();
}
/** No descriptions */
void ScanAreaCanvas::setBrx(double value)
{
  mRectVector[mActiveRect]->setBrx(value);
  redrawRect(mActiveRect);
  mpCanvas->update();
}
/** No descriptions */
void ScanAreaCanvas::setBry(double value)
{
  mRectVector[mActiveRect]->setBry(value);
  redrawRect(mActiveRect);
  mpCanvas->update();
}
/** No descriptions */
void ScanAreaCanvas::redrawRect(int num)
{
  if((num < 0) || (num > int(mRectVector.size())-1))
    return;
  QPixmap pix = mpCanvas->backgroundPixmap();
  int x = int(mRectVector[num]->tlx()*double(pix.width()));
  int y = int(mRectVector[num]->tly()*double(pix.height()));
  int w = int(mRectVector[num]->brx()*double(pix.width())) - x;
  int h = int(mRectVector[num]->bry()*double(pix.height())) -y;
  mRectVector[num]->setX(double(x));
  mRectVector[num]->setY(double(y));
  mRectVector[num]->setSize(w,h);
}
/** No descriptions */
double ScanAreaCanvas::overallTlx()
{
  double tlx;
  tlx = 1.0;
  for(int i=0;i<mRectVector.size();i++)
  {
    if((mRectVector[i]->tlx() < tlx) &&
       mRectVector[i]->visible())
      tlx = mRectVector[i]->tlx();
  }
  return tlx;
}
/** No descriptions */
double ScanAreaCanvas::overallTly()
{
  double tly;
  tly = 1.0;
  for(int i=0;i<mRectVector.size();i++)
  {
    if((mRectVector[i]->tly() < tly) &&
       mRectVector[i]->visible())
      tly = mRectVector[i]->tly();
  }
  return tly;
}
/** No descriptions */
double ScanAreaCanvas::overallBrx()
{
  double brx;
  brx = 0.0;
  for(int i=0;i<mRectVector.size();i++)
  {
    if((mRectVector[i]->brx() > brx) &&
       mRectVector[i]->visible())
      brx = mRectVector[i]->brx();
  }
  return brx;
}
/** No descriptions */
double ScanAreaCanvas::overallBry()
{
  double bry;
  bry = 0.0;
  for(int i=0;i<mRectVector.size();i++)
  {
    if((mRectVector[i]->bry() > bry) &&
       mRectVector[i]->visible())
      bry = mRectVector[i]->bry();
  }
  return bry;
}
/** No descriptions */
void ScanAreaCanvas::forceOptionUpdate()
{
  //force update of scan area options
  if(mMultiSelectionMode)
  {
    emit signalTlxPercent(overallTlx());
    emit signalTlyPercent(overallTly());
    emit signalBrxPercent(overallBrx());
    emit signalBryPercent(overallBry());
  }
  else
  {
    emit signalTlxPercent(mRectVector[mActiveRect]->tlx());
    emit signalTlyPercent(mRectVector[mActiveRect]->tly());
    emit signalBrxPercent(mRectVector[mActiveRect]->brx());
    emit signalBryPercent(mRectVector[mActiveRect]->bry());
  }
}
/** No descriptions */
const QVector <double> ScanAreaCanvas::selectedRects()
{
  QVector <double> da;
  da.resize(0);
  double tlx,tly,brx,bry;
  double tlxm,tlym,brxm,brym;//mapped values
  double factorx;
  double factory;
  tlx = overallTlx();
  tly = overallTly();
  brx = overallBrx();
  bry = overallBry();
  for(int ui=0;ui<mRectVector.size();ui++)
  {
    if(mRectVector[ui]->visible())
    {
      da.resize(da.size() + 4);
      tlxm = mRectVector[ui]->tlx();
      tlym = mRectVector[ui]->tly();
      brxm = mRectVector[ui]->brx();
      brym = mRectVector[ui]->bry();
      
      factorx = (brx - tlx);
      factory = (bry - tly);
      
      
      tlxm = (tlxm -tlx) / factorx;
      tlym = (tlym -tly) / factory;
      brxm = (brxm -tlx) / factorx;
      brym = (brym -tly) / factory;
      if(tlxm < 0.0) tlxm = 0.0;
      if(tlxm > 1.0) tlxm = 1.0;
      if(tlym < 0.0) tlym = 0.0;
      if(tlym > 1.0) tlym = 1.0;
      if(brxm < 0.0) brxm = 0.0;
      if(brxm > 1.0) brxm = 1.0;
      if(brym < 0.0) brym = 0.0;
      if(brym > 1.0) brym = 1.0;
      da[da.size()-4] = tlxm;
      da[da.size()-3] = tlym;
      da[da.size()-2] = brxm;
      da[da.size()-1] = brym;
    }
  }
  return da;
}
/** No descriptions */
void ScanAreaCanvas::resizePixmap()
{
  bool smooth_scale = xmlConfig->boolValue("PREVIEW_SMOOTH_SCALING",false);
  int nw,nh;
  int w1 = width();
  int h1 = height();
  int w2 = mCanvasPixmap.width();
  int h2 = mCanvasPixmap.height();

  double f1 = double(w1) / double(h1);
  double sfh,sfv;
  //width > height
  if(mAspectRatio < f1)
  {
    sfv = (double(h1)/double(h2));
    nh = h1;
    nw = int(double(nh) * mAspectRatio);
    sfh = (double(nw)/double(w2));
  }
  else
  {
    sfh = (double(w1)/double(w2));
    nw = w1;
    nh = int(double(nw) / mAspectRatio);
    sfv = (double(nh)/double(h2));
  }

  QPixmap pix;
  if(smooth_scale == true)
  {
    QImage im = mCanvasImage.smoothScale(nw,nh);
    pix.convertFromImage(im);
  }
  else
  {
    QMatrix m;				
    m.scale(sfh,sfv);
  	pix = mCanvasPixmap.xForm( m );
  }
  mpCanvas->resize(pix.width(),pix.height());
  viewport()->setMaximumSize(pix.width(),pix.height());
  resizeContents(pix.width(),pix.height());
  //resize rectangle
  mpCanvas->setBackgroundPixmap(pix);
  viewport()->update();
}
/** Sets the aspect ratio: aspect = width/height */
void ScanAreaCanvas::setAspectRatio(double aspect)
{
  mAspectRatio = aspect;
  resizePixmap();
}
/** No descriptions */
void ScanAreaCanvas::enableSelection(bool state)
{
  if(!state)
  {
    for(int i=0;i<mRectVector.size();i++)
    {
      mRectVector[i]->hide();
    }
    mpCanvas->setAdvancePeriod(2000);
    mSelectionEnabled = false;
  }
  else
  {
    mRectVector[0]->show();
    mpCanvas->setAdvancePeriod(200);
    mSelectionEnabled = true;
  }
}
/** No descriptions */
void ScanAreaCanvas::clearPreview()
{
  mCanvasImage.fill(0xffffff);
  resizePixmap();
}
/** No descriptions */
bool ScanAreaCanvas::selectionEnabled()
{
  return mSelectionEnabled;
}
/** No descriptions */
QVector <ScanArea*> ScanAreaCanvas::scanAreas()
{
  ScanArea* sca;
  QVector <ScanArea*> vec;
  vec.resize(0);
  for(int i=0;i<mRectVector.size();i++)
  {
    if(mRectVector[i]->visible())
    {
      sca = new ScanArea(QString::null,mRectVector[i]->tlx(),mRectVector[i]->tly(),
                                       mRectVector[i]->brx(),mRectVector[i]->bry());
      sca->setNumber(i);
      vec.append(sca);
    }
  }
  return vec;
}
