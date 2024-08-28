/***************************************************************************
                          qscandialog.cpp  -  description
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

#include "err.h"
#include "resource.h"

#include <QDesktopWidget>

#include <QDebug>
#include <QGroupBox>
#include <QListWidget>
#include <QStackedWidget>

#include "err.h"

#include "images/setup.xpm"
#include "images/maxview.xpm"
#include "images/fileopen.xpm"
#include "previewwidget.h"
#include "qbooloption.h"
#include "qbuttonoption.h"
#include "qcombooption.h"
#include "qdevicesettings.h"
#include "qlistviewitemext.h"
#include "qextensionwidget.h"
#include "qoptionscrollview.h"
#include "qreadonlyoption.h"
#include "qscannersetupdlg.h"
#include "qscrollbaroption.h"
#include "qscandialog.h"
#include "qsanestatusmessage.h"
#include "qstringoption.h"
#include "qwordarrayoption.h"
#include "qwordcombooption.h"
#include "qxmlconfig.h"
#include <QResizeEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include "sanefixedoption.h"
#include "saneintoption.h"
#include "sanewidgetholder.h"
#include "scanarea.h"

#include <qapplication.h>
#include <qcheckbox.h>
#include <qcolor.h>
#include <qcombobox.h>
#include <qdatastream.h>
#include <qdialog.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qfontmetrics.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qobject.h>
#include <qpainter.h>
#include <qpalette.h>
#include <qpixmap.h>
#include <qpoint.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qrect.h>
#include <qscrollbar.h>
#include <qsizepolicy.h>
#include <qstringlist.h>
#include <qtabwidget.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qwidget.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

extern "C"
{
#include <sane/sane.h>
}

#define PAGE_SIZE_MARGIN  0


QScanDialog::QScanDialog(QScanner* s,QWidget *parent, const char *name,Qt::WindowFlags f)
            :QWidget(parent,f)
{
  setObjectName(name);
  mMultiSelectionMode = false;
  mpScanner = s;
  mShowCnt = 0;
  mIgnoreInexact = 0;
  mOptionSubArray.resize(0);
  mpHistoryWidget = 0L;
  mpMultiScanWidget = 0L;
  mpOptionScrollView = 0L;
  mpOptionTabWidget = 0L;
  mpOptionMainWidget = 0L;
  mpOptionListWidget = 0L;
  mpOptionWidgetStack = 0L;
  mpPreviewWidget = 0L;
  mpButtonOption = 0L;
  mLayout = QIN::ScrollLayout;
  mStatus = initDialog();
}

QScanDialog::~QScanDialog()
{
   if (mpPreviewWidget)
     delete mpPreviewWidget;
}

QIN::Status QScanDialog::initDialog()
{
  if(!mpScanner) return QIN::InitFailed;

  QString qs;
  QString caption_string;
  int i;

  mDeviceName = mpScanner->name();

//////////
//main layout
  mpMainLayout = new QGridLayout(this);
  mpMainLayout->setMargin(3);
  mpMainLayout->setSpacing(5);
  mpMainLayout->setColumnStretch(0,1);
  mpInfoHBox = new QHBoxLayout();
  mpInfoHBox->setSpacing(2);
  mpLabelImageInfo = new QLabel("");
  mpInfoHBox->addWidget(mpLabelImageInfo);
  mpLabelImageInfo->setFrameStyle(QFrame::StyledPanel|QFrame::Sunken);
  mpInfoHBox->setStretchFactor(mpLabelImageInfo,1);

  mpAboutButton = new QPushButton(tr("&About..."));
  mpInfoHBox->addWidget(mpAboutButton);
  connect(mpAboutButton,SIGNAL(clicked()),this,SLOT(slotAbout()));

//s  mpHelpButton = new QPushButton(tr("&Help..."),mpInfoHBox);

  mpMainLayout->addLayout(mpInfoHBox,0,0);

//create help viewer
//s  mpHelpViewer = new QHTMLView(0);
//s  connect(mpHelpButton,SIGNAL(clicked()),this,SLOT(slotShowHelp()));
//in drag mode, the user can type in the filename directly
  QPixmap setpix((const char **)setup);
	QPixmap openpix((const char **)fileopen);
  mpDragHBox1 = new QHBoxLayout();
  mpDragHBox1->addWidget(new QLabel(tr("Save as:")));
  mpDragLineEdit = new QLineEdit();
  mpDragHBox1->addWidget(mpDragLineEdit);
  QPushButton* tb1 = new QPushButton();
  mpDragHBox1->addWidget(tb1);
  tb1->setIcon(openpix);
  mpDragHBox2 = new QHBoxLayout();
  QPushButton* pb_setfn = new QPushButton();
  mpDragHBox2->addWidget(pb_setfn);
  pb_setfn->setIcon(setpix);
  pb_setfn->resize(tb1->sizeHint());
  QWidget* dummy1 = new QWidget();
  mpDragHBox2->addWidget(dummy1);
  dummy1->setFixedWidth(10);
  mpDragHBox2->addWidget(new QLabel(tr("Image type")));
  mpDragTypeCombo = new QComboBox();
  mpDragHBox2->addWidget(mpDragTypeCombo);
//  QList<QByteArray> lin = QImageWriter::supportedImageFormats();
  mpDragTypeCombo->addItem(tr("by extension"));
  mpDragTypeCombo->addItem("BMP");
  mpDragTypeCombo->addItem("JPEG");
  mpDragTypeCombo->addItem("TIF");
  mpDragTypeCombo->addItem("PNG");
  mpDragTypeCombo->addItem("PBM");
  mpDragTypeCombo->addItem("PGM");
  mpDragTypeCombo->addItem("PPM");
  mpDragTypeCombo->addItem("PNM");
  mpDragTypeCombo->addItem("XBM");
  mpDragTypeCombo->addItem("XPM");
  QPushButton* tb2 = new QPushButton();
  mpDragHBox2->addWidget(tb2);
  tb2->setIcon(setpix);
  tb2->resize(tb1->sizeHint());
  mpMainLayout->addLayout(mpDragHBox1,2,0);
  mpMainLayout->addLayout(mpDragHBox2,3,0);
  mpDragHBox1->setSpacing(3);
  mpDragHBox2->setSpacing(3);
  mpDragTypeCombo->setCurrentIndex(xmlConfig->intValue("DRAG_IMAGE_TYPE"));
  mpDragLineEdit->setText(xmlConfig->stringValue("DRAG_FILENAME"));
  connect(tb2,SIGNAL(clicked()),this,SLOT(slotImageSettings()));
  connect(pb_setfn,SIGNAL(clicked()),this,SLOT(slotFilenameGenerationSettings()));
  connect(mpDragTypeCombo,SIGNAL(activated(int)),
          this,SLOT(slotDragType(int)));
  connect(mpDragLineEdit,SIGNAL(textChanged(const QString&)),
          this,SLOT(slotDragFilename(const QString&)));
  createOptionWidget();
  createPreviewWidget();

//first button row
  mpButtonHBox1 = new QHBoxLayout();
  mpButtonHBox1->setSpacing(2);

    mpOptionsButton = new QPushButton(tr("&Options..."));
    mpButtonHBox1->addWidget(mpOptionsButton);
  connect(mpOptionsButton,SIGNAL(clicked()),this,SLOT(slotShowOptionsWidget()));
    mpPreviewButton = new QPushButton(tr("&Preview..."));
    mpButtonHBox1->addWidget(mpPreviewButton);

//second button row
  mpButtonHBox2 = new QHBoxLayout();
  mpButtonHBox2->setSpacing(2);
  mpDeviceButton = new QPushButton(tr("&Device settings..."));
  mpButtonHBox2->addWidget(mpDeviceButton);
  connect(mpDeviceButton,SIGNAL(clicked()),this,SLOT(slotDeviceSettings()));
    mpQuitButton = new QPushButton(tr("&Close"));
    mpButtonHBox2->addWidget(mpQuitButton);
  connect(mpQuitButton,SIGNAL(clicked()),this,SLOT(close()));
    mpMainLayout->addLayout(mpButtonHBox1,6,0);
    mpMainLayout->addLayout(mpButtonHBox2,7,0);
	mpMainLayout->activate();
  slotReloadOptions();//check which options are active

  if(mpPreviewWidget)
	{
    //horizontal separator
    mpSeparator = new QVBoxLayout();
    QFrame* frame = new QFrame();
    mpSeparator->addWidget(frame);
    frame->setFrameStyle(QFrame::VLine|QFrame::Sunken);
    frame->setLineWidth(2);
    mpSeparator->setMargin(5);
      mpMainLayout->addLayout(mpSeparator,0,1,8,1);
    if(mpPreviewWidget->window() == mpPreviewWidget)
    {
      if(mpPreviewWidget->layout())
          mpPreviewWidget->layout()->setMargin(5);
//      mpSeparator->hide();
    }
    else
    {
      if(mpPreviewWidget->layout()) mpPreviewWidget->layout()->setMargin(0);
      mpMainLayout->addWidget(mpPreviewWidget,0, 2, 8, 1);
    }
//     connect(mpPreviewWidget,SIGNAL(signalPreviewRequest(double,double,double,double,int)),
//             this,SLOT(slotPreview(double,double,double,double,int)));
    connect(mpPreviewButton,SIGNAL(clicked()),this,SLOT(slotShowPreviewWidget()));
    if(mpTlxOption)
    {
      connect(this,SIGNAL(signalMetricSystem(QIN::MetricSystem)),
              mpPreviewWidget,SLOT(slotChangeMetricSystem(QIN::MetricSystem)));
      slotUserSize(0);
    }
  }
  else
  {
		mpPreviewButton->setEnabled(false);
  }
  connect(mpScanner,SIGNAL(signalReloadOptions()),this,SLOT(slotReloadOptions()));
  slotImageInfo();
  connect(mpScanner,SIGNAL(signalReloadParams()),this,SLOT(slotImageInfo()));
  connect(mpScanner,SIGNAL(signalInfoInexact(int)),this,SLOT(slotInfoInexact(int)));
  caption_string = "MaxView - ";
  caption_string += xmlConfig->stringValue("LAST_DEVICE_VENDOR",QString());
  caption_string += " ";
  caption_string += xmlConfig->stringValue("LAST_DEVICE_MODEL",QString());
  caption_string += " (";
  caption_string += mDeviceName;
  caption_string += ")";
  setWindowTitle(caption_string);
  i = xmlConfig->intValue("LAYOUT");
  switch(i)
  {
    case 0:
//      slotChangeLayout(QIN::ScrollLayout);
      break;
    case 1:
      changeLayout(QIN::TabLayout);
      break;
    case 2:
      changeLayout(QIN::MultiWindowLayout);
      break;
    case 3:
      changeLayout(QIN::ListLayout);
      break;
    default:
      changeLayout(QIN::ScrollLayout);
      break;
  }
//set metric system
  i = xmlConfig->intValue("METRIC_SYSTEM");
  emit signalMetricSystem((QIN::MetricSystem)i);
  //load last mode
  i = xmlConfig->intValue("SCAN_MODE");

//s  mpModeCombo->setCurrentItem(i);
  slotChangeMode(i);

  return QIN::ReadyToShow;
}
/**  */
void QScanDialog::slotUserSize(int)
{
  mpPreviewWidget->setRectSize(mpTlxOption->getPercentValue(),
                               mpTlyOption->getPercentValue(),
                               mpBrxOption->getPercentValue(),
                               mpBryOption->getPercentValue());
}

QSaneOption *QScanDialog::findOption (QString name, int option_type)
{
  for (int i = 0; i < mOptionWidgets.size (); i++)
  {
    SaneWidgetHolder *w = mOptionWidgets [i];

    if (w->saneOption ()->optionName () == name
        && (option_type == -1 || option_type == w->saneOption ()->saneValueType ())) //! added ()
    {
       return w->saneOption ();
    }
  }
  return 0;
}


