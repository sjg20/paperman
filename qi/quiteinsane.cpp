/***************************************************************************
                          quiteinsane.cpp  -  description
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


#include "resource.h"

#ifdef KDEAPP
#include <kmenubar.h>
#else
#include <qmenubar.h>
#endif

#include "quiteinsane.h"

#include "3rdparty/kde/temporaryfile.h"

#include "pics/stop.xpm"
#include "pics/stop_filter.xpm"
#include "pics/startocr.xpm"
#include "pics/splitterwindow.xpm"
#include "pics/fileopen.xpm"
#include "pics/moveselection.xpm"
#include "pics/newselection.xpm"
#include "pics/ocrselection.xpm"
#include "pics/open_image.xpm"
#include "pics/open_text.xpm"
#include "pics/printselection.xpm"
#include "pics/print_image.xpm"
#include "pics/print_text.xpm"
#include "pics/redo.xpm"
#include "pics/redo_text.xpm"
#include "pics/save_image.xpm"
#include "pics/save_text.xpm"
#include "pics/saveselection.xpm"
#include "pics/selectall.xpm"
#include "pics/undo.xpm"
#include "pics/undo_text.xpm"
#include "imagefilters/brightnesscontrastdialog.h"
#include "imagefilters/despeckledialog.h"
#include "imagefilters/gammadialog.h"
#include "imagefilters/gaussiirblurdialog.h"
#include "imagefilters/imageconverterdialog.h"
#include "imagefilters/invertdialog.h"
#include "imagefilters/normalizedialog.h"
#include "imagefilters/oilpaintingdialog.h"
#include "imagefilters/posterizedialog.h"
#include "imagefilters/rotationdialog.h"
#include "imagefilters/scaledialog.h"
#include "imagefilters/sharpendialog.h"
#include "imagefilters/sheardialog.h"
#include "imagefilters/transparencydialog.h"

#include "imageiosupporter.h"
#include "qbuttongroup.h"
#include "qcopyprint.h"
#include "qfiledialogext.h"
#include "qmultilineeditpe.h"

#ifndef USE_QT3
#include "3rdparty/qtbackport/qprocess.h"
#else
#include <qprocess.h>
#endif

#include "qpreviewfiledialog.h"
#include "qqualitydialog.h"
#include "qunknownprogresswidget.h"
#include "qviewercanvas.h"
#include "qxmlconfig.h"

#include <math.h>

#include <qaccel.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qcolor.h>
#include <qcolordialog.h>
#include <qcombobox.h>
#include <qcursor.h>
#include <qcstring.h>
#include <qdatastream.h>
#include <qdir.h>
#include <qdragobject.h>
#include <qevent.h>
#include <qfile.h>
#include <qfont.h>
#include <qfontdialog.h>
#include <qfontmetrics.h>
#include <qhbox.h>
#include <qiconset.h>
#include <qimage.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpaintdevicemetrics.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qprinter.h>
#include <qprogressbar.h>
#include <qpopupmenu.h>
#include <qstatusbar.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qstrlist.h>
#include <qsplitter.h>
#include <qtextstream.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qwidgetstack.h>
#include <stdlib.h>
#include <string.h>

QuiteInsane::QuiteInsane(Mode m,QWidget * parent,const char * name,WFlags f)
            :QMainWindow(parent,name,f)
{
  mpView = 0;
  mMode = m;
  mFilterRunning = false;
  mImageLoaded = false;
  mCopyAvailable = false;
  mTempFileValid = false;
  QString qs;
  if(mMode == Mode_ImageOcr)
    setCaption(tr("QuiteInsane - Image viewer"));
  else
    setCaption(tr("QuiteInsane - Editor"));
  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
//  mTempImageFilename = QString::null;
//  mTempOcrFilename = QString::null;
  mOriginalTextFilepath = QString::null;
  mOriginalImageFilepath = QString::null;
  loadConfig();
  initView();
  createIcons();
  initToolBar();
  initMenuBar();
  deleteIcons();
  initStatusBar();
  mImageModified = false;
  mImageUserModified = false;
  mpTempImageFile = 0;
  mpTempOcrFile =0;
  if(mMode == Mode_ImageOcr)
  {
    mpFileOcr->setOn(mViewerOcrMode);
    slotEnableOcr(int(mViewerOcrMode));
    mpOcrProcess = 0L;
    connect(mpZoomMenu,SIGNAL(activated(int)),this,SLOT(slotZoomMenu(int)));
    connect(mpFileZoomCombo,SIGNAL(activated(const QString&)),this,
            SLOT(slotZoomCombo(const QString&)));
    mImageVector.resize(0);
    mImageVector.setAutoDelete(true);
    mImageVectorIndex = -1;
    mSaveIndex = -1;
    mpEditMenu->setItemEnabled(ID_EDIT_UNDO,undoAvailable());
    mpEditMenu->setItemEnabled(ID_EDIT_REDO,redoAvailable());
    mpFileUndoImage->setEnabled(undoAvailable());
    mpFileRedoImage->setEnabled(redoAvailable());
    setImageViewerState(false);
    adjustToolbar();
    //we create two unique temp files
    //This one's used to store temporary images
    QString qs;
    qs = xmlConfig->stringValue("TEMP_PATH");
    if(qs.right(1) != "/")
      qs+= "/";
    mpTempImageFile = new TemporaryFile(qs+"qis_tempimage");
    mpTempOcrFile = new TemporaryFile(qs+"qis_tempocr");
    mpTempImageFile->setAutoDelete(true);
    mpTempOcrFile->setAutoDelete(true);
  }
}

QuiteInsane::~QuiteInsane()
{
  mImageVector.clear();
  if(mpTempImageFile)
    delete mpTempImageFile;
  if(mpTempOcrFile)
    delete mpTempOcrFile;
}

void QuiteInsane::initMenuBar()
{
#ifdef KDEAPP
  KMenuBar* mb = new KMenuBar(this);
#else
  QMenuBar* mb = new QMenuBar(this);
#endif
  if(mMode == Mode_ImageOcr)
  {
    ///////////////////////////////////////////////////////////////////
    //Menu bar ImageOCR mode
    ///////////////////////////////////////////////////////////////////
    // menuBar entry fileMenu
    mpFileMenu=new QPopupMenu(this);
    mpFileMenu->insertItem(*mpOpenImageIcon,tr("&Open image..."),this,
                    SLOT(slotFileLoadImage()),SHIFT+CTRL+Key_O,ID_FILE_LOAD_IMAGE);

    mpFileMenu->insertItem(*mpOpenTextIcon,tr("O&pen text..."), this,
                      SLOT(slotFileLoadText()),CTRL+Key_O, ID_FILE_LOAD_TEXT);

    mpFileMenu->insertItem(tr("Open &last scan"), this,
                      SLOT(slotFileLoadLastScan()),0, ID_FILE_LOAD_LAST_SCAN);

    mpFileMenu->insertSeparator();

    mpFileMenu->insertItem(*mpSaveImageIcon,tr("Save &image"), this,
                    SLOT(slotFileSaveImage()),SHIFT+CTRL+Key_S, ID_FILE_SAVE_IMAGE);

    mpFileMenu->insertItem(tr("Save i&mage as..."), this,
                    SLOT(slotFileSaveImageAs()),0, ID_FILE_SAVE_IMAGE_AS);

    mpFileMenu->insertItem(*mpSaveSelectionIcon,tr("Save image sele&ction as..."), this,
                    SLOT(slotFileSaveSelectionAs()),0, ID_FILE_SAVE_SELECTION_AS);

    mpFileMenu->insertItem(tr("Save te&xt"), this,
                    SLOT(slotFileSaveText()),CTRL+Key_S, ID_FILE_SAVE_TEXT);

    mpFileMenu->insertItem(*mpSaveTextIcon,tr("Save &text as..."), this,
                    SLOT(slotFileSaveTextAs()),0, ID_FILE_SAVE_TEXT_AS);

    mpFileMenu->insertSeparator();

    mpFileMenu->insertItem(*mpPrintImageIcon,tr("Print im&age..."), this, SLOT(slotPrint()),
                       SHIFT+CTRL+Key_P, ID_FILE_PRINT);

    mpFileMenu->insertItem(*mpPrintSelectionIcon,tr("Print &selection..."), this,SLOT(slotFilePrintSelection()),
                       0, ID_FILE_PRINT_SELECTION);

    mpFileMenu->insertItem(*mpPrintTextIcon,tr("Print t&ext..."), mpEditOcr, SLOT(slotPrint()),
                       CTRL+Key_P, ID_FILE_PRINT_TEXT);

    mpFileMenu->insertSeparator();

    mpFileMenu->insertItem(tr("&Quit"), this, SLOT(close()),
                       CTRL+Key_Q, ID_FILE_QUIT);
    connect(mpFileMenu,SIGNAL(aboutToShow()),this,
            SLOT(slotAboutToShowFileMenu()));
    ///////////////////////////////////////////////////////////////////
    // menuBar entry editMenu
    mpEditMenu=new QPopupMenu(this);
    mpEditMenu->setCheckable(true);

    mpEditMenu->insertItem(*mpUndoImageIcon,tr("&Undo (image)"), this,
                    SLOT(slotUndo()),SHIFT+CTRL+Key_Z, ID_EDIT_UNDO);

    mpEditMenu->insertItem(*mpRedoImageIcon,tr("&Redo (image)"), this,
                    SLOT(slotRedo()),SHIFT+CTRL+Key_Y, ID_EDIT_REDO);

    mpEditMenu->insertSeparator();

    mpEditMenu->insertItem(*mpUndoTextIcon,tr("U&ndo (text)"),mpEditOcr,
                    SLOT(undo()),CTRL+Key_Z,ID_EDIT_UNDO_TEXT);

    mpEditMenu->insertItem(*mpRedoTextIcon,tr("R&edo (text)"),mpEditOcr,
                    SLOT(redo()),CTRL+Key_Y,ID_EDIT_REDO_TEXT);

    mpEditMenu->insertSeparator();

    mpEditMenu->insertItem(tr("&Copy image"), this,
                    SLOT(slotCopyImage()),SHIFT+CTRL+Key_C, ID_EDIT_COPY_IMAGE);

    mpEditMenu->insertItem(tr("Copy image &selection"), this,
                    SLOT(slotCopyImageSelection()),0, ID_EDIT_COPY_IMAGE_SELECTION);

    mpEditMenu->insertItem(tr("Re&place image with selection"), this,
                    SLOT(slotReplaceImageWithSelection()),0, ID_EDIT_REPLACE_IMAGE_WITH_SELECTION);

    mpEditMenu->insertSeparator();

    mpEditMenu->insertItem(*mpMoveSelectionIcon,tr("&Move selection (image)"), this,
                    SLOT(slotMoveSelection()),0, ID_EDIT_MOVE_IMAGE_SELECTION);
    mpEditMenu->setItemChecked(ID_EDIT_MOVE_IMAGE_SELECTION,true);

    mpEditMenu->insertItem(*mpNewSelectionIcon,tr("Ne&w selection (image)"), this,
                    SLOT(slotNewSelection()),0, ID_EDIT_NEW_IMAGE_SELECTION);

    mpEditMenu->insertItem(*mpSelectAllIcon,tr("Se&lect all (image)"), this,
                    SLOT(slotSelectAll()),0, ID_EDIT_SELECT_ALL_IMAGE);

    mpEditMenu->insertSeparator();

    mpEditMenu->insertItem(tr("Cu&t text"), mpEditOcr,
                    SLOT(cut()),CTRL+Key_X, ID_EDIT_CUT_TEXT);

    mpEditMenu->insertItem(tr("C&opy text"), mpEditOcr,
                    SLOT(copy()),CTRL+Key_C, ID_EDIT_COPY_TEXT);

    mpEditMenu->insertItem(tr("P&aste text"), mpEditOcr,
                    SLOT(paste()),CTRL+Key_V, ID_EDIT_PASTE_TEXT);
    mpEditMenu->setItemEnabled(ID_EDIT_CUT_TEXT,false);
    mpEditMenu->setItemEnabled(ID_EDIT_COPY_TEXT,false);
    //This connection is needed to check, whether paste/undo/redo text is available
    connect(mpEditMenu,SIGNAL(aboutToShow()),this,
            SLOT(slotAboutToShowEditMenu()));
    ///////////////////////////////////////////////////////////////////
    // zoom Menu
    mpZoomMenu=new QPopupMenu(this);
    mpZoomMenu->setCheckable(true);
    mpZoomMenu->insertItem("10 %");
    mpZoomMenu->insertItem("20 %");
    mpZoomMenu->insertItem("30 %");
    mpZoomMenu->insertItem("40 %");
    mpZoomMenu->insertItem("50 %");
    mpZoomMenu->insertItem("60 %");
    mpZoomMenu->insertItem("70 %");
    mpZoomMenu->insertItem("80 %");
    mpZoomMenu->insertItem("90 %");
    mpZoomMenu->insertItem("100 %");
    mpZoomMenu->insertItem("125 %");
    mpZoomMenu->insertItem("150 %");
    mpZoomMenu->insertItem("175 %");
    mpZoomMenu->insertItem("200 %");
    mpZoomMenu->insertItem("250 %");
    mpZoomMenu->insertItem("300 %");
    mpZoomMenu->insertItem("400 %");
    mpZoomMenu->insertItem("500 %");
    mpZoomMenu->insertItem("600 %");
    mpZoomMenu->insertItem("700 %");
    mpZoomMenu->insertItem("800 %");
    mpZoomMenu->insertItem("900 %");
    mpZoomMenu->insertItem("1000 %");
    mpZoomMenu->setItemChecked(mpZoomMenu->idAt(9),true);

    ///////////////////////////////////////////////////////////////////
    // ImageBar entry viewMenu
    mpImageMenu=new QPopupMenu(this);
    mpImageMenu->insertItem(tr("&Brightness/Contrast..."), this,
                            SLOT(slotBrightnessContrast()));
    mpImageMenu->insertItem(tr("&Despeckle..."), this,
                            SLOT(slotDespeckle()));
    mpImageMenu->insertItem(tr("&Gamma..."), this,
                    SLOT(slotGamma()));
    mpImageMenu->insertItem(tr("G&aussian blur (IIR)..."), this,
                            SLOT(slotBlurIIR()));
    mpImageMenu->insertItem(tr("&Invert..."), this,
                    SLOT(slotInvert()));
    mpImageMenu->insertItem(tr("&Normalize..."), this,
                    SLOT(slotNormalize()));
    mpImageMenu->insertItem(tr("&Oilpainting..."), this,
                    SLOT(slotOilPainting()));
    mpImageMenu->insertItem(tr("&Posterize..."), this,
                    SLOT(slotPosterize()));
    mpImageMenu->insertItem(tr("&Rotate..."), this,
                    SLOT(slotRotate()));
    mpImageMenu->insertItem(tr("S&cale..."), this,
                    SLOT(slotScale()));
    mpImageMenu->insertItem(tr("&Sharpen..."), this,
                    SLOT(slotSharpen()));
    mpImageMenu->insertItem(tr("S&hear..."), this,
                    SLOT(slotShear()));
    mpImageMenu->insertItem(tr("&Transparency..."), this,
                    SLOT(slotTransparency()));
    mpImageMenu->insertSeparator();
    mpImageMenu->insertItem(tr("&Convert..."), this,
                    SLOT(slotConvert()));
    ///////////////////////////////////////////////////////////////////
    // ocrBar entry viewMenu
    mpOcrMenu=new QPopupMenu(this);
    mpOcrMenu->setCheckable(true);
    mpOcrMenu->insertItem(*mpOcrIconOn,tr("&Enable OCR"),mpFileOcr,SLOT(toggle()),
                        0,ID_OCR_MODE);

    mpOcrMenu->insertItem(*mpOcrStartIcon,tr("&Start OCR"), this, SLOT(slotStartOcr()),
                         0, ID_OCR_START);

    mpOcrMenu->insertItem(*mpOcrSelectionIcon,tr("S&tart OCR on selection"),this,
                          SLOT(slotStartOcrSelection()),0, ID_OCR_START_SELECTION);

    mpOcrMenu->insertSeparator();

    mpOcrMenu->insertItem(*mpOcrIconStop,tr("St&op OCR"), this, SLOT(slotStopOcr()),
                         0, ID_OCR_STOP);

    mpOcrMenu->setItemEnabled(ID_OCR_STOP,false);

    ///////////////////////////////////////////////////////////////////
    // menuBar entry viewMenu
    mpViewMenu=new QPopupMenu(this);
    mpViewMenu->setCheckable(true);
    mpViewMenu->insertItem(tr("&Toolbar"), this, SLOT(slotViewToolBar()),
                         0,ID_VIEW_TOOLBAR);
    mpViewMenu->insertItem(tr("&Statusbar"), this, SLOT(slotViewStatusBar()),
                         0, ID_VIEW_STATUSBAR);

    mpViewMenu->setItemChecked(ID_VIEW_TOOLBAR, true);
    mpViewMenu->setItemChecked(ID_VIEW_STATUSBAR, true);

    ///////////////////////////////////////////////////////////////////
    // menuBar entry HelpMenu
    mpSettingsMenu=new QPopupMenu(this);
    mpSettingsMenu->insertItem(tr("&Editor..."),mpEditOcr,SLOT(slotSetup()),
                         0, ID_SETTINGS_TEXTEDIT);
    mpSettingsMenu->insertItem(tr("&Save settings"),this,SLOT(slotSaveConfig()),
                         0, ID_SETTINGS_VIEWER);

    ///////////////////////////////////////////////////////////////////
    // menuBar entry helpMenu
    mpHelpMenu=new QPopupMenu(this);
    mpHelpMenu->insertItem(tr("&About..."), this, SLOT(slotHelpAbout()),
                         0, ID_HELP_ABOUT);

    mpHelpMenu->insertItem(tr("About &Qt..."), this, SLOT(slotHelpAboutQt()),
                         0, ID_HELP_ABOUT_QT);
    ///////////////////////////////////////////////////////////////////
    // MENUBAR CONFIGURATION
    mb->insertItem(tr("&File"), mpFileMenu);
    mb->insertItem(tr("&Edit"), mpEditMenu);
    mb->insertItem(tr("&Zoom"), mpZoomMenu);
    mb->insertItem(tr("&Image"), mpImageMenu);
    mb->insertItem(tr("&OCR"), mpOcrMenu);
    mb->insertItem(tr("&View"), mpViewMenu);
    mb->insertItem(tr("O&ptions"), mpSettingsMenu);
    mb->insertSeparator();
    mb->insertItem(tr("&Help"), mpHelpMenu);
    ///////////////////////////////////////////////////////////////////
    // CONNECT THE SUBMENU SLOTS WITH SIGNALS
    connect(mpFileMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpEditMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpOcrMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpViewMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpSettingsMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpHelpMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
  }
  else
  {
    ///////////////////////////////////////////////////////////////////
    // MENUBAR TextOnly mode
    ///////////////////////////////////////////////////////////////////
    // menuBar entry fileMenu
    mpFileMenu=new QPopupMenu(this);
    mpFileMenu->insertItem(*mpOpenTextIcon,tr("&Open"), this,
                      SLOT(slotFileLoadText()),CTRL+Key_O, ID_FILE_LOAD_TEXT);
    mpFileMenu->insertItem(tr("&Save"), this,
                      SLOT(slotFileSaveText()),CTRL+Key_S, ID_FILE_SAVE_TEXT);
    mpFileMenu->insertItem(*mpSaveTextIcon,tr("Save &as..."), this,
                      SLOT(slotFileSaveTextAs()),0, ID_FILE_SAVE_TEXT_AS);
    mpFileMenu->insertSeparator();
    mpFileMenu->insertItem(*mpPrintTextIcon,tr("&Print"), mpEditOcr, SLOT(slotPrint()),
                         CTRL+Key_P, ID_FILE_PRINT_TEXT);
    mpFileMenu->insertSeparator();
    mpFileMenu->insertItem(tr("&Quit"), this, SLOT(close()),
                         CTRL+Key_Q, ID_FILE_QUIT);

    ///////////////////////////////////////////////////////////////////
    // menuBar entry editMenu
    mpEditMenu=new QPopupMenu(this);
    mpEditMenu->insertItem(*mpUndoTextIcon,tr("U&ndo (text)"),mpEditOcr,
                    SLOT(undo()),SHIFT+CTRL+Key_Z,ID_EDIT_UNDO_TEXT);

    mpEditMenu->insertItem(*mpRedoTextIcon,tr("R&edo (text)"),mpEditOcr,
                    SLOT(redo()),SHIFT+CTRL+Key_Y,ID_EDIT_REDO_TEXT);
    mpEditMenu->insertSeparator();
    mpEditMenu->insertItem(tr("C&ut"), mpEditOcr,
                      SLOT(cut()),CTRL+Key_X, ID_EDIT_CUT_TEXT);
    mpEditMenu->insertItem(tr("C&opy"), mpEditOcr,
                      SLOT(copy()),CTRL+Key_C, ID_EDIT_COPY_TEXT);
    mpEditMenu->insertItem(tr("P&aste"), mpEditOcr,
                      SLOT(paste()),CTRL+Key_V, ID_EDIT_PASTE_TEXT);
    mpEditMenu->setItemEnabled(ID_EDIT_CUT_TEXT,false);
    mpEditMenu->setItemEnabled(ID_EDIT_COPY_TEXT,false);
    //This connection is needed to check, whether paste/undo/redo text is available
    connect(mpEditMenu,SIGNAL(aboutToShow()),this,
            SLOT(slotAboutToShowEditMenu()));

    ///////////////////////////////////////////////////////////////////
    // menuBar entry viewMenu
    mpViewMenu=new QPopupMenu(this);
    mpViewMenu->setCheckable(true);
    mpViewMenu->insertItem(tr("&Toolbar"), this, SLOT(slotViewToolBar()),
                         0,ID_VIEW_TOOLBAR);
    mpViewMenu->insertItem(tr("&Statusbar"), this, SLOT(slotViewStatusBar()),
                         0, ID_VIEW_STATUSBAR);

    mpViewMenu->setItemChecked(ID_VIEW_TOOLBAR, true);
    mpViewMenu->setItemChecked(ID_VIEW_STATUSBAR, true);

    ///////////////////////////////////////////////////////////////////
    // menuBar entry setupMenu
    mpSettingsMenu=new QPopupMenu(this);
    mpSettingsMenu->setCheckable(true);
    mpSettingsMenu->insertItem(tr("&Editor..."),mpEditOcr,SLOT(slotSetup()),
                          0,ID_SETTINGS_TEXTEDIT);

    ///////////////////////////////////////////////////////////////////
    // menuBar entry helpMenu
    mpHelpMenu=new QPopupMenu(this);
    mpHelpMenu->insertItem(tr("&About..."), this, SLOT(slotHelpAbout()),
                         0, ID_HELP_ABOUT);

    ///////////////////////////////////////////////////////////////////
    // MENUBAR CONFIGURATION
    mb->insertItem(tr("&File"), mpFileMenu);
    mb->insertItem(tr("&Edit"), mpEditMenu);
    mb->insertItem(tr("&View"), mpViewMenu);
    mb->insertItem(tr("&Options"), mpSettingsMenu);
    mb->insertSeparator();
    mb->insertItem(tr("&Help"), mpHelpMenu);

    ///////////////////////////////////////////////////////////////////
    // CONNECT THE SUBMENU SLOTS WITH SIGNALS
    connect(mpFileMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpEditMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpViewMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
    connect(mpHelpMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
  }
}

void QuiteInsane::initToolBar()
{
  if(mMode == Mode_ImageOcr)
  {
    ///////////////////////////////////////////////////////////////////
    // TOOLBAR ImageOCR mode
    mpImageToolbar = new QToolBar(this);
    addToolBar(mpImageToolbar,Top,true);

    mpFileLoadImage = new QToolButton(*mpOpenImageIcon,tr("Load image..."),0,this,
                                    SLOT(slotFileLoadImage()),mpImageToolbar);

    mpFileSaveImage = new QToolButton(*mpSaveImageIcon,
                                tr("Save image"),0,this,
                                SLOT(slotFileSaveImage()),mpImageToolbar);

    mpFilePrint = new QToolButton(*mpPrintImageIcon,tr("Print image..."),0,this,
                                  SLOT(slotPrint()),mpImageToolbar);

    mpImageToolbar->addSeparator();

    //selection
    mpFileSaveImageSelection = new QToolButton(*mpSaveSelectionIcon,
                                tr("Save selection as..."),0,this,
                                SLOT(slotFileSaveSelectionAs()),mpImageToolbar);

    mpFilePrintImageSelection = new QToolButton(*mpPrintSelectionIcon,tr("Print selection..."),0,this,
                                  SLOT(slotFilePrintSelection()),mpImageToolbar);

    mpImageToolbar->addSeparator();

    mpFileUndoImage = new QToolButton(*mpUndoImageIcon,tr("Undo"),0,this,
                                  SLOT(slotUndo()),mpImageToolbar);

    mpFileRedoImage = new QToolButton(*mpRedoImageIcon,tr("Redo"),0,this,
                                  SLOT(slotRedo()),mpImageToolbar);

    mpImageToolbar->addSeparator();

    mpFileZoomCombo = new QComboBox(mpImageToolbar);
    QToolTip::add(mpFileZoomCombo,tr("Zoom the image"));
    mpFileZoomCombo->insertItem("10 %");
    mpFileZoomCombo->insertItem("20 %");
    mpFileZoomCombo->insertItem("30 %");
    mpFileZoomCombo->insertItem("40 %");
    mpFileZoomCombo->insertItem("50 %");
    mpFileZoomCombo->insertItem("60 %");
    mpFileZoomCombo->insertItem("70 %");
    mpFileZoomCombo->insertItem("80 %");
    mpFileZoomCombo->insertItem("90 %");
    mpFileZoomCombo->insertItem("100 %");
    mpFileZoomCombo->insertItem("125 %");
    mpFileZoomCombo->insertItem("150 %");
    mpFileZoomCombo->insertItem("175 %");
    mpFileZoomCombo->insertItem("200 %");
    mpFileZoomCombo->insertItem("250 %");
    mpFileZoomCombo->insertItem("300 %");
    mpFileZoomCombo->insertItem("400 %");
    mpFileZoomCombo->insertItem("500 %");
    mpFileZoomCombo->insertItem("600 %");
    mpFileZoomCombo->insertItem("700 %");
    mpFileZoomCombo->insertItem("800 %");
    mpFileZoomCombo->insertItem("900 %");
    mpFileZoomCombo->insertItem("1000 %");
    mpFileZoomCombo->setCurrentItem(9);
    mpImageToolbar->addSeparator();

    mpFileOcr = new QToolButton(mpImageToolbar);
    mpFileOcr->setIconSet(*mpOcrIconOn);
    mpFileOcr->setToggleButton(true);
    QToolTip::add(mpFileOcr,tr("Enable/Disable OCR mode"));
    connect(mpFileOcr,SIGNAL(stateChanged(int)),
            this,SLOT(slotEnableOcr(int)));

    mpImageToolbar->addSeparator();
    QWhatsThis::whatsThisButton(mpImageToolbar);

    mpToolsToolbar = new QToolBar(this);
    addToolBar(mpToolsToolbar,Left,true);

    mpToolButtonGroup = new QButtonGroup(this);
    mpToolButtonGroup->hide();

    mpToolsMoveSelection = new QToolButton(*mpMoveSelectionIcon,tr("Move selection"),0,this,
                                           0,mpToolsToolbar);
    mpToolsMoveSelection->setToggleButton(true);

    mpToolsNewSelection = new QToolButton(*mpNewSelectionIcon,tr("New selection"),0,this,
                                          0,mpToolsToolbar);
    mpToolsNewSelection->setToggleButton(true);

    mpToolButtonGroup->insert(mpToolsMoveSelection,0);
    mpToolButtonGroup->insert(mpToolsNewSelection,1);
    mpToolButtonGroup->setExclusive(true);
    connect(mpToolButtonGroup,SIGNAL(clicked(int)),this,
            SLOT(slotToolMode(int)));
    if(!mpToolsMoveSelection->isOn())
      mpToolsMoveSelection->toggle();

    mpToolsSelectAll = new QToolButton(*mpSelectAllIcon,tr("Select all"),0,this,
                                    SLOT(slotSelectAll()),mpToolsToolbar);
/////////////////////////////
// text toolbar
    mpTextToolbar = new QToolBar(this);
    addToolBar(mpTextToolbar,Top,true);

    mpFileLoadText = new QToolButton(*mpOpenTextIcon,tr("Load text..."),0,this,
                                    SLOT(slotFileLoadText()),mpTextToolbar);

    mpFileSaveText = new QToolButton(*mpSaveTextIcon,
                                tr("Save text"),0,this,
                                SLOT(slotFileSaveText()),mpTextToolbar);

    mpFilePrintText = new QToolButton(*mpPrintTextIcon,tr("Print text..."),0,
                                     mpEditOcr,SLOT(slotPrint()),mpTextToolbar);

    mpTextToolbar->addSeparator();

    mpFileUndoText = new QToolButton(*mpUndoTextIcon,tr("Undo (text)"),0,
                                     mpEditOcr,SLOT(undo()),mpTextToolbar);
    mpFileUndoText->setEnabled(false);
    connect(mpEditOcr,SIGNAL(undoAvailable(bool)),
            mpFileUndoText,SLOT(setEnabled(bool)));

    mpFileRedoText = new QToolButton(*mpRedoTextIcon,tr("Redo (text)"),0,
                                     mpEditOcr,SLOT(redo()),mpTextToolbar);
    mpFileRedoText->setEnabled(false);
    connect(mpEditOcr,SIGNAL(redoAvailable(bool)),
            mpFileRedoText,SLOT(setEnabled(bool)));

    mpTextToolbar->addSeparator();

    mpFileOcrStart = new QToolButton(*mpOcrStartIcon,tr("Start OCR"),0,this,
                                   SLOT(slotStartOcr()),mpTextToolbar);

    mpFileOcrStartSelection = new QToolButton(*mpOcrSelectionIcon,tr("Start OCR on selection"),0,this,
                                   SLOT(slotStartOcrSelection()),mpTextToolbar);

    mpFileOcrStop = new QToolButton(mpTextToolbar);
#ifdef USE_QT3
    mpFileOcrStop->setIconSet(*mpOcrIconStop);
#else
    mpFileOcrStop->setOnIconSet(*mpOcrIconStop);
    mpFileOcrStop->setOffIconSet(*mpOcrIconStop);
#endif
    mpFileOcrStop->setEnabled(false);
    QToolTip::add(mpFileOcrStop,tr("Stop character recognition"));
    connect(mpFileOcrStop,SIGNAL(clicked()),
            this,SLOT(slotStopOcr()));

    mpFileOcrStart->setEnabled(false);
    connect(mpFileOcr,SIGNAL(toggled(bool)),
            mpFileOcrStart,SLOT(setEnabled(bool)));
    connect(mpFileOcr,SIGNAL(toggled(bool)),
            mpFileSaveText,SLOT(setEnabled(bool)));

    QWhatsThis::add(mpFileLoadImage,
                tr("<html>Click this button to load an image.<br><br>"
                "You can also select the <b>Load image...</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFileSaveImage,
                tr("<html>Click this button to save the image.<br><br>"
                "You can also select the <b>Save image</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFilePrint,
                tr("<html>Click this button to print the image.<br><br>"
                "You can also select the <b>Print image...</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFileSaveImageSelection,
                tr("<html>Click this button to save the image selection.<br><br>"
                "You can also select the <b>Save selection...</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFilePrintImageSelection,
                tr("<html>Click this button to print the image selection.<br><br>"
                "You can also select the <b>Print selection...</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFileZoomCombo,
                tr("<html>Use this combo box to zoom the image.<br><br>"
                "You can also select an entry in the <b>Zoom</b> menu.</html>"));
    QWhatsThis::add(mpFileUndoImage,
                tr("<html>Click this button do undo the last image "
                " modifications.<br><br>"
                "You can also select the <b>Undo (image)</b> command "
                "in the <b>Edit</b> menu.</html>"));
    QWhatsThis::add(mpFileRedoImage,
                tr("<html>Click this button do redo the last image "
                "modifications.<br><br>"
                "You can also select the <b>Redo (image)</b> command "
                "in the <b>Edit</b> menu.</html>"));
    QWhatsThis::add(mpFileOcr,
                tr("<html>Click this button to enable/disable optical "
                "character recognition.<br><br>"
                "You can also select the <b>Enable OCR</b> command "
                "from the <b>OCR</b> menu.</html>"));
    QWhatsThis::add(mpFileOcrStart,
                tr("<html>Click this button to start optical character "
                "recognition.<br><br>"
                "You can also select the <b>Start OCR</b> command "
                "from the <b>OCR</b> menu.</html>"));
    QWhatsThis::add(mpFileOcrStartSelection,
                tr("<html>Click this button to start optical character "
                "recognition on the image selection.<br><br>"
                "You can also select the <b>Start OCR on selection</b> command "
                "from the <b>OCR</b> menu.</html>"));
    QWhatsThis::add(mpFileOcrStop,
                tr("<html>Click this button to stop optical "
                "character recognition.<br><br>"
                "You can also select the <b>Stop OCR</b> command "
                "from the <b>OCR</b> menu.</html>"));
    QWhatsThis::add(mpFileLoadText,
                tr("<html>Click this button to load a text.<br><br>"
                "You can also select the <b>Load text...</b> "
                "command from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFileSaveText,
                tr("<html>Click this button to save the text "
                "under the current file name.<br><br>"
                "You can also select the <b>Save text</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFilePrintText,
                tr("<html>Click this button to print the text.<br><br>"
                "You can also select the <b>Print text...</b> command "
                "from the <b>File</b> menu.</html>"));
    QWhatsThis::add(mpFileUndoText,
                tr("<html>Click this button do undo the last text "
                "modifications.<br><br>"
                "You can also select the <b>Undo (text)</b> command "
                "in the <b>Edit</b> menu.</html>"));
    QWhatsThis::add(mpFileRedoText,
                tr("<html>Click this button do redo the last text "
                "modifications.<br><br>"
                "You can also select the <b>Redo (text)</b> command "
                "in the <b>Edit</b> menu.</html>"));
    QWhatsThis::add(mpToolsMoveSelection,
                tr("<html>Click this button to enable <b>Move selection</b><br> "
                "mode. That means, that you can move the selection "
                "rectangle by pressing the left mouse button inside the "
                "rectangle.<br><br>"
                "You can also select the <b>Move selection</b> command "
                "from the <b>Edit</b> menu.</html>"));
    QWhatsThis::add(mpToolsNewSelection,
                tr("<html>Click this button to enable <b>New selection</b><br> "
                "mode. That means, that pressing the left mouse button "
                "inside the selection rectangle result in a new "
                "rectangle.<br><br>"
                "You can also select the <b>New selection</b> command "
                "from the <b>Edit</b> menu.</html>"));
    QWhatsThis::add(mpToolsSelectAll,
                tr("<html>Click this button to select the entire image.<br><br>"
                "You can also select the <b>Select all</b> command "
                "from the <b>Edit</b> menu.</html>"));
  }
  else
  {
    ///////////////////////////////////////////////////////////////////
    // TOOLBAR TextOnly mode
    mpImageToolbar = new QToolBar(this, "file operations");

    mpFileLoadText = new QToolButton(*mpOpenTextIcon,tr("Open text"),0,this,
                                       SLOT(slotFileLoadText()),mpImageToolbar);

    mpFileSaveText = new QToolButton(*mpSaveTextIcon,tr("Save text"),0,this,
                                       SLOT(slotFileSaveText()),mpImageToolbar);

    mpFilePrint = new QToolButton(*mpPrintTextIcon,tr("Print image..."),0,mpEditOcr,
                                SLOT(slotPrint()),mpImageToolbar);

    mpImageToolbar->addSeparator();

    mpFileUndoText = new QToolButton(*mpUndoTextIcon,tr("Undo (text)"),0,
                                     mpEditOcr,SLOT(undo()),mpImageToolbar);
    mpFileUndoText->setEnabled(false);
    connect(mpEditOcr,SIGNAL(undoAvailable(bool)),
            mpFileUndoText,SLOT(setEnabled(bool)));

    mpFileRedoText = new QToolButton(*mpRedoTextIcon,tr("Redo (text)"),0,
                                     mpEditOcr,SLOT(redo()),mpImageToolbar);
    mpFileRedoText->setEnabled(false);
    connect(mpEditOcr,SIGNAL(redoAvailable(bool)),
            mpFileRedoText,SLOT(setEnabled(bool)));

    mpImageToolbar->addSeparator();
    QWhatsThis::whatsThisButton(mpImageToolbar);

    QWhatsThis::add(mpFileSaveText,
                tr("<html>Click this button to save the text "
                "under the current file name.<br><br>"
                "You can also select the <b>Save text</b> command "
                "from the <b>File</b> menu.</html>"));

    QWhatsThis::add(mpFilePrint,
                tr("<html>Click this button to print the text.<br><br>"
                "You can also select the <b>Print...</b> command "
                "from the <b>File</b> menu.</html>"));

    QWhatsThis::add(mpFileUndoText,
                tr("<html>Click this button do undo the last text "
                "modifications.<br><br>"
                "You can also select the <b>Undo (text)</b> command "
                "in the <b>Edit</b> menu.</html>"));

    QWhatsThis::add(mpFileRedoText,
                tr("<html>Click this button do redo the last text "
                "modifications.<br><br>"
                "You can also select the <b>Redo (text)</b> command "
                "in the <b>Edit</b> menu.</html>"));
  }
  connect(this,SIGNAL(toolBarPositionChanged(QToolBar*)),
          this,SLOT(slotToolBarPositionChanged(QToolBar*)));
}

void QuiteInsane::initStatusBar()
{
  if(mMode == Mode_ImageOcr)
  {
    QPixmap pix((const char **)stop_filter_xpm);
    mpProgressStack = new QWidgetStack(statusBar());
    mpProgressStack->setMinimumWidth(150);
    mpUnknownProgress = new QUnknownProgressWidget(mpProgressStack);
    mpProgressStack->addWidget(mpUnknownProgress,0);
    mpFilterHBox = new QHBox(mpProgressStack);
    mpFilterButton = new QToolButton(mpFilterHBox);
    mpFilterButton->setPixmap(pix);
    mpFilterProgress = new QProgressBar(mpFilterHBox);
    mpProgressStack->addWidget(mpFilterHBox,1);
    statusBar()->addWidget(mpProgressStack,0,TRUE);
    mpProgressStack->raiseWidget(1);
    mpFilterHBox->setEnabled(false);
  }
  statusBar()->message(IDS_STATUS_DEFAULT, 2000);
}

void QuiteInsane::initView()
{
  if(mMode == Mode_ImageOcr)
  {
    mpSplitter = new QSplitter(this);
    mpView = new QViewerCanvas(mpSplitter);
    mpEditOcr = new QMultiLineEditPE(mpSplitter);
    setCentralWidget(mpSplitter);
    mpEditOcr->hide();
    connect(mpView,SIGNAL(signalRectangleChanged()),this,
          SLOT(slotRectangleChanged()));
    connect(mpView,SIGNAL(signalLocalImageUriDropped(QStringList)),this,
          SLOT(slotLocalImageUriDropped(QStringList)));
  }
  else
  {
    mpEditOcr = new QMultiLineEditPE(this);
    setCentralWidget(mpEditOcr);
  }
  connect(mpEditOcr,SIGNAL(copyAvailable(bool)),this,
          SLOT(slotCopyAvailable(bool)));
  QWhatsThis::add(mpEditOcr,tr("Click the contents of this viewer with the "
                          "right mouse button to pop up a menu with "
                          "futher commands like cut, copy and paste."));
}

void QuiteInsane::slotFileSaveImageAs()
{
  QString qs;
  if(mMode != Mode_ImageOcr)
    return;
  if(mImageVectorIndex < 0)
    return;
  //Check whether we display a valid image at the moment
  QImage* image = mImageVector[mImageVectorIndex]->image();
  if(image)
  {
    if(!image->isNull())
    {
      //create a QFileDialog
      QPreviewFileDialog qpfd(false,false,this,0,TRUE);
      qpfd.setMode(QFileDialog::AnyFile);
      qpfd.setImage(image);
      if(qpfd.exec())
      {
        mImageModified = false;
        mImageUserModified = false;
        qs = tr("QuiteInsane - Image viewer");
        qs +="[";
        qs += qpfd.selectedFile();
        qs +="]";
        setCaption(qs);
        mOriginalImageFilepath = qpfd.selectedFile();
        mSaveIndex = mImageVectorIndex;
        emit signalImageSaved(qpfd.selectedFile());
      }
      statusBar()->message(IDS_STATUS_DEFAULT);
      return;
    }
  }
  QMessageBox::information(this,tr("Save image as..."),
        tr("There is no image loaded, which could be saved."),tr("&OK"));
}

void QuiteInsane::slotFileSaveTextAs()
{
  statusBar()->message(IDS_STATUS_DEFAULT);
  int i;
  QString filename;
	QString format;
  QString ext;
  QString qs;
  bool formatflag;
  formatflag = false;

  //create a QFileDialog
	QFileDialogExt qfd(mViewerSaveTextPath,0,this,"",TRUE);
  qfd.setMode(QFileDialog::AnyFile);
  qfd.setCaption(tr("Save text as ..."));
  qfd.setViewMode((QFileDialog::ViewMode)mSingleFileViewMode);
  if(qfd.exec())
	{
	  //the user entered a filename
    //save some settings
    mViewerSaveTextPath = qfd.dir()->absPath();

    //get the filename under which we can save the new image
    filename = qfd.selectedFile();
    if(QFile::exists(filename))
    {
      i = QMessageBox::warning(this,tr("Save text ..."),
          tr("This file already exists .\n"
    	  	"Do you want to overwrite it ?\n\n"),
          tr("&Overwrite"),tr("&No"));
	    if(i == 1) return;
    }
    QFile qf(filename);
    if(qf.open(IO_WriteOnly))
    {
      QTextStream ts(&qf);
      ts<<mpEditOcr->text();
      qf.close();
      mpEditOcr->setEdited(false);
      if(mMode == Mode_TextOnly)
      {
        qs = tr("QuiteInsane - Editor");
        qs +="[";
        qs += filename;
        qs +="]";
        setCaption(qs);
      }
      mOriginalTextFilepath = filename;
    }
    else
    {
       QMessageBox::warning(this,tr("Error"),
           tr("The file could not be saved.\n"),tr("OK"));	
    }
  }
  mSingleFileViewMode = qfd.intViewMode();
}

void QuiteInsane::slotFileQuit()
{
  statusBar()->message("Exiting application...");
  statusBar()->message(IDS_STATUS_DEFAULT);
}


void QuiteInsane::slotViewToolBar()
{
  statusBar()->message("Toggle toolbar...");
  ///////////////////////////////////////////////////////////////////
  // turn Toolbar on or off

  if (mpImageToolbar->isVisible())
  {
    mpImageToolbar->hide();
    mpTextToolbar->hide();
    mpToolsToolbar->hide();
    mpViewMenu->setItemChecked(ID_VIEW_TOOLBAR, false);
  }
  else
  {
    mpImageToolbar->show();
    if(mViewerOcrMode) mpTextToolbar->show();
    mpToolsToolbar->show();
    mpViewMenu->setItemChecked(ID_VIEW_TOOLBAR, true);
  };

  statusBar()->message(IDS_STATUS_DEFAULT);
}

void QuiteInsane::slotViewStatusBar()
{
  statusBar()->message("Toggle statusbar...");
  ///////////////////////////////////////////////////////////////////
  //turn Statusbar on or off

  if (statusBar()->isVisible())
  {
    statusBar()->hide();
    mpViewMenu->setItemChecked(ID_VIEW_STATUSBAR, false);
  }
  else
  {
    statusBar()->show();
    mpViewMenu->setItemChecked(ID_VIEW_STATUSBAR, true);
  }

  statusBar()->message(IDS_STATUS_DEFAULT);
}

void QuiteInsane::slotHelpAbout()
{
  QMessageBox::about(this,"About...",
  tr("<center><b><h3>QuiteInsane - Image/Text Viewer</h3></b></center><br>"
  "<center>&copy  2000-2003 Michael Herder</center>"
	"<center>This viewer is part of <b>QuiteInsane</b>,</center>"
  "<center>a graphical frontend for SANE.</center><br>"));
}

void QuiteInsane::slotStatusHelpMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message of whole statusbar temporary (text, msec)
  statusBar()->message(text, 2000);
}

void QuiteInsane::statusCallback(int id_)
{
  switch (id_)
  {
    case ID_OCR_MODE:
         slotStatusHelpMsg(tr("Enable/Disable optical character recognition"));
         break;

    case ID_OCR_START:
         slotStatusHelpMsg(tr("Start optical character recognition"));
         break;

    case ID_OCR_START_SELECTION:
         slotStatusHelpMsg(tr("Start optical character on image selection"));
         break;

    case ID_OCR_STOP:
         slotStatusHelpMsg(tr("Stop optical character recognition"));
         break;

    case ID_FILE_LOAD_IMAGE:
         slotStatusHelpMsg(tr("Load an existing image..."));
         break;

    case ID_FILE_LOAD_TEXT:
         slotStatusHelpMsg(tr("Load an existing text file..."));
         break;

    case ID_FILE_LOAD_LAST_SCAN:
         slotStatusHelpMsg(tr("Load the last scanned image"));
         break;

    case ID_FILE_SAVE_SELECTION_AS:
         slotStatusHelpMsg(tr("Save the image selection as..."));
         break;

    case ID_FILE_PRINT_SELECTION:
         slotStatusHelpMsg(tr("Print the image selection..."));
         break;

    case ID_FILE_SAVE_IMAGE_AS:
         slotStatusHelpMsg(tr("Save the actual image as..."));
         break;

    case ID_FILE_SAVE_IMAGE:
         slotStatusHelpMsg(tr("Save the image under the current name"));
         break;

    case ID_FILE_PRINT:
         slotStatusHelpMsg(tr("Print the image..."));
         break;

    case ID_FILE_PRINT_TEXT:
         slotStatusHelpMsg(tr("Print the text..."));
         break;

    case ID_FILE_SAVE_TEXT_AS:
         slotStatusHelpMsg(tr("Save the text under a new filename..."));
         break;

    case ID_FILE_SAVE_TEXT:
         slotStatusHelpMsg(tr("Save the text under the current name"));
         break;

    case ID_FILE_QUIT:
         slotStatusHelpMsg(tr("Quit the viewer."));
         break;

    case ID_EDIT_COPY_IMAGE:
         slotStatusHelpMsg(tr("Copy the image to the clipboard."));
         break;

    case ID_EDIT_MOVE_IMAGE_SELECTION:
         slotStatusHelpMsg(tr("Enable move selection mode"));
         break;

    case ID_EDIT_NEW_IMAGE_SELECTION:
         slotStatusHelpMsg(tr("Enable new selection mode"));
         break;

    case ID_EDIT_COPY_IMAGE_SELECTION:
         slotStatusHelpMsg(tr("Copy the image selection to the clipboard."));
         break;

    case ID_EDIT_SELECT_ALL_IMAGE:
         slotStatusHelpMsg(tr("Select the entire image."));
         break;

    case ID_EDIT_UNDO:
         slotStatusHelpMsg(tr("Undo the last image modification"));
         break;

    case ID_EDIT_REDO:
         slotStatusHelpMsg(tr("Redo the last image modification"));
         break;

    case ID_EDIT_UNDO_TEXT:
         slotStatusHelpMsg(tr("Undo the last text modification"));
         break;

    case ID_EDIT_REDO_TEXT:
         slotStatusHelpMsg(tr("Redo the last text modification"));
         break;

    case ID_EDIT_CUT_TEXT:
         slotStatusHelpMsg(tr("Cut the selected text."));
         break;

    case ID_EDIT_PASTE_TEXT:
         slotStatusHelpMsg(tr("Paste the clipboard contents."));
         break;

    case ID_EDIT_COPY_TEXT:
         slotStatusHelpMsg(tr("Copy the selected text to the clipboard."));
         break;

    case ID_VIEW_TOOLBAR:
         slotStatusHelpMsg(tr("Enable/disable the toolbar."));
         break;

    case ID_VIEW_STATUSBAR:
         slotStatusHelpMsg(tr("Enable/disable the statusbar."));
         break;

    case ID_SETTINGS_TEXTEDIT:
         slotStatusHelpMsg(tr("Configure the text editor..."));
         break;

    case ID_SETTINGS_VIEWER:
         slotStatusHelpMsg(tr("Save the current viewer settings"));
         break;

    case ID_HELP_ABOUT:
         slotStatusHelpMsg(tr("Show an aboutbox..."));
         break;

    case ID_HELP_ABOUT_QT:
         slotStatusHelpMsg(tr("Show an aboutbox about Qt..."));
         break;
  }
}
/**  */
void QuiteInsane::loadImage(QString path)
{
  ImageIOSupporter iisup;
  QString qs;
  if(mMode == Mode_TextOnly) return;//to be sure
  setCursor(Qt::waitCursor);
  mImagePath = path;
  QImage im;
  if(!iisup.loadImage(im,path))
  {
    //could not load  image
    if(!QFile::exists(path))
      qs = tr("The file \n %1 \n could not be loaded.\n"
              "It does not exist.").arg(path);
    else if (!QImage::imageFormat(path))
      qs = tr("The file \n %1 \n could not be loaded.\n"
              "It is not a valid image file.").arg(path);
    QMessageBox::warning(this,tr("Error"),qs,tr("OK"));	
  }
  else
  {
    if(mImageLoaded == false)
      setImageViewerState(true);
    mImageLoaded = true;
    qs = tr("QuiteInsane - Image viewer");
    if(path != xmlConfig->absConfDirPath()+".scantemp.pnm")
    {
      qs +="[";
      qs += path;
      qs +="]";
    }
    setCaption(qs);
    //set to 100%
    mpFileZoomCombo->setCurrentItem(9);
    slotZoomMenu(mpZoomMenu->idAt(9));
    clearImageQueue();
    mpView->setImage(&im);
    addImageToQueue(&im);
    //We don't allow to save under the filename of the last scan.
    if(path == xmlConfig->absConfDirPath()+".scantemp.pnm")
      mOriginalImageFilepath = QString::null;
    else
      mOriginalImageFilepath = path;
    setImageModified(false);
    updateZoomMenu();
    set100PercentZoom();
  }
  setCursor(Qt::arrowCursor);
}
/**  */
void QuiteInsane::setImageModified(bool flag)
{
  mImageModified = flag;
  if(!mImageModified) mImageUserModified = false;
}
/**  */
void QuiteInsane::setTextModified(bool flag)
{
  mpEditOcr->setEdited(flag);
}
/**  */
void QuiteInsane::closeEvent(QCloseEvent* e)
{
  if(mFilterRunning)
  {
    e->ignore();
    return;
  }
  QString qs;
  qs = "";
  if(imageModified())
  {
    qs = tr("The image has not been saved.");
    qs += "\n";
  }
  if(mpEditOcr->edited())
  {
    qs += tr("The text has not been saved.");
    qs += "\n";
  }

  qs+=tr("Do you really want to quit the viewer?");
  qs+="\n";
  qs+=tr("All data will be lost.");

  int exit = 0;
  if (imageModified() || mpEditOcr->edited())
  {
    exit=QMessageBox::warning(this, tr("Quit..."),qs,
                              tr("&Quit"),tr("&Cancel"));
  }
  if(exit != 0)
  {
    e->ignore();
  }
  else
  {
    if(mMode == Mode_ImageOcr)
    {
      if(mpOcrProcess)
      {
        if(mpOcrProcess->isRunning())
        {
          mpOcrProcess->kill();
        }
      }
//      //Delete images in undo buffer
      clearImageQueue();
    }
    qApp->clipboard()->clear();
    QWidget::closeEvent(e);
  }
}
/**  */
bool QuiteInsane::imageModified()
{
  if(mImageModified)
    return mImageModified;
  if(mImageUserModified && (mSaveIndex != mImageVectorIndex))
    return true;
  return false;
}
/**  */
bool QuiteInsane::textModified()
{
  return mpEditOcr->edited();
}
/**  */
void QuiteInsane::slotEnableOcr(int i)
{
  if(mMode != Mode_ImageOcr) return;//to be sure
  QString qs;
  if(i)
  {
    mpEditOcr->show();
	  mViewerOcrMode = true;
    //enable ocrstart button
    mpOcrMenu->setItemChecked(ID_OCR_MODE,true);
    mpOcrMenu->setItemEnabled(ID_OCR_START,true);
    mpOcrMenu->setItemEnabled(ID_OCR_START_SELECTION,true);
    mpFileMenu->setItemEnabled(ID_FILE_SAVE_TEXT_AS,true);
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_TEXT,true);
    mpFileMenu->setItemEnabled(ID_FILE_PRINT_TEXT,true);
    if(mCopyAvailable)
    {
      mpEditMenu->setItemEnabled(ID_EDIT_CUT_TEXT,true);
      mpEditMenu->setItemEnabled(ID_EDIT_COPY_TEXT,true);
    }
    if(qApp->clipboard()->text() != QString::null)
      mpEditMenu->setItemEnabled(ID_EDIT_PASTE_TEXT,true);
    mpFilePrintText->setEnabled(true);
    mpFileLoadText->setEnabled(true);
    mpFileSaveText->setEnabled(true);
    adjustToolbar();
    mpSplitter->setSizes(mSplitterSize);
  }
  else
  {
    //save splitter size
    if(isVisible()) mSplitterSize = mpSplitter->sizes();
    //disable ocrstart button
    mpOcrMenu->setItemChecked(ID_OCR_MODE,false);
    mpOcrMenu->setItemEnabled(ID_OCR_START,false);
    mpOcrMenu->setItemEnabled(ID_OCR_START_SELECTION,false);
    mpFileMenu->setItemEnabled(ID_FILE_SAVE_TEXT_AS,false);
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_TEXT,false);
    mpFileMenu->setItemEnabled(ID_FILE_PRINT_TEXT,false);
    mpEditMenu->setItemEnabled(ID_EDIT_PASTE_TEXT,false);
    mpEditMenu->setItemEnabled(ID_EDIT_CUT_TEXT,false);
    mpEditMenu->setItemEnabled(ID_EDIT_COPY_TEXT,false);
    mpFilePrintText->setEnabled(false);
    mpFileLoadText->setEnabled(false);
    mpFileSaveText->setEnabled(false);
    mpEditOcr->hide();
	  mViewerOcrMode = false;
    adjustToolbar();
  }
}
/**  */
void QuiteInsane::slotStartOcr()
{
  ImageIOSupporter iosup;
  if(mMode != Mode_ImageOcr) return;//to be sure
//We must check, whether the current image has a format which
//can be read by gocr.
//If not, we save it in pnm format first
  if(mImageVectorIndex < 0)
    return;
  //Check whether we display a valid image at the moment
  QImage* image = mImageVector[mImageVectorIndex]->image();
  if(!image)
    return;
  if(image->isNull())
    return;
  if(iosup.saveImage(mpTempImageFile->name(),*image,"PPM"))
  {
    mpProgressStack->raiseWidget(0);
    startOcr(mpTempImageFile->name());
  }
}
/**  */
void QuiteInsane::slotStopOcr2()
{
  if(mMode != Mode_ImageOcr) return;//to be sure
  if(!mpOcrProcess) return;
  int n;
  n = 1;
  QString qs;
  mpProgressStack->raiseWidget(1);
  statusBar()->message(IDS_STATUS_DEFAULT);
  if(mpOcrProcess->normalExit())
  {
    if(!mpOcrProcess->exitStatus())
    {//we can load the result
      QFile f(mpTempOcrFile->name());
      if ( f.open(IO_ReadOnly) )
      {
        QTextStream t( &f );
        mOcrText = "";
        while ( !t.eof() )
        {
          mOcrText += t.readLine() + "\n";
        }
        f.close();
      }
      mpEditOcr->setText(mOcrText.latin1());
      mpEditOcr->setEdited(true);
    }
  }
  delete mpOcrProcess;
  mpOcrProcess = 0L;
  mpUnknownProgress->stop();
  mpFileOcrStop->setEnabled(false);
  mpFileLoadText->setEnabled(true);
  mpFileSaveText->setEnabled(true);
  mpFileOcrStart->setEnabled(true);
  mpFileOcr->setEnabled(true);
  mpFileSaveImage->setEnabled(true);
  mpFilePrint->setEnabled(true);
  mpFileLoadImage->setEnabled(true);
  mpFilePrintText->setEnabled(true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT,true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT_TEXT,true);
  mpFileMenu->setItemEnabled(ID_FILE_LOAD_TEXT,true);
  mpFileMenu->setItemEnabled(ID_FILE_LOAD_IMAGE,true);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_TEXT_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_IMAGE_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_LOAD_LAST_SCAN,true);
  mpOcrMenu->setItemEnabled(ID_OCR_MODE,true);
  mpOcrMenu->setItemEnabled(ID_OCR_START,true);
  mpOcrMenu->setItemEnabled(ID_OCR_STOP,false);
  mpToolsMoveSelection->setEnabled(true);
  mpToolsNewSelection->setEnabled(true);

  mpEditMenu->setEnabled(true);
  mpImageMenu->setEnabled(true);
  mpEditMenu->setEnabled(true);
  mpViewMenu->setEnabled(true);
  mpSettingsMenu->setEnabled(true);
  mpHelpMenu->setEnabled(true);

  mpFileMenu->setItemEnabled(ID_FILE_SAVE_SELECTION_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT_SELECTION,true);
  mpFileSaveImageSelection->setEnabled(true);
  mpFilePrintImageSelection->setEnabled(true);
  mpToolsSelectAll->setEnabled(true);
  mpFileOcrStartSelection->setEnabled(true);
  mpOcrMenu->setItemEnabled(ID_OCR_START_SELECTION,true);
  setCursor(Qt::arrowCursor);
  mpEditOcr->setCursor(Qt::arrowCursor);
}
/**  */
void QuiteInsane::slotStopOcr()
{
  if(mMode != Mode_ImageOcr) return;//to be sure
  if(!mpOcrProcess) return;
  QString qs;
  statusBar()->message(IDS_STATUS_DEFAULT);
  mpProgressStack->raiseWidget(1);
  if(mpOcrProcess->isRunning())
  {
    mpOcrProcess->kill();
  }
  delete mpOcrProcess;
  mpOcrProcess = 0L;
  mpUnknownProgress->stop();
  mpFileOcrStop->setEnabled(false);
  mpFileSaveText->setEnabled(true);
  mpFileLoadText->setEnabled(true);
  mpFileOcr->setEnabled(true);
  mpFileSaveImage->setEnabled(true);
  mpFileOcrStart->setEnabled(true);
  mpFilePrint->setEnabled(true);
  mpFileLoadImage->setEnabled(true);
  mpFilePrintText->setEnabled(true);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_TEXT_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_IMAGE_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_LOAD_TEXT,true);
  mpFileMenu->setItemEnabled(ID_FILE_LOAD_IMAGE,true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT,true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT_TEXT,true);
  mpFileMenu->setItemEnabled(ID_FILE_LOAD_LAST_SCAN,true);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_SELECTION_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT_SELECTION,true);
  mpFileSaveImageSelection->setEnabled(true);
  mpFilePrintImageSelection->setEnabled(true);
  mpToolsSelectAll->setEnabled(true);
  mpFileOcrStartSelection->setEnabled(true);
  mpOcrMenu->setItemEnabled(ID_OCR_START_SELECTION,true);
  mpOcrMenu->setItemEnabled(ID_OCR_MODE,true);
  mpOcrMenu->setItemEnabled(ID_OCR_START,true);
  mpOcrMenu->setItemEnabled(ID_OCR_STOP,false);
  mpToolsMoveSelection->setEnabled(true);
  mpToolsNewSelection->setEnabled(true);
  mpEditMenu->setEnabled(true);
  mpImageMenu->setEnabled(true);
  mpEditMenu->setEnabled(true);
  mpViewMenu->setEnabled(true);
  mpSettingsMenu->setEnabled(true);
  mpHelpMenu->setEnabled(true);

  setCursor(Qt::arrowCursor);
  mpEditOcr->setCursor(Qt::arrowCursor);
}
/**  */
void QuiteInsane::slotReceivedStdout()
{
  if(mpOcrProcess)
  {
    QString qs(mpOcrProcess->readStdout());
    qDebug(qs);
  }
}
/** */
void QuiteInsane::slotReceivedStderr()
{
  if(mpOcrProcess)
  {
    QString qs(mpOcrProcess->readStderr());
    qDebug(qs);
  }
}
/**  */
void QuiteInsane::slotPrint()
{
  if(!mImageVector[mImageVectorIndex]->image()) return;
  if(mImageVector[mImageVectorIndex]->image()->isNull()) return;

  QCopyPrint qcp(this,"",TRUE);
  qcp.setImage(mImageVector[mImageVectorIndex]->image(),true);
  qcp.exec();
}
/**  */
void QuiteInsane::slotZoomMenu(int id)
{
  unsigned int ui;
  double scalefactor;
  mpFileZoomCombo->setCurrentItem(mpZoomMenu->indexOf(id));
  for(ui = 0;ui<mpZoomMenu->count();ui++)
  {
    if(id != mpZoomMenu->idAt(ui))
    {
      mpZoomMenu->setItemChecked(mpZoomMenu->idAt(ui),false);
    }
    else
    {
      mpZoomMenu->setItemChecked(id,true);
    }
  }
  if(mImageVectorIndex >= 0)
    mImageVector[mImageVectorIndex]->setZoomString(mpFileZoomCombo->text(mpZoomMenu->indexOf(id)));
  scalefactor = mpFileZoomCombo->text(mpZoomMenu->indexOf(id)).toDouble()/100.0;
  mpView->scale(scalefactor);
}
/**  */
void QuiteInsane::slotZoomCombo(const QString&)
{
  slotZoomMenu(mpZoomMenu->idAt(mpFileZoomCombo->currentItem()));
}
/**  */
void QuiteInsane::slotCopyImage()
{
  if(mImageVectorIndex < 0)
    return;
  QImage* im = mImageVector[mImageVectorIndex]->image();
  if(!im)
    return;
  if(im->isNull())
    return;
  QClipboard *cb = qApp->clipboard();
  QImageDrag *d;
  d = new QImageDrag(*im,this);
  cb->setData(d);
}
/**  */
void QuiteInsane::slotHelpAboutQt()
{
  QMessageBox::aboutQt(this);
}
/**  */
void QuiteInsane::slotFileLoadImage()
{
  ImageIOSupporter iisup;
  int i;
  if(imageModified())
  {
    i = QMessageBox::warning(this,tr("Load image ..."),
          tr("The current image has not been saved.\n"
             "Do you want to save it now or discard it?\n"),
            tr("&Save"),tr("&Discard"),tr("&Cancel"));
	  if(i == 2) return;
    if(i == 0)
    {
      slotFileSaveImageAs();
      if(imageModified()) return;
    }
  }
  QString first_filter;

  first_filter = xmlConfig->stringValue("VIEWER_LOADIMAGE_FILTER","ALL_IMAGES");
  QStringList slist;
  slist = iisup.getOrderedInFilterList(first_filter);

  QString qs;

  qs = mViewerLoadImagePath;
  QFileDialogExt qfd(qs,QString::null,this,0,true);
  qfd.setViewMode((QFileDialog::ViewMode)mSingleFileViewMode);
  qfd.setCaption(tr("Load image"));
  qfd.setMode(QFileDialog::ExistingFile);
  qfd.setFilters(slist);

  if(qfd.exec())
  {
    QString load_filename = qfd.selectedFile();
    loadImage(load_filename);
    mViewerLoadImagePath = qfd.dir()->absPath();
    xmlConfig->setStringValue("VIEWER_LOADIMAGE_FILTER",
                              iisup.getDataFromInFilter(qfd.selectedFilter()));
  }
  mSingleFileViewMode = qfd.intViewMode();
  statusBar()->message(IDS_STATUS_DEFAULT);
}
/**  */
void QuiteInsane::slotCopyAvailable(bool b)
{
  mCopyAvailable = b;
  mpEditMenu->setItemEnabled(ID_EDIT_CUT_TEXT,b);
  mpEditMenu->setItemEnabled(ID_EDIT_COPY_TEXT,b);
}
/**  */
void QuiteInsane::loadText(QString qs)
{
  setCursor(Qt::waitCursor);
  QString qs2;
  QFile f(qs);
  if ( f.open(IO_ReadOnly) )
  {
    QTextStream t( &f );
    QString s;
    while ( !t.eof() )
    {
      s += t.readLine();
      s += "\n";
    }
    mOcrText = s;
    mpEditOcr->setText(mOcrText);
    f.close();
    mOriginalTextFilepath = QString::null;
    if(mMode == Mode_TextOnly)
    {
      qs2 = tr("QuiteInsane - Editor");
      qs2 +="[";
      qs2 += qs;
      qs2 +="]";
      setCaption(qs2);
    }
  }
  else
  {
     QMessageBox::warning(this,tr("Error"),
         tr("Could not open file.\n"),tr("OK"));	
  }
  setCursor(Qt::arrowCursor);
}
void QuiteInsane::slotFileSaveText()
{
  statusBar()->message(IDS_STATUS_DEFAULT);
  //If the filename is null or doesn't exist, open a dialog.
  if((mOriginalTextFilepath.isNull()) ||
     (!QFile::exists(mOriginalTextFilepath)))
  {
    slotFileSaveTextAs();
    return;
  }
  QFile qf(mOriginalTextFilepath);
  if(qf.open(IO_WriteOnly))
  {
    QTextStream ts(&qf);
    ts<<mpEditOcr->text();
    qf.close();
    mpEditOcr->setEdited(false);
  }
  else
  {
    //Couldn't open for writing; that normally means, that we don't
    //have write permission. Let the user select a filename.
    slotFileSaveTextAs();
  }
}
/**  */
void QuiteInsane::slotFileLoadText()
{
  QString load_filename;
  QString qs;
  int i;
  if(mpEditOcr->edited())
  {
    i = QMessageBox::warning(this,tr("Load text ..."),
          tr("The current text has not been saved.\n"
             "Do you want to save it now or discard it?\n"),
            tr("&Save"),tr("&Discard"),tr("&Cancel"));
	  if(i == 2) return;
    if(i == 0)
    {
      slotFileSaveTextAs();
      if(mpEditOcr->edited()) return;
    }
  }
  QFileDialogExt qfd(qs,0,this,0,true);
  qfd.setCaption(tr("Load text"));
  qfd.setMode(QFileDialog::ExistingFile);
  qfd.setViewMode((QFileDialog::ViewMode)mSingleFileViewMode);
  if(qfd.exec())
  {
    load_filename = qfd.selectedFile();
    loadText(load_filename);
    mpEditOcr->setEdited(false);
  }
  mSingleFileViewMode = qfd.intViewMode();
  statusBar()->message(IDS_STATUS_DEFAULT);
}
/**  */
void QuiteInsane::slotFileLoadLastScan()
{
  int i;
  if(imageModified())
  {
    i = QMessageBox::warning(this,tr("Load image ..."),
          tr("The current image has not been saved.\n"
             "Do you want to save it now or discard it?\n"),
            tr("&Save"),tr("&Discard"),tr("&Cancel"));
	  if(i == 2) return;
    if(i == 0)
    {
      slotFileSaveImageAs();
      if(imageModified()) return;
    }
  }
  loadImage(xmlConfig->absConfDirPath()+".scantemp.pnm");
  setImageModified(false);
}
/**  */
void QuiteInsane::moveEvent(QMoveEvent* e)
{
  QWidget::moveEvent(e);
  mViewerPosX = x();
  mViewerPosY = y();
}
/**  */
void QuiteInsane::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  //it's possible to get a resize-event, even if the widget
  //isn't visible; this would cause problems when we try to
  //restore the saved size/position
  if(isVisible())
  {
    mViewerSizeX = width();
    mViewerSizeY = height();
  }
  if(mpView)
    mpView->update();
}
/**  */
bool QuiteInsane::statusOk()
{
  if((mpTempImageFile->status() < 0) ||
     (mpTempOcrFile->status() < 0))
  {
    return false;
  }
  return true;
}
/**  */
void QuiteInsane::show()
{
  int x;
  int y;
  x = mViewerSizeX;
  y = mViewerSizeY;
  if(x<0) x=200;
  if(y<0) y=100;
  resize(x,y);
  x = mViewerPosX;
  y = mViewerPosY;
  if(x<0) x=0;
  if(y<0) y=0;
  move(x,y);
  QWidget::show();
}
/**  */
void QuiteInsane::slotNormalize()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  NormalizeDialog nd(pre_size,0,this);
  nd.setImage(&im);
  connect(&nd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &nd,SLOT(slotStop()));
  if(nd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(nd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  mFilterRunning = false;
  enableGui(true);
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotBrightnessContrast()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  BrightnessContrastDialog bcd(pre_size,0,this);
  bcd.setImage(&im);
  connect(&bcd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &bcd,SLOT(slotStop()));
  if(bcd.exec())
  {
    enableGui(false);
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    if(bcd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
qDebug("BrightnessContrats: image added to Queue");
    }
  }
  mFilterRunning = false;
  enableGui(true);
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
qDebug("BrightnessContrats: leaving");
}
/**  */
void QuiteInsane::slotGamma()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  GammaDialog gd(pre_size,0,this);
  gd.setImage(&im);
  connect(&gd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &gd,SLOT(slotStop()));
  if(gd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(gd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  mFilterRunning = false;
  enableGui(true);
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotPosterize()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  PosterizeDialog pd(pre_size,0,this);
  pd.setImage(&im);
  connect(&pd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &pd,SLOT(slotStop()));
  if(pd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(pd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  mFilterRunning = false;
  enableGui(true);
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}

/**  */
void QuiteInsane::slotInvert()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  InvertDialog id(pre_size,0,this);
  id.setImage(&im);
  if(id.exec())
  {
    if(mImageVector.size() == 0) addImageToQueue(&im);
    if(id.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
}
/**  */
void QuiteInsane::slotConvert()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  ImageConverterDialog id(pre_size,0,this);
  id.setImage(&im);
  if(id.exec())
  {
    if(mImageVector.size() == 0) addImageToQueue(&im);
    if(id.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
}
/**  */
void QuiteInsane::slotRotate()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  RotationDialog rd(pre_size,0,this);
  rd.setImage(&im);
  connect(&rd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &rd,SLOT(slotStop()));
  if(rd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(rd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  mFilterRunning = false;
  enableGui(true);
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotShear()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  ShearDialog sd(pre_size,0,this);
  sd.setImage(&im);
  connect(&sd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &sd,SLOT(slotStop()));
  if(sd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(sd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  mFilterRunning = false;
  enableGui(true);
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotScale()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  ScaleDialog sd(pre_size,0,this);
  sd.setImage(&im);
  if(sd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(sd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  enableGui(true);
  mFilterRunning = false;
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotSharpen()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  SharpenDialog sd(pre_size,0,this);
  sd.setImage(&im);
  connect(&sd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &sd,SLOT(slotStop()));
  if(sd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(sd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  enableGui(true);
  mFilterRunning = false;
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotTransparency()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  TransparencyDialog td(pre_size,0,this);
  td.setImage(&im);
  connect(&td,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &td,SLOT(slotStop()));
  if(td.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(td.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  enableGui(true);
  mFilterRunning = false;
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotOilPainting()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  OilPaintingDialog od(pre_size,0,this);
  od.setImage(&im);
  connect(&od,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &od,SLOT(slotStop()));
  if(od.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(od.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  enableGui(true);
  mFilterRunning = false;
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotDespeckle()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  DespeckleDialog dd(pre_size,0,this);
  dd.setImage(&im);
  connect(&dd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &dd,SLOT(slotStop()));
  if(dd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(dd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  enableGui(true);
  mFilterRunning = false;
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotBlurIIR()
{
  int pre_size;
  pre_size = xmlConfig->intValue("FILTER_PREVIEW_SIZE",150);
  QImage im = mImageVector[mImageVectorIndex]->image()->copy();
  GaussIIRBlurDialog bd(pre_size,0,this);
  bd.setImage(&im);
  connect(&bd,SIGNAL(signalFilterProgress(int)),
          mpFilterProgress,SLOT(setProgress(int)));
  connect(mpFilterButton,SIGNAL(clicked()),
          &bd,SLOT(slotStop()));
  if(bd.exec())
  {
    mpFilterHBox->setEnabled(true);
    if(mImageVector.size() == 0) addImageToQueue(&im);
    mFilterRunning = true;
    enableGui(false);
    if(bd.apply(&im))
    {
      addImageToQueue(&im);
      mpView->setImage(mImageVector[mImageVectorIndex]->image());
      mpView->updatePixmap();
      mImageUserModified = true;
    }
  }
  enableGui(true);
  mFilterRunning = false;
  mpFilterProgress->reset();
  mpFilterHBox->setEnabled(false);
}
/**  */
void QuiteInsane::slotUndo()
{
  mImageVector[mImageVectorIndex]->setPercentSize(mpView->tlxPercent(),
                                                  mpView->tlyPercent(),
                                                  mpView->brxPercent(),
                                                  mpView->bryPercent());
  if(mImageVectorIndex > 0)
  {
   mImageVectorIndex -= 1;
   mpView->setImage(mImageVector[mImageVectorIndex]->image());
  }
  mpView->setRectPercent(mImageVector[mImageVectorIndex]->tlx(),
                         mImageVector[mImageVectorIndex]->tly(),
                         mImageVector[mImageVectorIndex]->brx(),
                         mImageVector[mImageVectorIndex]->bry());


  mpEditMenu->setItemEnabled(ID_EDIT_UNDO,undoAvailable());
  mpEditMenu->setItemEnabled(ID_EDIT_REDO,redoAvailable());
  mpFileUndoImage->setEnabled(undoAvailable());
  mpFileRedoImage->setEnabled(redoAvailable());
  mTempFileValid = false;
  if(mSaveIndex != mImageVectorIndex)
    mImageUserModified = true;
  else
    mImageUserModified = false;
  restoreZoomFactor();
}
/**  */
void QuiteInsane::slotRedo()
{
  if(mImageVectorIndex < 0) return;
  mImageVector[mImageVectorIndex]->setPercentSize(mpView->tlxPercent(),
                                                  mpView->tlyPercent(),
                                                  mpView->brxPercent(),
                                                  mpView->bryPercent());
  if(mImageVectorIndex < int(mImageVector.size())-1)
  {
   mImageVectorIndex += 1;
   mpView->setImage(mImageVector[mImageVectorIndex]->image());
  }
  mpView->setRectPercent(mImageVector[mImageVectorIndex]->tlx(),
                         mImageVector[mImageVectorIndex]->tly(),
                         mImageVector[mImageVectorIndex]->brx(),
                         mImageVector[mImageVectorIndex]->bry());

  mpEditMenu->setItemEnabled(ID_EDIT_UNDO,undoAvailable());
  mpEditMenu->setItemEnabled(ID_EDIT_REDO,redoAvailable());
  mpFileUndoImage->setEnabled(undoAvailable());
  mpFileRedoImage->setEnabled(redoAvailable());
  mTempFileValid = false;
  if(mSaveIndex != mImageVectorIndex)
    mImageUserModified = true;
  else
    mImageUserModified = false;
  restoreZoomFactor();
}
/**  */
void QuiteInsane::addImageToQueue(QImage* image)
{
  ImageBuffer* imagebuffer = new ImageBuffer();
  if(!imagebuffer)
    return;
  QImage* new_image = new QImage();
  if(!new_image)
  {
    delete imagebuffer;
    return;
  }
  *new_image = image->copy();
  imagebuffer->setImage(new_image);
  imagebuffer->setPercentSize(mpView->tlxPercent(),
                              mpView->tlyPercent(),
                              mpView->brxPercent(),
                              mpView->bryPercent());
  imagebuffer->setZoomString(mpFileZoomCombo->currentText());
  if(mImageVectorIndex < 0)
    mSaveIndex = 0;
  if((mImageVectorIndex > - 1) &&
     (mImageVectorIndex  < int(mImageVector.size()) - 1))
  {
    //This indicates, that the user has chosen undo before.
    //We delete the images after the current index.
    for(int i=mImageVectorIndex+1;i<int(mImageVector.size())-1;i++)
      mImageVector.remove(i);
    if(mImageVector.resize(mImageVectorIndex+2))
      mImageVector.insert(mImageVector.size()-1,imagebuffer);
    mImageVectorIndex = mImageVector.size()-1;
  }
  else if(mImageVectorIndex  < 0)
  {
    if(mImageVector.resize(mImageVector.size()+1))
    {
      mImageVector.insert(mImageVector.size()-1,imagebuffer);
      mImageVectorIndex = mImageVector.size()-1;
    }
  }
  else if(mImageVectorIndex == int(mImageVector.size()) - 1)
  {
    //If vector contains mUndoSteps elements already,
    //remove first (==oldest) element, reorder remaining elements and
    //add image.
    //mImageVectorIndex == -1 or == mImageVector.size()-1
    if(int(mImageVector.size()) == mUndoSteps)
    {
      mImageVector.remove(0);
      for(int i=0;i<int(mImageVector.size())-1;i++)
      {
        ImageBuffer* ib = mImageVector.take(i+1);
        mImageVector.insert(i,ib);
      }
      mImageVector.insert(mImageVector.size()-1,imagebuffer);
      mImageVectorIndex = mImageVector.size()-1;
    }
    else if(mImageVector.resize(mImageVector.size()+1))
    {
      mImageVector.insert(mImageVector.size()-1,imagebuffer);
      mImageVectorIndex = mImageVector.size()-1;
    }
  }
  else
    mImageVectorIndex = -1;
  mpEditMenu->setItemEnabled(ID_EDIT_UNDO,undoAvailable());
  mpEditMenu->setItemEnabled(ID_EDIT_REDO,redoAvailable());
  mpFileUndoImage->setEnabled(undoAvailable());
  mpFileRedoImage->setEnabled(redoAvailable());
  mTempFileValid = false;
}
/**  */
bool QuiteInsane::undoAvailable()
{
  if(mImageVectorIndex < 1)
    return false;
  return true;
}
/**  */
bool QuiteInsane::redoAvailable()
{
  if((mImageVectorIndex == -1) ||
     (mImageVectorIndex == int(mImageVector.size()-1)))
    return false;
  return true;
}
/**  */
void QuiteInsane::slotToolBarPositionChanged(QToolBar* toolbar)
{
  ToolBarDock dock;
  int index;
  int extra_offset;
  bool nl;

  if(getLocation(toolbar,dock,index,nl,extra_offset ))
  {
    if(toolbar == mpTextToolbar)
    {
      mTextDock = dock;
      mTextIndex = index;
      mTextNl = nl;
      mTextExtraOffset = extra_offset;
    }
    else if(toolbar == mpImageToolbar)
    {
      if(mViewerOcrMode)
      {
        mImageDock = dock;
        mImageIndex = index;
        mImageNl = nl;
        mImageExtraOffset = extra_offset;
      }
      else
      {
        mImageDockOcrOff = dock;
        mImageIndexOcrOff = index;
        mImageNlOcrOff = nl;
        mImageExtraOffsetOcrOff = extra_offset;
      }
    }
    else if(toolbar == mpToolsToolbar)
    {
      if(mViewerOcrMode)
      {
        mToolDock = dock;
        mToolIndex = index;
        mToolNl = nl;
        mToolExtraOffset = extra_offset;
      }
      else
      {
        mToolDockOcrOff = dock;
        mToolIndexOcrOff = index;
        mToolNlOcrOff = nl;
        mToolExtraOffsetOcrOff = extra_offset;
      }
    }
  }
}
/**  */
void QuiteInsane::clearImageQueue()
{
  mImageVector.clear();
//  if(mImageVector.size()>0)
//  {
//    for(unsigned int i=0;i<mImageVector.size();i++)
//      if(mImageVector[i]) delete mImageVector[i];
//    mImageVector.resize(0);
//  }
  mImageVectorIndex = -1;
  mpFileUndoImage->setEnabled(false);
  mpFileRedoImage->setEnabled(false);
}
/**  */
void QuiteInsane::slotSelectAll()
{
  mpView->setRectPercent(0.0,0.0,1.0,1.0);
  mImageVector[mImageVectorIndex]->setPercentSize(0.0,0.0,1.0,1.0);
}
/**  */
void QuiteInsane::slotFileSaveSelectionAs()
{
  QString qs;
  if(mMode != Mode_ImageOcr) return;//to be sure
  if(mImageVectorIndex < 0)
    return;
  //Check whether we display a valid image at the moment
  QImage image = mImageVector[mImageVectorIndex]->selectedImage();
  //Check whether we display a valid image at the moment
  if(!image.isNull())
  {
    //create a QFileDialog
    QPreviewFileDialog qpfd(true,false,this,0,TRUE);
    qpfd.setMode(QFileDialog::AnyFile);
    qpfd.setCaption(tr("Save image selection as..."));
    qpfd.setImage(&image);
    if(qpfd.exec())
      emit signalImageSaved(qpfd.selectedFile());
  }
  else
    QMessageBox::information(this,tr("Save image selection as..."),
                 tr("There is no image, which could be saved."),tr("&OK"));
  statusBar()->message(IDS_STATUS_DEFAULT);
}
/**  */
void QuiteInsane::slotFilePrintSelection()
{
  QImage image = mImageVector[mImageVectorIndex]->selectedImage();
  if(image.isNull())
    return;
  QCopyPrint qcp(this,"",TRUE);
  qcp.setImage(&image,true);
  qcp.exec();
}
/**  */
void QuiteInsane::slotStartOcrSelection()
{
  ImageIOSupporter iosup;
  if(mImageVectorIndex < 0)
    return;
  //Check whether we display a valid image at the moment
  QImage image = mImageVector[mImageVectorIndex]->selectedImage();
  if(image.isNull())
    return;
  if(iosup.saveImage(mpTempImageFile->name(),image,"PPM"))
  {
    mpProgressStack->raiseWidget(0);
    startOcr(mpTempImageFile->name());
    return;
  }
}
/**  */
void QuiteInsane::startOcr(QString imagepath)
{
#ifndef USE_QT3
  mpOcrProcess = new QProcessBackport(this);
#else
  mpOcrProcess = new QProcess(this);
#endif
  connect(mpOcrProcess,SIGNAL(processExited()),
          this,SLOT(slotStopOcr2()));
  connect(mpOcrProcess,SIGNAL(readyReadStdout()),
          this,SLOT(slotReceivedStdout()));
  connect(mpOcrProcess,SIGNAL(readyReadStderr()),
          this,SLOT(slotReceivedStderr()));
  QStringList al = QStringList::split(" ",
                                      xmlConfig->stringValue("GOCR_COMMAND"),
                                      false);
  for(QStringList::Iterator it = al.begin(); it != al.end(); ++it )
  {
    if((*it).left(2) != "-o")
    {
      mpOcrProcess->addArgument(*it);
    }
    else
      break;
  }
  mpOcrProcess->addArgument(imagepath);
  mpOcrProcess->addArgument("-o");
  mpOcrProcess->addArgument(mpTempOcrFile->name());
  statusBar()->message(tr("Optical character recognition..."));
  if(mpOcrProcess->start())
  {
    mpUnknownProgress->start(40);
    setCursor(Qt::waitCursor);
    mpEditOcr->setCursor(Qt::waitCursor);
    //file menu + toolbar
    mpFileOcrStop->setEnabled(true);
    mpFileSaveText->setEnabled(false);
    mpFileLoadText->setEnabled(false);
    mpFileLoadImage->setEnabled(false);
    mpFileSaveImage->setEnabled(false);
    mpFilePrint->setEnabled(false);
    mpFilePrintText->setEnabled(false);
    mpFileOcr->setEnabled(false);
    mpFileOcrStart->setEnabled(false);
    mpFileOcrStartSelection->setEnabled(false);
    mpFileSaveImageSelection->setEnabled(false);
    mpFilePrintImageSelection->setEnabled(false);
    mpFileMenu->setItemEnabled(ID_FILE_PRINT,false);
    mpFileMenu->setItemEnabled(ID_FILE_PRINT_TEXT,false);
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_TEXT,false);
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_IMAGE,false);
    mpFileMenu->setItemEnabled(ID_FILE_SAVE_TEXT_AS,false);
    mpFileMenu->setItemEnabled(ID_FILE_SAVE_IMAGE_AS,false);
    mpFileMenu->setItemEnabled(ID_FILE_SAVE_SELECTION_AS,false);
    mpFileMenu->setItemEnabled(ID_FILE_PRINT_SELECTION,false);
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_LAST_SCAN,false);
    mpFileMenu->setItemEnabled(ID_FILE_SAVE_SELECTION_AS,false);
    mpFileMenu->setItemEnabled(ID_FILE_PRINT_SELECTION,false);
    //edit menu + toolbar
    mpEditMenu->setEnabled(false);
    mpImageMenu->setEnabled(false);
    mpEditMenu->setEnabled(false);
    mpViewMenu->setEnabled(false);
    mpSettingsMenu->setEnabled(false);
    mpHelpMenu->setEnabled(false);

    mpOcrMenu->setItemEnabled(ID_OCR_MODE,false);
    mpOcrMenu->setItemEnabled(ID_OCR_START,false);
    mpOcrMenu->setItemEnabled(ID_OCR_START_SELECTION,false);
    mpOcrMenu->setItemEnabled(ID_OCR_STOP,true);
    mpToolsMoveSelection->setEnabled(false);
    mpToolsNewSelection->setEnabled(false);
    mpToolsSelectAll->setEnabled(false);
  }
  else
  {
    delete mpOcrProcess;
    mpOcrProcess = 0L;
  }
}
/**  */
void QuiteInsane::slotToolMode(int id)
{
  //move selection
  if(id == 0)
  {
    mMoveSelection = true;
    mpView->setMode(id);
    mpEditMenu->setItemChecked(ID_EDIT_MOVE_IMAGE_SELECTION,true);
    mpEditMenu->setItemChecked(ID_EDIT_NEW_IMAGE_SELECTION,false);
  }
  //new selection
  else if(id == 1)
  {
    mMoveSelection = false;
    mpView->setMode(id);
    mpEditMenu->setItemChecked(ID_EDIT_MOVE_IMAGE_SELECTION,false);
    mpEditMenu->setItemChecked(ID_EDIT_NEW_IMAGE_SELECTION,true);
  }
  mpFileSaveImageSelection->setEnabled(true);
  mpFilePrintImageSelection->setEnabled(true);
  mpToolsSelectAll->setEnabled(true);
  mpFileOcrStartSelection->setEnabled(true);
  mpOcrMenu->setItemEnabled(ID_OCR_START_SELECTION,true);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_SELECTION_AS,true);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT_SELECTION,true);
  mpEditMenu->setItemEnabled(ID_EDIT_SELECT_ALL_IMAGE,true);
}
/**  */
void QuiteInsane::slotCopyImageSelection()
{
  if(mImageVectorIndex < 0)
    return;
  QImage im = mImageVector[mImageVectorIndex]->selectedImage();
  if(im.isNull())
    return;
  QClipboard *cb = qApp->clipboard();
  QImageDrag *d;
  d = new QImageDrag(im,this);
  cb->setData(d);
}
/**  */
void QuiteInsane::slotMoveSelection()
{
  mpToolButtonGroup->setButton(0);
  slotToolMode(0);
}
/**  */
void QuiteInsane::slotNewSelection()
{
  mpToolButtonGroup->setButton(1);
  slotToolMode(1);
}
/**  */
void QuiteInsane::slotAboutToShowEditMenu()
{
  if((qApp->clipboard()->text() != QString::null) &&
     (mpEditOcr->isVisible()))
    mpEditMenu->setItemEnabled(ID_EDIT_PASTE_TEXT,true);
  mpEditMenu->setItemEnabled(ID_EDIT_UNDO_TEXT,mpFileUndoText->isEnabled());
  mpEditMenu->setItemEnabled(ID_EDIT_REDO_TEXT,mpFileRedoText->isEnabled());
}
/**  */
void QuiteInsane::slotAboutToShowFileMenu()
{
  if(QFile::exists(xmlConfig->absConfDirPath()+".scantemp.pnm"))
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_LAST_SCAN,true);
  else
    mpFileMenu->setItemEnabled(ID_FILE_LOAD_LAST_SCAN,false);
}
/**  */
void QuiteInsane::createIcons()
{
  if(mMode == Mode_ImageOcr)
  {
	  mpOpenImageIcon = new QIconSet(QPixmap((const char **)open_image_xpm));
    mpSaveImageIcon = new QIconSet(QPixmap((const char **)save_image_xpm));
    mpPrintImageIcon = new QIconSet(QPixmap((const char **)print_image_xpm));
    mpSaveSelectionIcon = new QIconSet(QPixmap((const char **)saveselection_xpm));
    mpPrintSelectionIcon = new QIconSet(QPixmap((const char **)printselection_xpm));
    mpUndoImageIcon = new QIconSet(QPixmap((const char **)undo_xpm));
    mpRedoImageIcon = new QIconSet(QPixmap((const char **)redo_xpm));
    mpUndoTextIcon = new QIconSet(QPixmap((const char **)undo_text_xpm));
    mpRedoTextIcon = new QIconSet(QPixmap((const char **)redo_text_xpm));
    mpOcrIconOn = new QIconSet(QPixmap((const char **)splitterwindow_xpm));
	  mpMoveSelectionIcon = new QIconSet(QPixmap((const char **)moveselection_xpm));
	  mpNewSelectionIcon = new QIconSet(QPixmap((const char **)newselection_xpm));
	  mpSelectAllIcon = new QIconSet(QPixmap((const char **)selectall_xpm));
	  mpOpenTextIcon = new QIconSet(QPixmap((const char **)open_text_xpm));
    mpSaveTextIcon = new QIconSet(QPixmap((const char **)save_text_xpm));
    mpPrintTextIcon = new QIconSet(QPixmap((const char **)print_text_xpm));
    mpOcrStartIcon = new QIconSet(QPixmap((const char **)startocr_xpm));
    mpOcrSelectionIcon = new QIconSet(QPixmap((const char **)ocrselection_xpm));
    mpOcrIconStop = new QIconSet(QPixmap((const char **)stop_xpm));
  }
  else
  {
    mpOpenTextIcon = new QIconSet(QPixmap((const char **)open_text_xpm));
    mpSaveTextIcon = new QIconSet(QPixmap((const char **)save_text_xpm));
    mpPrintTextIcon = new QIconSet(QPixmap((const char **)print_text_xpm));
    mpUndoTextIcon = new QIconSet(QPixmap((const char **)undo_text_xpm));
    mpRedoTextIcon = new QIconSet(QPixmap((const char **)redo_text_xpm));
  }
}
/**  */
void QuiteInsane::deleteIcons()
{
  if(mMode == Mode_ImageOcr)
  {
	  if(mpOpenImageIcon) delete mpOpenImageIcon;
    if(mpSaveImageIcon) delete mpSaveImageIcon;
    if(mpPrintImageIcon) delete mpPrintImageIcon;
    if(mpSaveSelectionIcon) delete mpSaveSelectionIcon;
    if(mpPrintSelectionIcon) delete mpPrintSelectionIcon;
    if(mpUndoImageIcon) delete mpUndoImageIcon;
    if(mpRedoImageIcon) delete mpRedoImageIcon;
    if(mpUndoTextIcon) delete mpUndoTextIcon;
    if(mpRedoTextIcon) delete mpRedoTextIcon;
    if(mpOcrIconOn) delete mpOcrIconOn;
	  if(mpMoveSelectionIcon) delete mpMoveSelectionIcon;
	  if(mpNewSelectionIcon) delete mpNewSelectionIcon;
	  if(mpSelectAllIcon) delete mpSelectAllIcon;
	  if(mpOpenTextIcon) delete mpOpenTextIcon;
    if(mpSaveTextIcon) delete mpSaveTextIcon;
    if(mpPrintTextIcon) delete mpPrintTextIcon;
    if(mpOcrStartIcon) delete mpOcrStartIcon;
    if(mpOcrSelectionIcon) delete mpOcrSelectionIcon;
    if(mpOcrIconStop) delete mpOcrIconStop;
  }
  else
  {
    if(mpOpenTextIcon) delete mpOpenTextIcon;
    if(mpSaveTextIcon) delete mpSaveTextIcon;
    if(mpPrintTextIcon) delete  mpPrintTextIcon;
    if(mpUndoTextIcon) delete mpUndoTextIcon;
    if(mpRedoTextIcon) delete mpRedoTextIcon;
  }
}
/**  */
void QuiteInsane::slotFileSaveImage()
{
  ImageIOSupporter iosup;
  QString format;
  if((mOriginalImageFilepath.isNull()) ||
     (!QFile::exists(mOriginalImageFilepath)))
  {
    slotFileSaveImageAs();
    return;
  }
  //Check whether we display a valid image at the moment
  QImage* image = mImageVector[mImageVectorIndex]->image();
  if(image)
  {
    if(!image->isNull())
    {
      format = QImage::imageFormat(mOriginalImageFilepath);
      if(iosup.saveImage(mOriginalImageFilepath,*image,format))
      {
        mImageModified = false;
        mImageUserModified = false;
        mSaveIndex = mImageVectorIndex;
        emit signalImageSaved(mOriginalImageFilepath);
      }
      else
        mImageModified = true;
      statusBar()->message(IDS_STATUS_DEFAULT);
      return;
    }
  }
}
/**  */
void QuiteInsane::setImageViewerState(bool image_loaded)
{
  if(mMode != Mode_ImageOcr) return;
  //file menu + toolbar
  mpFileOcrStop->setEnabled(image_loaded ^ true);
  mpFileSaveImage->setEnabled(image_loaded);
  mpFilePrint->setEnabled(image_loaded);
  mpFileOcr->setEnabled(image_loaded);
  mpFileOcrStart->setEnabled(image_loaded);
  mpFileOcrStartSelection->setEnabled(image_loaded);
  mpFileOcrStop->setEnabled(false);
  mpFileSaveImageSelection->setEnabled(image_loaded);
  mpFilePrintImageSelection->setEnabled(image_loaded);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT,image_loaded);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_IMAGE,image_loaded);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_IMAGE_AS,image_loaded);
  mpFileMenu->setItemEnabled(ID_FILE_SAVE_SELECTION_AS,image_loaded);
  mpFileMenu->setItemEnabled(ID_FILE_PRINT_SELECTION,image_loaded);

  mpEditMenu->setItemEnabled(ID_EDIT_COPY_IMAGE,image_loaded);
  mpEditMenu->setItemEnabled(ID_EDIT_COPY_IMAGE_SELECTION,image_loaded);
  mpEditMenu->setItemEnabled(ID_EDIT_SELECT_ALL_IMAGE,image_loaded);
  mpEditMenu->setItemEnabled(ID_EDIT_MOVE_IMAGE_SELECTION,image_loaded);
  mpEditMenu->setItemEnabled(ID_EDIT_NEW_IMAGE_SELECTION,image_loaded);

  mpImageMenu->setEnabled(image_loaded);
  mpOcrMenu->setEnabled(image_loaded);
  mpToolsMoveSelection->setEnabled(image_loaded);
  mpToolsNewSelection->setEnabled(image_loaded);
  mpToolsSelectAll->setEnabled(image_loaded);
  //zoom menu + toolbar
  mpZoomMenu->setEnabled(image_loaded);
  mpFileZoomCombo->setEnabled(image_loaded);
}
/**  */
void QuiteInsane::slotReplaceImageWithSelection()
{
  QRect rect = mpView->selectedRect();
  QImage* image = mImageVector[mImageVectorIndex]->image();
  if(!image)
    return;
  QImage im = image->copy(rect);
  im.setDotsPerMeterX(image->dotsPerMeterX());
  im.setDotsPerMeterY(image->dotsPerMeterY());
  if(!im.isNull())
  {
    addImageToQueue(&im);
    mpView->setImage(mImageVector[mImageVectorIndex]->image());
    mpView->updatePixmap();
    mImageUserModified = true;
    slotSelectAll();
  }
}
/**  */
void QuiteInsane::adjustToolbar()
{
  if(mMode != Mode_ImageOcr) return;//to be sure
  //if toolbar is hidden, return
  if(!mpViewMenu->isItemChecked(ID_VIEW_TOOLBAR)) return;
  if(mViewerOcrMode)
  {
    mpTextToolbar->show();
    moveToolBar(mpImageToolbar,mImageDock,mImageNl,mImageIndex,mImageExtraOffset);
    moveToolBar(mpTextToolbar,mTextDock,mTextNl,mTextIndex,mTextExtraOffset);
    moveToolBar(mpToolsToolbar,mToolDock,mToolNl,mToolIndex,mToolExtraOffset);
  }
  else
  {
    mpTextToolbar->hide();
    moveToolBar(mpImageToolbar,mImageDockOcrOff,mImageNlOcrOff,
                mImageIndexOcrOff,mImageExtraOffsetOcrOff);
    moveToolBar(mpToolsToolbar,mToolDockOcrOff,mToolNlOcrOff,
                mToolIndexOcrOff,mToolExtraOffsetOcrOff);
  }
}
/**  */
void QuiteInsane::loadConfig()
{
  if(mMode == Mode_ImageOcr)
  {
    mSingleFileViewMode = xmlConfig->intValue("SINGLEFILE_VIEW_MODE");
    mSplitterSize = xmlConfig->intValueList("VIEWER_SPLITTER_SIZE");
    mViewerSaveTextPath = xmlConfig->stringValue("VIEWER_SAVETEXT_PATH");
    //switch to last ocr mode
    mViewerOcrMode = xmlConfig->boolValue("VIEWER_OCR_MODE");
    mSplitterSize = xmlConfig->intValueList("VIEWER_SPLITTER_SIZE");

    mTextDock = (QMainWindow::ToolBarDock)xmlConfig->intValue("TEXTTOOLBAR_DOCK");
    mTextIndex = xmlConfig->intValue("TEXTTOOLBAR_INDEX");
    mTextNl = xmlConfig->boolValue("TEXTTOOLBAR_NL");
    mTextExtraOffset = xmlConfig->intValue("TEXTTOOLBAR_EXTRA_OFFSET");

    mImageDock = (QMainWindow::ToolBarDock)xmlConfig->intValue("IMAGETOOLBAR_DOCK");
    mImageIndex = xmlConfig->intValue("IMAGETOOLBAR_INDEX");
    mImageNl = xmlConfig->boolValue("IMAGETOOLBAR_NL");
    mImageExtraOffset = xmlConfig->intValue("IMAGETOOLBAR_EXTRA_OFFSET");

    mToolDock = (QMainWindow::ToolBarDock)xmlConfig->intValue("TOOLSTOOLBAR_DOCK");
    mToolIndex = xmlConfig->intValue("TOOLSTOOLBAR_INDEX");
    mToolNl = xmlConfig->boolValue("TOOLSTOOLBAR_NL");
    mToolExtraOffset = xmlConfig->intValue("TOOLSTOOLBAR_EXTRA_OFFSET");

    mImageDockOcrOff = (QMainWindow::ToolBarDock)xmlConfig->intValue("IMAGETOOLBAR_DOCK_OCROFF");
    mImageIndexOcrOff = xmlConfig->intValue("IMAGETOOLBAR_INDEX_OCROFF");
    mImageNlOcrOff = xmlConfig->boolValue("IMAGETOOLBAR_NL_OCROFF");
    mImageExtraOffsetOcrOff = xmlConfig->intValue("IMAGETOOLBAR_EXTRA_OFFSET_OCROFF");

    mToolDockOcrOff = (QMainWindow::ToolBarDock)xmlConfig->intValue("TOOLSTOOLBAR_DOCK_OCROFF");
    mToolIndexOcrOff = xmlConfig->intValue("TOOLSTOOLBAR_INDEX_OCROFF");
    mToolNlOcrOff = xmlConfig->boolValue("TOOLSTOOLBAR_NL_OCROFF");
    mToolExtraOffsetOcrOff = xmlConfig->intValue("TOOLSTOOLBAR_EXTRA_OFFSET_OCROFF");

    mUndoSteps = xmlConfig->intValue("VIEWER_UNDO_STEPS");

    mViewerLoadImagePath = xmlConfig->stringValue("VIEWER_LOADIMAGE_PATH");
    mViewerSaveTextPath = xmlConfig->stringValue("VIEWER_SAVETEXT_PATH");
  }
  mViewerSizeX = xmlConfig->intValue("VIEWER_SIZE_X",500);
  mViewerSizeY = xmlConfig->intValue("VIEWER_SIZE_Y",500);
  mViewerPosX = xmlConfig->intValue("VIEWER_POS_X",0);
  mViewerPosY = xmlConfig->intValue("VIEWER_POS_Y",0);
}
/**  */
void QuiteInsane::slotSaveConfig()
{
//save toolbar position
  if(mMode == Mode_ImageOcr)
  {
    if(mViewerOcrMode)
    {
      mSplitterSize = mpSplitter->sizes();
      xmlConfig->setBoolValue("VIEWER_OCR_MODE",true);
      xmlConfig->setIntValueList("VIEWER_SPLITTER_SIZE",mSplitterSize);
      if(mpTextToolbar->isVisible())
      {
        xmlConfig->setIntValue("TEXTTOOLBAR_DOCK",int(mTextDock));
        xmlConfig->setIntValue("TEXTTOOLBAR_INDEX",mTextIndex);
        xmlConfig->setBoolValue("TEXTTOOLBAR_NL",mTextNl);
        xmlConfig->setIntValue("TEXTTOOLBAR_EXTRA_OFFSET",mTextExtraOffset);
      }
      if(mpImageToolbar->isVisible())
      {
        xmlConfig->setIntValue("IMAGETOOLBAR_DOCK",int(mImageDock));
        xmlConfig->setIntValue("IMAGETOOLBAR_INDEX",mImageIndex);
        xmlConfig->setBoolValue("IMAGETOOLBAR_NL",mImageNl);
        xmlConfig->setIntValue("IMAGETOOLBAR_EXTRA_OFFSET",mImageExtraOffset);
      }
      if(mpToolsToolbar->isVisible())
      {
         xmlConfig->setIntValue("TOOLSTOOLBAR_DOCK",int(mToolDock));
         xmlConfig->setIntValue("TOOLSTOOLBAR_INDEX",mToolIndex);
         xmlConfig->setBoolValue("TOOLSTOOLBAR_NL",mToolNl);
         xmlConfig->setIntValue("TOOLSTOOLBAR_EXTRA_OFFSET",mToolExtraOffset);
      }
    }
    else
    {
      xmlConfig->setBoolValue("VIEWER_OCR_MODE",false);
      if(mpImageToolbar->isVisible())
      {
        xmlConfig->setIntValue("IMAGETOOLBAR_DOCK_OCROFF",int(mImageDockOcrOff));
        xmlConfig->setIntValue("IMAGETOOLBAR_INDEX_OCROFF",mImageIndexOcrOff);
        xmlConfig->setBoolValue("IMAGETOOLBAR_NL_OCROFF",mImageNlOcrOff);
        xmlConfig->setIntValue("IMAGETOOLBAR_EXTRA_OFFSET_OCROFF",mImageExtraOffsetOcrOff);
      }
      if(mpToolsToolbar->isVisible())
      {
         xmlConfig->setIntValue("TOOLSTOOLBAR_DOCK_OCROFF",int(mToolDockOcrOff));
         xmlConfig->setIntValue("TOOLSTOOLBAR_INDEX_OCROFF",mToolIndexOcrOff);
         xmlConfig->setBoolValue("TOOLSTOOLBAR_NL_OCROFF",mToolNlOcrOff);
         xmlConfig->setIntValue("TOOLSTOOLBAR_EXTRA_OFFSET_OCROFF",mToolExtraOffsetOcrOff);
      }
    }
    xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",mSingleFileViewMode);
    xmlConfig->setIntValue("VIEWER_SIZE_X",mViewerSizeX);
    xmlConfig->setIntValue("VIEWER_SIZE_Y",mViewerSizeY);
    xmlConfig->setIntValue("VIEWER_POS_X",mViewerPosX);
    xmlConfig->setIntValue("VIEWER_POS_Y",mViewerPosY);
    xmlConfig->setStringValue("VIEWER_LOADIMAGE_PATH",mViewerLoadImagePath);
  }
}
/** No descriptions */
void QuiteInsane::updateZoomMenu()
{
  int zoomfactors[] = {10,20,30,40,50,60,70,80,90,100,125,150,
                       175,200,250,300,400,500,600,700,800,900,1000};
  QString qs;
  if(mImageVectorIndex < 0)
    return;
  QImage* image = mImageVector[mImageVectorIndex]->image();
  if(!image)
    return;
  double w = double(image->width());
  double h = double(image->height());
  mpZoomMenu->clear();
  mpZoomMenu->setCheckable(true);
  mpFileZoomCombo->clear();
  for(int i=0;i<23;i++)
  {
    qs.setNum(zoomfactors[i]);
    qs += " %";
    double fac = double(zoomfactors[i])/100.0;
    if(((w * fac >= 1.0) && (w * fac < 32767.0) &&
       (h * fac >= 1.0) && (h * fac < 32767.0)) || (i == 9))
    {
      mpZoomMenu->insertItem(qs,-1,i);
      mpFileZoomCombo->insertItem(qs);
    }
  }
}
/** No descriptions */
void QuiteInsane::restoreZoomFactor()
{
  if(mImageVectorIndex < 0)
    return;
  unsigned int ui;
  int id = -1;
  QString qs;
//  mpFileZoomCombo->setCurrentItem(mpZoomMenu->indexOf(id));
  for(ui = 0;ui<mpZoomMenu->count();ui++)
  {
    id = mpZoomMenu->idAt(ui);
    qs = mpFileZoomCombo->text(ui);
    if(qs == mImageVector[mImageVectorIndex]->zoomString().left(qs.length()))
      break;
  }
  if(id != -1)
  {
    mpFileZoomCombo->setCurrentItem(int(ui));
    slotZoomMenu(id);
  }
  else
  {
    set100PercentZoom();
  }
}
/** No descriptions */
void QuiteInsane::set100PercentZoom()
{
  unsigned int ui;
  int id = 0;
  QString qs;
  for(ui = 0;ui<mpZoomMenu->count();ui++)
  {
    id = mpZoomMenu->idAt(ui);
    qs = mpFileZoomCombo->text(ui);
    if(qs.left(3) == "100")
      break;
  }
  if(id != -1)
  {
    mpFileZoomCombo->setCurrentItem(int(ui));
    slotZoomMenu(id);
  }
}
/** No descriptions */
void QuiteInsane::slotRectangleChanged()
{
  if(mImageVectorIndex < 0)
    return;
  mImageVector[mImageVectorIndex]->setTlx(mpView->tlxPercent());
  mImageVector[mImageVectorIndex]->setTly(mpView->tlyPercent());
  mImageVector[mImageVectorIndex]->setBrx(mpView->brxPercent());
  mImageVector[mImageVectorIndex]->setBry(mpView->bryPercent());
}
/** No descriptions */
void QuiteInsane::setImage(QImage* image)
{
  if(!image)
    return;
  if(mMode == Mode_TextOnly)
    return;
  setCursor(Qt::waitCursor);
  mImagePath = QString::null;
  setImageViewerState(true);
  mImageLoaded = true;
  setCaption(tr("QuiteInsane - Image viewer"));
  clearImageQueue();
  mpView->setImage(image);
  addImageToQueue(image);
  mOriginalImageFilepath = QString::null;
  setImageModified(false);
  updateZoomMenu();
  set100PercentZoom();
  setCursor(Qt::arrowCursor);
}
/** No descriptions */
void QuiteInsane::slotLocalImageUriDropped(QStringList urilist)
{
  for(QStringList::Iterator it=urilist.begin();it!=urilist.end();++it)
  {
    if(QImage::imageFormat(*it))
    {
      QuiteInsane* qi = new QuiteInsane(QuiteInsane::Mode_ImageOcr,0);
      if(!qi->statusOk())
      {
        QMessageBox::warning(0,tr("Error"),
                             tr("Could not create image viewer."),tr("Cancel"));	

        delete qi;
        return;
      }
      qi->show();
      qi->loadImage(*it);
    }
  }
}
/** No descriptions */
void QuiteInsane::enableGui(bool state)
{
  if(menuBar())
    menuBar()->setEnabled(state);
  if(mpImageToolbar)
    mpImageToolbar->setEnabled(state);
  if(mpTextToolbar)
    mpTextToolbar->setEnabled(state);
  if(mpToolsToolbar)
    mpToolsToolbar->setEnabled(state);
}
