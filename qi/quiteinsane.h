/***************************************************************************
                          quiteinsane.h  -  description
                             -------------------
    begin                : Don Jul 13 20:27:22 CEST 2000
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

#ifndef QUITEINSANE_H
#define QUITEINSANE_H

#include "resource.h"

#include "imagebuffer.h"

//s #include <qcstring.h>

#ifdef KDEAPP
#include <kmainwindow.h>
#else
#include <qmainwindow.h>
#endif

#include <qnamespace.h>
#include <qpixmap.h>
#include <qstring.h>
#include <qtimer.h>
#include <QIconSet>

#include <qvector.h>
///////////////////////////////////////////////////////////////////
// File-menu entries
#define ID_FILE_LOAD_TEXT                     10010
#define ID_FILE_LOAD_IMAGE                    10020
#define ID_FILE_LOAD_LAST_SCAN                10030
#define ID_FILE_SAVE_IMAGE                    10040
#define ID_FILE_SAVE_IMAGE_AS                 10050
#define ID_FILE_SAVE_TEXT                     10060
#define ID_FILE_SAVE_TEXT_AS                  10070
#define ID_FILE_QUIT                          10080
#define ID_FILE_PRINT                         10090
#define ID_FILE_PRINT_TEXT                    10100
#define ID_FILE_IMAGE_HISTORY                 10120
#define ID_FILE_TEXT_HISTORY                  10130
#define ID_FILE_SAVE_SELECTION_AS             10140
#define ID_FILE_PRINT_SELECTION               10150
///////////////////////////////////////////////////////////////////
// OCR-menu entries
#define ID_OCR_MODE                           11010
#define ID_OCR_START                          11020
#define ID_OCR_STOP                           11030
#define ID_OCR_START_SELECTION                11040
///////////////////////////////////////////////////////////////////
// View-menu entries
#define ID_VIEW_TOOLBAR                       12010
#define ID_VIEW_STATUSBAR                     12020
///////////////////////////////////////////////////////////////////
// Edit-menu entries
#define ID_EDIT_COPY_IMAGE                    13010
#define ID_EDIT_COPY_TEXT                     13020
#define ID_EDIT_CUT_TEXT                      13030
#define ID_EDIT_PASTE_TEXT                    13040
#define ID_EDIT_UNDO                          13050
#define ID_EDIT_REDO                          13060
#define ID_EDIT_COPY_IMAGE_SELECTION          13070
#define ID_EDIT_REPLACE_IMAGE_WITH_SELECTION  13080
#define ID_EDIT_SELECT_ALL_IMAGE              13090
#define ID_EDIT_MOVE_IMAGE_SELECTION          13100
#define ID_EDIT_NEW_IMAGE_SELECTION           13110
#define ID_EDIT_DRAG_IMAGE                    13120
#define ID_EDIT_UNDO_TEXT                     13130
#define ID_EDIT_REDO_TEXT                     13140
///////////////////////////////////////////////////////////////////
// Settings-menu entries
#define ID_SETTINGS_TEXTEDIT                  14010
#define ID_SETTINGS_VIEWER                    14020

///////////////////////////////////////////////////////////////////
// Help-menu entries
#define ID_HELP_ABOUT                         1002
#define ID_HELP_ABOUT_QT                      1003
//forward declarations
class TemporaryFile;
class QAccel;
class QButtonGroup;
class QCloseEvent;
class QComboBox;
class QFileDialog;
class QHBox;
//class QIconSet;
class QImage;
class QMenuBar;
class QMessageBox;
class QMultiLineEditPE;
class QPainter;
class QPopupMenu;
#ifdef USE_QT3
class QProcess;
#else
class QProcessBackport;
#endif
class QProgressBar;
class QScanDialog;
class QScannerSetupDlg;
class QSplitter;
class QStatusBar;
class QToolBar;
class QToolButton;
class QUnknownProgressWidget;
class QViewerCanvas;
class QWhatsThis;
class QWidgetStack;
/**
  */