/**  */
void QScanDialog::createOptionWidget()
{
  QGroupBox* qgb;
  int groupcount;
  int c;
  groupcount =0;

  mOptionWidgets.resize(0);
  mGroupBoxArray.resize(0);
//check whether there are options at all
  if(mpScanner->optionCount()>1)
  {
    mpOptionScrollView = new QOptionScrollView(this);
    mpOptionScrollView->setFrameStyle(QScrollArea::NoFrame);
    mpOptionScrollView->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
//     mpOptionScrollView->setHScrollBarMode(Q3ScrollView::AlwaysOff);
    //check whether there are grouped options
    //and put them in a groupbox
    groupcount = mpScanner->getGroupCount();
    if(groupcount > 0)
    {
      for(c=1;c<=groupcount;c++)
      {
        //create a group box with a QVBoxLayout
        qgb = createOptionGroupBox(mpScanner->getGroupTitle(c),
                                   mpScanner->firstGroupItem(c),
                                   mpScanner->lastGroupItem(c));
        mpOptionScrollView->addWidget(qgb);
     }
    }
    //check whether there are options that don't belong
    //to a group and put them in a group box
    if(mpScanner->nonGroupOptionCount()>0)
    {
      qgb = createOptionGroupBox(tr("Other options"),1,mpScanner->nonGroupOptionCount());
      mpOptionScrollView->addWidget(qgb);
    }
    mpMainLayout->addWidget(mpOptionScrollView,5,0);
  }

  // now search for widgets that we need to know about
  mOptionSource = findOption (SANE_NAME_SCAN_SOURCE);
  mOptionDuplex = findOption ("duplex", (int)SANE_TYPE_STRING);
  mOptionXRes = findOption (SANE_NAME_SCAN_X_RESOLUTION);
  if (!mOptionXRes)
      mOptionXRes = findOption (SANE_NAME_SCAN_RESOLUTION);
  mOptionYRes = findOption (SANE_NAME_SCAN_Y_RESOLUTION);
  mOptionFormat = findOption (SANE_NAME_SCAN_MODE);
  mOptionCompression = findOption ("compression");
  mOptionThreshold = findOption (SANE_NAME_THRESHOLD);
  mOptionBrightness = findOption (SANE_NAME_BRIGHTNESS);
  mOptionContrast = findOption (SANE_NAME_CONTRAST);
}


void QScanDialog::setPageSize (double dwidth, double dheight)
{
    SANE_Word width, height;

    if (mpScanner->getOptionType(mpWidthOption->saneOptionNumber()) == SANE_TYPE_FIXED)
    {
      width = SANE_FIX (dwidth) + PAGE_SIZE_MARGIN;
      height = SANE_FIX (dheight) + PAGE_SIZE_MARGIN;
    }
    else
    {
      width = (int)dwidth;
      height = (int)dheight;
    }

//    printf ("QScanDialog::setPageSize, %1.15lf, %1.15lf\n", SANE_UNFIX (width), SANE_UNFIX (height));
    mpScanner->setOption (mpWidthOption->saneOptionNumber(), &width);
    mpScanner->setOption (mpHeightOption->saneOptionNumber(), &height);
}


/**Check all options visible in the dialog. Inactive options
   are disabled.  */
void QScanDialog::slotReloadOptions()
{
  SANE_Int i_val;
  SANE_Fixed f_val;
  SANE_Bool b_val;
  QVector<SANE_Word> array;
  QString stringval;
  QScrollBarOption* qsbo;
  QComboOption* qco;
  QBoolOption* qboolo;
  QButtonOption* qbutt;
  QStringOption* qso;
  QWordArrayOption* qwao;
  QWordComboOption* qwco;
  QReadOnlyOption* qroo;
  SaneIntOption* sint;
  SaneFixedOption* sfix;
  int cnt;
//the hide/show counter, to ensure that we only use my "somewhat"
//flickering recalculation of this widget if necessary
  int hscnt = 0;
  int sane_opt_num = -1;
  bool scan_area_changed = false;

   static bool in_here = false;

  if (in_here)
     return;
  in_here = true;

  /* if the width / height options were hidden, but are now shown, then change the width/height
     to at least as big as the scan area. This copes with the case where we move from flatbed
     to ADF, but the ADF paper size is smaller than the old flatbed scan area */

  if (mpWidthOption && mpTlxOption
     && mpScanner->isOptionActive(mpWidthOption->saneOptionNumber())
     && mpWidthOption->isHidden ())
  {
    double dwidth, dheight;
    static bool busy = false;  // avoid an infinite loop here

    if (!busy)
    {
      busy = true;
      // get current X, Y extent
      dwidth = mpScanner->saneWordValue(mpBrxOption->saneOptionNumber());
      dheight = mpScanner->saneWordValue(mpBryOption->saneOptionNumber());
      if (mpScanner->getOptionType(mpTlxOption->saneOptionNumber()) == SANE_TYPE_FIXED)
      {
        dwidth = SANE_UNFIX (dwidth + PAGE_SIZE_MARGIN);
        dheight = SANE_UNFIX (dheight + PAGE_SIZE_MARGIN);
      }
      printf ("slotReloadOptions  %1.1lf  %1.1lf\n", dwidth, dheight);
      setPageSize (dwidth, dheight);
      busy = false;
    }
  }

  for(cnt=0;cnt<int(mOptionWidgets.size());cnt++)
  {
    //check whether the widget must be re-created
    //this can happen, e.g. if the backend decides to
    //change the option value constraint from
    //SANE_CONSTRAINT_RANGE to SANE_CONSTRAINT_WORD_LIST
    //or vice versa
    checkOptionValidity(cnt);
    qsbo = 0L;
    QObject *obj = (QObject*)(mOptionWidgets[cnt]->saneOption());
    QString objname = obj->metaObject()->className();
    if (objname == "QScrollBarOption")
      qsbo=(QScrollBarOption*)(mOptionWidgets[cnt]->saneOption());
    if(qsbo)
    {
    //scrollbar option found
      sane_opt_num = qsbo->saneOptionNumber();
      if(mpScanner->isOptionActive(qsbo->saneOptionNumber()))
      {
        if(qsbo->getSaneType() == SANE_TYPE_INT)
        {
          int range_min;
          int range_max;
          range_min = int(mpScanner->getRangeMin(sane_opt_num));
          range_max = int(mpScanner->getRangeMax(sane_opt_num));
          if(range_max != qsbo->maxValue())
          {
            if((mOptionWidgets[cnt]->saneOption() == (void*)mpTlxOption) ||
               (mOptionWidgets[cnt]->saneOption() == (void*)mpTlyOption) ||
               (mOptionWidgets[cnt]->saneOption() == (void*)mpBrxOption) ||
               (mOptionWidgets[cnt]->saneOption() == (void*)mpBryOption))
            {
              scan_area_changed = true;
            }
          }
          i_val = (SANE_Int)mpScanner->saneWordValue(sane_opt_num);
          qsbo->setRange(range_min,range_max,
                         mpScanner->getRangeQuant(sane_opt_num));
          qsbo->setValue(i_val);
        }
        if(qsbo->getSaneType() == SANE_TYPE_FIXED)
        {
          int range_min;
          int range_max;
          range_min = int(mpScanner->getRangeMin(sane_opt_num));
          range_max = int(mpScanner->getRangeMax(sane_opt_num));
          if(range_max != qsbo->maxValue())
          {
            if((mOptionWidgets[cnt]->saneOption() == (void*)mpTlxOption) ||
               (mOptionWidgets[cnt]->saneOption() == (void*)mpTlyOption) ||
               (mOptionWidgets[cnt]->saneOption() == (void*)mpBrxOption) ||
               (mOptionWidgets[cnt]->saneOption() == (void*)mpBryOption))
            {
              scan_area_changed = true;
            }
          }
          qsbo->setRange(range_min,range_max,
                         mpScanner->getRangeQuant(sane_opt_num));
          f_val = (SANE_Fixed)mpScanner->saneWordValue(qsbo->saneOptionNumber());
          qsbo->setValue(f_val);
        }
        if(qsbo->isHidden())
        {
          qsbo->show();
          qsbo->layout()->activate();
          hscnt += 1;
        }
      }
      else
      {
        if(!qsbo->isHidden())
        {
          qsbo->hide();
          hscnt += 1;
        }
      }
    }
    qco = 0L;
    sint = 0L;
    if (objname == "SaneIntOption")
      sint=(SaneIntOption*)(mOptionWidgets[cnt]->saneOption());
    if(sint)
    {
    //int option found
      sane_opt_num = sint->saneOptionNumber();
      if(mpScanner->isOptionActive(sint->saneOptionNumber()))
      {
        i_val = (SANE_Int)mpScanner->saneWordValue(sane_opt_num);
        sint->setValue(i_val);
        if(sint->isHidden())
        {
          sint->show();
          sint->layout()->activate();
          hscnt += 1;
        }
      }
      else
      {
        if(!sint->isHidden())
        {
          sint->hide();
          hscnt += 1;
        }
      }
    }
    sint = 0L;
    sfix = 0L;
    if (objname == "SaneFixedOption")
      sfix=(SaneFixedOption*)(mOptionWidgets[cnt]->saneOption());
    if(sfix)
    {
    //fixed option found
      sane_opt_num = sfix->saneOptionNumber();
      if(mpScanner->isOptionActive(sfix->saneOptionNumber()))
      {
        i_val = (SANE_Int)mpScanner->saneWordValue(sane_opt_num);
        sfix->setValue(i_val);
        if(sfix->isHidden())
        {
          sfix->show();
          sfix->layout()->activate();
          hscnt += 1;
        }
      }
      else
      {
        if(!sfix->isHidden())
        {
          sfix->hide();
          hscnt += 1;
        }
      }
    }
    sfix = 0L;
    if(objname == "QComboOption")
   	  qco=(QComboOption*)(mOptionWidgets[cnt]->saneOption());
		if(qco)
		{
			//always string type
			if(mpScanner->isOptionActive(qco->saneOptionNumber()))
      {
        QStringList slist = mpScanner->getStringList(qco->saneOptionNumber());
        qco->setStringList(slist);
          qco->setCurrentValue(mpScanner->saneStringValue(
                                   qco->saneOptionNumber()).toLatin1());
        if(qco->isHidden())
        {
          qco->show();
          qco->layout()->activate();
          hscnt += 1;
        }
      }
			else
      {
        if(!qco->isHidden())
        {
          qco->hide();
          hscnt += 1;
        }
      }
		}
    qwco = 0L;
    if(objname == "QWordComboOption")
      qwco=(QWordComboOption*)(mOptionWidgets[cnt]->saneOption());
    if(qwco)
	  {
			if(mpScanner->isOptionActive(qwco->saneOptionNumber()))
      {
   		  qwco->setValue((SANE_Word)mpScanner->saneWordValue(qwco->saneOptionNumber()));
        if(qwco->isHidden())
        {
          qwco->show();
          qwco->layout()->activate();
          hscnt += 1;
        }
      }
			else
      {
        if(!qwco->isHidden())
        {
          qwco->hide();
          hscnt += 1;
        }
      }
    }
    qboolo = 0L;
    if(objname == "QBoolOption")
   	  qboolo=(QBoolOption*)(mOptionWidgets[cnt]->saneOption());
		if(qboolo)
		{
			if(mpScanner->isOptionActive(qboolo->saneOptionNumber()))
      {
				b_val = (SANE_Bool)mpScanner->saneWordValue(qboolo->saneOptionNumber());
        qboolo->setState(b_val);
        if(qboolo->isHidden())
        {
          qboolo->show();
          qboolo->layout()->activate();
          hscnt += 1;
        }
      }
			else
      {
        if(!qboolo->isHidden())
        {
          qboolo->hide();
          hscnt += 1;
        }
      }
		}
    qbutt = 0L;
    if(objname == "QButtonOption")
   	  qbutt=(QButtonOption*)(mOptionWidgets[cnt]->saneOption());
  	if(qbutt)
		{
			//has no value
			if(mpScanner->isOptionActive(qbutt->saneOptionNumber()))
      {
        if(qbutt->isHidden())
        {
          qbutt->show();
          qbutt->layout()->activate();
          hscnt += 1;
        }
      }
			else
      {
        if(!qbutt->isHidden())
        {
          qbutt->hide();
          hscnt += 1;
        }
      }
		}
    //check whether it's a string option
    qso = 0L;
    if(objname == "QStringOption")
   	  qso=(QStringOption*)(mOptionWidgets[cnt]->saneOption());
    QString optionstring;
	  if(qso)
	  {
			//always string type
			if(mpScanner->isOptionActive(qso->saneOptionNumber()))
      {
				stringval = mpScanner->saneStringValue(qso->saneOptionNumber());
        qso->setText(stringval.toLatin1());
        if(qso->isHidden())
        {
          qso->show();
          qso->layout()->activate();
          hscnt += 1;
        }
      }
			else
      {
        if(!qso->isHidden())
        {
          qso->hide();
          hscnt += 1;
        }
      }
    }
    qwao = 0L;
    if(objname == "QWordArrayOption")
      qwao=(QWordArrayOption*)(mOptionWidgets[cnt]->saneOption());
		if(qwao)
		{
			if(mpScanner->isOptionActive(qwao->saneOptionNumber()))
      {
				array = mpScanner->saneWordArray(qwao->saneOptionNumber());
        qwao->setValue(array);
        if(qwao->isHidden())
        {
          qwao->show();
          qwao->layout()->activate();
          hscnt += 1;
        }
        //switch to curve mode free
      }
			else
      {
        if(!qwao->isHidden())
        {
          qwao->hide();
          qwao->closeCurveWidget();
          hscnt += 1;
        }
      }
		}
    qroo = 0L;
    if(objname == "QReadOnlyOption")
   	  qroo=(QReadOnlyOption*)(mOptionWidgets[cnt]->saneOption());
		if(qroo)
		{
			if(mpScanner->isOptionActive(qroo->saneOptionNumber()))
      {
        qroo->setText(mpScanner->saneReadOnly(qroo->saneOptionNumber()));
        if(qroo->isHidden())
        {
          qroo->show();
          qroo->layout()->activate();
          hscnt += 1;
        }
      }
			else
      {
        if(!qroo->isHidden())
        {
          qroo->hide();
          hscnt += 1;
        }
      }
		}
  }

  //If no option was shown/hidden we must not resize the layout.
  if(hscnt == 0)
  {
//    qDebug("QScanDialog::slotReloadOptions - leave no resize");
    in_here = false;
    return;
  }
  //We also don't resize, if the preview widget is a part of the
  //main dialog (== isn't a toplevel widget).
  if(mLayout == QIN::ScrollLayout)
  {
    QWidget *w = mpOptionScrollView->widget ();
    w->setMinimumSize(w->layout ()->minimumSize ());
  }
  if(mLayout == QIN::TabLayout)
  {
    //stupid, but works (?)
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionTabWidget->hide();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionTabWidget->show();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    if(mpPreviewWidget)
    {
      if(scan_area_changed)
      {
          setPreviewRange();
      }
      if(!mpPreviewWidget->topLevel())
      {
        if(height() < minimumSizeHint().height())
          resize(width(),minimumSizeHint().height());
      }
      else
        resize(width(),minimumSizeHint().height());
    }
    else
      resize(width(),minimumSizeHint().height());
  }
  else if(mLayout == QIN::MultiWindowLayout)
  {
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionMainWidget->hide();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionMainWidget->show();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    if(mpPreviewWidget)
    {
      if(scan_area_changed)
      {
          setPreviewRange();
      }
      if(!mpPreviewWidget->topLevel())
      {
        if(height() < minimumSizeHint().height())
          resize(width(),minimumSizeHint().height());
      }
      else
        resize(width(),sizeHint().height());
    }
    else
      resize(width(),sizeHint().height());
    for(cnt=0;cnt<int(mOptionSubArray.size());cnt++)
    {
      mOptionSubArray[cnt]->resize(mOptionSubArray[cnt]->width(),
                                  mOptionSubArray[cnt]->sizeHint().height());
    }
  }
  else if(mLayout == QIN::ListLayout)
  {
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionWidgetStack->hide();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionWidgetStack->show();
    mpOptionListWidget->layout()->activate();
    mpOptionWidgetStack->resize(mpOptionWidgetStack->width(),
                              mpOptionWidgetStack->sizeHint().height());
    qApp->processEvents();
    QApplication::sendPostedEvents();
    if(mpPreviewWidget)
    {
      if(scan_area_changed)
      {
        setPreviewRange();
      }
      if(!mpPreviewWidget->topLevel())
      {
        if(height() < minimumSizeHint().height())
          resize(width(),minimumSizeHint().height());
      }
      else
        resize(width(),minimumSizeHint().height());
    }
    else
      resize(width(),minimumSizeHint().height());
  }
  //Did status of scan area change ?
  if(mpTlxOption && mpTlyOption && mpBrxOption && mpBryOption)
  {
     if(!mpScanner->isOptionActive(mpTlxOption->saneOptionNumber()) ||
        !mpScanner->isOptionActive(mpTlyOption->saneOptionNumber()) ||
        !mpScanner->isOptionActive(mpBrxOption->saneOptionNumber()) ||
        !mpScanner->isOptionActive(mpBryOption->saneOptionNumber()))
     {
//       mpPreviewWidget->setMetrics(QIN::NoMetricSystem,SANE_UNIT_NONE);
//       mpPreviewWidget->setAspectRatio(1.0);
     }
     else
     {
//       mpPreviewWidget->setMetrics(QIN::Millimetre,
//                           mpScanner->getUnit(mpTlxOption->saneOptionNumber()));
       setPreviewRange();
     }
  }
//  qDebug("QScanDialog::slotReloadOptions - leave");
  in_here = false;
}


