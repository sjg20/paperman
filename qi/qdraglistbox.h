/***************************************************************************
                          qdraglistbox.h  -  description
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

#ifndef QDRAGLISTBOX_H
#define QDRAGLISTBOX_H

#include <qlistview.h>

/**
  *@author Michael Herder
  */
class QListViewItem;
class QKeyEvent;

class QDragListBox : public QListView
{
Q_OBJECT
public:
    QDragListBox(QWidget* parent=0,const char* name=0,Qt::WindowFlags f=Qt::WindowFlags());
	~QDragListBox();
  /**  */
  void insertItem(QString item);
private:
  /**  */
  bool mShiftOrCtrlPressed;
  /**  */
  bool mLmbPressed;
protected: // Protected methods
  /**  */
  void contentsMouseMoveEvent(QMouseEvent* me);
  /**  */
  void keyReleaseEvent(QKeyEvent* e);
  /**  */
  void keyPressEvent(QKeyEvent* e);
  /** No descriptions */
  void contentsMousePressEvent(QMouseEvent* e);
  /** No descriptions */
  void contentsMouseReleaseEvent(QMouseEvent* e);
signals: // Signals
  /**  */
  void signalDragStarted(QListViewItem* lbi);
};

#endif
