/***************************************************************************
                          qscanareawidget.h  -  description
                             -------------------
    begin                : Sun Jul 2 2000
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

#ifndef QSCANAREAWIDGET_H
#define QSCANAREAWIDGET_H

#include <qwidget.h>
#include <qpixmap.h>
#include <qimage.h>
/**
  *@author M. Herder
  */
//forward declarations
class QPainter;
class QColor;
class QPoint;
class QTimer;

class QScanAreaWidget : public QWidget
{
   Q_OBJECT
public: 
	QScanAreaWidget(QWidget *parent=0, const char *name=0);
	~QScanAreaWidget();
  QRect getSizeRect();
  /**  */
  void setPixmap(QString path);
  void startTimer();
  /**  */
  void scalePreviewPixmap();
  void drawScanRect();
  void resizeRect(double leftpercent,double toppercent,double rightpercent,double bottompercent);
  /**  */
  void setTlx(double pval);
  /**  */
  void setTly(double pval);
  /**  */
  void setBrx(double pval);
  /**  */
  void setBry(double pval);
protected: // Protected methods
  /**  */
  virtual void mouseMoveEvent(QMouseEvent *qme);
  /**  */
  virtual void paintEvent(QPaintEvent*);
  /**  */
  virtual void mouseReleaseEvent(QMouseEvent *e);
  /**  */
  virtual void mousePressEvent(QMouseEvent *e);
  /**  */
  void resizeEvent(QResizeEvent* e);
private: // Private attributes
  /** */
  double mTlxPercent;
  /** */
  double mTlyPercent;
  /** */
  double mBrxPercent;
  /** */
  double mBryPercent;
  /** */
  bool mTlxPercentChanged;
  /** */
  bool mTlyPercentChanged;
  /** */
  bool mBrxPercentChanged;
  /** */
  bool mBryPercentChanged;
  /**  */
  QPoint mStartPoint;
  /**  */
  bool mLmbFlag;
  /**  */
  QRect* mpRect;
  /**  */
  int mCursorState;
  /**  */
  QPixmap mPixmap;
  /**  */
  QImage mImage;
  /**  */
  bool mSizeChanged;
  /**  */
  QRect mOldRect;
  /**  */
  int mLineOffset;
//	/**  */
	QTimer* mTimer;
public slots: // Public slots
  /**  */
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
private slots: // Private slots
  /**  */
  void slotTimerEvent();
};

#endif