void QScanDialog::slotDoPendingChanges (void)
{
  int cnt, number = 0;

  for(cnt=0;cnt<int(mOptionWidgets.size());cnt++)
  {
    SaneWidgetHolder *wh = mOptionWidgets [cnt];

    if (wh->getPending ())
    {
//      printf ("Pending %d, %s\n", cnt, wh->saneOption ()->optionName ().latin1 ());
      slotOptionChanged (wh->saneOption ()->optionNumber ());
      wh->setPending (false);
      number++;
    }
  if (number)
    emit signalPendingDone ();
  }
}


/** called when the value of one of the widgets is changed - we need to update the relevant sane option  */

void QScanDialog::slotOptionChanged(int num)
{
  SANE_Int   si;
  SANE_Word  sw;
  SANE_Bool  sb;
  QScrollBarOption* qsbo;
  QComboOption*     qco;
  QButtonOption*    qbo;
  QBoolOption*      qboolo;
  QStringOption*    qso;
  QWordComboOption* qwco;
  QWordArrayOption* qwao;
  SaneFixedOption*  sfix;
  SaneIntOption*    sint;
  QString combostring;
  QString optionstring;
  int i;
  QVector<SANE_Word> qa;
  SaneWidgetHolder *wh = mOptionWidgets [num];

  if (mpScanner->isScanning ())
  {
//    printf ("scanning, got num = %d: %d, %s\n", num, wh->saneOption ()->optionNumber (), wh->saneOption ()->optionName ().latin1 ());
    wh->setPending (true);
    return;
  }
  i = 0;
  //check whether it's a bool option
  qboolo = 0L;
  QObject *obj = (QObject*)(mOptionWidgets[num]->saneOption());
  QString objname = obj->metaObject()->className();
  if (objname == "QBoolOption")
    qboolo=(QBoolOption*)(mOptionWidgets[num]->saneOption());
  if(qboolo)
  {
    i = qboolo->saneOptionNumber();
    sb = qboolo->state();
    mpScanner->setOption(i,&sb);
//    qDebug("QScanDialog::slotOptionChanged - QBoolOption %i - %i",i,sb);
    return;
  }
  //check whether it's a button option
  qbo = 0L;
  if (objname == "QButtonOption")
    qbo=(QButtonOption*)(mOptionWidgets[num]->saneOption());
  if(qbo)
  {
    i = qbo->saneOptionNumber();
    mpScanner->setOption(i,0L);
    return;
  }
  qco = 0L;
  if (objname == "QComboOption")
    qco=(QComboOption*)(mOptionWidgets[num]->saneOption());
  if(qco)
  {
    //always string type
    i = qco->saneOptionNumber();
    combostring = qco->getCurrentText();
//     qDebug () << "combostring" << combostring;
    if (!combostring.isEmpty())
      mpScanner->setOption(i,(SANE_String*)combostring.toLatin1().constData());
    return;
  }
  //check whether it's a word combo option
  qwco = 0L;
  if (objname == "QWordComboOption")
    qwco=(QWordComboOption*)(mOptionWidgets[num]->saneOption());
  if(qwco)
  {
    i = qwco->saneOptionNumber();
    si = qwco->getCurrentValue();
    mpScanner->setOption(i,&si);
    return;
  }
  //check whether it's a word array option
  qwao = 0L;
  if (objname == "QWordArrayOption")
    qwao=(QWordArrayOption*)(mOptionWidgets[num]->saneOption());
  if(qwao)
  {
    i = qwao->saneOptionNumber();
    qa = qwao->getValue();
    mpScanner->setOption(i,qa.data());
    return;
  }
  //check whether it's a scrollbaroption
  qsbo = 0L;
  if (objname == "QScrollBarOption")
    qsbo=(QScrollBarOption*)(mOptionWidgets[num]->saneOption());
  if(qsbo)
  {
    i = qsbo->saneOptionNumber();
    sw = (SANE_Int)qsbo->getValue();  // if this is page width, need to change scan area
    mpScanner->setOption(i,&sw);
    return;
  }
  //check whether it's a sanefixedoption
  sfix = 0L;
  if (objname == "SaneFixedOption")
    sfix=(SaneFixedOption*)(mOptionWidgets[num]->saneOption());
  if(sfix)
  {
    i = sfix->saneOptionNumber();
    sw = (SANE_Fixed)sfix->value();
    mpScanner->setOption(i,&sw);
  }
  //check whether it's a sanefixedoption
  sint = 0L;
  if (objname == "SaneIntOption")
    sint=(SaneIntOption*)(mOptionWidgets[num]->saneOption());
  if(sint)
  {
    i = sint->saneOptionNumber();
    sw = (SANE_Int)sint->value();
    mpScanner->setOption(i,&sw);
  }
  //check whether it's a string option
  qso = 0L;
  if (objname == "QStringOption")
    qso=(QStringOption*)(mOptionWidgets[num]->saneOption());
  if(qso)
  {
    //always string type
    i = qso->saneOptionNumber();
    optionstring = qso->text();
    if (!optionstring.isEmpty())
       mpScanner->setOption(i,(SANE_String*)optionstring.toLatin1().constData());
  }
}
/**  */
void QScanDialog::setAllOptions()
{
  SANE_Int si;
  SANE_Fixed sf;
  QScrollBarOption* qsbo;
  QComboOption* qco;
  QString combostring;
  int i;
  i = 0;

//don't set button options
  for(i=0;i<int(mOptionWidgets.size());i++)
  {
    //check whether it's a combo option
    qco = 0L;
    QObject *obj = (QObject*)(mOptionWidgets[i]->saneOption());
    QString objname = obj->metaObject()->className();
    if (objname == "QComboOption")
      qco=(QComboOption*)(mOptionWidgets[i]->saneOption());
    if(qco)
    {
      combostring = qco->getCurrentText();
      if (!combostring.isEmpty())
         mpScanner->setOption(i,(SANE_String*)combostring.toLatin1().constData());
    }
    //check whether it's a scrollbaroption
    qsbo = 0L;
    if (objname == "QScrollBarOption")
      qsbo=(QScrollBarOption*)(mOptionWidgets[i]->saneOption());
    if(qsbo)
    {
      switch(qsbo->getSaneType())
      {
        case SANE_TYPE_INT:
          si = (SANE_Int)qsbo->getValue();
          mpScanner->setOption(i,&si);
          break;
        case SANE_TYPE_FIXED:
          sf = (SANE_Fixed)qsbo->getValue();
          mpScanner->setOption(i,&sf);
          break;
        default:;
      }
    }
  }
}
/**  */
void QScanDialog::slotShowOptionsWidget()
{
  QIN::Layout l;
  QIN::MetricSystem ms;
  bool sepprev;
  bool visible;
  sepprev =  xmlConfig->boolValue("SEPARATE_PREVIEW");
  QExtensionWidget ew(this);
  if(ew.exec())
  {
    //check whether the layout must be changed
    l = (QIN::Layout)xmlConfig->intValue("LAYOUT");
    if(l != mLayout) changeLayout(l);
#ifndef QIS_NO_STYLES
    int s;
    //check whether the style must be changed
    s = xmlConfig->intValue("STYLE");
    emit signalChangeStyle(s);
#endif
    //check whether the metric system changed
    ms = (QIN::MetricSystem)xmlConfig->intValue("METRIC_SYSTEM");
    if(ms != mMetricSystem) emit signalMetricSystem(ms);
    //Did preview mode change?
    if(sepprev != xmlConfig->boolValue("SEPARATE_PREVIEW"))
    {
      if(mpPreviewWidget)
      {
        visible = mpPreviewWidget->isVisible();
        if(mpPreviewWidget->topLevel())
        {
          if(visible) mpPreviewWidget->hide();
          mpPreviewWidget->setParent(this);
          mpPreviewWidget->move(QPoint(0,0));
          mpPreviewWidget->hide();
          mpPreviewWidget->changeLayout(false);
          mpMainLayout->addWidget(mpPreviewWidget,0,2,8,1);
          mpMainLayout->setColumnStretch(0,0);
          mpMainLayout->setColumnStretch(2,1);
        }
        else
        {
          QPoint p;
          p=mpPreviewWidget->pos();
          if(visible) slotHidePreview();
          mpPreviewWidget->setParent(this);
          /*p
           * ,Qt::WType_TopLevel | Qt::WStyle_Title | Qt::WStyle_ContextHelp |
                                    Qt::WStyle_DialogBorder | Qt::WStyle_SysMenu |
                                    Qt::WStyle_Customize
          */
          mpPreviewWidget->move(p);
          mpPreviewWidget->hide();
          mpPreviewWidget->changeLayout(true);
          mpMainLayout->setColumnStretch(0,1);
          mpMainLayout->setColumnStretch(2,0);
        }
        if(visible) slotShowPreviewWidget();
      }
    }
  }
}
/**  */
void QScanDialog::slotShowPreviewWidget()
{
  int w = xmlConfig->intValue("SCANDIALOG_INTEGRATED_PREVIEW_WIDTH",0);
  int h = xmlConfig->intValue("SCANDIALOG_INTEGRATED_PREVIEW_HEIGHT",0);
  if(!mpPreviewWidget->topLevel())
  {
    if(w < width())
      w = width();
    if(h < height())
      h = height();
    resize(w,h);
    mpPreviewWidget->show();
//p    mpSeparator->show();
  }
  else
  {
    //crappy qt bug workaround
  	if(mpPreviewWidget->isMinimized())
      mpPreviewWidget->hide();
    mpPreviewWidget->show();
    mpPreviewWidget->raise();
  }
}

