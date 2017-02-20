/***************************************************************************
                          canvasrubberrectangle.h  -  description
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

#ifndef CANVASRUBBERRECTANGLE_H
#define CANVASRUBBERRECTANGLE_H

#include <qcolor.h>
#include <qrect.h>

#include <QGraphicsRectItem>
#include <QPen>

class QGraphicsScene;

/**
  *@author Michael Herder
  */

class CanvasRubberRectangle : public QGraphicsRectItem
{
public: 
 CanvasRubberRectangle(QGraphicsScene* canvas);
 CanvasRubberRectangle(const QRect& rect,QGraphicsScene* canvas);
 CanvasRubberRectangle(int x,int y,int width,int height,QGraphicsScene* canvas);
	~CanvasRubberRectangle();
  /** No descriptions */
  void advance(int stage);
  /** No descriptions */
  void setFgColor(QRgb rgb);
  /** No descriptions */
  void setBgColor(QRgb rgb);
  /** No descriptions */
  void setUserSelected(bool state);
  /** No descriptions */
  bool userSelected();
  /** No descriptions */
  void setTlx(double d);
  /** No descriptions */
  void setTly(double d);
  /** No descriptions */
  void setBrx(double d);
  /** No descriptions */
  void setBry(double d);
  /** No descriptions */
  double tlx();
  /** No descriptions */
  double tly();
  /** No descriptions */
  double brx();
  /** No descriptions */
  double bry();
  void setSize(int width, int height);
private: // Private methods
  /** No descriptions */
  void initRect();
private:
  double mTlx;
  double mTly;
  double mBrx;
  double mBry;
  int mUserSelected;
  int mLineOffset;
  QRgb mFgColor;
  QPen mFgPen;
protected: // Protected methods
  /** No descriptions */
  void drawShape(QPainter& p);
};

#endif
