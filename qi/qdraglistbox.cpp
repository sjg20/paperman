/***************************************************************************
                          qdraglistbox.cpp  -  description
                             -------------------
    begin                : Wed Jan 31 2001
    copyright            : (C) 2001 by Michael Herder
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

#include "qdraglistbox.h"
#include <qevent.h>
#include <qnamespace.h>
#include <qpoint.h>

QDragListBox::QDragListBox(QWidget* parent,const char* name,WFlags f)
             :QListView(parent,name,f)
{
  mShiftOrCtrlPressed = false;
  mLmbPressed = false;
}
QDragListBox::~QDragListBox()
{
}
/**  */
void QDragListBox::contentsMouseMoveEvent(QMouseEvent* me)
{
  QPoint point;
  QListViewItem* lbi;
  //check whether we are on a ListBoxItem
  if((me->state() & Qt::LeftButton) &&
      mLmbPressed && !mShiftOrCtrlPressed)
  {
    point = contentsToViewport(me->pos());
    lbi = itemAt(point);
    if(lbi)
    {
      emit signalDragStarted(lbi);
      return;
    }
  }
  QListView::contentsMouseMoveEvent(me);
}
/**  */
void QDragListBox::insertItem(QString item)
{
  new QListViewItem(this,item);
}
/**  */
void QDragListBox::keyPressEvent(QKeyEvent* e)
{
  if((e->key() == Key_Shift) || (e->key() == Key_Control))
    mShiftOrCtrlPressed = true;
  e->ignore();
}
/**  */
void QDragListBox::keyReleaseEvent(QKeyEvent* e)
{
  if((e->key() == Key_Shift) || (e->key() == Key_Control))
    mShiftOrCtrlPressed = false;
  e->ignore();
}
/** No descriptions */
void QDragListBox::contentsMousePressEvent(QMouseEvent* e)
{
  if(e->button() & Qt::LeftButton)
  {
    mLmbPressed = true;
  }
  QListView::contentsMousePressEvent(e);
}
/** No descriptions */
void QDragListBox::contentsMouseReleaseEvent(QMouseEvent* e)
{
  if(e->button() & Qt::LeftButton)
  {
    mLmbPressed = false;
  }
  QListView::contentsMouseReleaseEvent(e);
}