/**  */
void QScanDialog::slotAbout()
{
	QPixmap qp((const char **)maxview_logo_xpm);
  QString text;
  text =
  tr("<center><b><h1>QuiteInsane V%1</h1></b></center>"
     "<center>&copy  2000-2003 Michael Herder crapsite@gmx.net</center><br>"
     "<center><b>QuiteInsane</b> is a graphical frontend for SANE</center><br>"
 	   "<center>This program is free software; you can redistribute it and/or</center>"
     "<center>modify it under the terms of the GNU General Public License version 2</center>"
     "<center>as published by the Free Software Foundation.</center><br>"
     "<center>The program is provided AS IS with NO WARRANTY OF ANY KIND,</center>"
     "<center>INCLUDING THE WARRANTY OF DESIGN, MERCHANTABILITY AND</center>"
     "<center>FITNESS FOR A PARTICULAR PURPOSE.</center><br>"
    ).arg( VERSION );

  QMessageBox qmb(tr("About QuiteInsane"),text,
               QMessageBox::NoIcon,QMessageBox::Ok | QMessageBox::Default |
               QMessageBox::Escape , Qt::NoButton,Qt::NoButton,
               this);
  qmb.setIconPixmap(qp);
  qmb.exec();
}
/**  */

/** No descriptions */
QSaneOption* QScanDialog::createSaneOptionWidget(QWidget* parent,int opt_num)
{
  QSaneOption* ret = 0;
  QScrollBarOption*  qsb = 0;
  QScrollBarOption*  qsf = 0;
  QStringOption*     qso = 0;
  QComboOption*      qcb = 0;
  QButtonOption*     qbo = 0;
  QBoolOption*       qbool = 0;
  QWordArrayOption*  qwao = 0;
  QWordComboOption*  qwco = 0;
  QReadOnlyOption*   qroo = 0;
  SaneIntOption*     sint = 0;
  SaneFixedOption*   sfix = 0;
  SANE_Int           i_val;
  SANE_Fixed         f_val;
  QString            stringval;

//  const char *title = mpScanner->getOptionName(opt_num);
//  qDebug("create saneoption widget: %i, %s: %d",opt_num, title, mpScanner->isOptionSettable(opt_num));
  //check whether it's a read only option
  if(!mpScanner->isOptionSettable(opt_num) && mpScanner->isReadOnly(opt_num))
  {
    qroo = new QReadOnlyOption(mpScanner->getOptionTitle(opt_num).toLatin1(),parent,//qvbox,
                               (const char*)mpScanner->getOptionName(opt_num));
    qroo->setSaneOptionNumber(opt_num);
    qroo->setOptionDescription(mpScanner->getOptionDescription(opt_num));
    qroo->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
    qroo->setSaneValueType(mpScanner->getOptionType(opt_num));
    qroo->setText(mpScanner->saneReadOnly(opt_num));
    if(qroo->optionName() == "button-state")
      mpButtonOption = qroo;
    ret=(QSaneOption*)qroo;
  }
//  if((mpScanner->isOptionSettable(opt_num)))
  else
  {
    switch(mpScanner->getOptionType(opt_num))
    {
      case SANE_TYPE_BUTTON:
        qbo = new QButtonOption(mpScanner->getOptionTitle(opt_num),parent,//qvbox,
                                (const char*)mpScanner->getOptionName(opt_num));
        qbo->setOptionDescription(mpScanner->getOptionDescription(opt_num));
        qbo->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
        qbo->setSaneValueType(mpScanner->getOptionType(opt_num));
        qbo->setSaneOptionNumber(opt_num);
        connect(qbo,SIGNAL(signalOptionChanged(int)),this,
                SLOT(slotOptionChanged(int)));
        ret = (QSaneOption*)qbo;
        break;
      case SANE_TYPE_BOOL:
        //don't add preview option
        if(mpScanner->previewOption() == opt_num) break;
        qbool = new QBoolOption(mpScanner->getOptionTitle(opt_num),parent,//qvbox,
                                (const char*)mpScanner->getOptionName(opt_num));
        qbool->setOptionDescription(mpScanner->getOptionDescription(opt_num));
        qbool->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
        qbool->setSaneValueType(mpScanner->getOptionType(opt_num));
        qbool->setSaneOptionNumber(opt_num);
        //CAP_AUTOMATIC ?
        if(mpScanner->automaticOption(opt_num))
        {
          qbool->enableAutomatic(true);
          connect(qbool,SIGNAL(signalAutomatic(int,bool)),
                  this,SLOT(slotAutoMode(int,bool)));
        }
        connect(qbool,SIGNAL(signalOptionChanged(int)),this,
                SLOT(slotOptionChanged(int)));
        ret = (QSaneOption*)qbool;
        break;
      case SANE_TYPE_INT:
        if(mpScanner->optionValueSize(opt_num) == sizeof(SANE_Word))
        {
          i_val = (SANE_Int) mpScanner->saneWordValue(opt_num);
          if(mpScanner->getConstraintType(opt_num)==SANE_CONSTRAINT_RANGE)
          {
            qsb = new QScrollBarOption(mpScanner->getOptionTitle(opt_num),parent,
                                       SANE_TYPE_INT,(const char*)mpScanner->getOptionName(opt_num));
            qsb->setSaneOptionNumber(opt_num);
            qsb->setRange(int(mpScanner->getRangeMin(opt_num)),
                          int(mpScanner->getRangeMax(opt_num)),
                          int(mpScanner->getRangeQuant(opt_num)));
            qsb->setUnit(mpScanner->getUnit(opt_num));
            //if unit mm then connect to signalMetricSystem
            if(mpScanner->getUnit(opt_num) == SANE_UNIT_MM)
               connect(this,
                       SIGNAL(signalMetricSystem(QIN::MetricSystem)),
                       qsb,
                       SLOT(slotChangeMetricSystem(QIN::MetricSystem)));
            qsb->setValue(i_val);
            qsb->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            qsb->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            qsb->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(qsb,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)qsb;
          }
          else if(mpScanner->getConstraintType(opt_num)==SANE_CONSTRAINT_NONE)
          {
            sint = new SaneIntOption(mpScanner->getOptionTitle(opt_num),parent,
                                      SANE_TYPE_INT,(const char*)mpScanner->getOptionName(opt_num));
            sint->setSaneOptionNumber(opt_num);
            sint->setUnit(mpScanner->getUnit(opt_num));
            sint->setValue(i_val);
            sint->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            sint->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            sint->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(sint,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)sint;
          }
          else if(mpScanner->getConstraintType(opt_num)==SANE_CONSTRAINT_WORD_LIST)
          {
            qwco = new QWordComboOption(mpScanner->getOptionTitle(opt_num),parent,
                                SANE_TYPE_INT,(const char*)mpScanner->getOptionName(opt_num));
            //if unit mm then connect to option widget
            qwco->appendArray(mpScanner->saneWordList(opt_num));
            qwco->setSaneOptionNumber(opt_num);
            //CAP_AUTOMATIC ?
            if(mpScanner->automaticOption(opt_num))
            {
              qwco->enableAutomatic(true);
              connect(qwco,SIGNAL(signalAutomatic(int,bool)),
                      this,SLOT(slotAutoMode(int,bool)));
            }
            qwco->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            qwco->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            qwco->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(qwco,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)qwco;
          }
        }
        else if(mpScanner->optionValueSize(opt_num) > int(sizeof(SANE_Word)))//we have a vector
        {
          if(mpScanner->getConstraintType(opt_num)==SANE_CONSTRAINT_RANGE)
          {
            qwao = new QWordArrayOption(mpScanner->getOptionTitle(opt_num),parent,
                                        SANE_TYPE_INT,(const char*)mpScanner->getOptionName(opt_num));
            qwao->setSaneOptionNumber(opt_num);
            qwao->setOptionSize(mpScanner->optionValueSize(opt_num));
            qwao->setRange(int(mpScanner->getRangeMin(opt_num)),
                             int(mpScanner->getRangeMax(opt_num)));
            qwao->setQuant(mpScanner->getRangeQuant(opt_num));
            qwao->setValue(mpScanner->saneWordArray(opt_num));
            qwao->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            qwao->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            qwao->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(qwao,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)qwao;
          }
        }
        break;
      case SANE_TYPE_FIXED:
        if(mpScanner->optionValueSize(opt_num) == sizeof(SANE_Word))
        {
          f_val = (SANE_Fixed)mpScanner->saneWordValue(opt_num);
          if(mpScanner->getConstraintType(opt_num) == SANE_CONSTRAINT_RANGE)
          {
            qsf = new QScrollBarOption(mpScanner->getOptionTitle(opt_num),parent,
                                       SANE_TYPE_FIXED,(const char*)mpScanner->getOptionName(opt_num));
            qsf->setSaneOptionNumber(opt_num);
            qsf->setRange(mpScanner->getRangeMin(opt_num),
                          mpScanner->getRangeMax(opt_num),
                          mpScanner->getRangeQuant(opt_num));
            qsf->setUnit(mpScanner->getUnit(opt_num));
            //if unit mm then connect to signalMetricSystem
            if(mpScanner->getUnit(opt_num) == SANE_UNIT_MM)
            {
              connect(this,
                      SIGNAL(signalMetricSystem(QIN::MetricSystem)),
                      qsf,
                      SLOT(slotChangeMetricSystem(QIN::MetricSystem)));
            }
            qsf->setValue(f_val);
            qsf->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            qsf->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            qsf->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(qsf,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)qsf;
          }
          else if(mpScanner->getConstraintType(opt_num)==SANE_CONSTRAINT_NONE)
          {
            sfix = new SaneFixedOption(mpScanner->getOptionTitle(opt_num),parent,
                                       SANE_TYPE_INT,(const char*)mpScanner->getOptionName(opt_num));
            sfix->setSaneOptionNumber(opt_num);
            sfix->setUnit(mpScanner->getUnit(opt_num));
            sfix->setValue(f_val);
            sfix->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            sfix->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            sfix->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(sfix,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)sfix;
          }
          else if(mpScanner->getConstraintType(opt_num)==SANE_CONSTRAINT_WORD_LIST)
          {
            qwco = new QWordComboOption(mpScanner->getOptionTitle(opt_num),parent,
                                        SANE_TYPE_FIXED,(const char*)mpScanner->getOptionName(opt_num));
            qwco->setSaneOptionNumber(opt_num);
            //if unit mm then connect to option widget
            qwco->appendArray(mpScanner->saneWordList(opt_num));
            //CAP_AUTOMATIC ?
            if(mpScanner->automaticOption(opt_num))
            {
              qwco->enableAutomatic(true);
              connect(qwco,SIGNAL(signalAutomatic(int,bool)),
                      this,SLOT(slotAutoMode(int,bool)));
            }
            qwco->setOptionDescription(mpScanner->getOptionDescription(opt_num));
            qwco->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
            qwco->setSaneValueType(mpScanner->getOptionType(opt_num));
            connect(qwco,SIGNAL(signalOptionChanged(int)),this,
                    SLOT(slotOptionChanged(int)));
            ret = (QSaneOption*)qwco;
          }
        }
        break;
      case SANE_TYPE_STRING:
        if(mpScanner->getConstraintType(opt_num) == SANE_CONSTRAINT_STRING_LIST)
        {
          qcb = new QComboOption(mpScanner->getOptionTitle(opt_num).toLatin1(),
                                 parent,//qvbox,
                                 (const char*)mpScanner->getOptionName(opt_num));
          qcb->setSaneOptionNumber(opt_num);
          QStringList slist = mpScanner->getStringList(opt_num);
          qcb->setStringList(slist);
          //CAP_AUTOMATIC ?
          if(mpScanner->automaticOption(opt_num))
          {
            qcb->enableAutomatic(true);
            connect(qcb,SIGNAL(signalAutomatic(int,bool)),
                    this,SLOT(slotAutoMode(int,bool)));
          }
          qcb->setOptionDescription(mpScanner->getOptionDescription(opt_num));
          qcb->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
          qcb->setSaneValueType(mpScanner->getOptionType(opt_num));
          stringval = mpScanner->saneStringValue(opt_num);
          qcb->setCurrentValue(stringval.toLatin1());
          connect(qcb,SIGNAL(signalOptionChanged(int)),this,
                  SLOT(slotOptionChanged(int)));
          ret = (QSaneOption*)qcb;
        }
        if(mpScanner->getConstraintType(opt_num) == SANE_CONSTRAINT_NONE)
        {
          qso = new QStringOption(mpScanner->getOptionTitle(opt_num).toLatin1(),
                                  parent,//qvbox,
                                  (const char*)mpScanner->getOptionName(opt_num));
          qso->setSaneOptionNumber(opt_num);
          qso->setOptionDescription(mpScanner->getOptionDescription(opt_num));
          qso->setSaneConstraintType(mpScanner->getConstraintType(opt_num));
          qso->setSaneValueType(mpScanner->getOptionType(opt_num));
          qso->setMaxLength(mpScanner->optionSize(opt_num)-1);
          stringval = mpScanner->saneStringValue(opt_num);
          qso->setText(stringval.toLatin1());
          connect(qso,SIGNAL(signalOptionChanged(int)),this,
                  SLOT(slotOptionChanged(int)));
          ret=(QSaneOption*)qso;
        }
        break;
      default:;
    }
  }
//  qDebug("create saneoption widget - leave");
  return ret;
}
/** No descriptions */
QGroupBox* QScanDialog::createOptionGroupBox(QString title,int firstoption,int lastoption)
{
  QGroupBox* qgb       = 0;
  QSaneOption* widget_pointer = 0;
  QString stringval    = QString();
  int c2               = 0;
  //create a group box with a QVBoxLayout
  qgb = new QGroupBox(title);
  QVBoxLayout *vbox = new QVBoxLayout;
// lastoption-firstoption+1,Qt::Vertical,
//                       mpOptionScrollView->getMainWidget()
//   qgb->setFrameStyle(QGroupBox::StyledPanel);
//   qgb->setTitle();

  for(c2=firstoption;c2<=lastoption;c2++)
  {
    SaneWidgetHolder* swh = new SaneWidgetHolder(qgb);
    widget_pointer = createSaneOptionWidget(swh,c2);
    if(widget_pointer)
    {
      swh->addWidget(widget_pointer);
      mOptionWidgets.resize(mOptionWidgets.size()+1);
      mOptionWidgets[mOptionWidgets.size()-1] = swh;
      widget_pointer->setOptionNumber(mOptionWidgets.size()-1);
      vbox->addWidget(widget_pointer);
// printf ("   %d: size %d,%d\n", c2, vbox->minimumSize ().width (), vbox->minimumSize ().height ());
    }
    else
      delete swh;
  }
  qgb->setLayout(vbox);
  mGroupBoxArray.resize(mGroupBoxArray.size()+1);
  mGroupBoxArray[mGroupBoxArray.size()-1] = qgb;
// printf ("qgb size %d,%d\n", vbox->minimumSize ().width (), vbox->minimumSize ().height ());
//   qgb->setMinimumSize(vbox->minimumSize ());
  return qgb;
}

