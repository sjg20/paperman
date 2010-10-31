/***************************************************************************
                          qmultiscan.cpp  -  description
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

#include "./pics/setup.xpm"
#include "fileiosupporter.h"
#include "imageiosupporter.h"
#include "qextensionwidget.h"
#include "qfilelistwidget.h"
#include "qmultiscan.h"
#include "qcopyprint.h"
#include "qswitchoffmessage.h"
#include "qxmlconfig.h"
#include "textiosupporter.h"

#include <limits.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qevent.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qtabwidget.h>
#include <qtextstream.h>
#include <qtoolbutton.h>
#include <qwhatsthis.h>

QMultiScan::QMultiScan(QWidget *parent, const char *name,WFlags f )
           :QWidget(parent,name,f)
{
  setCaption(tr("QuiteInsane - Multi scan"));
  mLastImageFilename = QString::null;
  mLastTextFilename = QString::null;
  mErrorString = QString::null;
  mpCopyPrint = new QCopyPrint(this,"",true);
  mpCopyPrint->setShowPrintDialog(false);
  initWidget();
  loadSettings();
  createWhatsThisHelp();
}
QMultiScan::~QMultiScan()
{
}
/**  */
void QMultiScan::initWidget()
{
  mpMainGrid = new QGridLayout(this,4,2);
  mpMainGrid->setSpacing( 6 );
  mpMainGrid->setMargin( 6 );

///////////////////////////////////////////
//What's this
  QHBox* wthb = new QHBox(this);
  QWidget* dummy = new QWidget(wthb);
  QToolButton* WhatsThisButton = QWhatsThis::whatsThisButton(wthb);
	WhatsThisButton->setAutoRaise(FALSE);	
  wthb->setStretchFactor(dummy,1);
  mpMainGrid->addMultiCellWidget(wthb,0,0,0,1);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    wthb->hide();
/////////////////////////////////////////////////////////////////
// Main
  mpMultiBox = new QGroupBox(this);
  mpMultiBox->setTitle(tr("Multi scan options"));
  mpMultiBox->setColumnLayout(0, Qt::Vertical );
  mpMultiBox->layout()->setSpacing( 4 );
  mpMultiBox->layout()->setMargin( 4 );

  QGridLayout* mpMultiGrid = new QGridLayout(mpMultiBox->layout(),3,1);
  mpMultiGrid->setSpacing( 6 );
  mpMultiGrid->setMargin( 11 );
  mpMultiGrid->setAlignment( Qt::AlignTop );

////////////
  QHBox* hbox1 = new QHBox(mpMultiBox);
  hbox1->setSpacing(4);
  new QLabel(tr("Number:"),hbox1);
  mpNumberSpin = new QSpinBox(hbox1);
  mpNumberSpin->setRange(1,9999);//why 9999; I don't know
  mpAdfCheckBox = new QCheckBox(tr("AD&F"),hbox1);
  mpConfirmCheckBox = new QCheckBox(tr("C&onfirm scan"),hbox1);
  mpImageCheckBox = new QCheckBox(tr("Save &images"),hbox1);
  mpTextCheckBox = new QCheckBox(tr("OCR/Save &text"),hbox1);
  hbox1->setStretchFactor(mpAdfCheckBox,1);
  hbox1->setStretchFactor(mpNumberSpin,1);
  hbox1->setStretchFactor(mpConfirmCheckBox,1);
  hbox1->setStretchFactor(mpImageCheckBox,1);
  hbox1->setStretchFactor(mpTextCheckBox,1);
  mpMultiGrid->addWidget(hbox1,0,0);

  QHBox* hbox1b = new QHBox(mpMultiBox);
  hbox1b->setSpacing(4);
  mpPrintCheckBox = new QCheckBox(tr("Print i&mages"),hbox1b);
  mpResolutionCheckBox = new QCheckBox(tr("&Use own resolution settings"),hbox1b);
  mpResolutionCheckBox->setEnabled(false);
  mpAutoCheckBox = new QCheckBox(tr("Automatic filename &generation"),hbox1b);
  mpAutoCheckBox->setEnabled(false);
  mpPrinterButton = new QPushButton(tr("Setup &printer..."),hbox1b);
  mpPrinterButton->setEnabled(false);
  hbox1b->setStretchFactor(mpPrintCheckBox,1);
  hbox1b->setStretchFactor(mpPrinterButton,1);
  hbox1b->setStretchFactor(mpResolutionCheckBox,1);
  mpMultiGrid->addWidget(hbox1b,1,0);

  QHBox* hbox1c = new QHBox(mpMultiBox);
  hbox1c->setSpacing(4);
  mpPrintToFileLabel = new QLabel(tr("Print to file:"),hbox1c);
  mpPrintToFileLineEdit = new QLineEdit(hbox1c);
  mpPrintToFileLineEdit->setEnabled(false);
  mpFileGenerationButton = new QToolButton(hbox1c);
  mpFileGenerationButton->setPixmap(QPixmap((const char **)setup));
  hbox1c->setStretchFactor(mpPrintToFileLineEdit,1);
  mpMultiGrid->addWidget(hbox1c,2,0);

  mpFileList = new QFileListWidget(false,this);

  mpTextList = new QFileListWidget(true,this);

  mpMainGrid->addMultiCellWidget(mpMultiBox,1,1,0,1);
  mpMainGrid->addWidget(mpFileList,2,0);
  mpMainGrid->addWidget(mpTextList,2,1);

  QHBox* qhbox3 = new QHBox(this);
  mpScanButton = new QPushButton(tr("&Scan"),qhbox3);
  dummy = new QWidget(qhbox3);
  mpCloseButton = new QPushButton(tr("&Close"),qhbox3);
  qhbox3->setStretchFactor(dummy,1);
  mpMainGrid->addMultiCellWidget(qhbox3,3,3,0,1);
  mpMainGrid->activate();

  connect(mpCloseButton,SIGNAL(clicked()),this,SLOT(close()));
  connect(mpScanButton,SIGNAL(clicked()),this,SIGNAL(signalStartScan()));
  connect(mpPrinterButton,SIGNAL(clicked()),this,SLOT(slotSetupPrinter()));
  connect(mpPrintCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotCheckPrintImage(bool)));
  connect(mpConfirmCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotCheckConfirm(bool)));
  connect(mpAdfCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotADFMode(bool)));
  connect(mpTextCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotCheckOCR(bool)));
  connect(mpImageCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotCheckSaveImage(bool)));
  connect(mpNumberSpin,SIGNAL(valueChanged(int)),
          this,SLOT(slotScanNumber(int)));
  connect(mpPrintToFileLineEdit,SIGNAL(textChanged(const QString &)),
          this,SLOT(slotPrintFilenameChanged(const QString &)));
  connect(mpFileGenerationButton,SIGNAL(clicked()),
          this,SLOT(slotFilenameGenerationSettings()));
}
/** */
void QMultiScan::loadSettings()
{
  mpNumberSpin->setValue(xmlConfig->intValue("MULTI_NUMBER"));
  mpImageCheckBox->setChecked(xmlConfig->boolValue("MULTI_SAVE_IMAGE"));
  mpTextCheckBox->setChecked(xmlConfig->boolValue("MULTI_SAVE_TEXT"));
//  mpPrintCheckBox->setChecked(xmlConfig->boolValue("MULTI_PRINT"));
  mpConfirmCheckBox->setChecked(xmlConfig->boolValue("MULTI_CONFIRM"));
  mpAdfCheckBox->setChecked(xmlConfig->boolValue("MULTI_ADF"));
}
/**  */
void QMultiScan::slotMustResize()
{
  resize(minimumSizeHint());
}
/**  */
void QMultiScan::showEvent(QShowEvent* e)
{
  QWidget::showEvent(e);
  int w = xmlConfig->intValue("MULTISCAN_WIDTH",0);
  int h = xmlConfig->intValue("MULTISCAN_HEIGHT",0);
  if(w < width())
    w = width();
  if(h < height())
    h = height();
  resize(w,h);
}
/**  */
void QMultiScan::slotCheckConfirm(bool b)
{
  if(b != mpConfirmCheckBox->isChecked())
    mpConfirmCheckBox->setChecked(b);//for external calls
  xmlConfig->setBoolValue("MULTI_CONFIRM",b);
}
/**  */
void QMultiScan::slotCheckSaveImage(bool b)
{
  xmlConfig->setBoolValue("MULTI_SAVE_IMAGE",b);
}
/**  */
void QMultiScan::slotCheckPrintImage(bool b)
{
  bool is_setup = true;
  if(b == true)
  {
    if(mpCopyPrint->isSetup() == false)
    {
      if(xmlConfig->boolValue("WARNING_PRINTERSETUP",true) == true)
      {
        QSwitchOffMessage som(QSwitchOffMessage::Type_PrinterSetupWarning,this);
        som.exec();
      }
      slotSetupPrinter();
    }
    is_setup = mpCopyPrint->isSetup();
    mpPrinterButton->setEnabled(is_setup);
    mpResolutionCheckBox->setEnabled(is_setup);
    if(is_setup == true)
    {
      mpAutoCheckBox->setEnabled(mpCopyPrint->printToFile());
      mpPrintToFileLineEdit->setEnabled(mpCopyPrint->printToFile());
    }
    else
    {
      mpAutoCheckBox->setEnabled(false);
      mpPrintToFileLineEdit->setEnabled(false);
    }
    xmlConfig->setBoolValue("MULTI_PRINT",is_setup);
  }
  else
  {
    mpPrinterButton->setEnabled(b);
    mpResolutionCheckBox->setEnabled(b);
    mpAutoCheckBox->setEnabled(b);
    mpPrintToFileLineEdit->setEnabled(b);
    xmlConfig->setBoolValue("MULTI_PRINT",b);
  }
}
/**  */
void QMultiScan::slotCheckOCR(bool b)
{
  xmlConfig->setBoolValue("MULTI_SAVE_TEXT",b);
}
/**  */
void QMultiScan::createContents()
{
  mpFileList->createContents();
  mpTextList->createContents();
}
/**  */
void QMultiScan::slotScanNumber(int value)
{
  xmlConfig->setIntValue("MULTI_NUMBER",value);
}
/**  */
bool QMultiScan::mustConfirm()
{
  return mpConfirmCheckBox->isChecked();
}
/**  */
bool QMultiScan::mustOCR()
{
  return mpTextCheckBox->isChecked();
}
/**  */
bool QMultiScan::mustPrint()
{
  return mpPrintCheckBox->isChecked();
}
/**  */
bool QMultiScan::mustSave()
{
  return mpImageCheckBox->isChecked();
}
/**  */
int QMultiScan::scanNumber()
{
  if(!mpAdfCheckBox->isChecked())
    return mpNumberSpin->value();
  else
    return INT_MAX;
}
/**  */
bool QMultiScan::saveImage(QImage* image)
{
  ImageIOSupporter iisup;
  FileIOSupporter fisup;
  int counter_width,step_size;
  QString ext;
  QString qs;
  QString filename;
  bool formatflag;
  bool fill_gap;
  formatflag = false;
  ext = "";
  QStringList filters;
  mErrorString = QString::null;
  if(xmlConfig->boolValue("FILE_GENERATION_PREPEND_ZEROS",false) == true)
    counter_width = xmlConfig->intValue("FILE_GENERATION_COUNTER_WIDTH",3);
  else
    counter_width = 0;
  step_size = xmlConfig->intValue("FILE_GENERATION_STEP",1);
  fill_gap = xmlConfig->boolValue("FILE_GENERATION_FILL_GAPS",false);

qDebug("QMultiScan: get filename %s",mpFileList->getFilename().latin1());
qDebug("QMultiScan: format %s",mpFileList->format().latin1());
  filename = iisup.validateExtension(mpFileList->getFilename(),mpFileList->format());

qDebug("QMultiScan: filename %s",filename.latin1());

  if(filename.isEmpty())
  {
    mErrorString = iisup.lastErrorString();
    return false;
  }
  filename = fisup.getIncreasingFilename(filename,step_size,counter_width,fill_gap);

qDebug("QMultiScan: increasedfilename %s",filename.latin1());

  if(filename.isEmpty())
  {
    mErrorString = fisup.lastErrorString();
    return false;
  }

  if(!image)
  {
    mErrorString = tr("The image does not exist.");
    return false;
  }
  if(image->isNull())
  {
    mErrorString = tr("The image is empty.");
    return false;
  }
  QString iformat;
  iformat = mpFileList->format();
  if(!iisup.saveImage(filename,*image,iformat,this))
  {
    mLastImageFilename = QString::null;
    return false;
  }
  emit signalImageSaved(filename);
  mLastImageFilename = filename;
  QString fn;
  QFileInfo fi(filename);
  fn = fi.fileName();
  mpFileList->addFilename(fn);
  return true;
}
/**  */
bool QMultiScan::saveText(QString text)
{
  TextIOSupporter tisup;
  FileIOSupporter fisup;
  int counter_width,step_size;
  QString ext;
  QString qs;
  QString filename;
  bool formatflag;
  bool fill_gap;
  formatflag = false;
  ext = "";
  QStringList filters;

  if(xmlConfig->boolValue("FILE_GENERATION_PREPEND_ZEROS",false) == true)
    counter_width = xmlConfig->intValue("FILE_GENERATION_COUNTER_WIDTH",3);
  else
    counter_width = 0;
  step_size = xmlConfig->intValue("FILE_GENERATION_STEP",1);
  fill_gap = xmlConfig->boolValue("FILE_GENERATION_FILL_GAPS",false);

qDebug("QMultiScan: get filename %s",mpTextList->getFilename().latin1());
qDebug("QMultiScan: format %s",mpTextList->format().latin1());

  filename = tisup.validateExtension(mpTextList->getFilename(),mpTextList->format());

qDebug("QMultiScan: filename %s",filename.latin1());

  if(filename.isEmpty())
  {
    mErrorString = tisup.lastErrorString();
    return false;
  }
  filename = fisup.getIncreasingFilename(filename,step_size,counter_width,fill_gap);
  if(filename.isEmpty())
  {
    mErrorString = fisup.lastErrorString();
    return false;
  }

  QFile f(filename);
  if ( f.open(IO_WriteOnly) )
  {
    QTextStream t( &f );
    t<<text;
    f.close();
    QString fn;
    QFileInfo fi(filename);
    fn = fi.fileName();
    mpTextList->addFilename(fn);
    mLastTextFilename = filename;
    return true;
  }
  mLastTextFilename = QString::null;
  return false;
}
/**  */
void QMultiScan::slotSetupPrinter()
{
  QString qs;
  if(!mpCopyPrint)
    return;
  if(ownResolution())
  {
    mOwnXRes = xmlConfig->intValue("MULTI_XRESOLUTION");
    mOwnYRes = xmlConfig->intValue("MULTI_YRESOLUTION");
    mpCopyPrint->setUnknownImage(mWidth,mHeight,mOwnXRes,mOwnYRes);
  }
  else
    mpCopyPrint->setUnknownImage(mWidth,mHeight,mXRes,mYRes);
  mpCopyPrint->exec();
  if(mpCopyPrint->isSetup() == false)
  {
    mpPrintCheckBox->setChecked(false);
  }
  else
  {
    mpAutoCheckBox->setEnabled(mpCopyPrint->printToFile());
    mpPrintToFileLineEdit->setEnabled(mpCopyPrint->printToFile());
    mpPrintToFileLineEdit->setText(mpCopyPrint->printFilename());
  }
}
/**  */
void QMultiScan::setImageValues(int w,int h,int xres,int yres)
{
  mWidth = w;
  mHeight = h;
  mXRes = xres;
  mYRes = yres;
}
/**  */
bool QMultiScan::printImage(QImage* image)
{
  int counter_width;
  int step_size;
  bool fill_gap;
  FileIOSupporter fisup;

  if(xmlConfig->boolValue("FILE_GENERATION_PREPEND_ZEROS",false) == true)
    counter_width = xmlConfig->intValue("FILE_GENERATION_COUNTER_WIDTH",3);
  else
    counter_width = 0;
  step_size = xmlConfig->intValue("FILE_GENERATION_STEP",1);
  fill_gap = xmlConfig->boolValue("FILE_GENERATION_FILL_GAPS",false);

  QString qs;
  if((mpCopyPrint->printToFile() == true) &&
     (mpAutoCheckBox->isChecked() == true))
  {
    qs = mpCopyPrint->printFilename();
    qs = fisup.getIncreasingFilename(qs,step_size,counter_width,fill_gap);
    if(qs.isEmpty())
    {
      mErrorString = tr("Could not generate a valid filename.");
      return false;
    }
    if(!fisup.isValidFilename(qs))
    {
      mErrorString = fisup.lastErrorString();
      return false;
    }
    mpCopyPrint->setPrintFilename(qs);
    mpPrintToFileLineEdit->setText(qs);
  }
  mpCopyPrint->setImage(image);
  if(ownResolution())
  {
    mOwnXRes = xmlConfig->intValue("MULTI_XRESOLUTION");
    mOwnYRes = xmlConfig->intValue("MULTI_YRESOLUTION");
    mpCopyPrint->setResolutionX(mOwnXRes);
    mpCopyPrint->setResolutionY(mOwnYRes);
  }
  else
  {
    mpCopyPrint->setResolutionX(mXRes);
    mpCopyPrint->setResolutionY(mYRes);
  }
  mpCopyPrint->slotPrint();
  return true;
}
/**  */
void QMultiScan::printImage()
{
  QString qs;
  mpCopyPrint->loadImage(xmlConfig->absConfDirPath()+".scantemp.pnm",true);
  if(ownResolution())
  {
    mOwnXRes = xmlConfig->intValue("MULTI_XRESOLUTION");
    mOwnYRes = xmlConfig->intValue("MULTI_YRESOLUTION");
    mpCopyPrint->setResolutionX(mOwnXRes);
    mpCopyPrint->setResolutionY(mOwnYRes);
  }
  else
  {
    mpCopyPrint->setResolutionX(mXRes);
    mpCopyPrint->setResolutionY(mYRes);
  }
  mpCopyPrint->slotPrint();
}
/**  */
void QMultiScan::slotCheckResolution(bool b)
{
  xmlConfig->setBoolValue("MULTI_OWN_RESOLUTION",b);
}
/**  */
bool QMultiScan::ownResolution()
{
  return mpResolutionCheckBox->isChecked();
}
/**  */
void QMultiScan::createWhatsThisHelp()
{
//close button
  QWhatsThis::add(mpCloseButton,tr("Close this window."));
//confirm
  QWhatsThis::add(mpConfirmCheckBox,
                  tr("If you activate this checkbox, a message box will be "
                     "shown before the next scan is done. If you select "
    							   "Cancel, the operation will be aborted."));
//ImageCheckBox
  QWhatsThis::add(mpImageCheckBox,
                  tr("If you activate this checkbox, the scanned images will "
                     "be saved."));
//Resolution checkbox
  QWhatsThis::add(mpResolutionCheckBox,
                  tr("If you activate this checkbox, the resolution settings "
                     "you made in the printer setup dialog will be used "
                     "for printing. Otherwise the current device settings "
                     "will be used. This checkbox is only enabled, if the "
                     "Print checkbox is enabled."));
//TextCheckBox
  QWhatsThis::add(mpTextCheckBox,
                  tr("If you activate this checkbox, optical character "
                     "recognition (OCR) will be started on the scanned images. "
                     "The recognized text will be saved. "));
////print button
  QWhatsThis::add(mpPrinterButton,
                tr("Click this button to show the printer setup dialog."));
//PrintCheckBox
  QWhatsThis::add(mpPrintCheckBox,
                  tr("If you activate this checkbox, the scanned images will "
                     "be printed."));
//number spin box
  QWhatsThis::add(mpNumberSpin,
                tr("Use this spin box to select the number of scans."
                   "If you don't have an automatic document feeder, "
                   "you should activate the Confirm checkbox too. "
                   "Otherwise the same document is scanned repeatedly."));
//number spin box
  QWhatsThis::add(mpAdfCheckBox,
                tr("If you activate this checkbox, all documents in "
                   "your document feeder will be scanned. You can also "
                   "use this mode, if you don't have an automatic "
                   "document feeder. In this case you should activate "
                   "the Confirm checkbox too. Otherwise the same document "
                   "is scanned repeatedly."));
}
/**  */
void QMultiScan::slotADFMode(bool b)
{
  mpNumberSpin->setEnabled(!b);
  xmlConfig->setBoolValue("MULTI_ADF",b);
}
/**  */
bool QMultiScan::adfMode()
{
  return mpAdfCheckBox->isChecked();
}
/** No descriptions */
QString QMultiScan::lastImageFilename()
{
  return mLastImageFilename;
}
/** No descriptions */
QString QMultiScan::lastTextFilename()
{
  return mLastTextFilename;
}
/** No descriptions */
void QMultiScan::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  if(!isVisible())
    return;
  xmlConfig->setIntValue("MULTISCAN_WIDTH",width());
  xmlConfig->setIntValue("MULTISCAN_HEIGHT",height());
}
/** No descriptions */
QString QMultiScan::lastErrorString()
{
  return mErrorString;
}
/** No descriptions */
void QMultiScan::slotPrintFilenameChanged(const QString & print_filename)
{
  if(mpCopyPrint->isSetup() == true)
  {
    mpCopyPrint->setPrintFilename(print_filename);
  }
}
/**  */
void QMultiScan::slotFilenameGenerationSettings()
{
  QExtensionWidget ew(this);
  ew.setPage(10);
  ew.exec();
}