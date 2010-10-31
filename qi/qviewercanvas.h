/***************************************************************************
                          qviewercanvas.h  -  description
                             -------------------
    begin                : Sun Aug 12 2001
    copyright            : (C) 2001 by M. Herder
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

#ifndef QVIEWERCANVAS_H
#define QVIEWERCANVAS_H

#include <qcanvas.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qrect.h>
#include <qstring.h>

/**
  *@author M. Herder
  */
class CanvasRubberRectangle;

class QViewerCanvas : public QCanvasView
{
Q_OBJECT
public: 
	QViewerCanvas(QWidget* parent=0,const char* name=0,WFlags f=0);
	~QViewerCanvas();
  /**  */
  void setImage(QImage* image);
  /**  */
  QImage* currentImage();
  /**  */
  void scale(double scalefactor);
  /** */
  void updatePixmap();
  /**  */
  void setMode(int mode);
  /**  */
  void setRectPercent(double left, double top,double right,double bottom);
  /**  */
  QRect selectedRect();
  /**  */
  double tlxPercent();
  /**  */
  double tlyPercent();
  /**  */
  double brxPercent();
  /**  */
  double bryPercent();
  /**  */
  double widthPercent();
  /**  */
  double heightPercent();
private: // Private methods
  /**  */
  void initWidget();
  /**  */
  void calcPercentValues();
private:
  /**The QCanvas we view with this QCanvasView */
  QCanvas* mpCanvas;
  /** */
  QPixmap mCanvasPixmap;
  /** */
  CanvasRubberRectangle* mpCanvasRect;
  /** */
  QCanvasLine* mpCanvasLine1;
  /** */
  QCanvasLine* mpCanvasLine2;
  /** */
  QCanvasLine* mpCanvasLine3;
  /** */
  QCanvasLine* mpCanvasLine4;
  /** */
  int mLineOffset;
  /** */
  double mScaleFactor;
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
  double mXPercent;
  /** */
  double mYPercent;
  /** */
  double mWidthPercent;
  /** */
  double mHeightPercent;
protected: // Protected methods
  /**  */
  void contentsMousePressEvent(QMouseEvent* me);
  /**  */
  void contentsMouseReleaseEvent(QMouseEvent*);
  /**  */
  void contentsMouseMoveEvent(QMouseEvent* me);
  /** No descriptions */
  void dragMoveEvent(QDragMoveEvent* e);;
  /** No descriptions */
  void dropEvent(QDropEvent* e);
  /** No descriptions */
  void resizeEvent(QResizeEvent* e);
signals: // Signals
  /**  */
  void signalDragStarted();
  /**  */
  void signalRectangleChanged();
  /** No descriptions */
  void signalLocalImageUriDropped(QStringList urilist);
};

#endif
