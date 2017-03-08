/***************************************************************************
                          qscanareawidget.cpp  -  description
                             -------------------
    begin                : Sun Jul 2 2000
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

#include "resource.h"

#include "qscanareawidget.h"
#include <qcolor.h>
#include <qimage.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qtimer.h>

QScanAreaWidget::QScanAreaWidget(QWidget *parent, const char *name )
								:QWidget(parent,name)
{
  mLineOffset = 0;
  mTlxPercent = 0.0;
  mTlyPercent = 0.0;
  mBrxPercent = 1.0;
  mBryPercent = 1.0;
  mTlxPercentChanged = false;
  mTlyPercentChanged = false;
  mBrxPercentChanged = false;
  mBryPercentChanged = false;
  setMouseTracking(true);
  setPalette(QColor(255,255,255));
	mpRect=new QRect(0,0,width()-4,height()-4);
	mLmbFlag=false;
  mCursorState=0;
  mSizeChanged=false;
}

QScanAreaWidget::~QScanAreaWidget()
{
}
/**  */
void QScanAreaWidget::mouseMoveEvent(QMouseEvent *qme)
{
	QPoint qp=qme->pos();

  int dx;
	int dy;
	int x,y;
	int r,b;
	int imin;
  dx=0;
  dy=0;
  x=0;
	y=0;
	r=0;
	b=0;
	imin=20;
  if(mLmbFlag==false)
	{
	  mCursorState=0;
  	if(mpRect->contains(qp,true)==true)
		{
			//near left border ?
 		 	if((qp.x()-mpRect->left())<6) mCursorState=1;
 		 	//near top border ?
 		 	if((qp.y()-mpRect->top())<6) mCursorState+=2;
  		//near right border ?
  		if((mpRect->right()-qp.x())<6) mCursorState+=4;
  		//near bottom border ?
  		if((mpRect->bottom()-qp.y())<6) mCursorState+=8;
   	  if((mCursorState==1)||(mCursorState==4)) setCursor(sizeHorCursor);
   	  if((mCursorState==2)||(mCursorState==8)) setCursor(sizeVerCursor);
   	  if((mCursorState==3)||(mCursorState==12)) setCursor(sizeFDiagCursor);
   	  if((mCursorState==6)||(mCursorState==9)) setCursor(sizeBDiagCursor);
			if(mCursorState==0)
      {
				setCursor(sizeAllCursor);
        mCursorState=16;
			}
     }
		 else
	   {
#ifdef USE_QT3
       setCursor(arrowCursor);
#else
       setCursor(ArrowCursor);
#endif
     }
  }
	else
	{
  	if((mCursorState==1)||(mCursorState==3)||(mCursorState==9))
	 	{
 	 	//	setCursor(sizeHorCursor);
	 		mpRect->setLeft(mpRect->left()+qp.x()-mStartPoint.x());
     	if(mpRect->left()<0)mpRect->setLeft(0);
     	if(mpRect->left()>(mpRect->right()-imin)) mpRect->setLeft(mpRect->right()-imin);
      mStartPoint.setX(qp.x());
   	}
  	if((mCursorState==2)||(mCursorState==3)||(mCursorState==6))
	 	{
 	 	//	setCursor(sizeHorCursor);
	 		mpRect->setTop(mpRect->top()+qp.y()-mStartPoint.y());
     	if(mpRect->top()<0)mpRect->setTop(0);
     	if(mpRect->top()>(mpRect->bottom()-imin)) mpRect->setTop(mpRect->bottom()-imin);
      mStartPoint.setY(qp.y());
   	}
  	if((mCursorState==4)||(mCursorState==6)||(mCursorState==12))
	 	{
 	 	//	setCursor(sizeHorCursor);
	 		mpRect->setRight(mpRect->right()+qp.x()-mStartPoint.x());
     	if(mpRect->right()>width())mpRect->setRight(width());
     	if(mpRect->right()<(mpRect->left()+imin)) mpRect->setRight(mpRect->left()+imin);
      mStartPoint.setX(qp.x());
    }
   	if((mCursorState==8)||(mCursorState==9)||(mCursorState==12))
	 	{
 	 	//	setCursor(sizeHorCursor);
			mpRect->setBottom(mpRect->bottom()+qp.y()-mStartPoint.y());
     	if(mpRect->bottom()>height())mpRect->setBottom(height());
     	if(mpRect->bottom()<(mpRect->top()+imin)) mpRect->setBottom(mpRect->top()+imin);
      mStartPoint.setY(qp.y());
   	}
  	if(mCursorState==16)
	 	{
	    dx=qp.x()-mStartPoint.x();
	    dy=qp.y()-mStartPoint.y();
 	    //check whether we move ouside the widget
	    if(mpRect->left()+dx<0) dx=-mpRect->left();
	    if(mpRect->right()+dx>width()-1) dx=width()-1-mpRect->right();
	    if(mpRect->top()+dy<0) dy=-mpRect->top();
	    if(mpRect->bottom()+dy>height()-1) dy=height()-1-mpRect->bottom();
	    mpRect->moveBy(dx,dy);
  	 	mStartPoint=qp;
		}
    if(mpRect->right()>width()-1)mpRect->setRight(width()-1);
    if(mpRect->bottom()>height()-1)mpRect->setBottom(height()-1);

    if(mTlxPercent != double(mpRect->left())/double(width()-1))
    {
      mTlxPercentChanged = true;
      mTlxPercent = double(mpRect->left())/double(width()-1);
      if(mTlxPercent<0.0) mTlxPercent = 0.0;
      if(mTlxPercent>1.0) mTlxPercent = 1.0;
    }
    if(mTlyPercent != double(mpRect->top())/double(height()-1))
    {
      mTlyPercentChanged = true;
      mTlyPercent = double(mpRect->top())/double(height()-1);
      if(mTlyPercent<0.0) mTlyPercent = 0.0;
      if(mTlyPercent>1.0) mTlyPercent = 1.0;
    }
    if(mBrxPercent != double(mpRect->right())/double(width()-1))
    {
      mBrxPercentChanged = true;
      mBrxPercent = double(mpRect->right())/double(width()-1);
      if(mBrxPercent<0.0) mBrxPercent = 0.0;
      if(mBrxPercent>1.0) mBrxPercent = 1.0;
    }
    if(mBryPercent != double(mpRect->bottom())/double(height()-1))
    {
      mBryPercentChanged = true;
      mBryPercent = double(mpRect->bottom())/double(height()-1);
      if(mBryPercent<0.0) mBryPercent = 0.0;
      if(mBryPercent>1.0) mBryPercent = 1.0;
    }
    drawScanRect();
    mSizeChanged=true;
  }		
}

