/***************************************************************************
                          qunknownprogresswidget.h  -  description
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

#ifndef QUNKNOWNPROGRESSWIDGET_H
#define QUNKNOWNPROGRESSWIDGET_H

#include <qframe.h>
#include <qimage.h>
#include <qpixmap.h>

/**
  *@author M. Herder
  */
class QLabel;
class QTimer;

class QUnknownProgressWidget : public QFrame
{
Q_OBJECT
public:
	QUnknownProgressWidget(QWidget* parent=0,const char* name=0,WFlags f=0);
	~QUnknownProgressWidget();
  /**  */
  void start(int ms = 20);
  /**  */
  void stop();
private:
  int mPos;
  QTimer* mpTimer;
  QLabel* mpProgressWidget;
  QPixmap mPixmap;
  QImage mImage;
  /**  */
  int mStep;
private: //methods
  /**  */
  void initWidget();
private slots:
  /**  */
  void slotMoveIndicator();
protected: // Protected methods
  /** No descriptions */
  virtual void resizeEvent(QResizeEvent* e);
};

#endif