class QuiteInsane : public QMainWindow
{
 Q_OBJECT
 public:
  enum Mode
  {
    Mode_ImageOcr  = 1,
    Mode_TextOnly  = 2
  };
  /**  */
  QuiteInsane(Mode m = Mode_ImageOcr,QWidget * parent = 0, const char * name = 0,
                Qt::WFlags f = Qt::WType_TopLevel|Qt::WDestructiveClose);
  /**  */
  ~QuiteInsane();
  /**  */
  void loadImage(QString path);
  /**  */
  void loadText(QString qs);
  /**  */
  void setImageModified(bool flag);
  /**  */
  void setTextModified(bool flag);
  /**  */
  bool imageModified();
  /**  */
  bool textModified();
  /** No descriptions */
  void setImage(QImage* image);
  /**  */
  bool statusOk();
public slots:
  /**  */
  void show();
  /** switch argument for Statusbar help entries on slot selection */
  void statusCallback(int id_);
  /** save image under a different filename*/
  void slotFileSaveImageAs();
  /** save text under a different filename*/
  void slotFileSaveTextAs();
  /** save text under the current filename*/
  void slotFileSaveText();
  /** exits the application */
  void slotFileQuit();
  /** toggle the toolbar*/
  void slotViewToolBar();
  /** toggle the statusbar*/
  void slotViewStatusBar();
  /** shows an about dlg*/
  void slotHelpAbout();
  /** change the status message of the whole statusbar temporary */
  void slotStatusHelpMsg(const QString &text);
  /**  */
  void slotStopOcr();
private:
  int mUndoSteps;
  bool mImageLoaded;
  bool mCopyAvailable;
  bool mMoveSelection;
  TemporaryFile* mpTempImageFile;
  TemporaryFile* mpTempOcrFile;
  bool mTempFileValid;
  bool mFilterRunning;
  QVector <ImageBuffer> mImageVector;
  int mImageVectorIndex;
  int mSaveIndex;
  Mode mMode;
  QString mOcrText;
  QString mOriginalTextFilepath;
  QString mOriginalImageFilepath;
#ifndef USE_QT3
  QProcessBackport* mpOcrProcess;
#else
  QProcess* mpOcrProcess;
#endif
  QHBox* mpFilterHBox;
  QViewerCanvas* mpView;
  QUnknownProgressWidget* mpUnknownProgress;
  QProgressBar* mpFilterProgress;
  QWidgetStack* mpProgressStack;
  QToolButton* mpFilterButton;
  QPopupMenu* mpFileMenu;
  QPopupMenu* mpZoomMenu;
  QPopupMenu* mpOcrMenu;
  QPopupMenu* mpEditMenu;
  QPopupMenu* mpViewMenu;
  QPopupMenu* mpSettingsMenu;
  QPopupMenu* mpHelpMenu;
  QPopupMenu* mpImageMenu;
  QToolBar* mpImageToolbar;
  QToolBar* mpTextToolbar;
  QToolBar* mpToolsToolbar;
  QButtonGroup* mpToolButtonGroup;
  QToolButton* mpFileLoadImage;
  QToolButton* mpFileLoadText;
  QToolButton* mpFileSaveImage;
  QToolButton* mpFilePrintImageSelection;
  QToolButton* mpFileSaveImageSelection;
  QToolButton* mpFileSaveText;
  QToolButton* mpFileUndoImage;
  QToolButton* mpFileRedoImage;
  QToolButton* mpFileOcrStart;
  QToolButton* mpFileOcrStartSelection;
  QToolButton* mpFileOcr;
  QToolButton* mpFilePrint;
  QToolButton* mpFilePrintText;
  QToolButton* mpFileOcrStop;
  QToolButton* mpFileUndoText;
  QToolButton* mpFileRedoText;
  QToolButton* mpToolsMoveSelection;
  QToolButton* mpToolsNewSelection;
  QToolButton* mpToolsSelectAll;

