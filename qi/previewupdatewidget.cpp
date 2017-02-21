/***************************************************************************
                          previewupdatewidget.cpp  -  description
                             -------------------
    begin                : Mon May 13 2002
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

#include <QImageReader>

#include "err.h"
#include "previewupdatewidget.h"
#include "qimageioext.h"

#include <qapplication.h>
#include <qbuffer.h>
#include <qpainter.h>
#include <qpaintdevice.h>
#include <qmatrix.h>
#include <QPixmap>
#include <QPaintEvent>

PreviewUpdateWidget::PreviewUpdateWidget(QWidget *parent,const char *name)
                    :QWidget(parent)
{
    setObjectName(name);
}
PreviewUpdateWidget::~PreviewUpdateWidget()
{
}
/** No descriptions */
void PreviewUpdateWidget::paintEvent(QPaintEvent* e)
{
  UNUSED(e);

  if(!mPixmap.isNull())
  {
    QPainter p(this);
//p    bitBlt(this,0,e->rect().y(),&mPixmap,0,e->rect().y(),mPixmap.width(),e->rect().height());
    p.end();
  }
}
/** No descriptions */
void PreviewUpdateWidget::setData(QByteArray & data)
{
  UNUSED(data);
#if 0 //p
  double f1,f2;
  int h,j,i;
  QPixmap pix;
  QBuffer b(&data);
  b.open( QIODevice::ReadOnly );
  QImageReader io( &b,0);
  QImage *image = qis_read_pbm_image(&io);
  if(!image)
  {
    b.close();
    return;
  }
  b.close();
  pix.convertFromImage(*image);
  delete image;
  h = pix.height();
  if(!mPixmap.isNull())
  {
    f1 = double(width())/double(mRealWidth);
    f2 = double(height())/double(mRealHeight);
    if(f1 < f2)
      f2 = f1;
    else if(f2 < f1)
      f1 = f2;
    if(f1 > 1.0) f1 = 1.0/f1;
    if(f2 > 1.0) f2 = 1.0/f2;
    QMatrix m;
    m.scale(f1,f2);
//p    pix = pix.xForm(m);
    m.map(i,h,&i,&j);
//p    bitBlt(&mPixmap,0,mBegin,&pix,0,0);
    QRect r(0,mBegin,mPixmap.width(),j);
    QPaintEvent e( r);
    QApplication::sendEvent( this, &e );
    m.map(h,0,&i,&j);
    mBegin += i;
  }
#endif
}
/** No descriptions */
void PreviewUpdateWidget::clearWidget()
{
//p  mPixmap.resize(width(),height());
  mPixmap.fill();
  QRect r(0,0,width(),height());
  QPaintEvent e( r);
  QApplication::sendEvent( this, &e );
}
/** No descriptions */
void PreviewUpdateWidget::initPixmap(int rw,int rh)
{
//p  mPixmap.resize(width(),height());
  mRealWidth = rw;
  mRealHeight = rh;
  mBegin = 0;
  mPixmap.fill();
  QRect r(0,0,width(),height());
  QPaintEvent e( r);
  QApplication::sendEvent( this, &e );
}
