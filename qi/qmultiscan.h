/***************************************************************************
                          qmultiscan.h  -  description
                             -------------------
    begin                : Sun Jan 14 2001
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

#ifndef QMULTISCAN_H
#define QMULTISCAN_H

#include <qwidget.h>

class QCheckBox;
class QCopyPrint;
class QFileListWidget;
class QGridLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QSpinBox;
class QToolButton;
class QVBoxLayout;
/**
  *@author Michael Herder
  */

class QMultiScan : public QWidget
{
Q_OBJECT
public: 
	QMultiScan(QWidget *parent=0, const char *name=0,WFlags f=0);
	~QMultiScan();
  /**  */
  void createContents();
  /**  */
  bool mustSave();
  /**  */
  bool mustPrint();
  /**  */
  bool mustOCR();
  /**  */
  bool mustConfirm();
  /**  */
  bool saveText(QString text);
  /**  */
  bool saveImage(QImage* image);
  /**  */
  bool printImage(QImage* image);
  /**  */
  void printImage();
  /**  */
  void setImageValues(int w,int h,int xres,int yres);
  /** */
  int scanNumber();
  /**  */
  bool ownResolution();
  /**  */
  bool adfMode();
  /** No descriptions */
  QString lastImageFilename();
  /** No descriptions */
  QString lastTextFilename();
  /** No descriptions */
  QString lastErrorString();
private: // Private methods
  /**  */
  void initWidget();
  /**  */
  void loadSettings();
  /**  */
  void createWhatsThisHelp();
private:
  QGridLayout* mpMainGrid;
  QGroupBox* mpMultiBox;
  QVBoxLayout* mpMultiGrid;
  QSpinBox* mpNumberSpin;
  QCheckBox* mpConfirmCheckBox;
  QCheckBox* mpAdfCheckBox;
  QCheckBox* mpImageCheckBox;
  QCheckBox* mpPrintCheckBox;
  QCheckBox* mpAutoCheckBox;
  QPushButton* mpPrinterButton;
  QCheckBox* mpTextCheckBox;
  QCopyPrint* mpCopyPrint;
  QFileListWidget* mpFileList;
  QFileListWidget* mpTextList;
  QPushButton* mpCloseButton;
  QPushButton* mpScanButton;
  QToolButton* mpFileGenerationButton;
  QLabel* mpPrintToFileLabel;
  QLineEdit* mpPrintToFileLineEdit;
  QString mLastImageFilename;
  QString mLastTextFilename;
  int mWidth;
  int mHeight;
  int mXRes;
  int mYRes;
  /**  */
  QCheckBox* mpResolutionCheckBox;
  /**  */
  int mOwnXRes;
  /**  */
  int mOwnYRes;
  /**  */
  QString mErrorString;
protected:
  void showEvent(QShowEvent* e);
  /** No descriptions */
  void resizeEvent(QResizeEvent* e);
public slots:
  /**  */
  void slotCheckConfirm(bool b);
private slots: // Private slots
  /**  */
  void slotMustResize();
  /**  */
  void slotCheckOCR(bool b);
  /**  */
  void slotCheckPrintImage(bool b);
  /**  */
  void slotCheckSaveImage(bool b);
  /**  */
  void slotSetupPrinter();
  /**  */
  void slotScanNumber(int value);
  /**  */
  /**  */
  void slotADFMode(bool b);
  /** No descriptions */
  void slotCheckResolution(bool b);
  /** No descriptions */
  void slotPrintFilenameChanged(const QString & print_filename);
  /**  */
  void slotFilenameGenerationSettings();
signals:
  /** */
  void signalStartScan();
  /**  */
  void signalImageSaved(QString abspath);
};

#endif