  QComboBox* mpFileZoomCombo;
  QSplitter* mpSplitter;
  /**Indicates whether the image has been modified by the user, e.g
     normalized. Especially important for OCR.*/
  bool mImageUserModified;;
  /**  */
  bool mImageModified;;
  /**  */
  QMultiLineEditPE* mpEditOcr;
  /**  */
  int mXRes;
  /**  */
  int mYRes;
  /**  */
  QString mImagePath;
  QIconSet* mpOpenImageIcon;
  QIconSet* mpOpenTextIcon;
  QIconSet* mpSaveImageIcon;
  QIconSet* mpSaveTextIcon;
  QIconSet* mpOcrIconOn;
  QIconSet* mpOcrIconStop;
  QIconSet* mpPrintTextIcon;
  QIconSet* mpPrintImageIcon;
  QIconSet* mpOcrIcon;
  QIconSet* mpOcrStartIcon;
  QIconSet* mpUndoImageIcon;
  QIconSet* mpRedoImageIcon;
  QIconSet* mpUndoTextIcon;
  QIconSet* mpRedoTextIcon;
  QIconSet* mpMoveSelectionIcon;
  QIconSet* mpNewSelectionIcon;
  QIconSet* mpSelectAllIcon;
  QIconSet* mpPrintSelectionIcon;
  QIconSet* mpSaveSelectionIcon;
  QIconSet* mpOcrSelectionIcon;
  //config
  int mViewerSizeX;
  int mViewerSizeY;
  int mViewerPosX;
  int mViewerPosY;
  bool mViewerOcrMode;
  int mSingleFileViewMode;
  QString mViewerSaveTextPath;
  QString mViewerLoadImagePath;
  Q3ValueList<int> mSplitterSize;
  Qt::ToolBarDock mToolDock;
  int mToolIndex;
  int mToolExtraOffset;
  bool mToolNl;
  Qt::ToolBarDock mToolDockOcrOff;
  int mToolIndexOcrOff;
  int mToolExtraOffsetOcrOff;
  bool mToolNlOcrOff;
  Qt::ToolBarDock mTextDock;
  int mTextIndex;
  int mTextExtraOffset;
  bool mTextNl;
  Qt::ToolBarDock mImageDock;
  int mImageIndex;
  int mImageExtraOffset;
  bool mImageNl;
  Qt::ToolBarDock mImageDockOcrOff;
  int mImageIndexOcrOff;
  int mImageExtraOffsetOcrOff;
  bool mImageNlOcrOff;

private://methods
  /**  */
  void createIcons();
  /**  */
  void adjustToolbar();
  /**  */
  void initMenuBar();
  /**  */
  bool redoAvailable();
  /**  */
  bool undoAvailable();
  /**  */
  void addImageToQueue(QImage* image);
  /**  */
  void initToolBar();
  /**  */
  void initStatusBar();
  /**  */
  void initView();
  /**  */
  void clearImageQueue();
  /**  */
  void startOcr(QString imagepath);
  /**  */
  void setImageViewerState(bool image_loaded);
  /**  */
  void loadConfig();
  /**  */
  void deleteIcons();
  /** No descriptions */
  void updateZoomMenu();
  /** No descriptions */
  void restoreZoomFactor();
  /** No descriptions */
  void set100PercentZoom();
  /** No descriptions */
  void enableGui(bool state);
protected: // Protected methods
  /**  */
  virtual void closeEvent(QCloseEvent* e);
  /** */
  void moveEvent(QMoveEvent* e);
  /**  */
  void resizeEvent(QResizeEvent* e);
private slots: // Private slots
  /**  */
  void slotSaveConfig();
  /**  */
  void slotStartOcr();
  /**  */
  void slotStopOcr2();
  /**  */
  void slotEnableOcr(int i);
  /**  */
  void slotReceivedStdout();
  /**  */
  void slotReceivedStderr();
  /**  */
  void slotPrint();
  /**  */
  void slotFileSaveImage();
  /**  */
  void slotZoomMenu(int id);
  /**  */
  void slotZoomCombo(const QString&);
  /**  */
  void slotCopyImage();
  /**  */
  void slotHelpAboutQt();
  /**  */
  void slotFileLoadImage();
  /**  */
  void slotFileLoadText();
  /**  */
  void slotCopyAvailable(bool b);
  /**  */
  void slotDespeckle();
  /**  */
  void slotNormalize();
  /**  */
  void slotBlurIIR();
  /**  */
  void slotBrightnessContrast();
  /**  */
  void slotPosterize();
  /**  */
  void slotGamma();
  /**  */
  void slotOilPainting();
  /**  */
  void slotRotate();
  /**  */
  void slotScale();
  /**  */
  void slotSharpen();
  /**  */
  void slotShear();
  /**  */
  void slotTransparency();
  /**  */
  void slotFileLoadLastScan();
  /**  */
  void slotConvert();
  /**  */
  void slotInvert();
  /**  */
  void slotRedo();
  /**  */
  void slotUndo();
  /**  */
  void slotToolBarPositionChanged(QToolBar* toolbar);
  /**  */
  void slotSelectAll();
  /**  */
  void slotFileSaveSelectionAs();
  /**  */
  void slotFilePrintSelection();
  /**  */
  void slotStartOcrSelection();
  /**  */
  void slotToolMode(int id);
  /**  */
  void slotNewSelection();
  /**  */
  void slotMoveSelection();
  /**  */
  void slotCopyImageSelection();
  /**  */
  void slotAboutToShowEditMenu();
  /**  */
  void slotAboutToShowFileMenu();
  /**  */
  void slotReplaceImageWithSelection();
  /** No descriptions */
  void slotRectangleChanged();
  /** No descriptions */
  void slotLocalImageUriDropped(QStringList urilist);
signals: // Signals
  /**  */
  void signalImageSaved(QString abspath);
};
#endif

