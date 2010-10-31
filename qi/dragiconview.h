/***************************************************************************
                          dragiconview.h  -  description
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

#ifndef DRAGICONVIEW_H
#define DRAGICONVIEW_H

#include <qiconview.h>

/**
  *@author Michael Herder
  */
class QListViewItem;
class QKeyEvent;

class DragIconView : public QIconView
{
Q_OBJECT
public:
	DragIconView(QWidget* parent=0,const char* name =0,WFlags f=0);
	~DragIconView();
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
  void signalDragStarted(DragIconView* iconview);
};

#endif
