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

#include <q3canvas.h>
#include <qcolor.h>
#include <qrect.h>

/**
  *@author Michael Herder
  */

class CanvasRubberRectangle : public Q3CanvasRectangle
{
public: 
 CanvasRubberRectangle(Q3Canvas* canvas);
 CanvasRubberRectangle(const QRect& rect,Q3Canvas* canvas);
 CanvasRubberRectangle(int x,int y,int width,int height,Q3Canvas* canvas);
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