/**  */
void QScanAreaWidget::resizeRect(double leftpercent,double toppercent,
                                 double rightpercent,double bottompercent)
{
  mTlxPercent = leftpercent;
  mTlyPercent = toppercent;
  mBrxPercent = rightpercent;
  mBryPercent = bottompercent;

  int left,top,right,bottom;
  left = int(double(width())*leftpercent);
  top = int(double(height())*toppercent);
  right = int(double(width())*rightpercent);
  bottom = int(double(height())*bottompercent);

  if(left<0) left=0;
  if(top<0) top=0;
  if(right>width()-1) right=width()-1;
  if(bottom>height()-1) bottom=height()-1;
	mpRect->setLeft(left);
	mpRect->setTop(top);
	mpRect->setRight(right);
	mpRect->setBottom(bottom);
	drawScanRect();
}
/**  */
void QScanAreaWidget::paintEvent(QPaintEvent*)
{
  if(mPixmap.isNull() != true) bitBlt( this,0,0,&mPixmap,0,0,-1,-1);
	drawScanRect();
}
/**  */
void QScanAreaWidget::mousePressEvent(QMouseEvent *e)
{
	if(e->button()==LeftButton)
	{
 		mLmbFlag=true;
    mStartPoint=e->pos();
	}
}
/**  */
void QScanAreaWidget::mouseReleaseEvent(QMouseEvent *e)
{
	if(e->button()==LeftButton)
	{
		mLmbFlag=false;
    mSizeChanged=false;
    //emit percent signal, if value has changed
    if(mTlxPercentChanged)
    {
      emit signalTlxPercent(mTlxPercent);
      mTlxPercentChanged = false;
    }
    if(mTlyPercentChanged)
    {
      emit signalTlyPercent(mTlyPercent);
      mTlyPercentChanged = false;
    }
    if(mBrxPercentChanged)
    {
      emit signalBrxPercent(mBrxPercent);
      mBrxPercentChanged = false;
    }
    if(mBryPercentChanged)
    {
      emit signalBryPercent(mBryPercent);
      mBryPercentChanged = false;
    }
		emit signalUserSetSize();
	}
}