/**  */
void QScanDialog::createPreviewWidget()
{
//Currently the size widget is only created
//if all scan area options are available
  int cnt;
  QScrollBarOption* qsbo;
  SANE_Unit unit;

	mpPreviewWidget = 0L;
//at the moment the preview widget is only supported
//if the scan area options can be represented by
//scrollbars
  mpTlxOption = 0L;
  mpTlyOption = 0L;
  mpBrxOption = 0L;
  mpBryOption = 0L;
  mpWidthOption = 0L;
  mpHeightOption = 0L;

  for(cnt=0;cnt<int(mOptionWidgets.size());cnt++)
  {
    qsbo = 0L;
    QObject *obj = (QObject*)(mOptionWidgets[cnt]->saneOption());
    QString objname = obj->metaObject()->className();
    if (objname == "QScrollBarOption")
 	    qsbo=(QScrollBarOption*)(mOptionWidgets[cnt]->saneOption());
    if(qsbo)
		{
	  	if(qsbo->optionName()=="tl-x") mpTlxOption = qsbo;
			if(qsbo->optionName()=="tl-y") mpTlyOption = qsbo;
			if(qsbo->optionName()=="br-x")	mpBrxOption = qsbo;
			if(qsbo->optionName()=="br-y")	mpBryOption = qsbo;
            if(qsbo->optionName()=="pagewidth")  mpWidthOption = qsbo;
            if(qsbo->optionName()=="pageheight")  mpHeightOption = qsbo;
#ifdef SANE_NAME_PAGE_WIDTH
            if (qsbo->optionName()==SANE_NAME_PAGE_WIDTH) mpWidthOption = qsbo;
            if (qsbo->optionName()==SANE_NAME_PAGE_HEIGHT) mpHeightOption = qsbo;
#endif
    }
  }
//check whether all scan area options are present
	if((mpTlxOption) && (mpTlyOption) &&
     (mpBrxOption) && (mpBryOption))
	{
   //check whether unit is of type SANE_UNIT_MM
   //or SANE_UNIT_PIXEL
   //it's sufficient to check one option
   //since all scan area option must have the same unit
    unit = mpScanner->getUnit(mpTlxOption->saneOptionNumber());
    if((unit == SANE_UNIT_MM) || (unit == SANE_UNIT_PIXEL))
    {
      //create preview widget#endif
      if(xmlConfig->boolValue("SEPARATE_PREVIEW"))
      {
        mpPreviewWidget = new PreviewWidget(this, "", Qt::Window |
            Qt::WindowContextHelpButtonHint | Qt::WindowTitleHint |
            Qt::MSWindowsFixedSizeDialogHint | Qt::WindowSystemMenuHint);
        mpMainLayout->setColumnStretch(0,1);
        mpMainLayout->setColumnStretch(2,0);
      }
      else
      {
        mpPreviewWidget = new PreviewWidget(this, "", Qt::WindowFlags());
        mpMainLayout->setColumnStretch(0,0);
        mpMainLayout->setColumnStretch(2,1);
      }
      mpPreviewWidget->setMetrics(QIN::Millimetre,unit);
      setPreviewRange();
      connect(mpPreviewWidget,SIGNAL(signalTlxPercent(double)),
              mpTlxOption,SLOT(slotSetPercentValue(double)));
      connect(mpPreviewWidget,SIGNAL(signalTlyPercent(double)),
              mpTlyOption,SLOT(slotSetPercentValue(double)));
      connect(mpPreviewWidget,SIGNAL(signalBrxPercent(double)),
              mpBrxOption,SLOT(slotSetPercentValue(double)));
      connect(mpPreviewWidget,SIGNAL(signalBryPercent(double)),
              mpBryOption,SLOT(slotSetPercentValue(double)));
      connect(mpTlxOption,SIGNAL(signalValuePercent(double)),
              mpPreviewWidget,SLOT(slotSetTlxPercent(double)));
      connect(mpTlyOption,SIGNAL(signalValuePercent(double)),
              mpPreviewWidget,SLOT(slotSetTlyPercent(double)));
      connect(mpBrxOption,SIGNAL(signalValuePercent(double)),
              mpPreviewWidget,SLOT(slotSetBrxPercent(double)));
      connect(mpBryOption,SIGNAL(signalValuePercent(double)),
              mpPreviewWidget,SLOT(slotSetBryPercent(double)));
      connect(mpPreviewWidget,SIGNAL(signalPredefinedSize(ScanArea*)),
              this,SLOT(slotSetPredefinedSize(ScanArea*)));
      connect(mpPreviewWidget,SIGNAL(signalHidePreview()),
              this,SLOT(slotHidePreview()));
      connect(mpPreviewWidget,SIGNAL(signalMultiSelectionMode(bool)),
              this,SLOT(slotMultiSelectionMode(bool)));
      connect(mpPreviewWidget,SIGNAL(signalEnableScanAreaOptions(bool)),
              this,SLOT(slotEnableScanAreaOptions(bool)));
    }
  }
  else
	{
    //create preview widget
printf ("separate\n");
    if(xmlConfig->boolValue("SEPARATE_PREVIEW"))
    {
      mpPreviewWidget = new PreviewWidget(this,"",Qt::Window |
          Qt::WindowContextHelpButtonHint | Qt::WindowTitleHint |
          Qt::MSWindowsFixedSizeDialogHint | Qt::WindowSystemMenuHint);
      mpMainLayout->setColumnStretch(0,1);
      mpMainLayout->setColumnStretch(2,0);
    }
    else
    {
      mpPreviewWidget = new PreviewWidget(this, "", Qt::WindowFlags());
      mpMainLayout->setColumnStretch(0,0);
      mpMainLayout->setColumnStretch(2,1);
    }
    mpPreviewWidget->setMetrics(QIN::NoMetricSystem,SANE_UNIT_NONE);
    connect(mpPreviewWidget,SIGNAL(signalHidePreview()),
            this,SLOT(slotHidePreview()));
  }
}
/**  */
void QScanDialog::slotPreviewSize(QRect rect)
{
  SANE_Word tlx1;
  SANE_Word tly1;
  SANE_Word brx1;
  SANE_Word bry1;

  //s i1 = mpTlxOption->saneOptionNumber();
  tlx1 = (SANE_Word) rect.left();
  //s tlx2 = tlx1;
  mpTlxOption->setValueExt(tlx1);

  //s i2 = mpTlyOption->saneOptionNumber();
  tly1 = (SANE_Word) rect.top();
  //s tly2 = tly1;
  mpTlyOption->setValueExt(tly1);

  //i3 = mpBrxOption->saneOptionNumber();
  brx1 = (SANE_Word) rect.right();
  //s brx2 = brx1;
  mpBrxOption->setValueExt(brx1);

  //i4 = mpBryOption->saneOptionNumber();
  bry1 = (SANE_Word) rect.bottom();
  //s bry2 = bry1;
  mpBryOption->setValueExt(bry1);
//reset, just to be sure
  mpTlxOption->setValueExt(tlx1);
  mpTlyOption->setValueExt(tly1);
}

