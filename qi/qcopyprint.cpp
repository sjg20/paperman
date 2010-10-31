/***************************************************************************
                          qcopyprint.cpp  -  description
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
#include "imageiosupporter.h"
#include "qcopyprint.h"
#include "qdoublespinbox.h"
#include "qpreviewfiledialog.h"
#include "qqualitydialog.h"
#include "qxmlconfig.h"
#include "ruler.h"
#include <math.h>

#include <qapplication.h>
#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qcolor.h>
#include <qframe.h>
#include <qfile.h>
#include <qfiledialog.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qimage.h>
#include <qlabel.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpainter.h>
#include <qpaintdevicemetrics.h>
#include <qpen.h>
#include <qprogressdialog.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qsizepolicy.h>
#include <qspinbox.h>
#include <qstringlist.h>
#include <qlayout.h>
#include <qvalidator.h>
#include <qvariant.h>
#include <qvbox.h>
#include <qtoolbutton.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qwmatrix.h>

QCopyPrint::QCopyPrint(QWidget* parent,const char* name,bool modal,WFlags f)
           :QDialog(parent,name,modal,f)
{
  if ( !name ) setName( "QCopyPrint" );
  setCaption( tr( "QuiteInsane - Copy/Print"  ) );
  mShowPrintDialog = true;
  mMetricFactor = 1.0;
  mImagePath = "";
  mpPixmap = 0L;
  mpImage = 0L;
  initWidget();
  initSettings();
  createWhatsThisHelp();
  mPrinted = true;
  mIsSetup = false;
  //connect close button
  if(modal)
  {
    mpSaveButton->hide();
    connect(mpCloseButton,SIGNAL(clicked()),this,SLOT(accept()));
  }
  else
  {
    connect(mpCloseButton,SIGNAL(clicked()),this,SLOT(close()));
  }
  updateDeviceMetrics(true);
}

/*
 *  Destroys the object and frees any allocated resources
 */
QCopyPrint::~QCopyPrint()
{
  if(mpImage) delete mpImage;
  if(mpPixmap) delete mpPixmap;
}

