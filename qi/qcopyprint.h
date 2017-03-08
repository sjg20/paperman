/***************************************************************************
                          qcopyprint.h  -  description
                             -------------------
    begin                : Mon Dec 11 2000
    copyright            : (C) 2000 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2        *
 *   as published by the Free Software Foundation.                         *
 *                                                                         *
 ***************************************************************************/
#ifndef QCOPYPRINT_H
#define QCOPYPRINT_H

#include "resource.h"

#include <qbuttongroup.h>
#include <qdialog.h>
#include <qimage.h>

#ifdef KDEAPP
#include <kprinter.h>
#else
#include <qprinter.h>
#endif

#include <qpixmap.h>
#include <qvariant.h>

class QVBoxLayout; 
class QHBoxLayout; 
class QButtonGroup;
class QGridLayout;
class QCheckBox;
class QDoubleSpinBox;
class QFrame;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QSpinBox;
class Ruler;


#ifdef KDEAPP
class ExtKPrinter : public KPrinter
{
public:
    ExtKPrinter() : KPrinter() {}
    void doPrepare() { preparePrinting(); }
    void doFinish() { finishPrinting(); }
};
#endif

class QCopyPrint : public QDialog
{ 
Q_OBJECT

public:
  enum ScaleMode
	{
    ScaleMode_Free       = 0,
    ScaleMode_MaxSize    = 1,
    ScaleMode_MarginSize = 2
  };
    QCopyPrint(QWidget* parent=0,const char* name=0,bool modal=false,WFlags f=0);
    ~QCopyPrint();
  /** Loads an image for printing. Returns true, if this
was successfull, or false, if an error occured. */
  bool loadImage(QString path,bool b);
  /**  */
  void setResolutionY(int yres);
  /**  */
  void setResolutionX(int xres);
  /**  */
  void setImage(QImage* image,bool b=false);
  /**  */
  void setUnknownImage(int w,int h,int xres,int yres);
  /**  */
  bool printed();
  /** No descriptions */
  bool isSetup();
  /** No descriptions */
  bool printToFile();
  /** No descriptions */
  void setShowPrintDialog(bool show_dlg);
  /** No descriptions */
  void setPrintFilename(QString name);
  /** No descriptions */
  QString printFilename();
protected:
  QGridLayout* mpMainGrid;
  QGridLayout* mpMarginGrid;
private: // Private methods
  QButtonGroup* mpScaleGroup;
  QPushButton* mpPrintButton;
  QPushButton* mpSaveButton;
  QGroupBox* mpScaleBox;
  QGroupBox* mpImageResBox;
  QRadioButton* mpRadioButtonFree;
  QRadioButton* mpRadioButtonMax;
  QRadioButton* mpRadioButtonMargin;
  QPushButton* mpSetupButton;
  QPushButton* mpSettingsButton;
  QGroupBox* mpMarginBox;
  QDoubleSpinBox* mpLeftSpin;
  QDoubleSpinBox* mpTopSpin;
  QDoubleSpinBox* mpRightSpin;
  QDoubleSpinBox* mpBottomSpin;
  QSpinBox* mpImageXResSpin;
  QSpinBox* mpImageYResSpin;
  QCheckBox* mpImageResCheckBox;
  QLabel* mpSizeLabel;
  QLabel* mpTextLabel1;
  QLabel* mpTextLabel2;
  QLabel* mpTextLabel3;
  QLabel* mpTextLabel4;
  QWidget* mpPreviewWidget;
  QWidget* mpPreviewFrame;
  Ruler* mpVRuler;
  Ruler* mpHRuler;
  QDoubleSpinBox* mpScaleSpin;
#ifdef KDEAPP
  /**  */
  ExtKPrinter mPrinter;
#else
  /**  */
  QPrinter mPrinter;
#endif
  /**  */
  double mPrinterRes;
  /**  */
  int mStartHeight;
  /**  */
  int mHeightMM;
  /**  */
  int mWidthMM;
  /**  */
  int mHeight;
  /**  */
  int mWidth;
  /**  */
  QPixmap* mpPixmap;
  /**  */
  QImage* mpImage;
  /**  */
  ScaleMode mScaleMode;
  /**  */
  QCheckBox* mpProportionCheckBox;
  /**  */
  double mFreescaleFactor;
  /**  */
  QLabel* mpMetricLabel;
  /**  */
  double mMetricFactor;
  /**  */
  bool mShowPrintDialog;
  /**  */
  bool mPrinted;
  /**  */
  bool mIsSetup;
  /**  */
  QPushButton* mpCloseButton;
  /**  */
  QString mImagePath;
private:
  /**  */
  void setDoubleSpinRange();
  /**  */
  void initWidget();
  /**  */
  void drawMargins();
  /**  */
  void resizePreviewPixmap();
  /**  */
  bool fileCopy(QString source, QString dest);
  /**  */
  void initSettings();
  /**  */
  double leftMarginMM();
  /**  */
  double topMarginMM();
  /**  */
  double rightMarginMM();
  /**  */
  double bottomMarginMM();
  /**  */
  QString infoString();
  /**  */
  void createWhatsThisHelp();
  /**  */
  void setPreviewSize();
  /** No descriptions */
  void updateDeviceMetrics(bool full_page=false);
  /**  */
  bool setupPrinter();
protected:
  /**  */
  virtual void closeEvent(QCloseEvent* e);
public slots:
  /**  */
  void slotPrint();
private slots: // Private slots
  /**  */
  void slotSaveImage();
  /**  */
  void slotSetupPrinter();
  /**  */
  void slotLeftSpinValue(int);
  /**  */
  void slotRightSpinValue(int);
  /**  */
  void slotTopSpinValue(int);
  /**  */
  void slotBottomSpinValue(int);
  /**  */
  void slotBindImageRes(bool b);
  /**  */
  void slotImageYRes(int);
  /**  */
  void slotImageXRes(int);
  /**  */
  void slotScaling(int i);
  /**  */
  void slotProportion(bool);
  /**  */
  void slotFreeScale(int value);
  /**  */
  void slotSaveSettings();
protected: // Protected methods
  /**  */
  virtual void paintEvent(QPaintEvent*);
  /**  */
  virtual void showEvent(QShowEvent* e);
signals: // Signals
  /**  */
  void signalResized();
  /**  */
  void signalImageSaved(QString abspath);
};

#endif // QCOPYPRINT_H
