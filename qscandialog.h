#include <QLabel>
#include <QGridLayout>
#include <QResizeEvent>
#include <QShowEvent>
#include <QCloseEvent>
/***************************************************************************
                          qscandialog.h  -  description
                             -------------------
    begin                : Thu Jun 22 2000
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

#ifndef QScanDialog_H
#define QScanDialog_H

class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QPushButton;
class QTableWidget;

//s #include <qarray.h>
#include <qnamespace.h>
#include <qwidget.h>
#include "qscanner.h"
//s #include "qextensionwidget.h"
#include "quiteinsanenamespace.h"
/**
  *@author M. Herder
  */
//forward declarations
class ImageHistoryBrowser;
class PreviewWidget;
class QCheckBox;
class QComboBox;
class QDragLabel;
class QFileListWidget;
class QGridLayout;
class QGroupBox;
class QHBoxLayout;
class QHTMLView;
class QLabel;
class QLineEdit;
class QMultiScan;
class QOptionScrollView;
class QReadOnlyOption;
class QRect;
class QSaneOption;
class QScanner;
class QScrollBarOption;
class QTimer;
class QTabWidget;
class QToolButton;
class QWidget;
class SaneWidgetHolder;
class ScanArea;


class QScanRange
{
public:
  QScanRange () { mXmin = mYmin = mXmax = mYmax = -1; }
  void getRange (QScrollBarOption *mpTlxOption, QScrollBarOption *mpTlyOption, QScrollBarOption *mpBrxOption, QScrollBarOption *mpBryOption);
  void unfix ();
  double Xmin () { return mXmin; }
  double Ymin () { return mYmin; }
  double Xmax () { return mXmax; }
  double Ymax () { return mYmax; }

private:
   double mXmin, mYmin;
   double mXmax, mYmax;
};


class QScanDialog : public QWidget
{
   Q_OBJECT
public:
	QScanDialog(QScanner* s,QWidget *parent=0, const char *name=0,Qt::WFlags f=0);
  ~QScanDialog();
  /**  */
  QIN::Status status();

  PreviewWidget *getPreview () { return mpPreviewWidget; }

  /** get the current selected format */
  QScanner::format_t getFormat ();

  void setFormat (QScanner::format_t f, bool select_compression);

  /** set adf */
  bool setAdf (bool adf);

  /** set duplex */
  bool setDuplex (bool duplex);

  /** set DPI */
  void setDpi (int dpi);

  /** set the 'exposure'. This is the threshold for monochrome scans */
  void setExposure (int exposure);

  /** set the brightness */
  void setBrightness (int value);

  /** set the contrast */
  void setContrast (int value);

protected:
  /**  */
  virtual void resizeEvent(QResizeEvent* e);
  /**  */
  virtual void closeEvent(QCloseEvent* e);
  /**  */
  void showEvent(QShowEvent* se);

private:
  QReadOnlyOption* mpButtonOption;
  QPushButton* mpBrowserButton;
  QPushButton* mpDeviceButton;
  QPushButton* mpQuitButton;
  QPushButton* mpAboutButton;
  QPushButton* mpScanButton;
  QPushButton* mpOptionsButton;
  QPushButton* mpPreviewButton;
  PreviewWidget *mpPreviewWidget;
  ImageHistoryBrowser* mpHistoryWidget;
  /** the widgets in the dialogue - these are indexed sequentially, but they don't line up with the
      sane option numbers */
  QVector <SaneWidgetHolder *> mOptionWidgets;
  /**  */
  QScanner* mpScanner;
  /**  */
  QScrollBarOption* mpTlxOption;
  /**  */
  QScrollBarOption* mpTlyOption;
  /**  */
  QScrollBarOption* mpBrxOption;
  /**  */
  QScrollBarOption* mpBryOption;

  /** option for page width (ADF) */
  QScrollBarOption* mpWidthOption;

  /** option for page height (ADF) */
  QScrollBarOption* mpHeightOption;

  /**  */
  QMultiScan* mpMultiScanWidget;
  /**  */
  QPushButton* mpMultiScanButton;
  /**  */
  QTabWidget* mpOptionTabWidget;
  /**  */
  QOptionScrollView* mpOptionScrollView;
  /** */
  QWidget* mpOptionMainWidget;
  /** */
  QVector <QWidget*> mOptionSubArray;
  /**  */
  QGridLayout* mpMainLayout;
  /**  */
  QVector <QGroupBox*> mGroupBoxArray;
  /**  */
  QLabel* mpLabelImageInfo;
  /**  */
  QPushButton* mpViewerButton;
  /**  */
  QPushButton* mpHelpButton;
  QString mDeviceName;
  /**  */
  QHTMLView* mpHelpViewer;
  /**  */
  QIN::Status mStatus;
  /**  */
  QIN::Layout mLayout;
  /** */
  QIN::MetricSystem mMetricSystem;
  /** */
  QComboBox* mpModeCombo;
  /**  */
  int mShowCnt;
  /**  */
  QStackedWidget* mpOptionWidgetStack;
  /**  */
  QDragLabel* mpDragLabel;
  /**  */
  QLineEdit* mpDragLineEdit;
  /**  */
  QComboBox* mpDragTypeCombo;
  /**  */
  QCheckBox* mpAutoNameCheckBox;
  /**  */
  QHBoxLayout* mpInfoHBox;
  /**  */
  QHBoxLayout* mpModeHBox;
  /**  */
  QHBoxLayout* mpButtonHBox1;
  /**  */
  QHBoxLayout* mpButtonHBox2;
  /**  */
  QHBoxLayout* mpDragHBox1;
  /**  */
  QHBoxLayout* mpDragHBox2;
  /**  */
  QWidget* mpOptionListWidget;
  /**  */
  QTableWidget* mpOptionListView;
  /**  */
  QVBoxLayout* mpSeparator;

