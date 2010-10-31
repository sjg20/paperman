/***************************************************************************
                          dragiconview.cpp  -  description
                             -------------------
    begin                : Tue Feb 26 2002
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

#include "dragiconview.h"
#include <qevent.h>
#include <qnamespace.h>
#include <qpoint.h>

DragIconView::DragIconView(QWidget* parent,const char* name,WFlags f)
             :QIconView(parent,name,f)
{
  mShiftOrCtrlPressed = false;
  mLmbPressed = false;
}
DragIconView::~DragIconView()
{
}
/**  */
void DragIconView::contentsMouseMoveEvent(QMouseEvent* me)
{
  //check whether we are on a ListBoxItem
  if((me->state() & Qt::LeftButton) &&
      mLmbPressed && !mShiftOrCtrlPressed)
  {
    QIconViewItem *item = findItem(contentsToViewport(me->pos()));
    if(item)
    {
      emit signalDragStarted(this);
      return;
    }
  }
  QIconView::contentsMouseMoveEvent(me);
}
/**  */
void DragIconView::keyPressEvent(QKeyEvent* e)
{
  if((e->key() == Key_Shift) || (e->key() == Key_Control))
    mShiftOrCtrlPressed = true;
  e->ignore();
}
/**  */
void DragIconView::keyReleaseEvent(QKeyEvent* e)
{
  if((e->key() == Key_Shift) || (e->key() == Key_Control))
    mShiftOrCtrlPressed = false;
  e->ignore();
}
/** No descriptions */
void DragIconView::contentsMousePressEvent(QMouseEvent* e)
{
  if(e->button() & Qt::LeftButton)
  {
    if(findItem(contentsToViewport(e->pos())))
      mLmbPressed = true;
    else
      mLmbPressed = false;
  }
  QIconView::contentsMousePressEvent(e);
}
/** No descriptions */
void DragIconView::contentsMouseReleaseEvent(QMouseEvent* e)
{
  if(e->button() & Qt::LeftButton)
  {
    mLmbPressed = false;
  }
  QIconView::contentsMouseReleaseEvent(e);
}
