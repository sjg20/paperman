/***************************************************************************
                          scanareacanvas.h  -  description
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

#ifndef SCANAREACANVAS_H
#define SCANAREACANVAS_H

#include <QGraphicsView>

#include "canvasrubberrectangle.h"
#include "scanarea.h"

#include <qimage.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qrect.h>
#include <qstring.h>
#include <QResizeEvent>
#include <QMouseEvent>

class QGraphicsScene;

/**
  *@author M. Herder
  */

class ScanAreaCanvas : public QGraphicsView
{
Q_OBJECT
public:
  ScanAreaCanvas(QWidget* parent=0,const char* name=0,Qt::WindowFlags f=0);
  ~ScanAreaCanvas();
  /**  */
  void setImage(QImage* image);
  /**  */
  void scaleRects();
  /**  */
  void setMode(int mode);
  /**  */
  void setRectSize(double left, double top,double right,double bottom);
  /**  */
  double tlxPercent();
  /**  */
  double tlyPercent();
  /**  */
  double brxPercent();
  /**  */
  double bryPercent();
  /** No descriptions */
  void hideRect(int num);
  /** No descriptions */
  void showRect(int num);
  /** No descriptions */
  void setActiveRect(int num);
  /** No descriptions */
  void setRectBgColor(int num,QRgb rgb);
  /** No descriptions */
  void setRectFgColor(int num,QRgb rgb);
  /** No descriptions */
  void setUserSelected(int num,bool state);
  /** No descriptions */
  void setMultiSelectionMode(bool on);
  /** No descriptions */
  void setTlx(double value);
  /** No descriptions */
  void setTly(double value);
  /** No descriptions */
  void setBrx(double value);
  /** No descriptions */
  void setBry(double value);
  /** No descriptions */
  const QVector <double> selectedRects();
  /** No descriptions */
  void setAspectRatio(double aspect);
  /** No descriptions */
  void enableSelection(bool state);
  /** No descriptions */
  void clearPreview();
  /** No descriptions */
  bool selectionEnabled();
  /** No descriptions */
  QVector <ScanArea *> scanAreas();  //s
private: // Private methods
  /** */
  bool mSelectionEnabled;
  /** */
  bool mTlxPercentChanged;
  /** */
  bool mTlyPercentChanged;
  /** */
  bool mBrxPercentChanged;
  /** */
  bool mBryPercentChanged;
  /**  */
  bool mMultiSelectionMode;
  /**  */
  QVector <CanvasRubberRectangle*> mRectVector;
  /**  */
  int mRectVectorIndex;
  /**  */
  int mActiveRect;
  /**  */
  void initWidget();
  /** No descriptions */
  void redrawRect(int num);
  /** No descriptions */
  double overallTlx();
  /** No descriptions */
  double overallTly();
  /** No descriptions */
  double overallBrx();
  /** No descriptions */
  double overallBry();
  /** No descriptions */
  void forceOptionUpdate();
  /** No descriptions */
  void resizePixmap();
  /**The QCanvas we view with this QCanvasView */
  QGraphicsScene* mpCanvas;
  /** */
  QPixmap mCanvasPixmap;
  /** */
  QImage mCanvasImage;
  /** */
  int mLineOffset;
  /** */
  double mScaleFactor;
  /** */
  double mAspectRatio;
  /** */
  bool mLmbPressed;
  /** */
  bool mMustMove;
  /** */
  int mMode;
  /** */
  QPoint mOldMousePoint;
  /** */
  int mCursorState; //cursor state
  /** */
  int mLineToggle;
  /** */
protected: // Protected methods
  /**  */
  void contentsMousePressEvent(QMouseEvent* me);
  /**  */
  void contentsMouseReleaseEvent(QMouseEvent* me);
  /**  */
  void contentsMouseMoveEvent(QMouseEvent* me);
  /** No descriptions */
  void resizeEvent(QResizeEvent* e);
signals: // Signals
  /**  */
  void signalUserSetSize();
  /**  */
  void signalTlxPercent(double val);
  /**  */
  void signalTlyPercent(double val);
  /**  */
  void signalBrxPercent(double val);
  /**  */
  void signalBryPercent(double val);
  /**  */
  void signalPreviewSize(QRect size);
  /**  */
  void signalNewActiveRect(int num);
};

#endif