/**  */
void QScanAreaWidget::drawScanRect()
{
  mOldRect = mOldRect.normalize();
  if(mPixmap.isNull() != true)
	{
  	bitBlt( this,mOldRect.x(),mOldRect.y(),&mPixmap,
								mOldRect.x(),mOldRect.y(), mOldRect.width(),mOldRect.height() );
  }
	else
	{
		erase();
	}
  QPainter painter;
	painter.begin(this);
  painter.setPen(white);
  *mpRect = mpRect->normalize();
  painter.drawRect(*mpRect);
  int l,t,r,b;
  l=mpRect->left();
  t=mpRect->top();
  r=mpRect->right();
  b=mpRect->bottom();

  painter.setPen(DotLine);
  if((r - l) < 6)
  {
    painter.drawLine(l,t,r,t);
    painter.drawLine(r,b,l,b);
  }
  else
  {
    painter.drawLine(l+mLineOffset,t,r,t);
    painter.drawLine(r-mLineOffset,b,l,b);
  }
  if((b - t) < 6)
  {
    painter.drawLine(r,t,r,b);
    painter.drawLine(l,b,l,t);
  }
  else
  {
    painter.drawLine(r,t+mLineOffset,r,b);
    painter.drawLine(l,b-mLineOffset,l,t);
  }
  painter.end();
  mOldRect=*mpRect;
}
/**  */
QRect QScanAreaWidget::getSizeRect()
{
	return *mpRect;
}
/**  */
void QScanAreaWidget::setPixmap(QString path)
{
  mImage.load(path);
  QImage image2=mImage.smoothScale(width(),height());
  mPixmap.convertFromImage(image2);
  if(mPixmap.isNull() != true) bitBlt( this,0,0,&mPixmap, 0, 0, -1, -1 );
}
/**  */
void QScanAreaWidget::slotTimerEvent()
{
  QPainter painter;
	painter.begin(this);
  painter.setPen(white);
  painter.drawRect(*mpRect);
  mLineOffset += 1;
  if(mLineOffset > 5) mLineOffset = 0;
  painter.drawRect(*mpRect);
  int l,t,r,b;
  l=mpRect->left();
  t=mpRect->top();
  r=mpRect->right();
  b=mpRect->bottom();

  painter.setPen(DotLine);
  if((r - l) < 6)
  {
    painter.drawLine(l,t,r,t);
    painter.drawLine(r,b,l,b);
  }
  else
  {
    painter.drawLine(l+mLineOffset,t,r,t);
    painter.drawLine(r-mLineOffset,b,l,b);
  }
  if((b - t) < 6)
  {
    painter.drawLine(r,t,r,b);
    painter.drawLine(l,b,l,t);
  }
  else
  {
    painter.drawLine(r,t+mLineOffset,r,b);
    painter.drawLine(l,b-mLineOffset,l,t);
  }
  painter.end();
}
void QScanAreaWidget::startTimer()
{
 mTimer = new QTimer(this);
	connect(mTimer,SIGNAL(timeout()),this,SLOT(slotTimerEvent()));
	mTimer->start(200);
}/**  */
void QScanAreaWidget::scalePreviewPixmap()
{
  if(mPixmap.isNull()) return;
  QImage qi2=mImage.smoothScale(width(),height());
  mPixmap.convertFromImage(qi2);
  bitBlt( this,0,0,&mPixmap, 0, 0, -1, -1 );
}
/**  */
void QScanAreaWidget::resizeEvent(QResizeEvent* e)
{
  //recalc rect size
  mpRect->setLeft(int(mTlxPercent*double(width()-1)));
  mpRect->setTop(int(mTlyPercent*double(height()-1)));
  mpRect->setRight(int(mBrxPercent*double(width()-1)));
  mpRect->setBottom(int(mBryPercent*double(height()-1)));
  scalePreviewPixmap();
  QWidget::resizeEvent(e);
}
/**  */
void QScanAreaWidget::setTlx(double pval)
{
  mTlxPercent = pval;
  resizeRect(mTlxPercent,mTlyPercent,mBrxPercent,mBryPercent);
}
/**  */
void QScanAreaWidget::setTly(double pval)
{
  mTlyPercent = pval;
  resizeRect(mTlxPercent,mTlyPercent,mBrxPercent,mBryPercent);
}
/**  */
void QScanAreaWidget::setBrx(double pval)
{
  mBrxPercent = pval;
  resizeRect(mTlxPercent,mTlyPercent,mBrxPercent,mBryPercent);
}
/**  */
void QScanAreaWidget::setBry(double pval)
{
  mBryPercent = pval;
  resizeRect(mTlxPercent,mTlyPercent,mBrxPercent,mBryPercent);
}
