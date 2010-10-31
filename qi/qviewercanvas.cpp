/***************************************************************************
                          qviewercanvas.cpp  -  description
                             -------------------
    begin                : Sun Aug 12 2001
    copyright            : (C) 2001 by M. Herder
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
#include "qviewercanvas.h"

#include <qapplication.h>
#include <qcolor.h>
#include <qcursor.h>
#include <qdragobject.h>
#include <qpaintdevice.h>
#include <qpen.h>
#include <qtimer.h>
#include <qwmatrix.h>

QViewerCanvas::QViewerCanvas(QWidget* parent,const char* name,WFlags f)
              :QCanvasView(0,parent,name,f)
{
  mMode = 0;
  mXPercent = 0.0;
  mYPercent = 0.0;
  mWidthPercent = 1.0;
  mHeightPercent = 1.0;
  mLineOffset = 0;
  mLineToggle = 0;
  mpCanvasRect = 0;
  mScaleFactor = 1.0;
  mLmbPressed = false;
  initWidget();
  viewport()->setMouseTracking(true);
}
QViewerCanvas::~QViewerCanvas()
{
  if(mpCanvas)
    delete mpCanvas;
}
/**  */
void QViewerCanvas::initWidget()
{
  //Construct a QCanvas which is viewed through this QCanvasView
  mpCanvas = new QCanvas();
  mpCanvas->setBackgroundColor(QColor(gray));
  setCanvas(mpCanvas);
  mpCanvasRect = new CanvasRubberRectangle(mpCanvas);
  mpCanvasRect->setZ(0.0);
  setAcceptDrops(true);
}
/**  */
QImage* QViewerCanvas::currentImage()
{
//  return &mImage;
  return 0;
}
/**  */
void QViewerCanvas::scale(double scalefactor)
{
  QPixmap pix;
  mScaleFactor = scalefactor;

  QApplication::setOverrideCursor(Qt::waitCursor);

  QWMatrix m;				
  m.scale(scalefactor,scalefactor);
	pix = mCanvasPixmap.xForm( m );
  viewport()->setMaximumSize(pix.width(),pix.height());
  resizeContents(pix.width(),pix.height());
  mpCanvas->resize(pix.width(),pix.height());
  //resize rectangle
  mpCanvas->setBackgroundPixmap(pix);
  mpCanvasRect->setX(mXPercent*double(pix.width()));
  mpCanvasRect->setY(mYPercent*double(pix.height()));
  mpCanvasRect->setSize(int(mWidthPercent*double(pix.width())),
                          int(mHeightPercent*double(pix.height())));
  viewport()->update();
  mpCanvas->setAdvancePeriod(200);
  if(!mpCanvasRect->visible())
    mpCanvasRect->setVisible(true);
  QApplication::restoreOverrideCursor();
}