void QScanDialog::slotSetPredefinedSize(ScanArea* sca)
{
  double tlx,tly,brx,bry;

  // set page size if required
  if (mpWidthOption
     && mpScanner->isOptionActive(mpWidthOption->saneOptionNumber()))
  {
//    printf ("change slot under ADF\n");
    QString oldname = sca->getName ();
    setPageSize (sca->width (), sca->height ());
    setPreviewRange ();

    // this may invalidate sca, so find it again, by name!
    QString name;
    int i = 0;

    do
    {
      name = mpPreviewWidget->getSizeName (i);
      if (name == oldname)
         sca = mpPreviewWidget->getSize (i);
      i++;
    } while (!name.isEmpty());
    if (!sca)
    {
      printf ("Warning: size lost\n");
      return;
    }
  }

//  if(!sca->isValid()) return;
  tlx = sca->tlx();
  tly = sca->tly();
  brx = sca->brx();
  bry = sca->bry();
   printf ("%1.2lf %1.2lf %1.2lf %1.2lf, valid=%d\n",
      sca->tlx(),sca->tly(),sca->brx(),sca->bry(), sca->isValid ());
//change scrollbar options
  mpTlxOption->slotSetPercentValue(tlx);
  mpTlyOption->slotSetPercentValue(tly);
  mpBrxOption->slotSetPercentValue(brx);
  mpBryOption->slotSetPercentValue(bry);
//set tlx/tly option again (to be sure)
  mpTlxOption->slotSetPercentValue(tlx);
  mpTlyOption->slotSetPercentValue(tly);
}

/**  */
void QScanDialog::changeLayout(QIN::Layout l)
{
  //check whether a change is necessary at all
  QString titlestring;
  if(mLayout == l) return;
  int c;
  mLayout = l;
  QGroupBox* qgb;
  QWidget* qw;
  QVBoxLayout* qvbl;
  QVBoxLayout* qvbl2;
  QPushButton* qpb1;
  QPushButton* qpb2;
  QPoint p(0,0);
  if(mLayout == QIN::ScrollLayout)
  {
    mpOptionScrollView = new QOptionScrollView(this);
    mpOptionScrollView->setFrameStyle(QFrame::NoFrame);
    mpOptionScrollView->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
//     mpOptionScrollView->setHScrollBarMode(Q3ScrollView::AlwaysOff);
    //reparent the groupboxes in mGroupBoxArray
    //this way there's no need to requery the device
    for(c=0;c<mGroupBoxArray.size();c++)
    {
      qgb=(QGroupBox*)mGroupBoxArray[c];
      qgb->setParent(mpOptionScrollView->widget());
      qgb->move(p);
      qgb->hide();
//       qgb->setFrameStyle(QGroupBox::StyledPanel);
      mpOptionScrollView->addWidget(qgb);
    }
    if(mpOptionTabWidget)
    {
      delete mpOptionTabWidget;
      mpOptionTabWidget = 0;
    }
    if(mOptionSubArray.size() > 0)
    {
      for(c=0;c<mOptionSubArray.size();c++)
        delete mOptionSubArray[c];
      mOptionSubArray.resize(0);
    }
    if(mpOptionMainWidget)
    {
      delete mpOptionMainWidget;
      mpOptionMainWidget = 0;
    }
    if(mpOptionListWidget)
    {
      delete mpOptionListWidget;
      mpOptionListWidget = 0;
    }
    mpMainLayout->addWidget(mpOptionScrollView,5,0);
    mpOptionScrollView->show();
    //hack to get it correctly resized
    resize(width()+1,height()+1);
    qApp->processEvents();
    resize(width()-1,height()-1);
  }
  if(mLayout == QIN::TabLayout)
  {
    mpOptionTabWidget = new QTabWidget(this);
    mpMainLayout->addWidget(mpOptionTabWidget,5,0);
    //reparent the groupboxes in mGroupBoxArray
    //this way there's no need to requery the device
    for(c=0;c<mGroupBoxArray.size();c++)
    {
      qw = new QWidget(mpOptionTabWidget);
      qvbl2 = new QVBoxLayout(qw);
      qgb=(QGroupBox*)mGroupBoxArray[c];
      qgb->setParent(qw);
      qgb->move(p);
      qgb->hide();
//       qgb->setFrameStyle(QGroupBox::NoFrame);
      qvbl2->addWidget(qgb,0);
      qvbl2->addStretch(1);
      titlestring = tr("&%1. ").arg(c+1);
      titlestring += qgb->title();
      mpOptionTabWidget->addTab(qw,titlestring);
    }
    if(mpOptionScrollView)
    {
      delete mpOptionScrollView;
      mpOptionScrollView = 0;
    }
    if(mOptionSubArray.size() > 0)
    {
      for(c=0;c<mOptionSubArray.size();c++)
        delete mOptionSubArray[c];
      mOptionSubArray.resize(0);
    }
    if(mpOptionMainWidget)
    {
      delete mpOptionMainWidget;
      mpOptionMainWidget = 0;
    }
    if(mpOptionListWidget)
    {
      delete mpOptionListWidget;
      mpOptionListWidget = 0;
    }
    //stupid, but works (?)
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionTabWidget->hide();
    mpOptionTabWidget->show();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    if(!mpPreviewWidget)
      resize(width(),minimumSizeHint().height());
    else if(mpPreviewWidget->topLevel())
      resize(width(),minimumSizeHint().height());
  }
  if(mLayout == QIN::MultiWindowLayout)
  {
    mpOptionMainWidget = new QWidget(this);
    qvbl = new QVBoxLayout(mpOptionMainWidget);
    mOptionSubArray.resize(0);
    if(mGroupBoxArray.size()>=1)
    {
      qgb=(QGroupBox*)mGroupBoxArray[0];
//       qgb->setFrameStyle(QGroupBox::StyledPanel);
      qgb->setParent(mpOptionMainWidget);
      qgb->move(p);
      qgb->hide();

      qvbl->addWidget(qgb);
      qvbl->setStretchFactor(qgb,1);
      for(c=1;c<mGroupBoxArray.size();c++)
      {
        titlestring = tr("&%1. ").arg(c+1);
        titlestring += mGroupBoxArray[c]->title();
        qpb1 = new QPushButton(titlestring,mpOptionMainWidget);
        qvbl->addWidget(qpb1);
        mOptionSubArray.resize(mOptionSubArray.size()+1);
        mOptionSubArray[mOptionSubArray.size()-1] = new QWidget(0);
        mOptionSubArray[c-1]->setWindowTitle((mGroupBoxArray[c])->title());
        qvbl2 = new QVBoxLayout(mOptionSubArray[c-1]);
        connect(qpb1,SIGNAL(clicked()),mOptionSubArray[c-1],SLOT(show()));
        qgb=(QGroupBox*)mGroupBoxArray[c];
//         qgb->setFrameStyle(QGroupBox::StyledPanel);
        qgb->setParent(mOptionSubArray[c-1]);
        qgb->move(p);
        qgb->hide();

        qvbl2->addWidget(qgb);
        qpb2 = new QPushButton(tr("&Close"),mOptionSubArray[c-1]);
        connect(qpb2,SIGNAL(clicked()),mOptionSubArray[c-1],SLOT(close()));
        qvbl2->addWidget(qpb2);
      }
    }
    if(mpOptionTabWidget)
    {
      delete mpOptionTabWidget;
      mpOptionTabWidget = 0;
    }
    if(mpOptionScrollView)
    {
      delete mpOptionScrollView;
      mpOptionScrollView = 0;
    }
    if(mpOptionListWidget)
    {
      delete mpOptionListWidget;
      mpOptionListWidget = 0;
    }
    mpMainLayout->addWidget(mpOptionMainWidget,5,0);

    //stupid, but works (?)
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionMainWidget->hide();
    mpOptionMainWidget->show();
    qApp->processEvents();
    QApplication::sendPostedEvents();
    if(!mpPreviewWidget)
      resize(width(),minimumSizeHint().height());
    else if(mpPreviewWidget->topLevel())
      resize(width(),minimumSizeHint().height());
  }
  if(mLayout == QIN::ListLayout)
  {
    mpOptionListWidget = new QWidget(this);
    QGridLayout* listgrid = new QGridLayout(mpOptionListWidget);
    listgrid->setSpacing(5);
    listgrid->setColumnStretch(1,1);
    mpOptionListView = new QTableWidget(mpOptionListWidget);
//p    mpOptionListView->addColumn(tr("SANE Options"));
//p    mpOptionListView->setSorting(-1);
    mpOptionWidgetStack = new QStackedWidget(mpOptionListWidget);
    listgrid->addWidget(mpOptionListView,0,0);
    listgrid->addWidget(mpOptionWidgetStack,0,1);
    QListViewItemExt* nlv = 0;
    for(c=0;c<mGroupBoxArray.size();c++)
    {
      titlestring = mGroupBoxArray[c]->title();
      if(!nlv)
      {
        nlv = new QListViewItemExt(mpOptionListView,titlestring);
        nlv->setIndex(c);
//p        mpOptionListView->setSelected(nlv,true);
      }
      else
      {
        nlv = new QListViewItemExt(mpOptionListView,(QTableWidgetItem*)nlv,
                                        titlestring);
        nlv->setIndex(c);
      }
      qgb=(QGroupBox*)mGroupBoxArray[c];
//       qgb->setFrameStyle(QGroupBox::StyledPanel);
      //this also reparents the widgets
      mpOptionWidgetStack->addWidget(qgb);
    }
    if(mOptionSubArray.size() > 0)
    {
      for(c=0;c<mOptionSubArray.size();c++)
        delete mOptionSubArray[c];
      mOptionSubArray.resize(0);
    }
    if(mpOptionMainWidget)
    {
      delete mpOptionMainWidget;
      mpOptionMainWidget = 0;
    }
    if(mpOptionTabWidget)
    {
      delete mpOptionTabWidget;
      mpOptionTabWidget = 0;
    }
    if(mpOptionScrollView)
    {
      delete mpOptionScrollView;
      mpOptionScrollView = 0;
    }
    mpMainLayout->addWidget(mpOptionListWidget,5,0);
    connect(mpOptionListView,SIGNAL(selectionChanged(QListWidgetItem*)),
            this,SLOT(slotRaiseOptionWidget(QListWidgetItem*)));
    mpOptionListView->setMinimumWidth(mpOptionListView->sizeHint().width()+8);
    mpOptionListWidget->show();
    //stupid, but works (?)
    qApp->processEvents();
    QApplication::sendPostedEvents();
    mpOptionWidgetStack->hide();
    mpOptionWidgetStack->show();
    mpOptionWidgetStack->resize(mpOptionWidgetStack->width(),
                              mpOptionWidgetStack->sizeHint().height());
    mpOptionWidgetStack->setCurrentIndex(0);
    qApp->processEvents();
    QApplication::sendPostedEvents();
    if(!mpPreviewWidget)
      resize(width(),minimumSizeHint().height());
    else if(mpPreviewWidget->topLevel())
      resize(width(),minimumSizeHint().height());
//p    mpOptionListView->setColumnWidthMode(0,QListWidget::Manual);
    mpOptionListView->setColumnWidth(0,mpOptionListView->viewport()->width());
  }
  if(mLayout != QIN::ScrollLayout)
  {
    if(!mpPreviewWidget)
      resize(minimumSizeHint());
    else if(mpPreviewWidget->topLevel() || !mpPreviewWidget->isVisible())
      resize(minimumSizeHint());
  }
  if(mpPreviewWidget)
  {
    if(mpPreviewWidget->topLevel())
    {
      mpMainLayout->setColumnStretch(0,1);
      mpMainLayout->setColumnStretch(2,0);
    }
    else
    {
      mpMainLayout->setColumnStretch(0,0);
      mpMainLayout->setColumnStretch(2,1);
    }
  }
}
/**  */
QIN::Status QScanDialog::status()
{
  return mStatus;
}
/**  */
void QScanDialog::slotImageInfo()
{
  double mbsize;
//  int w,h,xres,yres;
  QString qs;
  qs = mpScanner->imageInfo();
  int warn_size = xmlConfig->intValue("SCAN_SIZE_WARNING",2);
  mbsize = mpScanner->imageInfoMB();
  if(mbsize > warn_size)
  {
    mpLabelImageInfo->setPalette( QPalette( QColor(240, 30, 30) ) );
    qs = "<b>" + qs +" !" + "<b>";
  }
  else
    mpLabelImageInfo->setPalette(palette());
  mpLabelImageInfo->setText(qs);
  if(mpMultiScanWidget)
  {
#if 0 //s
    w = mpScanner->pixelWidth();
    h = mpScanner->pixelHeight();
    if(h <= 0) w = 0;
    xres = mpScanner->xResolutionDpi();
    yres = mpScanner->yResolutionDpi();
    if(yres == 0) yres = xres;
    mpMultiScanWidget->setImageValues(w,h,xres,yres);
#endif //s
  }
}
/** This slot is called, when setting an option results
in an return value SANE_INFO_INEXACT.
This normally happens, when the value set in
the backend  is different from the requested value. */
void QScanDialog::slotInfoInexact(int num)
{
  SANE_Int i_val;
  SANE_Fixed f_val;
  SANE_Bool b_val;
  QString stringval;
  QScrollBarOption* qsbo;
  QBoolOption* qboolo;
  QWordComboOption* qwco;
  QStringOption* qso;
  int cnt;

  // protect against recursion
  if (mIgnoreInexact)
     return;
  mIgnoreInexact++;

  for(cnt=0;cnt<int(mOptionWidgets.size());cnt++)
  {
    qsbo = 0L;
    QObject *obj = (QObject*)(mOptionWidgets[cnt]->saneOption());
    QString objname = obj->metaObject()->className();
    if (objname == "QScrollBarOption")
        qsbo=(QScrollBarOption*)(mOptionWidgets[cnt]->saneOption());
    if(qsbo)
    {
    //scrollbar option found
      if(qsbo->saneOptionNumber() == num)
      {
        if(qsbo->getSaneType() == SANE_TYPE_INT)
        {
          i_val = (SANE_Int)mpScanner->saneWordValue(qsbo->saneOptionNumber());
//        printf ("qsbo int slotInfoInexact %d\n", i_val);
          qsbo->setValue(i_val);
          break;
        }
        if(qsbo->getSaneType() == SANE_TYPE_FIXED)
        {
          f_val = (SANE_Fixed)mpScanner->saneWordValue(qsbo->saneOptionNumber());
//        printf ("qsbo fixed slotInfoInexact %d\n", f_val);
          qsbo->setValue(f_val);
          break;
        }
      }
    }
    qwco = 0L;
    if (objname == "QWordComboOption")
      qwco=(QWordComboOption*)(mOptionWidgets[cnt]->saneOption());
    if(qwco)
    {
      if(qwco->saneOptionNumber() == num)
      {
        i_val = (SANE_Int)mpScanner->saneWordValue(qwco->saneOptionNumber());
//        printf ("qwco slotInfoInexact %d\n", i_val);
        qwco->setValue(i_val);
        break;
      }
    }
    qboolo = 0L;
    if (objname == "QBoolOption")
      qboolo=(QBoolOption*)(mOptionWidgets[cnt]->saneOption());
    if(qboolo)
    {
      if(qboolo->saneOptionNumber() == num)
      {
        b_val = (SANE_Bool)mpScanner->saneWordValue(qboolo->saneOptionNumber());
        qboolo->setState(b_val);
        break;
      }
    }
    //check whether it's a string option
    qso = 0L;
    if (objname == "QStringOption")
      qso=(QStringOption*)(mOptionWidgets[cnt]->saneOption());
    QString optionstring;
    if(qso)
    {
      //always string type
      if(qso->saneOptionNumber() == num)
      {
        stringval = mpScanner->saneStringValue(qso->saneOptionNumber());
        qso->setText(stringval.toLatin1());
        break;
      }
    }
  }
  mIgnoreInexact--;
}