/**  */
void QCopyPrint::initWidget()
{
  mpMainGrid = new QGridLayout(this,4,3);
  mpMainGrid->setSpacing( 6 );
  mpMainGrid->setMargin( 11 );

/////////////////////////////////////////////////////////////////
// Scaling
  QVBox* vbox1 = new QVBox(this);
  vbox1->setSpacing(4);
  mpScaleBox = new QGroupBox( vbox1, "ScaleBox" );
  mpScaleBox->setTitle( tr( "Scaling"  ) );
  mpScaleBox->setColumnLayout(0, Qt::Vertical );
  mpScaleBox->layout()->setSpacing( 0 );
  mpScaleBox->layout()->setMargin( 0 );

  QGridLayout* scalegrid = new QGridLayout(mpScaleBox->layout(),5,3);
  scalegrid->setSpacing( 6 );
  scalegrid->setMargin( 11 );
  scalegrid->setAlignment( Qt::AlignTop );
  scalegrid->addColSpacing(0,20);
  scalegrid->setColStretch(1,1);

  mpRadioButtonFree = new QRadioButton( mpScaleBox, "RadioButtonFree" );
  mpRadioButtonFree->setText( tr( "&Free"  ) );
  scalegrid->addMultiCellWidget(mpRadioButtonFree,0,0,0,2);

  QLabel* ScaleLabel1 = new QLabel(tr("Factor:"),mpScaleBox);
  scalegrid->addWidget(ScaleLabel1,1,1);

  mpScaleSpin = new QDoubleSpinBox(mpScaleBox);
  mpScaleSpin->setButtonSymbols( QSpinBox::PlusMinus );
  QDoubleValidator* scalevali = new QDoubleValidator(0.1,10.0,2,mpScaleSpin);
  mpScaleSpin->setValidator(scalevali);

  scalegrid->addWidget(mpScaleSpin,1,2);
  mpScaleSpin->setMaxValue(1000);
  mpScaleSpin->setValue(100);
  connect(mpScaleSpin,SIGNAL(valueChanged(int)),this,SLOT(slotFreeScale(int)));

  mpRadioButtonMax = new QRadioButton( mpScaleBox, "RadioButton2" );
  mpRadioButtonMax->setText( tr( "&Maximum size"  ) );
  scalegrid->addMultiCellWidget(mpRadioButtonMax,2,2,0,2);

  mpRadioButtonMargin = new QRadioButton( mpScaleBox, "RadioButton3" );
  mpRadioButtonMargin->setText( tr( "M&argin size"  ) );
  scalegrid->addMultiCellWidget(mpRadioButtonMargin,3,3,0,2);

  mpProportionCheckBox = new QCheckBox(tr( "&Keep aspect ratio"),mpScaleBox);
  scalegrid->addMultiCellWidget(mpProportionCheckBox,4,4,0,2);
  connect(mpProportionCheckBox,SIGNAL(toggled(bool)),this,
          SLOT(slotProportion(bool)));
  //button group
  mpScaleGroup = new QButtonGroup(this);
  mpScaleGroup->hide();
  mpScaleGroup->insert(mpRadioButtonFree);
  mpScaleGroup->insert(mpRadioButtonMax);
  mpScaleGroup->insert(mpRadioButtonMargin);
  mpScaleGroup->setExclusive(true);
  mpScaleGroup->setButton(0);
  connect(mpScaleGroup,SIGNAL(clicked(int)),this,SLOT(slotScaling(int)));
  mpProportionCheckBox->setChecked(true);
  mpProportionCheckBox->setEnabled(false);
///////////////////////////////////////////////////////
// Margins
  mpMarginBox = new QGroupBox( vbox1, "MarginBox" );
  mpMarginBox->setTitle( tr( "Margins"  ) );
  mpMarginBox->setColumnLayout(0, Qt::Vertical );
  mpMarginBox->layout()->setSpacing( 0 );
  mpMarginBox->layout()->setMargin( 0 );
  mpMarginGrid = new QGridLayout( mpMarginBox->layout() );
  mpMarginGrid->setAlignment( Qt::AlignTop );
  mpMarginGrid->setSpacing( 6 );
  mpMarginGrid->setMargin( 11 );

  mpLeftSpin = new QDoubleSpinBox( mpMarginBox, "LeftSpin" );
  mpLeftSpin->setButtonSymbols( QSpinBox::PlusMinus );
  mpLeftSpin->setSuffix( " mm" );
  QDoubleValidator* leftvali = new QDoubleValidator(mpLeftSpin);
  mpLeftSpin->setValidator(leftvali);
  mpLeftSpin->setMaxValue(200000);
  mpMarginGrid->addWidget( mpLeftSpin, 0, 1 );
  connect(mpLeftSpin,SIGNAL(valueChanged(int)),this,SLOT(slotLeftSpinValue(int)));

  mpTopSpin = new QDoubleSpinBox( mpMarginBox, "TopSpin" );
  mpTopSpin->setButtonSymbols( QSpinBox::PlusMinus );
  mpTopSpin->setSuffix( " mm" );
  QDoubleValidator* topvali = new QDoubleValidator(mpTopSpin);
  mpTopSpin->setValidator(topvali);
  mpTopSpin->setMaxValue(200000);
  mpMarginGrid->addWidget( mpTopSpin, 1, 1 );
  connect(mpTopSpin,SIGNAL(valueChanged(int)),this,SLOT(slotTopSpinValue(int)));

  mpRightSpin = new QDoubleSpinBox( mpMarginBox, "RightSpin" );
  mpRightSpin->setButtonSymbols( QSpinBox::PlusMinus );
  mpRightSpin->setSuffix( " mm" );
  QDoubleValidator* rightvali = new QDoubleValidator(mpRightSpin);
  mpRightSpin->setValidator(rightvali);
  mpRightSpin->setMaxValue(200000);
  mpMarginGrid->addWidget( mpRightSpin, 2, 1 );
  connect(mpRightSpin,SIGNAL(valueChanged(int)),this,SLOT(slotRightSpinValue(int)));

  mpBottomSpin = new QDoubleSpinBox( mpMarginBox, "BottomSpin" );
  mpBottomSpin->setButtonSymbols( QSpinBox::PlusMinus );
  mpBottomSpin->setSuffix( " mm" );
  QDoubleValidator* bottomvali = new QDoubleValidator(mpBottomSpin);
  mpBottomSpin->setValidator(bottomvali);
  mpBottomSpin->setMaxValue(200000);
  mpMarginGrid->addWidget( mpBottomSpin, 3, 1 );
  connect(mpBottomSpin,SIGNAL(valueChanged(int)),this,SLOT(slotBottomSpinValue(int)));


  mpTextLabel1 = new QLabel( mpMarginBox, "TextLabel1" );
  mpTextLabel1->setText( tr( "Left:"  ) );
  mpMarginGrid->addWidget( mpTextLabel1, 0, 0 );

  mpTextLabel2 = new QLabel( mpMarginBox, "TextLabel2" );
  mpTextLabel2->setText( tr( "Top:"  ) );
  mpMarginGrid->addWidget( mpTextLabel2, 1, 0 );

  mpTextLabel3 = new QLabel( mpMarginBox, "TextLabel3" );
  mpTextLabel3->setText( tr( "Right:"  ) );
  mpMarginGrid->addWidget( mpTextLabel3, 2, 0 );

  mpTextLabel4 = new QLabel( mpMarginBox, "TextLabel4" );
  mpTextLabel4->setText( tr( "Bottom:"  ) );
  mpMarginGrid->addWidget( mpTextLabel4, 3, 0 );

/////////////////////////////////////////////////////////////////
// resolution
  QVBox* vbox2 = new QVBox(this);
  vbox2->setSpacing(4);
  mpImageResBox = new QGroupBox(vbox2);
  mpImageResBox->setTitle( tr( "Image resolution"  ) );
  mpImageResBox->setColumnLayout(0, Qt::Vertical );
  mpImageResBox->layout()->setSpacing( 0 );
  mpImageResBox->layout()->setMargin( 0 );

  QGridLayout* imagegrid = new QGridLayout(mpImageResBox->layout(),4,2);
  imagegrid->setSpacing( 6 );
  imagegrid->setMargin( 11 );
  imagegrid->setAlignment( Qt::AlignTop );
  imagegrid->addColSpacing(0,20);
  imagegrid->setColStretch(1,1);

  QLabel* reslabel1 = new QLabel("X:",mpImageResBox);
  imagegrid->addWidget(reslabel1,0,0);

  QLabel* reslabel2 = new QLabel("Y:",mpImageResBox);
  imagegrid->addWidget(reslabel2,1,0);

  mpImageXResSpin = new QSpinBox(mpImageResBox);
  mpImageXResSpin->setButtonSymbols( QSpinBox::PlusMinus );
  mpImageXResSpin->setMaxValue(20000);
  mpImageXResSpin->setSuffix( " dpi" );
  mpImageXResSpin->setMinValue(50);
  mpImageXResSpin->setValue(72);
  imagegrid->addWidget(mpImageXResSpin,0,1);

  mpImageYResSpin = new QSpinBox(mpImageResBox);
  mpImageYResSpin->setButtonSymbols( QSpinBox::PlusMinus );
  mpImageYResSpin->setMaxValue(20000);
  mpImageYResSpin->setSuffix( " dpi" );
  mpImageYResSpin->setMinValue(50);
  mpImageYResSpin->setValue(72);
  imagegrid->addWidget(mpImageYResSpin,1,1);

  connect(mpImageXResSpin,SIGNAL(valueChanged(int)),
          this,SLOT(slotImageXRes(int)));
  connect(mpImageYResSpin,SIGNAL(valueChanged(int)),
          this,SLOT(slotImageYRes(int)));

  mpImageResCheckBox = new QCheckBox(tr("&Bind values"),mpImageResBox);
  imagegrid->addMultiCellWidget(mpImageResCheckBox,2,2,0,1);
  connect(mpImageResCheckBox,SIGNAL(toggled(bool)),
          this,SLOT(slotBindImageRes(bool)));

  //dummy
  QWidget* resdummy = new QWidget(vbox2);
  vbox2->setStretchFactor(resdummy,1);

  //////////////////////////////////////////////////////
  // Setup button
  mpSetupButton = new QPushButton(tr( "S&etup printer..."),vbox2);
  connect(mpSetupButton,SIGNAL(clicked()),this,SLOT(slotSetupPrinter()));

  //////////////////////////////////////////////////////
  // Save button
  mpSaveButton = new QPushButton(tr("Save &image..."),vbox2);
  connect(mpSaveButton,SIGNAL(clicked()),this,SLOT(slotSaveImage()));

  //////////////////////////////////////////////////////
  // Save settings button
  mpSettingsButton = new QPushButton(tr("Save &settings"),vbox2);
  connect(mpSettingsButton,SIGNAL(clicked()),this,SLOT(slotSaveSettings()));

  //////////////////////////////////////////////////////
  // Print button
  mpPrintButton = new QPushButton( tr( "&Print"  ),vbox2);
  connect(mpPrintButton,SIGNAL(clicked()),this,SLOT(slotPrint()));

  //////////////////////////////////////////////////////
  // Close button
  mpCloseButton = new QPushButton( tr( "&Close"  ),vbox2);

  mpMainGrid->addWidget(vbox1,1,1);
  mpMainGrid->addWidget(vbox2,1,2);

//preview widget
  mpPreviewWidget = new QWidget(this);
  QGridLayout* previewgrid = new QGridLayout(mpPreviewWidget,3,2);
  mpMetricLabel = new QLabel("",mpPreviewWidget);
  mpVRuler = new Ruler(mpPreviewWidget,"",Ruler::Vertical);
  mpVRuler->setFixedWidth(30);
  mpHRuler = new Ruler(mpPreviewWidget,"",Ruler::Horizontal);
  mpHRuler->setFixedHeight(30);
  mpPreviewFrame = new QWidget(mpPreviewWidget);
  mpPreviewFrame->setBackgroundColor(QColor(white));
  previewgrid->addWidget(mpMetricLabel,0,0);
  previewgrid->addWidget(mpHRuler,0,1);
  previewgrid->addWidget(mpVRuler,1,0);
  previewgrid->addWidget(mpPreviewFrame,1,1);
  previewgrid->setRowStretch(2,1);
  previewgrid->setColStretch(1,1);
  previewgrid->setColStretch(2,1);
  previewgrid->activate();
  mpMainGrid->addWidget(mpPreviewWidget,1,0);
//////////////////////////////////////////////////////
// Size label + what's this
  QHBox* tophb = new QHBox(this);
  mpSizeLabel = new QLabel( tophb, "SizeLabel" );
  mpSizeLabel->setText( tr( "Image size:"  ) );
  QToolButton* tb = QWhatsThis::whatsThisButton(tophb);
	tb->setAutoRaise(FALSE);	
  tophb->setStretchFactor(mpSizeLabel,1);
  mpMainGrid->addMultiCellWidget(tophb,0,0,0,2);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    tb->hide();

  mpMainGrid->setRowStretch(2,1);
  mpMainGrid->setColStretch(1,1);
  mpMainGrid->activate();
}
/**  */
void QCopyPrint::setPreviewSize()
{
  int w;
  int h;
  QString qs;
  h = mHeightMM;
  mStartHeight = mpScaleBox->height() + mpMarginBox->height() - 30;
  h = mStartHeight;
  w = mWidthMM*h/mHeightMM;
  if(w>mStartHeight)
  {
    w = mStartHeight;
    h = mHeightMM*w/mWidthMM;
  }
  mpPreviewFrame->setFixedHeight(h);
  mpPreviewFrame->setFixedWidth(w);
//adjust rulers
  QIN::MetricSystem ms;
  ms = (QIN::MetricSystem)xmlConfig->intValue("METRIC_SYSTEM");
  switch(ms)
  {
    case QIN::Millimetre:
      mpMetricLabel->setText(tr("mm"));
      mpHRuler->setRange(0.0,mWidthMM);
      mpHRuler->setFixedHeight(30);
      mpVRuler->setRange(0.0,mHeightMM);
      mpVRuler->setFixedWidth(30);
      break;
    case QIN::Centimetre:
      mpMetricLabel->setText(tr("cm"));
      mpHRuler->setRange(0.0,mWidthMM/10.0);
      mpHRuler->setFixedHeight(30);
      mpVRuler->setRange(0.0,mHeightMM/10.0);
      mpVRuler->setFixedWidth(30);
      break;
    case  QIN::Inch:
      mpMetricLabel->setText(tr("inch"));
      mpHRuler->setRange(0.0,mWidthMM/25.4);
      mpHRuler->setFixedHeight(30);
      mpVRuler->setRange(0.0,mHeightMM/25.4);
      mpVRuler->setFixedWidth(30);
      break;
    default:;//do nothing--shouldn't happen
  }
  setDoubleSpinRange();
  repaint();
}
/**  */
void QCopyPrint::slotSetupPrinter()
{
  qDebug("QCopyPrint::slotSetupPrinter()");
  setupPrinter();
}
/**  */
bool QCopyPrint::setupPrinter()
{
  qDebug("QCopyPrint::setupPrinter()");
  mPrinter.setFullPage(true);
  if(mPrinter.setup())
  {
#ifdef KDEAPP
    mPrinter.doPrepare();
#endif
    QPaintDeviceMetrics pdm(&mPrinter);
    mWidthMM = pdm.widthMM();
    mHeightMM = pdm.heightMM();
#ifdef KDEAPP
    mPrinter.doFinish();
#endif
    qDebug("mWidthMM: %i",mWidthMM);
    qDebug("mHeightMM: %i",mHeightMM);
    setPreviewSize();
    resizePreviewPixmap();
    qApp->processEvents();
    resize(minimumSizeHint());
    emit signalResized();
    mIsSetup = true;
    return true;
  }
  return false;
}
/**  */
void QCopyPrint::setDoubleSpinRange()
{
  int w;
  int h;
  QString qs;
  h = mHeightMM;
  w = mWidthMM;
  QIN::MetricSystem ms;
  ms = (QIN::MetricSystem)xmlConfig->intValue("METRIC_SYSTEM");
  switch(ms)
  {
    case QIN::Millimetre:
      mpRightSpin->setSuffix( tr(" mm") );
      mpRightSpin->setMaxValue(w*100);
      mpLeftSpin->setSuffix( tr(" mm") );
      mpLeftSpin->setMaxValue(w*100);
      mpTopSpin->setSuffix( tr(" mm") );
      mpTopSpin->setMaxValue(h*100);
      mpBottomSpin->setSuffix( tr(" mm") );
      mpBottomSpin->setMaxValue(h*100);
      break;
    case QIN::Centimetre:
      mpRightSpin->setSuffix( tr(" cm") );
      mpRightSpin->setMaxValue(w*10);
      mpLeftSpin->setSuffix( tr(" cm") );
      mpLeftSpin->setMaxValue(w*10);
      mpTopSpin->setSuffix( tr(" cm") );
      mpTopSpin->setMaxValue(h*10);
      mpBottomSpin->setSuffix( tr(" cm") );
      mpBottomSpin->setMaxValue(h*10);
      break;
    case  QIN::Inch:
      mpRightSpin->setSuffix( tr(" inch") );
      mpRightSpin->setMaxValue(100.0*double(w)/25.4);
      mpLeftSpin->setSuffix( tr(" inch") );
      mpLeftSpin->setMaxValue(100.0*double(w)/25.4);
      mpTopSpin->setSuffix( tr(" inch") );
      mpTopSpin->setMaxValue(100.0*double(h)/25.4);
      mpBottomSpin->setSuffix( tr(" inch") );
      mpBottomSpin->setMaxValue(100.0*double(h)/25.4);
      break;
    default:;//do nothing--shouldn't happen
  }
}
/**  */
void QCopyPrint::paintEvent(QPaintEvent*)
{
  qDebug("QCopyPrint::paintEvent()");
  double dx;
  double dy;
  if(mStartHeight != (mpScaleBox->height() + mpMarginBox->height() - 30))
    setPreviewSize();
  mpPreviewFrame->erase();
  double d;
  d = double(mpPreviewFrame->height())/double(mHeightMM);
  QPainter qp;
  qp.begin(mpPreviewFrame);
//mpPixmap
  dx = int(leftMarginMM()*d);
  dy = int(topMarginMM()*d);
  if(mpPixmap && !mpPixmap->isNull())
  {
    if(mScaleMode == ScaleMode_MaxSize)
      qp.drawPixmap(0,0,*mpPixmap);
    else
      qp.drawPixmap(dx,dy,*mpPixmap);
  }
  else
    mpPreviewFrame->erase();
  qp.setPen(QPen(QColor(red),1,Qt::DashLine));
//left
  qp.drawLine(int(d*leftMarginMM()),1,
              int(d*leftMarginMM()),mpPreviewFrame->height()-1);
//top
  qp.drawLine(1,int(d*topMarginMM()),mpPreviewFrame->width()-1,
              int(d*topMarginMM()));
//bottom
  qp.drawLine(1,mpPreviewFrame->height()-1-int(d*bottomMarginMM()),
              mpPreviewFrame->width()-1,
              mpPreviewFrame->height()-1-int(d*bottomMarginMM()));
//right
  qp.drawLine(mpPreviewFrame->width()-1-int(d*rightMarginMM()),1,
              mpPreviewFrame->width()-1-int(d*rightMarginMM()),
              mpPreviewFrame->height()-1);
  qp.end();
}
/**  */
void QCopyPrint::slotPrint()
{
  qDebug("QCopyPrint::slotPrint()");
  bool printit;
  int printcnt;
  int i;
  double m11;
  double m22;
  double xfak;
  double yfak;
  int dx;
  int dy;
  int xsize;//size - margins
  int ysize;
  QImage im;

  printit = false;
  if(mShowPrintDialog == true)
  {
    if(!setupPrinter())
      return;
  }
  mPrinter.setFullPage(true);
  QPainter painter;
  if(!painter.begin(&mPrinter))
  {
    qDebug("p.begin(&mPrinter) failed");
    return;
  }
  setCursor(Qt::waitCursor);
  //adjust printer resolution to image resolution
  //has nothing to do with the physical printer resolution
  m11 = double(mpImageXResSpin->value())/mPrinterRes;
  if(!mpImageResCheckBox->isOn())
    m22 = double(mpImageYResSpin->value())/mPrinterRes;
  else
    m22 = m11;
  painter.setWindow(0,0,int(double(mWidth)*m11),
                int(double(mHeight)*m22));
  switch(mScaleMode)
  {
    case ScaleMode_Free:
      dx = int(leftMarginMM()*m11*mPrinterRes/25.4);
      dy = int(topMarginMM()*m22*mPrinterRes/25.4);
      im = mpImage->smoothScale(mpImage->width()*mFreescaleFactor,
                                mpImage->height()*mFreescaleFactor);
      printit = true;
      break;
    case ScaleMode_MaxSize:
      if(mpProportionCheckBox->isOn())
      {
        xfak = double(mWidth*m11)/double(mpImage->width());
        yfak = double(mHeight*m22)/double(mpImage->height());
        if(xfak<yfak)
          yfak = xfak;
        else
          xfak = yfak;
        im = mpImage->smoothScale(mpImage->width()*xfak,
                            mpImage->height()*yfak);
      }
      else
      {
        im = mpImage->smoothScale(mWidth*m11,mHeight*m22);
      }
      dx = 0;
      dy = 0;
      printit = true;
      break;
    case ScaleMode_MarginSize:
      dx = int(leftMarginMM()*m11*mPrinterRes/25.4);
      dy = int(topMarginMM()*m22*mPrinterRes/25.4);
      xsize = int(double(mWidth)*m11) - dx -
              int(rightMarginMM()*m11*mPrinterRes/25.4);
      ysize = int(double(mHeight)*m22) - dy -
              int(bottomMarginMM()*m22*mPrinterRes/25.4);
      if(mpProportionCheckBox->isOn())
      {
        xfak = double(xsize)/double(mpImage->width());
        yfak = double(ysize)/double(mpImage->height());
        if(xfak<yfak)
          yfak = xfak;
        else
          xfak = yfak;
        im = mpImage->smoothScale(mpImage->width()*xfak,
                            mpImage->height()*yfak);
      }
      else
      {
        im = mpImage->smoothScale(xsize,ysize);
      }
      printit = true;
      break;
    default:;
  }

  int x,y;
  QRgb rgb_b = QColor(255,255,255).rgb();
  QRgb rgb;
  int alpha,inv_alpha;
  if(im.hasAlphaBuffer() && (im.depth() == 32))
  {
    for(y=0;y<im.height();y++)
    {
      for(x=0;x<im.width();x++)
      {
        rgb = im.pixel(x,y);
        inv_alpha = 255 - qAlpha(rgb);
        alpha = qAlpha(rgb);
        im.setPixel(x,y,
                    qRgb(qRed(rgb_b)*inv_alpha/255 + qRed(rgb)*alpha/255,
                         qGreen(rgb_b)*inv_alpha/255 + qGreen(rgb)*alpha/255,
                         qBlue(rgb_b)*inv_alpha/255 + qBlue(rgb)*alpha/255));
      }
    }
    im.setAlphaBuffer(false);
  }

  int xc,yc;
  int sw,sh;
  int xpos,ypos;
  int n1,n2;
  int steps;
  int stepcnt;
  bool cancel;
  cancel = false;
  stepcnt = 0;
  QString ps;
  if(printit)
  {
    printcnt = mPrinter.numCopies();
    steps = printcnt;
    if(steps <= 1)//work around qt bug
      steps = 2;
    QProgressDialog progress("",tr("Stop"),steps,
                             this,"progress",TRUE);
    progress.setCaption(tr("Printing..."));
    progress.setMinimumDuration(0);
    progress.setProgress(0);
    qApp->processEvents();
    for (i=0; i<printcnt; i++)
    {
      ps = tr("Printing page %1 of %2.").arg(i+1).arg(printcnt);
      progress.setLabelText(ps);
      progress.setProgress(i+1);
      qApp->processEvents();
      stepcnt = 0;
      painter.drawImage(dx,dy,im);
      qApp->processEvents();
      if(cancel) break;
      if(i<printcnt - 1)
        mPrinter.newPage();
    }
    progress.setProgress(steps);
  }
  painter.end();
  if(!cancel)
    mPrinted = true;
  setCursor(Qt::arrowCursor);
  qDebug("QCopyPrint::slotPrint() END");
}
/**  */
void QCopyPrint::slotLeftSpinValue(int)
{
  if(mScaleMode == ScaleMode_MarginSize) resizePreviewPixmap();
  repaint();
}
/**  */
void QCopyPrint::slotRightSpinValue(int)
{
  if(mScaleMode == ScaleMode_MarginSize) resizePreviewPixmap();
  repaint();
}
/**  */
void QCopyPrint::slotTopSpinValue(int)
{
  if(mScaleMode == ScaleMode_MarginSize) resizePreviewPixmap();
  repaint();
}
/**  */
void QCopyPrint::slotBottomSpinValue(int)
{
  if(mScaleMode == ScaleMode_MarginSize) resizePreviewPixmap();
  repaint();
}
/** Loads an image for printing. Returns true, if this
was successfull, or false, if an error occured. */
bool QCopyPrint::loadImage(QString path,bool b)
{
  ImageIOSupporter iisup;
  int dpix;
  int dpiy;

  mPrinted = b;
  if(mpImage)
  {
    delete mpImage;
    mpImage = 0L;
  }
  mpImage = new QImage();
  if(!iisup.loadImage(*mpImage,path))
    return false;
  if(!mpPixmap)
  {
    mpPixmap = new QPixmap();
  }
//ckeck whether the physical pixel size is available
  if(mpImage->dotsPerMeterX() >0)
  {//yes
    dpix = int(double(mpImage->dotsPerMeterX())*0.0254);
    mpImageXResSpin->setValue(dpix);
  }
  else
  {
    dpix = 75;
    mpImageXResSpin->setValue(75);
  }
  if(mpImage->dotsPerMeterY() >0)
  {//yes
    dpiy = int(double(mpImage->dotsPerMeterY())*0.0254);
    mpImageYResSpin->setValue(dpiy);
  }
  else
  {
    dpiy = 75;
    mpImageYResSpin->setValue(75);
  }
  if(mpPrintButton->isHidden()) mpPrintButton->show();
  resizePreviewPixmap();
  mImagePath = path;
}
/**  */
void QCopyPrint::slotBindImageRes(bool b)
{
  mpImageYResSpin->setEnabled(b^true);
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::slotImageXRes(int)
{
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::slotImageYRes(int)
{
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::resizePreviewPixmap()
{
  qDebug("QCopyPrint::resizePreviewPixmap()");
  if(!mpImage || !mpPixmap ) return;
  if(!isVisible() || mpImage->isNull()) return;
  QImage im;
  int w;
  int h;
  double d;
  double xfak;
  double yfak;
  double wmm;//image width in mm
  double hmm;//image height in mm
//create the pixmap for drawing in the preview widget
//size depends on selected scale mode
  switch(mScaleMode)
  {
    case ScaleMode_Free:
      yfak = double(mpPreviewFrame->height())/double(mHeightMM);
      xfak = double(mpPreviewFrame->width())/double(mWidthMM);
      wmm = double(mpImage->width())/double(mpImageXResSpin->value()) * 25.4;
      //bind values ?
      if(mpImageResCheckBox->isOn())
        hmm = double(mpImage->height())/double(mpImageXResSpin->value()) * 25.4;
      else
        hmm = double(mpImage->height())/double(mpImageYResSpin->value()) * 25.4;
      im = mpImage->smoothScale(wmm*xfak*mFreescaleFactor,hmm*yfak*mFreescaleFactor);
      break;
    case ScaleMode_MaxSize:
      //create a temporary image with the same resolution  for height
      //and width if neccessary
      im = mpImage->copy();
      if(!mpImageResCheckBox->isOn() &&
         (mpImageXResSpin->value() != mpImageYResSpin->value()))
      {
        if(mpImageXResSpin->value()< mpImageYResSpin->value())
        {
          yfak = double(mpImageXResSpin->value())/double(mpImageYResSpin->value());
          im = im.smoothScale(im.width(),im.height()*yfak);
        }
        else
        {
          xfak = double(mpImageYResSpin->value())/double(mpImageXResSpin->value());
          im = im.smoothScale(im.width()*xfak,im.height());
        }
      }
      if(mpProportionCheckBox->isOn())
      {
        xfak = double(mpPreviewFrame->width())/double(im.width());
        yfak = double(mpPreviewFrame->height())/double(im.height());
        if(xfak<yfak)
          yfak = xfak;
        else
          xfak = yfak;
        im = im.smoothScale(im.width()*xfak,
                               im.height()*yfak);
      }
      else
      {
        im = im.smoothScale(mpPreviewFrame->width(),
                            mpPreviewFrame->height());
      }
      break;
    case ScaleMode_MarginSize:
      //create a temporary image with the same resolution  for height
      //and width if neccessary
      im = mpImage->copy();
      if(!mpImageResCheckBox->isOn() &&
         (mpImageXResSpin->value() != mpImageYResSpin->value()))
      {
        if(mpImageXResSpin->value()< mpImageYResSpin->value())
        {
          yfak = double(mpImageXResSpin->value())/double(mpImageYResSpin->value());
          im = im.smoothScale(im.width(),im.height()*yfak);
        }
        else
        {
          xfak = double(mpImageYResSpin->value())/double(mpImageXResSpin->value());
          im = im.smoothScale(im.width()*xfak,im.height());
        }
      }
      d = double(mpPreviewFrame->width())/double(mWidthMM);
      w = mpPreviewFrame->width() -
          int(d*leftMarginMM()) -
          int(d*rightMarginMM());
      d = double(mpPreviewFrame->height())/double(mHeightMM);
      h = mpPreviewFrame->height() -
          int(d*topMarginMM()) -
          int(d*bottomMarginMM());
      if(mpProportionCheckBox->isOn())
      {
        xfak = double(w)/double(im.width());
        yfak = double(h)/double(im.height());
        if(xfak<yfak)
          yfak = xfak;
        else
          xfak = yfak;
        im = im.smoothScale(im.width()*xfak,
                               im.height()*yfak);
      }
      else
      {
        im = im.smoothScale(w,h);
      }
      break;
    default:;
  }

  if(im.depth() < 32)
    im = im.convertDepth(32);
  int x,y;
  QRgb rgb_b = QColor(255,255,255).rgb();
  QRgb rgb;
  int alpha,inv_alpha;
  if(im.hasAlphaBuffer())
  {
    for(y=0;y<im.height();y++)
    {
      for(x=0;x<im.width();x++)
      {
        rgb = im.pixel(x,y);
        inv_alpha = 255 - qAlpha(rgb);
        alpha = qAlpha(rgb);
        im.setPixel(x,y,
                    qRgb(qRed(rgb_b)*inv_alpha/255 + qRed(rgb)*alpha/255,
                         qGreen(rgb_b)*inv_alpha/255 + qGreen(rgb)*alpha/255,
                         qBlue(rgb_b)*inv_alpha/255 + qBlue(rgb)*alpha/255));
      }
    }
    im.setAlphaBuffer(false);
  }
  if(!im.isNull())
    mpPixmap->convertFromImage(im);
  else
    mpPixmap->resize(0,0);
  repaint();
  mpSizeLabel->setText(infoString());
}
/**  */
void QCopyPrint::slotScaling(int i)
{
  QString qs;
  switch(i)
  {
    case 0://free scaling
      mpScaleSpin->setEnabled(true);
      mpProportionCheckBox->setEnabled(false);
      mScaleMode = ScaleMode_Free;
      break;
    case 1://maximum size
      mpScaleSpin->setEnabled(false);
      mpProportionCheckBox->setEnabled(true);
      mScaleMode = ScaleMode_MaxSize;
      break;
    case 2://margin size
      mpScaleSpin->setEnabled(false);
      mpProportionCheckBox->setEnabled(true);
      mScaleMode = ScaleMode_MarginSize;
      break;
    default:;
  }
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::slotFreeScale(int value)
{
  mFreescaleFactor = double(value)/100.0;
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::slotProportion(bool)
{
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::setResolutionX(int xres)
{
  mpImageXResSpin->setValue(xres);
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::setResolutionY(int yres)
{
  mpImageYResSpin->setValue(yres);
  resizePreviewPixmap();
}
void QCopyPrint::slotSaveImage()
{
  QPreviewFileDialog qpfd(false,false,this,0,true);
  qpfd.setMode(QFileDialog::AnyFile);
  qpfd.loadImage(mImagePath);
  if(qpfd.exec())
    emit signalImageSaved(qpfd.selectedFile());
}
/**  */
bool QCopyPrint::fileCopy(QString source, QString dest)
{
  const int bufSize = 16384; // 16Kb buffer
  QFile s(source);
  QFile t(dest);
  char *buf;
  int len;
  if(s.open(IO_ReadOnly) && t.open(IO_WriteOnly))
  {
    buf = new char[bufSize];
    len = s.readBlock(buf, bufSize);
    do
    {
      t.writeBlock(buf, len);
      len = s.readBlock(buf, len);
    } while (len > 0);
    s.close();
    t.close();
    delete[] buf;
    if(len > -1) return true;
  }
  return false;
}
/**  */
void QCopyPrint::initSettings()
{
  qDebug("QCopyPrint::initSettings()");
  int rmm,lmm,tmm,bmm;
//copyprint settings
  mpScaleSpin->setValue(xmlConfig->intValue("COPY_SCALE_FACTOR",100));
  mpProportionCheckBox->setChecked(xmlConfig->boolValue("COPY_KEEP_ASPECT",true));
  slotFreeScale(mpScaleSpin->value());
  mpScaleGroup->setButton(xmlConfig->intValue("COPY_SCALE",0));
  slotScaling(xmlConfig->intValue("COPY_SCALE",0));
//margins are alway stored in mm * 100
  lmm = xmlConfig->intValue("COPY_MARGIN_LEFT");

  rmm = xmlConfig->intValue("COPY_MARGIN_RIGHT");

  tmm = xmlConfig->intValue("COPY_MARGIN_TOP");

  bmm = xmlConfig->intValue("COPY_MARGIN_BOTTOM");

  QIN::MetricSystem ms;
  ms = (QIN::MetricSystem)xmlConfig->intValue("METRIC_SYSTEM");
  switch(ms)
	{
		case QIN::Millimetre:
      mMetricFactor = 1.0;
			break;
		case QIN::Centimetre:
      mMetricFactor = 0.1;
			break;
		case  QIN::Inch:
	     mMetricFactor = 1.0/25.4;
			break;
    default:;//do nothing--shouldn't happen
	}
  mpLeftSpin->setValue(int(double(lmm)*mMetricFactor/10.0));
  mpRightSpin->setValue(int(double(rmm)*mMetricFactor/10.0));
  mpTopSpin->setValue(int(double(tmm)*mMetricFactor/10.0));
  mpBottomSpin->setValue(int(double(bmm)*mMetricFactor/10.0));

  mpImageResCheckBox->setChecked(xmlConfig->boolValue("COPY_RESOLUTION_BIND"));

//printer settings
#ifndef KDEAPP
  mPrinter.setPrinterName(xmlConfig->stringValue("PRINTER_NAME"));
  mPrinter.setOutputFileName(xmlConfig->stringValue("PRINTER_FILENAME"));
  mPrinter.setOutputToFile(xmlConfig->boolValue("PRINTER_MODE"));
  mPrinter.setNumCopies(xmlConfig->intValue("PRINTER_COPIES"));
  mPrinter.setColorMode((QPrinter::ColorMode)xmlConfig->intValue("PRINTER_COLOR"));
  mPrinter.setOrientation((QPrinter::Orientation)xmlConfig->intValue("PRINTER_PAPER_ORIENTATION"));
  mPrinter.setPageSize((QPrinter::PageSize)xmlConfig->intValue("PRINTER_PAPER_FORMAT"));
#endif
}
/**  */
void QCopyPrint::showEvent(QShowEvent* e)
{
  QWidget::showEvent(e);
  qApp->processEvents();
  resize(minimumSizeHint());
  qApp->processEvents();
  setPreviewSize();
  resizePreviewPixmap();
}
/**  */
void QCopyPrint::setImage(QImage* i,bool b)
{
  int dpix;
  int dpiy;
  mPrinted = b;
  if(mpImage)
  {
    delete mpImage;
    mpImage = 0L;
  }
  mpImage = new QImage(i->copy());
  if(!mpImage) return;
  if(mpImage->isNull()) return;
  if(!mpPixmap)
  {
    mpPixmap = new QPixmap();
  }
//ckeck whether the physical pixel size is available
  if(mpImage->dotsPerMeterX() >0)
  {//yes
    dpix = int(double(mpImage->dotsPerMeterX())*0.0254);
    mpImageXResSpin->setValue(dpix);
  }
  else
  {
    dpix = 75;
    mpImageXResSpin->setValue(75);
  }
  if(mpImage->dotsPerMeterY() >0)
  {//yes
    dpiy = int(double(mpImage->dotsPerMeterY())*0.0254);
    mpImageYResSpin->setValue(dpiy);
  }
  else
  {
    dpiy = 75;
    mpImageYResSpin->setValue(75);
  }
  if(mpPrintButton->isHidden()) mpPrintButton->show();
  resizePreviewPixmap();
}
/**  */
double QCopyPrint::leftMarginMM()
{
 return (double(mpLeftSpin->value())/100.0/mMetricFactor);
}
/**  */
double QCopyPrint::rightMarginMM()
{
 return (double(mpRightSpin->value())/100.0/mMetricFactor);
}
/**  */
double QCopyPrint::topMarginMM()
{
 return (double(mpTopSpin->value())/100.0/mMetricFactor);
}
/**  */
double QCopyPrint::bottomMarginMM()
{
 return (double(mpBottomSpin->value())/100.0/mMetricFactor);
}
/**  */
void QCopyPrint::closeEvent(QCloseEvent* e)
{
  int exit;
  if(!mPrinted)
  {
    exit=QMessageBox::warning(this, tr("Close..."),
                              tr("The image has not been printed.\n"
                              "Do you really want to close the dialog?\n"),
                              tr("&Close"),tr("C&ancel"));
    if(exit == 1)
    {
      e->ignore();
      return;
    }
    mPrinted = true;
  }
  QDialog::closeEvent(e);
}
/**  */
void QCopyPrint::setUnknownImage(int w,int h,int xres,int yres)
{
  qDebug("QCopyPrint::setUnknownImage()");
  if(mpImage)
  {
    delete mpImage;
    mpImage = 0L;
  }
  mpImage = new QImage();
  if(!mpImage) return;
  mpImage->create(w,h,8,256);
  mpImage->fill(100);
  if(!mpPixmap)
  {
    mpPixmap = new QPixmap();
  }
  if(!mpPixmap) return;
  mpImageXResSpin->setValue(xres);
  mpImageYResSpin->setValue(yres);
  mpPrintButton->hide();
  mpSaveButton->hide();
  resizePreviewPixmap();
}
/**  */
QString QCopyPrint::infoString()
{
  qDebug("QCopyPrint::infoString()");
  QString qs;
  QString qs2;
  double xfak,yfak;
  double mmwidth,mmheight;
  QIN::MetricSystem ms;

  if(!mpPixmap || mpPixmap->isNull()) return tr("not available");
  //size of one pixel in the preview in mm
  yfak = double(mHeightMM)/double(mpPreviewFrame->height());
  xfak = double(mWidthMM)/double(mpPreviewFrame->width());

  mmwidth = double(mpPixmap->width())*xfak;
  mmheight = double(mpPixmap->height())*yfak;

  ms = (QIN::MetricSystem)(xmlConfig->intValue("METRIC_SYSTEM"));
  qs2 = tr("Image size: ");
  switch(ms)
  {
   case QIN::Millimetre:
      qs.sprintf("%.2f",mmwidth);
      qs2 +=qs+tr(" mm x ");
      qs.sprintf("%.2f",mmheight);
      qs2 +=qs+tr(" mm");
      break;
   case QIN::Centimetre:
      qs.sprintf("%.2f",mmwidth/10.0);
      qs2 +=qs+tr(" cm x ");
      qs.sprintf("%.2f",mmheight/10.0);
      qs2 +=qs+tr(" cm");
      break;
    case  QIN::Inch:
      qs.sprintf("%.2f",mmwidth/25.4);
      qs2 +=qs+tr(" inch x ");
      qs.sprintf("%.2f",mmheight/25.4);
      qs2 +=qs+tr(" inch");
      break;
    default:
      qs.sprintf("%.2f",mmwidth);
      qs2 +=qs+tr(" mm x ");
      qs.sprintf("%.2f",mmheight);
      qs2 +=qs+tr(" mm");
	}
  return qs2;
}
/**  */
void QCopyPrint::createWhatsThisHelp()
{
//left spin
  QWhatsThis::add(mpLeftSpin,tr("Use this spinbox to adjust the left margin."));
//right spin
  QWhatsThis::add(mpRightSpin,tr("Use this spinbox to adjust the right margin."));
//top spin
  QWhatsThis::add(mpTopSpin,tr("Use this spinbox to adjust the top margin."));
//bottom spin
  QWhatsThis::add(mpBottomSpin,tr("Use this spinbox to adjust the bottom margin."));
//bind resolution check box
  QWhatsThis::add(mpImageResCheckBox,tr("Activate this checkbox to bind the "
														 "resolution values. This means, that the "
                             "x-resolution value is also used for the "
                             "y-resolution."));
//x-res spin
  QWhatsThis::add(mpImageXResSpin,tr("Use this spinbox to adjust the x-resolution."));
//y-res spin
  QWhatsThis::add(mpImageYResSpin,tr("Use this spinbox to adjust the y-resolution."));
//scale spin
  QWhatsThis::add(mpScaleSpin,tr("Use this spinbox to adjust the scaling factor."));
//free radio
  QWhatsThis::add(mpRadioButtonFree,tr("If you activate this radiobutton, the "
                              "image is scaled dependent on the scale factor "
                              "and the resolution settings."));
//maximum radio
  QWhatsThis::add(mpRadioButtonMargin,tr("If you activate this radiobutton, the "
                              "image is scaled dependent on the margin sizes."));
//margin radio
  QWhatsThis::add(mpRadioButtonMax,tr("If you activate this radiobutton, the "
                              "image is scaled dependent on the page size."));
//margin radio
  QWhatsThis::add(mpProportionCheckBox,tr("If you activate this checkbox, the "
                              "proportion between height and width is kept "
                              "when scaling."));
//save button
  QWhatsThis::add(mpSaveButton,tr("Click this button to save the image. "
                              "A normal filedialog will be shown."));
//setup button
  QWhatsThis::add(mpSetupButton,tr("Click this button to setup the printer. "
                              "This is done with a Qt standard printer dialog."));
//print button
  QWhatsThis::add(mpPrintButton,tr("Click this button to start printing."));
//close button
  QWhatsThis::add(mpCloseButton,tr("Click this button to close the window."));
//save settings button
  QWhatsThis::add(mpSettingsButton,tr("Click this button to save the current "
                              "settings to the configuration file. This "
                              "means, that these settings are used as "
                              "the default settings for all print dialogs. "
                              "It does not affect other print dialogs, "
                              "which are currently shown."));
}
/**  */
void QCopyPrint::slotSaveSettings()
{
  qDebug("QCopyPrint::slotSaveSettings()");
  double d;
  //save printer settings
#ifndef KDEAPP
  xmlConfig->setStringValue("PRINTER_NAME",mPrinter.printerName());
  xmlConfig->setStringValue("PRINTER_FILENAME",mPrinter.outputFileName());
  xmlConfig->setBoolValue("PRINTER_MODE",mPrinter.outputToFile());
  xmlConfig->setIntValue("PRINTER_COLOR",int(mPrinter.colorMode()));
  xmlConfig->setIntValue("PRINTER_PAPER_ORIENTATION",
                                     int(mPrinter.orientation()));
  xmlConfig->setIntValue("PRINTER_PAPER_FORMAT",int(mPrinter.pageSize()));
  xmlConfig->setIntValue("PRINTER_COPIES",mPrinter.numCopies());
#endif
  d = (double(mpLeftSpin->value())/mMetricFactor);
  xmlConfig->setIntValue("COPY_MARGIN_LEFT",int(d*10.0));

  d = (double(mpRightSpin->value())/mMetricFactor);
  xmlConfig->setIntValue("COPY_MARGIN_RIGHT",int(d*10.0));

  d = (double(mpTopSpin->value())/mMetricFactor);
  xmlConfig->setIntValue("COPY_MARGIN_TOP",int(d*10.0));

  d = (double(mpBottomSpin->value())/mMetricFactor);
  xmlConfig->setIntValue("COPY_MARGIN_BOTTOM",int(d*10.0));

  xmlConfig->setBoolValue("COPY_RESOLUTION_BIND",
                      mpImageResCheckBox->isChecked());
  xmlConfig->setIntValue("MULTI_XRESOLUTION",mpImageXResSpin->value());
  xmlConfig->setIntValue("MULTI_YRESOLUTION",mpImageYResSpin->value());
  xmlConfig->setIntValue("COPY_SCALE_FACTOR",mpScaleSpin->value());
  xmlConfig->setIntValue("COPY_SCALE",int(mScaleMode));
}
/**  */
bool QCopyPrint::printed()
{
  return mPrinted;
}
/** No descriptions */
void QCopyPrint::updateDeviceMetrics(bool full_page)
{
  qDebug("QCopyPrint::updateDeviceMetrics()");
//get real page size
  if(full_page)
    mPrinter.setFullPage(true);
  else
    return;
//    printer.setFullPage(false);
#ifdef KDEAPP
  mPrinter.doPrepare();
#endif
#ifdef USE_QT3
  mPrinterRes = mPrinter.resolution();
#else
  mPrinterRes = 72.0;
#endif
  QPaintDeviceMetrics pdm(&mPrinter);
  mWidthMM = pdm.widthMM();
  mHeightMM = pdm.heightMM();
  mWidth = pdm.width();
  mHeight = pdm.height();
  qDebug("mWidthMM: %i",mWidthMM);
  qDebug("mHeightMM: %i",mHeightMM);
  qDebug("mWidth: %i",mWidth);
  qDebug("mHeight: %i",mHeight);
#ifdef KDEAPP
  mPrinter.doFinish();
#endif
}
/** No descriptions */
bool QCopyPrint::isSetup()
{
  return mIsSetup;
}
/** No descriptions */
bool QCopyPrint::printToFile()
{
  if(isSetup())
    return mPrinter.outputToFile();
  return false;
}
/** No descriptions */
void QCopyPrint::setShowPrintDialog(bool show_dlg)
{
  mShowPrintDialog = show_dlg;
}
/** No descriptions */
QString QCopyPrint::printFilename()
{
  if(isSetup())
    return mPrinter.outputFileName();
  return QString::null;
}
/** No descriptions */
void QCopyPrint::setPrintFilename(QString name)
{
  if(isSetup())
    mPrinter.setOutputFileName(name);
}
