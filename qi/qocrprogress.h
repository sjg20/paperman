/***************************************************************************
                          qocrprogress.h  -  description
                             -------------------
    begin                : Tue Jan 16 2001
    copyright            : (C) 2001 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#ifndef QOCRPROGRESS_H
#define QOCRPROGRESS_H

#include "resource.h"
#include <qdialog.h>
#include <qimage.h>
#include <qstring.h>

/**
  *@author Michael Herder
  */
class QUnknownProgressWidget;
class QPushButton;
class QLabel;
class QGridLayout;
#ifndef USE_QT3
class QProcessBackport;
#else
class QProcess;
#endif

class QOCRProgress : public QDialog
{
Q_OBJECT
public: 
	QOCRProgress(QWidget* parent=0,const char* name = 0,bool modal = false);
	~QOCRProgress();
  /**  */
  QString ocrText();
  /** No descriptions */
  void setImagePath(QString path);
  /** No descriptions */
  void setImage(QImage& image);
private: // Private methods
  /**  */
  QImage mImage;
  /**  */
  QString mOcrText;
  /**  */
  QString mImagePath;
  /**  */
  QString mOcrImagePath;
  /**  */
#ifndef USE_QT3
  QProcessBackport* mpOcrProcess;
#else
  QProcess* mpOcrProcess;
#endif
  /**  */
  QGridLayout* mpMainLayout;
  /**  */
  QLabel* mpInfoLabel;
  /**  */
  QPushButton* mpCancelButton;
  /** */
  QUnknownProgressWidget* mpUnknownProgress;
private: //methods
  /**  */
  void initDlg();
  /**  */
  void startOCR();
private slots:
  /**  */
  void slotStopOCR2();
  /**  */
  void slotStopOCR();
  /**  */
  void slotReceivedStdout();
  /** */
  void slotReceivedStderr();
protected: // Protected methods
  /**  */
  virtual void showEvent(QShowEvent * e);
};

#endif