void QScanDialog::closeEvent(QCloseEvent* e)
{
  UNUSED(e);
  xmlConfig->writeConfigFile();
  QDeviceSettings ds(mpScanner);
  ds.saveDeviceSettings("Last settings");
  if(xmlConfig->boolValue("HISTORY_DELETE_EXIT"))
  {
    QFile::remove(xmlConfig->absConfDirPath()+"history.xml");
  }
  emit closed ();
}

/**  */
void QScanDialog::slotChangeMode(int index)
{
  int scan_mode;
  QString qs;
  switch(index)
  {
    case 0:
      scan_mode = QIN::Temporary;
      break;
    case 1:
      scan_mode = QIN::SingleFile;
      break;
    case 2:
      scan_mode = QIN::OCR;
      break;
    case 3:
      scan_mode = QIN::CopyPrint;
      break;
    case 4:
      scan_mode = QIN::MultiScan;
      break;
    case 5:
      scan_mode = QIN::Direct;
      break;
    default://huh, shouldn't happen ?
      scan_mode = QIN::SingleFile;
  }
  if(scan_mode == QIN::Direct)
  {
    qApp->processEvents();
    if(!mpPreviewWidget)
      resize(width(),minimumSizeHint().height());
    else if(mpPreviewWidget->topLevel())
      resize(width(),minimumSizeHint().height());
  }
  if(mpPreviewWidget)
  {
    if(scan_mode == QIN::SingleFile)
      mpPreviewWidget->enableMultiSelection(false);
    else
      mpPreviewWidget->enableMultiSelection(true);
  }
  xmlConfig->setIntValue("SCAN_MODE",int(scan_mode));
}

/**  */
void QScanDialog::enableGUI(bool enable,bool preview_scan)
{
  int i;
//s  mpModeHBox->setEnabled(enable);
  mpInfoHBox->setEnabled(enable);
  mpDragHBox1->setEnabled(enable);
  mpDragHBox2->setEnabled(enable);
  mpButtonHBox1->setEnabled(enable);
  mpButtonHBox2->setEnabled(enable);
  switch(mLayout)
  {
    case QIN::MultiWindowLayout:
      for(i=0;i<mOptionWidgets.size();i++)
        ((QWidget*)(mOptionWidgets[i]->saneOption()))->setEnabled(enable);
      mpOptionMainWidget->setEnabled(enable);
      break;
    case QIN::ScrollLayout:
      mpOptionScrollView->setEnabled(enable);
      break;
    case QIN::TabLayout:
      mpOptionTabWidget->setEnabled(enable);
      break;
    case QIN::ListLayout:
      mpOptionListWidget->setEnabled(enable);
      break;
  }
#if 0 //s
  if(mpMultiScanWidget)
    mpMultiScanWidget->setEnabled(enable);
#endif //s
  if(mpPreviewWidget && (preview_scan != true))
    mpPreviewWidget->setEnabled(enable);
  if(mMultiSelectionMode)
    slotEnableScanAreaOptions(false);
}
/**  */
void QScanDialog::slotAutoMode(int num,bool b)
{
  SANE_Int   si;
  QBoolOption*      qboolo;
  QComboOption*     qco;
  QWordComboOption* qwco;
  QString combostring;
  int i;
  SANE_Bool sb;
  i = 0;
  qco = 0L;
  QObject *obj = (QObject*)(mOptionWidgets[num]->saneOption());
  QString objname = obj->metaObject()->className();
  if (objname == "QComboOption")
    qco=(QComboOption*)(mOptionWidgets[num]->saneOption());
  if(qco)
  {
    //always string type
    i = qco->saneOptionNumber();
    if(!b)
    {
      combostring = qco->getCurrentText();
      if (!combostring.isEmpty())
          mpScanner->setOption(i,(SANE_String*)combostring.toLatin1().constData());
    }
    else
      mpScanner->setOption(i,0L,true);
    return;
  }
  qboolo = 0L;
  if (objname == "QBoolOption")
    qboolo=(QBoolOption*)(mOptionWidgets[num]->saneOption());
  if(qboolo)
  {
    //always string type
    i = qboolo->saneOptionNumber();
    if(!b)
    {
      sb = qboolo->state();
      mpScanner->setOption(i,&sb);
    }
    else if(mpScanner->setOption(i,0L,true) == SANE_STATUS_GOOD)
      qboolo->setState((SANE_Word)mpScanner->saneWordValue(i));
    return;
  }
  //check whether it's a word combo option
  qwco = 0L;
  if (objname == "QWordComboOption")
    qwco=(QWordComboOption*)(mOptionWidgets[num]->saneOption());
  if(qwco)
  {
    i = qwco->saneOptionNumber();
    if(!b)
    {
      si = qwco->getCurrentValue();
      mpScanner->setOption(i,&si);
    }
    else if(mpScanner->setOption(i,0L,true) == SANE_STATUS_GOOD)
      qwco->setValue((SANE_Word)mpScanner->saneWordValue(i));
    return;
  }
}
/**  */
void QScanDialog::showEvent(QShowEvent* se)
{
  QWidget::showEvent(se);
  //make sure that the window isn't heigher than the desktop
  //right after program start, at least in scrollview mode
  //and therefore also if the user starts Qis for the first time
  if((mLayout == QIN::ScrollLayout) && mShowCnt == 0)
  {
    qApp->processEvents();
    mpOptionScrollView->hide();
    qApp->processEvents();
    mpOptionScrollView->show();
    qApp->processEvents();

    // make it a little bit bigger
    int w = sizeHint().width() * 3/2;
    int h = qApp->desktop()->height()*2/3;
    if (sizeHint().height() < h)
       resize(w, h);
    if(height()>qApp->desktop()->height()*5/6)
      resize(width(),qApp->desktop()->height()*5/6);
    mShowCnt += 1;
  }
  if(!mpPreviewWidget)
    return;
  int w = xmlConfig->intValue("SCANDIALOG_INTEGRATED_PREVIEW_WIDTH",0);
  int h = xmlConfig->intValue("SCANDIALOG_INTEGRATED_PREVIEW_HEIGHT",0);
  if(!mpPreviewWidget->topLevel() && mpPreviewWidget->isVisible())
  {
    if(w < width())
      w = width();
    if(h < height())
      h = height();
    resize(w,h);
  }
}
/**  */
void QScanDialog::slotShowHelp()
{
#if 0 //s
  //crappy qt bug workaround
  if(mpHelpViewer->isMinimized())
    mpHelpViewer->hide();
  mpHelpViewer->show();
  mpHelpViewer->raise();
#endif //s
}
/**  */
void QScanDialog::slotDeviceSettings()
{
  QDeviceSettings devset(mpScanner,this);
  devset.exec();
}
/**  */
void QScanDialog::slotRaiseOptionWidget(QListWidgetItem* lvi)
{
  if(!lvi) return;
  QListViewItemExt* nlv;
  //to be sure
  if(!mpOptionListWidget || !mpOptionWidgetStack) return;
  nlv = (QListViewItemExt*) lvi;
  mpOptionWidgetStack->setCurrentIndex(nlv->index());
}
/**  */
void QScanDialog::slotHidePreview()
{
  if(mpPreviewWidget->topLevel()) return;
  mpPreviewWidget->hide();
//  mpSeparator->hide();
  qApp->processEvents();
  resize(minimumSizeHint());
}
/**  */
void QScanDialog::slotDragType(int index)
{

  xmlConfig->setIntValue("DRAG_IMAGE_TYPE",index);
}
/**  */
void QScanDialog::slotAutoName(bool b)
{
  xmlConfig->setBoolValue("DRAG_AUTOMATIC_FILENAME",b);
}
/**  */
void QScanDialog::slotDragFilename(const QString& qs)
{
  xmlConfig->setStringValue("DRAG_FILENAME",qs);
}
/**  */
void QScanDialog::slotImageSettings()
{
  QExtensionWidget ew(this);
  ew.setPage(4);
  ew.exec();
}