  /** ignore an inexact signal (used to avoid recusion) */
  int mIgnoreInexact;

  /** Sane options for common options */
  QSaneOption *mOptionSource;   // option number of 'source'
  QSaneOption *mOptionDuplex;   // option number of 'duplex'
  QSaneOption *mOptionXRes;     // option number of DPI x
  QSaneOption *mOptionYRes;     // option number of DPI y
  QSaneOption *mOptionFormat;   // option number of format
  QSaneOption *mOptionCompression; // option number of compression (for JPEG)
  QSaneOption *mOptionThreshold;
  QSaneOption *mOptionBrightness;
  QSaneOption *mOptionContrast;

private: //methods
  bool mMultiSelectionMode;
	QIN::Status initDialog();
  /**  */
  void changeLayout(QIN::Layout l);
  /** Do a scan with the current settings. Return false
if an error occurs. Otherwise true is returned and the
image is saved to .scantemp.pnm.*/
  bool scanImage(bool pre = false,bool adf_warning=true,QWidget* parent=0);
  /**  */
  bool scanPreviewImage(double tlx=0.0,double tly=0.0,
                        double brx=1.0,double bry=1.0,int res=50);
  /**  */
  void enableGUI(bool enable,bool preview_scan=false);
  /**  */
  void createOptionWidget();
  /**  */
  void setAllOptions();
  /**  */
  QGroupBox* createOptionGroupBox(QString title,int firstoption,int lastoption);
  /**  */
  void createPreviewWidget();
  /** No descriptions */
  void resizeScanArea();
  /** No descriptions */
  void setPreviewRange();
  /** No descriptions */
  void scanInTemporaryMode();
  /** No descriptions */
  void scanInSingleFileMode();
  /** No descriptions */
  void scanInCopyPrintMode();
  /** No descriptions */
  void scanInMultiScanMode();
  /** No descriptions */
  void scanInOcrMode();
  /** No descriptions */
  void scanInSaveMode();
  /** No descriptions */
  QSaneOption* createSaneOptionWidget(QWidget* parent,int opt_num);
  /** No descriptions */
  void checkOptionValidity(int opt_num);

  /** set the page size (used for ADF) */
  void setPageSize (double dwidth, double dheight);

  /** find a widget based on name and return a pointer to it

   \param name  name of option
   \param option_type   SANE_Value_Type to expect, else -1 for any
   \returns pointer to QSaneOption, or 0 if none */
  QSaneOption *findOption (QString name, int option_type = -1);

  /**
   * set the value of an option as a string
   *
   * \param opt     Option to set
   * \param str     Value for that option
   * \return true if ok, false if option not found
   */
  bool setOption (QSaneOption *opt, QString str);

  /** set the value of an option as an integer */
  void setOption (QSaneOption *opt, int value);

  // set a single option
  void set256 (QSaneOption *opt, int value);

public slots:
  /**  */
  void slotUserSize(int);
  /**  */
  void slotOptionChanged(int num);
  /**  */
  void slotPreviewSize(QRect rect);
  /**  */
  void slotResizeScanRect();
  /**  */
  void slotSetPredefinedSize(ScanArea* sca);

  /** do any pending changes */
  void slotDoPendingChanges (void);

public slots:
  /**  */
  void slotShowOptionsWidget();

private slots:
  /**  */
  void slotScan();
  /**  */
//   void slotPreview(double tlx,double tly,double brx,double bry,int res);
  /**  */
  void slotReloadOptions();
  /**  */
  void slotShowPreviewWidget();
  /**  */
  void slotAbout();
  /**  */
  void slotShowMultiScanWidget();
  /**  */
  void slotImageInfo();
  /** This slot is called, when setting an option results
in an return value SANE_INFO_INEXACT.
This normally happens, when the value set in
the backend  is different from the requested value. */
  void slotInfoInexact(int num);
  /**  */
  void slotViewer();
  /**  */
  void slotChangeMode(int index);
  /**  */
  void slotAutoMode(int num,bool b);
  /**  */
  void slotShowHelp();
  /**  */
  void slotDeviceSettings();
  /**  */
  void slotRaiseOptionWidget(QListWidgetItem* lvi);
  /**  */
  void slotHidePreview();
  /**  */
  void slotImageSettings();
  /**  */
  void slotChangeFilename();
  /**  */
  void slotDragType(int index);
  /**  */
  void slotAutoName(bool b);
  /**  */
  void slotDragFilename(const QString& qs);
  /**  */
  void slotShowBrowser();
  /**  */
  void slotShowImage(QString filename);
  /**  */
  void slotAddImageToHistory(QString abspath);
  /** No descriptions */
  void slotMultiSelectionMode(bool state);
  /** No descriptions */
  void slotEnableScanAreaOptions(bool on);
  /** No descriptions */
  void slotFilenameGenerationSettings();
signals:
  /** */
  void signalChangeStyle(int s);
  /** */
  void signalQuit();
  /**  */
  void signalNewImage();
  /**  */
  void signalMetricSystem(QIN::MetricSystem ms);
  void closed ();

  void warning (QString &);
  /** emitted when pending dialogue changes have been completed */
  void signalPendingDone();

private:

};

#endif

