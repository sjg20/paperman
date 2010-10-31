/***************************************************************************
                          qunknownprogresswidget.cpp  -  description
                             -------------------
    begin                : Sat Dec 30 2000
    copyright            : (C) 2000 by M. Herder
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

#include "qunknownprogresswidget.h"

#include <qapplication.h>
#include <qcolor.h>
#include <qlabel.h>
#include <qpixmap.h>
#include <qrect.h>
#include <qtimer.h>
#include <qwidget.h>
#include <qwmatrix.h>
#include "progress.xpm"

QUnknownProgressWidget::QUnknownProgressWidget(QWidget* parent,
                                               const char* name, WFlags f)
                       :QFrame(parent,name,f)
{
  mpTimer = 0L;
  mPos = 0;
  mStep = 1;
  initWidget();
}
QUnknownProgressWidget::~QUnknownProgressWidget()
{
}
/**  */
void QUnknownProgressWidget::initWidget()
{
  mImage = QImage(progress_xpm);
  mPixmap.convertFromImage(mImage);

  setMinimumWidth(100);
  setFrameStyle( QFrame::Panel|QFrame::Sunken);
  setLineWidth( 2 );

  mpProgressWidget = new QLabel(this);
  mpProgressWidget->move(1,frameWidth());
  mpProgressWidget->resize(mPixmap.width(),mPixmap.height());
  mpProgressWidget->setPixmap(mPixmap);
  mpProgressWidget->hide();
}
/**  */
void QUnknownProgressWidget::slotMoveIndicator()
{
  if(!isVisible()) return;
  QRect rect = contentsRect();
  if(mPos >= rect.width()-mpProgressWidget->width()) mStep = -2;
  if(mPos < 3) mStep = 2;
  mPos += mStep;
  mpProgressWidget->move(mPos,mpProgressWidget->y());
}
/**  */
void QUnknownProgressWidget::start(int ms)
{
  stop();
  if(!mpTimer)
    mpTimer = new QTimer(this);
  if(mpTimer)
  {
    mpTimer->start(ms,FALSE);
    connect(mpTimer,SIGNAL(timeout()),this,SLOT(slotMoveIndicator()));
  }
  mpProgressWidget->show();
}
/**  */
void QUnknownProgressWidget::stop()
{
  if(mpTimer)
  {
    mpTimer->stop();
    delete mpTimer;
    mpTimer = 0L;
  }
  mpProgressWidget->hide();
}
/** No descriptions */
void QUnknownProgressWidget::resizeEvent(QResizeEvent* e)
{
  QFrame::resizeEvent(e);
  qApp->processEvents();
  if(!mImage.isNull())
  {
    QImage im = mImage.smoothScale(mImage.width(),contentsRect().height());
    mPixmap.convertFromImage(im);
    mpProgressWidget->setFixedSize(mPixmap.width(),mPixmap.height());
    mpProgressWidget->setPixmap(mPixmap);
  }
}