/** No descriptions */
void QScanDialog::slotMultiSelectionMode(bool state)
{
  mMultiSelectionMode = state;
  slotEnableScanAreaOptions(!state);
}
/** No descriptions */
void QScanDialog::slotEnableScanAreaOptions(bool on)
{
  //the scan area options are disabled in multi selection mode
  if(mpTlxOption)
    mpTlxOption->setEnabled(on);
  if(mpTlyOption)
    mpTlyOption->setEnabled(on);
  if(mpBrxOption)
    mpBrxOption->setEnabled(on);
  if(mpBryOption)
    mpBryOption->setEnabled(on);
}
/** No descriptions */
void QScanDialog::resizeScanArea()
{
}


void QScanRange::getRange (QScrollBarOption *Tlx, QScrollBarOption *Tly, QScrollBarOption *Brx, QScrollBarOption *Bry)
{

  mXmin = Tlx->minValue();
  mYmin = Tly->minValue();
  mXmax = Brx->maxValue();
  mYmax = Bry->maxValue();
}


void QScanRange::unfix ()
{

  mXmin = SANE_UNFIX (mXmin);
  mYmin = SANE_UNFIX (mYmin);
  mXmax = SANE_UNFIX (mXmax);
  mYmax = SANE_UNFIX (mYmax);
}


/** No descriptions */
void QScanDialog::setPreviewRange()
{
  if((!mpTlxOption) || (!mpTlyOption) ||
     (!mpBrxOption) || (!mpBryOption))
    return;

  double resx = 1,resy = 1;

  SANE_Unit unit;
  unit = mpScanner->getUnit(mpTlxOption->saneOptionNumber());
  SANE_Value_Type type;
  type = mpScanner->getOptionType(mpTlxOption->saneOptionNumber());

  QScanRange scan;
  QScanRange page;

  scan.getRange (mpTlxOption, mpTlyOption, mpBrxOption, mpBryOption);

  // handle the page width option - we assume it is in mm
  if (mpWidthOption && mpHeightOption)
  {
    // if option is not active, just leave it
    if(mpScanner->isOptionActive(mpWidthOption->saneOptionNumber()))
    {
        page.getRange (mpWidthOption, mpHeightOption, mpWidthOption, mpHeightOption);
        if (mpScanner->getOptionType (mpWidthOption->saneOptionNumber()) == SANE_TYPE_FIXED)
          page.unfix ();
    }
  }

  if(type == SANE_TYPE_FIXED)
    scan.unfix ();
  if(unit == SANE_UNIT_MM)
    ;
    //If unit is MM, then we can simply check for the min and max
    //values of the scan area options, since they won't change if
    //the resolution changes (unless some drunken backend author
    //has better ideas :-)
  else if(unit == SANE_UNIT_PIXEL)
  {
    //try to find xresolution
    resx = mpScanner->xResolution();
    if(resx > 0)
    {
       //try to find yresolution
       resy = mpScanner->yResolution();
       if(resy <= 0)
         //no yres, use xres instead
         resy = resx;
    }
    //else no resolution settings at all
  }

  double ar;
  ar = ((scan.Xmax () - scan.Xmin ()) / double(resx))
     / ((scan.Ymax () - scan.Ymin ()) / double(resy));
  mpPreviewWidget->setAspectRatio(ar);
  mpPreviewWidget->setRange (scan.Xmin (), scan.Xmax (), scan.Ymin (), scan.Ymax (),
        page.Xmax (), page.Ymax ());
}
/** No descriptions */
void QScanDialog::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  if(!mpPreviewWidget || !isVisible())
    return;
  if(!xmlConfig->boolValue("SEPARATE_PREVIEW"))
  {
    if(mpPreviewWidget->isVisible())
    {
      xmlConfig->setIntValue("SCANDIALOG_INTEGRATED_PREVIEW_WIDTH",width());
      xmlConfig->setIntValue("SCANDIALOG_INTEGRATED_PREVIEW_HEIGHT",height());
    }
  }
}
/** No descriptions */
void QScanDialog::slotFilenameGenerationSettings()
{
  QExtensionWidget ew(this);
  ew.setPage(10);
  ew.exec();
}
/** No descriptions */
void QScanDialog::checkOptionValidity(int opt_num)
{
  bool visible=false;
  QSaneOption* new_widget_pointer;
  QSaneOption* so = mOptionWidgets[opt_num]->saneOption();
  visible = so->isVisible();
  int so_number = so->saneOptionNumber();
  //compare some values with the current values from the QScanner class
  if((so->saneConstraintType() != mpScanner->getConstraintType(so_number)) ||
     (so->saneValueType() != mpScanner->getOptionType(so_number)))
  {
    qDebug("QScanDialog::checkOptionValidity - must recreate sane option %i",so_number);
    qDebug("QScanDialog::checkOptionValidity - opt_num %i",opt_num);
    //create new option
    new_widget_pointer = createSaneOptionWidget(mOptionWidgets[opt_num],so_number);
    if(!new_widget_pointer)
    {
      qDebug("QScanDialog::checkOptionValidity - could not create new widget");
      return;
    }
    new_widget_pointer->setOptionNumber(opt_num);
    //insert new option
    mOptionWidgets[opt_num]->replaceWidget(new_widget_pointer);
    if(visible)
     mOptionWidgets[opt_num]->show();
    qDebug("QScanDialog::checkOptionValidity - success");
  }
}


bool QScanDialog::setOption (QSaneOption *opt, QString str)
{
  QComboOption *wh = (QComboOption *)opt;

//   qDebug () << "setOption" << str;
  if (opt)
  {
     if (!wh->setCurrentValue (str.toLatin1 ()))
         return false;
     slotOptionChanged (wh->optionNumber ());
  }

  return true;
}


void QScanDialog::setOption (QSaneOption *opt, int value)
{
  if (!opt)
    return;

  if (opt->inherits ("QWordComboOption"))
  {
    qDebug () << "setOption" << opt->metaObject ()->className ();
    QWordComboOption *wh = (QWordComboOption *)opt;

    wh->setValue (value);
    slotOptionChanged (wh->optionNumber ());
  }

  else if (opt->inherits ("QScrollBarOption"))
  {
    QScrollBarOption *wh = (QScrollBarOption *)opt;

    wh->setValue (value);
    slotOptionChanged (wh->optionNumber ());
  }
}


void QScanDialog::setFormat (QScanner::format_t f, bool select_compression)
{
   QString fred = QString ("Lineart,Halftone,Gray,Color").section (',', f, f);
   QComboOption *wh = (QComboOption *)mOptionCompression;

    if (!setOption (mOptionFormat, fred))
    {
        // Try Binary for Lineart
        if (fred == "Lineart")
            setOption (mOptionFormat, "Binary");
    }

   bool want_jpeg = select_compression &&
           (f != QScanner::mono && f != QScanner::dither);
   setOption (mOptionCompression, want_jpeg ? "JPEG" : "None");
   QString msg;
   
   // either SANE doesn't have it, or CONFIG_sane_jpeg is 0 in config.h
   // or the scanner doesn't support it
   if (want_jpeg && (!mOptionCompression || wh->getCurrentText () != "JPEG"))
      msg = "JPEG mode not available in SANE or scanner";
   emit warning (msg);
}


QScanner::format_t QScanDialog::getFormat ()
{
  QComboOption *wh = (QComboOption *)mOptionFormat;

  if (wh)
     {
     QString str = wh->getCurrentText ();
     QScanner::format_t i;

     for (i = QScanner::mono; i <= QScanner::colour; i = (QScanner::format_t)(i + 1))
        if (str == QString ("Lineart,Halftone,Gray,Color").section (',', i, i))
           return i;
     }
  return QScanner::other;
}


bool QScanDialog::setAdf (bool adf)
{
   int option = -1;

   if (!mOptionSource)
      return false;

   QStringList name = mpScanner->getStringList (mOptionSource->saneOptionNumber ());

   // we expect either 'flatbed'/'fb' or 'adf'/'adf front'
   // find the option we need to set
//printf ("num %d\n", mOptionSource->saneOptionNumber ());
   for (int i = 0; option == -1 && i != name.count (); i++)
      {
      printf ("   ** %s\n", name [i].toLatin1 ().constData());
      if (!adf &&
          (name [i].indexOf ("flatbed", 0, Qt::CaseInsensitive) != -1
           || name [i].indexOf ("fb", 0, Qt::CaseInsensitive) != -1))
         option = i;
      else if (adf &&
         (name [i].indexOf ("front", 0, Qt::CaseInsensitive) != -1
          || name [i].indexOf ("adf", 0, Qt::CaseInsensitive) != -1))
         option = i;
      }
//   printf ("option = %d\n", option);
   if (option != -1)
      setOption (mOptionSource, name [option]);
   return option != -1;
}


bool QScanDialog::setDuplex (bool duplex)
{
   QSaneOption *which;
   int option = -1;

   // use the duplex option if present
   which = mOptionDuplex;
   if (!which)
      which = mOptionSource;
//printf ("which = %p, %d\n", which, which ? which->saneOptionNumber () : -1);
   if (!which)
      return false;

   QStringList name = mpScanner->getStringList (which->saneOptionNumber ());

   // we expect either 'front' or 'both'/'duplex'
   // find the option we need to set
   for (int i = 0; option == -1 && i != name.count (); i++)
      {
//      printf ("   * %s\n", name [i].latin1 ());
      if (!duplex && name [i].indexOf ("front", 0, Qt::CaseInsensitive) != -1)
         option = i;
      else if (duplex &&
         (name [i].indexOf ("both", 0, Qt::CaseInsensitive) != -1
          || name [i].indexOf ("duplex", 0, Qt::CaseInsensitive) != -1))
         option = i;
      }
   if (option != -1)
      setOption(which, name [option]);
   return option != -1;
}


  /** set DPI */
void QScanDialog::setDpi (int dpi)
{
   setOption (mOptionXRes, dpi);
   setOption (mOptionYRes, dpi);
}


void QScanDialog::set256 (QSaneOption *opt, int value)
{
   int max;

   UNUSED (max);
   if (opt)
   {
      max = mpScanner->getRangeMax (opt->saneOptionNumber ());
      //      word = (int)(((double)exposure * max + 0.5) / 100);
//      word = value;
      setOption (opt, value);
   }
}


void QScanDialog::setExposure (int value)
{
   set256 (mOptionThreshold, value);
}


void QScanDialog::setBrightness (int value)
{
   set256 (mOptionBrightness, value);
}


void QScanDialog::setContrast (int value)
{
   set256 (mOptionContrast, value);
}