/**  */
void QViewerCanvas::updatePixmap()
{
  //We simply call scale with the current scale factor
  scale(mScaleFactor);
}
/**  */
void QViewerCanvas::contentsMousePressEvent(QMouseEvent* me)
{
	if(me->button()!=LeftButton) return;
  mOldMousePoint = me->pos();
  mLmbPressed = true;
  if (mpCanvasRect->rect().contains(mOldMousePoint))
  {
    mMustMove  = true;
    if((mMode != 0) && (mCursorState == 0))
    {
      mpCanvasRect->move(mOldMousePoint.x(),mOldMousePoint.y());
      mpCanvasRect->setSize(0,0);
    }
  }
  else
  {
    mMustMove  = false;
    mpCanvasRect->move(mOldMousePoint.x(),mOldMousePoint.y());
    mpCanvasRect->setSize(0,0);
  }
}
/**  */
void QViewerCanvas::contentsMouseReleaseEvent(QMouseEvent*)
{
  if(mLmbPressed)
  {
    QRect rect = mpCanvasRect->rect();
    int l,r,t,b;
    l = rect.left();
    r = rect.right();
    t = rect.top();
    b = rect.bottom();
    if(l>r)
    {
      rect.setLeft(r+2);
      rect.setRight(l);
    }
    if(t>b)
    {
      rect.setTop(b+2);
      rect.setBottom(t);
    }
    mpCanvasRect->move(rect.x(),rect.y());
    mpCanvasRect->setSize(rect.width(),rect.height());
    emit signalRectangleChanged();
  }
  mLmbPressed = false;
  mMustMove = false;
}
/**  */
void QViewerCanvas::contentsMouseMoveEvent(QMouseEvent* me)
{
  if(!mpCanvasRect) return;
  int dx,dy;

  QPoint point;
  point = me->pos();
  if(!mLmbPressed)
  {
    mCursorState = 0;
    if(mpCanvasRect->rect().contains(point))
    {
  		//near left border ?
  	 	if((point.x()-mpCanvasRect->x())<5) mCursorState=1;
  	 	//near top border ?
  	 	if((point.y()-mpCanvasRect->y())<5) mCursorState+=2;
  		//near right border ?
  		if((mpCanvasRect->x()+mpCanvasRect->width()-point.x())<5) mCursorState+=4;
   		//near bottom border ?
  		if((mpCanvasRect->y()+mpCanvasRect->height()-point.y())<5) mCursorState+=8;
   	  if((mCursorState==1)||(mCursorState==4)) viewport()->setCursor(sizeHorCursor);
   	  if((mCursorState==2)||(mCursorState==8)) viewport()->setCursor(sizeVerCursor);
   	  if((mCursorState==3)||(mCursorState==12)) viewport()->setCursor(sizeFDiagCursor);
   	  if((mCursorState==6)||(mCursorState==9)) viewport()->setCursor(sizeBDiagCursor);
  		if(mCursorState==0)
        if(mMode == 0)
    			viewport()->setCursor(sizeAllCursor);
        else
    			viewport()->setCursor(arrowCursor);
    }
    else
    {
			viewport()->setCursor(arrowCursor);
    }
  }
  else
  {
    if(!mMustMove || ((mMode != 0) && (mCursorState == 0)))
    {
      int xp,yp,rwidth,rheight,visible_x,visible_y;
      visible_x = 0;
      visible_y = 0;
      xp = point.x();
      yp = point.y();
      if(xp <= mOldMousePoint.x())
      {
        //user moved mouse from right to left
        if(xp<0) xp = 0;
        rwidth = mpCanvasRect->rect().width()+
                  mpCanvasRect->rect().x() - xp;
        visible_x = xp;
      }
      else if(xp > mOldMousePoint.x())
      {
        //user moved mouse left to right
        if(xp > mpCanvas->width()) xp = mpCanvas->width();
        rwidth = xp-mOldMousePoint.x();
        xp = mpCanvasRect->rect().x();
        visible_x = xp + rwidth;
      }
      if(yp <= mOldMousePoint.y())
      {
        //user moved mouse bottom to top
        if(yp < 0) yp = 0;
        rheight = mpCanvasRect->rect().height()+
                  mpCanvasRect->rect().y() - yp;
        visible_y = yp;
      }
      else if(yp > mOldMousePoint.y())
      {
        //user moved mouse top to bottom
        if(yp > mpCanvas->height()) yp = mpCanvas->height();
        rheight = yp-mOldMousePoint.y();
        yp = mpCanvasRect->rect().y();
        visible_y = yp + rheight;
      }
      mpCanvasRect->move(xp,yp);
      mpCanvasRect->setSize(rwidth,rheight);
      ensureVisible(visible_x,visible_y,1,1);
      calcPercentValues();
    }
    else if(mMustMove)
    {
      dx = point.x()-mOldMousePoint.x();
      dy = point.y()-mOldMousePoint.y();
      ensureVisible (point.x(),point.y(),1,1);
      if((mCursorState == 0) && (mMode == 0))
      {
        if(mpCanvasRect->x()+dx < 0)
         dx = int(-1*mpCanvasRect->x());
        if(mpCanvasRect->x()+dx+mpCanvasRect->width() > mpCanvas->width())
         dx = int(mpCanvas->width()-mpCanvasRect->x()-mpCanvasRect->width());
        if(mpCanvasRect->y()+dy < 0)
         dy = int(-1*mpCanvasRect->y());
        if(mpCanvasRect->y()+dy+mpCanvasRect->height() > mpCanvas->height())
         dy = int(mpCanvas->height()-mpCanvasRect->y()-mpCanvasRect->height());
        mpCanvasRect->moveBy(dx,dy);
      }
      if(mCursorState & 1)
      {
        if(mpCanvasRect->x()+dx < 0)
          dx = int(-1*mpCanvasRect->x());
        else if(mpCanvasRect->width()-dx < 1)
          dx = -1*mpCanvasRect->width()+1;
        mpCanvasRect->setSize(mpCanvasRect->width()-dx,
                               mpCanvasRect->height());
        mpCanvasRect->setX(mpCanvasRect->x()+dx);
      }
      if(mCursorState & 2)
      {
        if(mpCanvasRect->y()+dy < 0)
          dy = int(-1*mpCanvasRect->y());
        else if(mpCanvasRect->height()-dy < 1)
          dy = mpCanvasRect->height()-1;
        mpCanvasRect->setSize(mpCanvasRect->width(),
                               mpCanvasRect->height()-dy);
        mpCanvasRect->setY(mpCanvasRect->y()+dy);
      }
      if(mCursorState & 4)
      {
        if(mpCanvasRect->x()+dx+mpCanvasRect->width() > mpCanvas->width())
          dx = int(mpCanvas->width()-mpCanvasRect->x()-mpCanvasRect->width());
        else if(mpCanvasRect->width()+dx < 1)
          dx = -1*mpCanvasRect->width()+1;
        mpCanvasRect->setSize(mpCanvasRect->width()+dx,
                               mpCanvasRect->height());
      }
      if(mCursorState & 8)
      {
        if(mpCanvasRect->y()+dy+mpCanvasRect->height() > mpCanvas->height())
          dy = int(mpCanvas->height()-mpCanvasRect->y()-mpCanvasRect->height());
        else if(mpCanvasRect->height()+dy < 1)
          dy = -1*mpCanvasRect->height()+1;
        mpCanvasRect->setSize(mpCanvasRect->width(),
                               mpCanvasRect->height()+dy);
      }
      mOldMousePoint = point;
      calcPercentValues();
    }
    mpCanvas->update();
  }
}
/**  */
void QViewerCanvas::setImage(QImage* image)
{
  QImage im;
  if(!image)
    return;
  //there's an annoying bug in Qt2.3.1 (at least)
  //which requires this crap
  if(image->depth() < 8)
  {
    im = image->copy();
    im = im.convertDepth(8);
  }
  else
    im = *image;
  if(!im.hasAlphaBuffer())
  {
    mCanvasPixmap.convertFromImage(im);
  }
  else
  {
    QImage alpha_image;
    alpha_image = im.copy();
    if(alpha_image.depth() < 32)
      alpha_image = alpha_image.convertDepth(32);
    alpha_image.setAlphaBuffer(false);
    QRgb rgb;
    QRgb rgb_b;
    QRgb rgb_l;
    QRgb rgb1 = QColor(gray).rgb();
    QRgb rgb2 = QColor(lightGray).rgb();
    int lcnt,xcnt,alpha,inv_alpha;
    bool b;
    b = false;
    lcnt = 0;
    xcnt = 0;
    rgb_b = rgb1;
    rgb_l = rgb1;
    for(int y=0;y<alpha_image.height();y++)
    {
      for(int x=0;x<alpha_image.width();x++)
      {
        rgb = image->pixel(x,y);
        inv_alpha = 255 - qAlpha(rgb);
        alpha = qAlpha(rgb);
        alpha_image.setPixel(x,y,
                     qRgb(qRed(rgb_b)*inv_alpha/255 + qRed(rgb)*alpha/255,
                          qGreen(rgb_b)*inv_alpha/255 + qGreen(rgb)*alpha/255,
                          qBlue(rgb_b)*inv_alpha/255 + qBlue(rgb)*alpha/255));
        ++xcnt;
        if(xcnt >= 20)
        {
          xcnt = 0;
          rgb_b = (rgb_b == rgb1) ? rgb2 : rgb1;
        }
      }
      ++lcnt;
      if(lcnt >= 20)
      {
        lcnt = 0;
        rgb_l = (rgb_l == rgb1) ? rgb2 : rgb1;
      }
      xcnt = 0;
      rgb_b = rgb_l;
    }
    mCanvasPixmap.convertFromImage(alpha_image);
  }


  updatePixmap();
  mpCanvas->setAdvancePeriod(200);
  if(!mpCanvasRect->visible())
    mpCanvasRect->setVisible(true);
}
/**  */
void QViewerCanvas::calcPercentValues()
{
  mXPercent = mpCanvasRect->x()/double(mpCanvas->width());
  mYPercent = mpCanvasRect->y()/double(mpCanvas->height());
  mWidthPercent = mpCanvasRect->width()/double(mpCanvas->width());
  mHeightPercent = mpCanvasRect->height()/double(mpCanvas->height());
}
/**  */
double QViewerCanvas::tlxPercent()
{
  return mpCanvasRect->x()/double(mpCanvas->width());
}
/**  */
double QViewerCanvas::tlyPercent()
{
  return mpCanvasRect->y()/double(mpCanvas->height());
}
/**  */
double QViewerCanvas::brxPercent()
{
  return (mpCanvasRect->width()+mpCanvasRect->x())/
          double(mpCanvas->width());
}
/**  */
double QViewerCanvas::bryPercent()
{
  return (mpCanvasRect->height()+mpCanvasRect->y())/
          double(mpCanvas->height());
}
/**  */
double QViewerCanvas::widthPercent()
{
  return mpCanvasRect->width()/double(mpCanvas->width());
}
/**  */
double QViewerCanvas::heightPercent()
{
  return mpCanvasRect->height()/double(mpCanvas->height());
}
/**  */
void QViewerCanvas::setMode(int mode)
{
  mMode = mode;
}
/**  */
void QViewerCanvas::setRectPercent(double left, double top,double right,double bottom)
{
  if(left < 0.0) left = 0.0;
  if(left > 1.0) left = 1.0;
  if(top < 0.0) top = 0.0;
  if(top > 1.0) top = 1.0;
  if(right < 0.0) right = 0.0;
  if(right > 1.0) right = 1.0;
  if(bottom < 0.0) bottom = 0.0;
  if(bottom > 1.0) bottom = 1.0;
  mXPercent = left;
  mYPercent = top;
  mWidthPercent = right - left;
  mHeightPercent = bottom - top;
  //resize rectangle
  mpCanvasRect->setX(mXPercent*double(mpCanvas->width()));
  mpCanvasRect->setY(mYPercent*double(mpCanvas->height()));
  mpCanvasRect->setSize(int(mWidthPercent*double(mpCanvas->width())),
                        int(mHeightPercent*double(mpCanvas->height())));
}
/**The caller is responsible for deletion.  */
QRect QViewerCanvas::selectedRect()
{
  calcPercentValues();
  QRect rect(int(mXPercent*double(mCanvasPixmap.width())),
             int(mYPercent*double(mCanvasPixmap.height())),
             int(mWidthPercent*double(mCanvasPixmap.width())),
             int(mHeightPercent*double(mCanvasPixmap.height())));
  return rect;
}
/** No descriptions */
void QViewerCanvas::dropEvent(QDropEvent* e)
{
  QStringList list;
  if(QUriDrag::decodeLocalFiles(e,list))
  {
    emit signalLocalImageUriDropped(list);
  }
}
/** No descriptions */
void QViewerCanvas::dragMoveEvent(QDragMoveEvent* e)
{
  if(QUriDrag::canDecode(e))
  {
    e->accept();
  }
  else
    e->ignore();
}
/** No descriptions */
void QViewerCanvas::resizeEvent(QResizeEvent* e)
{
  QCanvasView::resizeEvent(e);
  update();
}
