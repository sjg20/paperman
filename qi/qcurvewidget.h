/***************************************************************************
                          qcurvewidget.h  -  description
                             -------------------
    begin                : Mon Oct 30 2000
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

#ifndef QCURVEWIDGET_H
#define QCURVEWIDGET_H

#include <qwidget.h>
//s #include <qarray.h>
#include <qrect.h>
#include <qpoint.h>
#include "qsplinearray.h"
#include <QPaintEvent>
#include <QMouseEvent>
/**
  *@author M. Herder
  */

class QCurveWidget : public QWidget
{
   Q_OBJECT
public:
  enum CurveType
  {
    CurveType_Gamma,
    CurveType_Free,
    CurveType_LineSegments,
    CurveType_Interpolated
  };
	QCurveWidget(QWidget *parent=0, const char *name=0);
	~QCurveWidget();
  /**  */
  void setDataArray(QPolygon qpa);
  /**  */
  void reset();
  /**  */
  void set();
private:
  enum CurveType mCurveType;
  QPoint mOldPoint;
  /**  */
  bool mLmbPressed;
  QPoint mNewPoint;
  QSplineArray mSplineArray;
  QVector <QRect*> mRectArray;
  /**  */
  int mRectIndex;
  /**Holds the previous data  */
  QPolygon mOldDataArray;
  /**Holds the line that is drawn in the widget  */
  QPolygon mNewDataArray;
  /**  */
  double mGamma;
protected:
  /**  */
  virtual void mouseMoveEvent(QMouseEvent*);
  /**  */
  virtual void mousePressEvent(QMouseEvent* me);
  /**  */
  virtual void mouseReleaseEvent(QMouseEvent* me);
  virtual void paintEvent(QPaintEvent*);
private:
  /**  */
  void calcRectArray();
  /**  */
  void calcDataArray();
public slots:
  /**  */
  void slotChangeCurveType(int index);
public:
  /**  */
  QPolygon pointArray();
  /**  */
  void setGamma(double gamma);
signals:
  /**  */
  void signalGamma(bool b);
};

#endif
