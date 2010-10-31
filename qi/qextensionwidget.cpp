/***************************************************************************
                          qextensionwidget.cpp  -  description
                             -------------------
    begin                : Sun Aug 20 2000
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

#include "./images/fileopen.xpm"
#include "qdoublespinbox.h"
#include "qextensionwidget.h"
//s #include "qfiledialogext.h"
#include "qxmlconfig.h"

#include <qapplication.h>
//s #include <qarray.h>
#include <q3buttongroup.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qcolor.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <q3filedialog.h> //s
#include <q3groupbox.h>
#include <q3hbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <q3listbox.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpalette.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qstring.h>
#include <q3textstream.h>
#include <qtoolbutton.h>
#include <q3whatsthis.h>
#include <qwidget.h>
#include <q3widgetstack.h>
#ifndef QIS_NO_STYLES
#include <qcdestyle.h>
#include <qmotifstyle.h>
//#include <qmotifplusstyle.h>
//#include <qplatinumstyle.h>
//#include <qsgistyle.h>
#include <qwindowsstyle.h>
//Added by qt3to4:
#include <Q3HBoxLayout>
#include <Q3GridLayout>
#include <Q3Frame>
#endif

QExtensionWidget::QExtensionWidget(QWidget* parent,const char* name,
                                   bool modal,Qt::WFlags f)
                 :QDialog(parent,name,modal,f)
{
  setCaption(tr("MaxView - Options"));
  mMetricSystem = QIN::Millimetre;
  mFilenameGenerationChanged = false;
  initWidget();
  loadSettings();
}
QExtensionWidget::~QExtensionWidget()
{
}
/**  */
void QExtensionWidget::initWidget()
{
  QString qs;
  int subspacing;
  subspacing = 7;
	QPixmap* pixmap = new QPixmap((const char **)fileopen);
//the main layout
	mpMainLayout = new Q3GridLayout(this,5,2);
  mpMainLayout->setMargin(8);
  mpMainLayout->setSpacing(5);
//create mpWhatsThisButton in a HBox
	QWidget* wtwidget = new QWidget(this);
  Q3HBoxLayout* qhbwt = new Q3HBoxLayout( wtwidget,4);
  mpWhatsThisButton = Q3WhatsThis::whatsThisButton(wtwidget);
	mpWhatsThisButton->setAutoRaise(FALSE);
  qhbwt->addStretch(1);
  qhbwt->addWidget(mpWhatsThisButton);
  mpMainLayout->addWidget(wtwidget,0,1);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    wtwidget->hide();
//mpPageListBox
  mpPageListBox = new Q3ListBox(this);
  mpMainLayout->addMultiCellWidget(mpPageListBox,1,4,0,0);

//mpTitleLabel
  mpTitleLabel = new QLabel(this);
  mpMainLayout->addWidget(mpTitleLabel,1,1);

//horizontal separator
  Q3Frame* sep1 = new Q3Frame(this);
  sep1->setFrameStyle(Q3Frame::HLine|Q3Frame::Sunken);
  sep1->setLineWidth(2);
  mpMainLayout->addWidget(sep1,2,1);

//mpPagesStack
  mpPagesStack = new Q3WidgetStack(this);
  mpMainLayout->addWidget(mpPagesStack,3,1);

//buttons
  Q3HBox* bhb = new Q3HBox(this);
	QPushButton* qpb1 = new QPushButton(tr("&OK"),bhb);
  connect(qpb1,SIGNAL(clicked()),this,SLOT(accept()));
  QWidget* dummy = new QWidget(bhb);
  bhb->setStretchFactor(dummy,1);
	QPushButton* qpb2 = new QPushButton(tr("&Cancel"),bhb);
  connect(qpb2,SIGNAL(clicked()),this,SLOT(reject()));
  mpMainLayout->addMultiCellWidget(bhb,5,5,0,1);

//metric system
  QWidget* opage = new QWidget(mpPagesStack);
  Q3GridLayout* sublayout = new Q3GridLayout(opage,4,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(3,1);
  mpRadioMM = new QRadioButton(tr("&Millimetre"),opage);
  mpRadioCM = new QRadioButton(tr("C&entimetre"),opage);
  mpRadioInch = new QRadioButton(tr("&Inch"),opage);
  sublayout->addWidget(mpRadioMM,0,0);
  sublayout->addWidget(mpRadioCM,1,0);
  sublayout->addWidget(mpRadioInch,2,0);
  mpBGroupMetricSystem = new Q3ButtonGroup(0);
  mpBGroupMetricSystem->insert(mpRadioMM);
  mpBGroupMetricSystem->insert(mpRadioCM);
  mpBGroupMetricSystem->insert(mpRadioInch);
  mpRadioMM->setChecked(TRUE);
  mpPagesStack->addWidget(opage,0);
  mpPageListBox->insertItem(tr("Metric system"));

//Layout
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,7,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(6,1);
  mpRadioScrollLayout = new QRadioButton(tr("&Scrollview"),opage);
  mpRadioTabLayout = new QRadioButton(tr("&Tabwidget"),opage);
  mpRadioMultiWindowLayout = new QRadioButton(tr("&Multi window"),opage);
  mpRadioListLayout = new QRadioButton(tr("&List"),opage);
  mpCheckSeparatePreview = new QCheckBox(tr("&Use separate preview window"),
                                       opage);
  mpWhatsThisCheckBox = new QCheckBox(tr("&Enable context help buttons"),
                                       opage);
  sublayout->addWidget(mpRadioScrollLayout,0,0);
  sublayout->addWidget(mpRadioTabLayout,1,0);
  sublayout->addWidget(mpRadioMultiWindowLayout,2,0);
  sublayout->addWidget(mpRadioListLayout,3,0);
  sublayout->addWidget(mpCheckSeparatePreview,4,0);
  sublayout->addWidget(mpWhatsThisCheckBox,5,0);
  mpBGroupLayout = new Q3ButtonGroup(0);
  mpBGroupLayout->insert(mpRadioScrollLayout);
  mpBGroupLayout->insert(mpRadioTabLayout);
  mpBGroupLayout->insert(mpRadioMultiWindowLayout);
  mpBGroupLayout->insert(mpRadioListLayout);
  mpRadioScrollLayout->setChecked(true);
  mpCheckSeparatePreview->setChecked(true);
  mpWhatsThisCheckBox->setChecked(true);
  mpPagesStack->addWidget(opage,1);
  mpPageListBox->insertItem(tr("Layout"));

//scanner
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,5,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(4,1);
  mpCheckIoMode = new QCheckBox (tr("Use &non blocking IO if available"),opage);
  sublayout->addWidget(mpCheckIoMode,0,0);
  mpCheckIoMode->setChecked(true);
  mpTranslationsCheckBox = new QCheckBox (tr("&Enable backend translations (restart required)"),
                                          opage);
  sublayout->addWidget(mpTranslationsCheckBox,1,0);
  mpTranslationsCheckBox->setChecked(true);
  mpTransPathCheckBox = new QCheckBox (tr("&Set path to backend translations (restart required)"),
                                       opage);
  sublayout->addWidget(mpTransPathCheckBox,2,0);

  Q3HBox* transhb = new Q3HBox(opage);
  new QLabel(tr("Translation path:"),transhb);
  mpEditTransPath = new QLineEdit(transhb);
  mpEditTransPath->setText(xmlConfig->stringValue(QString::null));
	mpButtonTransPath = new QToolButton(transhb);
  mpButtonTransPath->setPixmap(*pixmap);
  transhb->setStretchFactor(mpEditTransPath,1);
  sublayout->addWidget(transhb,3,0);
	connect(mpButtonTransPath,SIGNAL(clicked()),this,
          SLOT(slotChangeTransPath()));
	connect(mpTransPathCheckBox,SIGNAL(toggled(bool)),transhb,
          SLOT(setEnabled(bool)));
  mpTransPathCheckBox->setChecked(false);
  transhb->setEnabled(false);

  mpPagesStack->addWidget(opage,2);
  mpPageListBox->insertItem(tr("Scanner"));

//OCR
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,2,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(1,1);
  Q3HBox* ocrhb = new Q3HBox(opage);
  new QLabel(tr("OCR command:"),ocrhb);
  mpEditOcr = new QLineEdit(ocrhb);
  mpEditOcr->setText(xmlConfig->stringValue("GOCR_COMMAND"));
  sublayout->addWidget(ocrhb,0,0);
  mpPagesStack->addWidget(opage,3);
  mpPageListBox->insertItem(tr("OCR"));

//Image compression/quality
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,6,5);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(5,1);
  sublayout->setColStretch(2,1);
  ////TIFF 8bit
  QLabel* label1 = new QLabel(tr("TIFF 8bit compression"),opage);
  mpTiff8Combo = new QComboBox(false,opage);
  mpTiff8Combo->insertItem(tr("none"),0);
  mpTiff8Combo->insertItem(tr("JPEG DCT"),1);
  mpTiff8Combo->insertItem(tr("packed bits"),2);
  sublayout->addWidget(label1,0,0);
  sublayout->addMultiCellWidget(mpTiff8Combo,0,0,1,4);
  ////TIFF lineart
  label1 = new QLabel(tr("TIFF lineart compression"),opage);
  mpTiffLineartCombo = new QComboBox(false,opage);
  mpTiffLineartCombo->insertItem(tr("none"),0);
  mpTiffLineartCombo->insertItem(tr("packed bits"),1);
  mpTiffLineartCombo->insertItem(tr("CCITT 1D Huffman"),2);
  mpTiffLineartCombo->insertItem(tr("CCITT group3 fax"),3);
  mpTiffLineartCombo->insertItem(tr("CCITT group4 fax"),4);
  sublayout->addWidget(label1,1,0);
  sublayout->addMultiCellWidget(mpTiffLineartCombo,1,1,1,4);
  ////TIFF JPEG-quality
  label1 = new QLabel(tr("TIFF-JPEG quality"),opage);
  QLabel* label1a = new QLabel(tr("low"),opage);
  QLabel* label1b = new QLabel(tr("high"),opage);
  mpTiffJpegSlider = new QSlider(Qt::Horizontal,opage);
  mpTiffJpegSlider->setRange(0,100);
  mpTiffJpegSlider->setLineStep(1);
  mpTiffJpegSlider->setPageStep(1);
  mpTiffJpegSlider->setFocusPolicy(Qt::StrongFocus);
  mpTiffJpegLabel = new QLabel("0000",opage);
  mpTiffJpegLabel->setFixedWidth(mpTiffJpegLabel->sizeHint().width());
  mpTiffJpegLabel->setText("0");
  sublayout->addWidget(label1,2,0);
  sublayout->addWidget(label1a,2,1);
  sublayout->addWidget(mpTiffJpegSlider,2,2);
  sublayout->addWidget(label1b,2,3);
  sublayout->addWidget(mpTiffJpegLabel,2,4);
  connect(mpTiffJpegSlider,SIGNAL(valueChanged(int)),
          this,SLOT(slotTiffJpegQuality(int)));

  ////JPEG-quality
  label1 = new QLabel(tr("JPEG quality"),opage);
  label1a = new QLabel(tr("low"),opage);
  label1b = new QLabel(tr("high"),opage);
  mpJpegSlider = new QSlider(Qt::Horizontal,opage);
  mpJpegSlider->setRange(0,100);
  mpJpegSlider->setLineStep(1);
  mpJpegSlider->setPageStep(1);
  mpJpegSlider->setFocusPolicy(Qt::StrongFocus);
  mpJpegLabel = new QLabel("0000",opage);
  mpJpegLabel->setFixedWidth(mpJpegLabel->sizeHint().width());
  mpJpegLabel->setText("0");
  sublayout->addWidget(label1,3,0);
  sublayout->addWidget(label1a,3,1);
  sublayout->addWidget(mpJpegSlider,3,2);
  sublayout->addWidget(label1b,3,3);
  sublayout->addWidget(mpJpegLabel,3,4);
  connect(mpJpegSlider,SIGNAL(valueChanged(int)),
          this,SLOT(slotJpegQuality(int)));

  ////PNG-compression
  label1 = new QLabel(tr("PNG compression"),opage);
  label1a = new QLabel(tr("low"),opage);
  label1b = new QLabel(tr("high"),opage);
  mpPngSlider = new QSlider(Qt::Horizontal,opage);
  mpPngSlider->setRange(0,9);
  mpPngSlider->setLineStep(1);
  mpPngSlider->setPageStep(1);
  mpPngSlider->setFocusPolicy(Qt::StrongFocus);
  mpPngLabel = new QLabel("0000",opage);
  mpPngLabel->setFixedWidth(mpPngLabel->sizeHint().width());
  mpPngLabel->setText("0");
  sublayout->addWidget(label1,4,0);
  sublayout->addWidget(label1a,4,1);
  sublayout->addWidget(mpPngSlider,4,2);
  sublayout->addWidget(label1b,4,3);
  sublayout->addWidget(mpPngLabel,4,4);
  mpPagesStack->addWidget(opage,4);
  mpPageListBox->insertItem(tr("Image compression/quality"));
  connect(mpPngSlider,SIGNAL(valueChanged(int)),
          this,SLOT(slotPngCompression(int)));

//History
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,5,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(4,1);
  mpCheckBoxHistory = new QCheckBox(tr("&Enable history"),opage);
  Q3HBox* histhb = new Q3HBox(opage);
  mpCheckBoxHistoryEntries = new QCheckBox(tr("&Maximum number of entries"),histhb);
  mpSpinHistoryNumber = new QSpinBox(1,9999,1,histhb);
  histhb->setStretchFactor(mpCheckBoxHistoryEntries,1);
  mpCheckBoxHistoryPreviews = new QCheckBox(tr("Create &previews"),opage);
  mpCheckBoxHistoryDelete = new QCheckBox(tr("&Delete on exit"),opage);

  sublayout->addWidget(mpCheckBoxHistory,0,0);
  sublayout->addWidget(histhb,1,0);
  sublayout->addWidget(mpCheckBoxHistoryPreviews,2,0);
  sublayout->addWidget(mpCheckBoxHistoryDelete,3,0);
  mpPagesStack->addWidget(opage,5);
  mpPageListBox->insertItem(tr("History"));

  connect(mpCheckBoxHistory,SIGNAL(toggled(bool)),
          this,SLOT(slotEnableHistory(bool)));
  connect(mpCheckBoxHistoryEntries,SIGNAL(toggled(bool)),
          this,SLOT(slotLimitHistory(bool)));
  connect(mpPageListBox,SIGNAL(highlighted(int)),
          this,SLOT(slotChangePage(int)));
//Viewer
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,3,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(2,1);
  Q3HBox* undohb = new Q3HBox(opage);
  QLabel* undolabel = new QLabel(tr("Number of undo steps:"),undohb);
  mpUndoSpin = new QSpinBox(undohb);
  mpUndoSpin->setRange(2,25);
  mpUndoSpin->setValue(2);
  undohb->setStretchFactor(undolabel,1);
  sublayout->addWidget(undohb,0,0);

  Q3HBox* previewhb = new Q3HBox(opage);
  QLabel* previewlabel = new QLabel(tr("Maximal filter preview size:"),previewhb);
  mpFilterSizeSpin = new QSpinBox(previewhb);
  mpFilterSizeSpin->setRange(150,450);
  mpFilterSizeSpin->setValue(150);
  previewhb->setStretchFactor(previewlabel,1);
  sublayout->addWidget(previewhb,1,0);

  Q3HBox* b1 = new Q3HBox(opage);
  QLabel* previewlabel2 = new QLabel(tr("Display subsystem:"),b1);
  mpDisplaySubsystemCombo = new QComboBox(b1);
  mpDisplaySubsystemCombo->insertItem (tr("Pixmap"));
  mpDisplaySubsystemCombo->insertItem (tr("GraphicsView"));
  mpDisplaySubsystemCombo->insertItem (tr("OpenGL"));
  b1->setStretchFactor(previewlabel2,1);
  sublayout->addWidget(b1,2,0);

  mpPagesStack->addWidget(opage,6);
  mpPageListBox->insertItem(tr("Viewer"));

//Preview
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,4,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(3,1);
  mpSmoothPreviewCheckBox = new QCheckBox(tr("&Use smooth scaling"),opage);
  sublayout->addWidget(mpSmoothPreviewCheckBox,0,0);
  mpPreviewUpdateCheckBox = new QCheckBox(tr("&Enable continous update"),opage);
  sublayout->addWidget(mpPreviewUpdateCheckBox,1,0);
  Q3HBox* pvhbox = new Q3HBox(opage);
  mpLimitPreviewCheckBox = new QCheckBox(tr("&Limit preview resolution"),pvhbox);
  mpLimitPreviewSpin = new QSpinBox(25,20000,1,pvhbox);
  pvhbox->setStretchFactor(mpLimitPreviewCheckBox,1);
  sublayout->addWidget(pvhbox,2,0);

  mpPagesStack->addWidget(opage,7);
  mpPageListBox->insertItem(tr("Preview"));
//automatic selection
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,7,2);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(7,1);
  sublayout->setColStretch(0,1);
  QLabel* collabel = new QLabel(tr("Standard deviation factor (color)"),opage);
  mpAutoColorSpin = new QDoubleSpinBox(opage);
  mpAutoColorSpin->setRange(1,80);
  mpAutoColorSpin->setValue(120);
  QLabel* graylabel = new QLabel(tr("Standard deviation factor (grayscale)"),opage);
  mpAutoGraySpin = new QDoubleSpinBox(opage);
  mpAutoGraySpin->setRange(1,80);
  mpAutoGraySpin->setValue(120);
  QLabel* sizelabel = new QLabel(tr("Minimal size"),opage);
  mpAutoSizeSpin = new QDoubleSpinBox(opage);
  mpAutoSizeSpin->setRange(1,100);
  mpAutoSizeSpin->setValue(5);
  Q3GroupBox* autogb = new Q3GroupBox(1,Qt::Horizontal,
                                    tr("Background gray value is"),opage);
  Q3HBox* autohb1 = new Q3HBox(autogb);
  autohb1->setSpacing(5);
  mpAutoSmallerRadio = new QRadioButton(tr("smaller than (black background):"),autohb1);
  mpAutoSmallerSpin = new QSpinBox(0,255,1,autohb1);
  autohb1->setStretchFactor(mpAutoSmallerRadio,1);

  Q3HBox* autohb2 = new Q3HBox(autogb);
  autohb2->setSpacing(5);
  mpAutoGreaterRadio = new QRadioButton(tr("greater than (white background):"),autohb2);
  mpAutoGreaterSpin = new QSpinBox(0,255,1,autohb2);
  autohb2->setStretchFactor(mpAutoGreaterRadio,1);

  mpAutoCheckBox = new QCheckBox(tr("Enable automatic preview selection"),opage);
  mpAutoCheckBox->setChecked(false);

  mpAutoTemplateCheckBox = new QCheckBox(tr("Disable automatic preview selection, "
                                       "when template is selected in preview window"),opage);
  mpAutoTemplateCheckBox->setChecked(true);

  mpAutoButtonGroup = new Q3ButtonGroup(this);
  mpAutoButtonGroup->hide();
  mpAutoButtonGroup->insert(mpAutoSmallerRadio);
  mpAutoButtonGroup->insert(mpAutoGreaterRadio);
  mpAutoSmallerRadio->setChecked(true);

  sublayout->addWidget(collabel,0,0);
  sublayout->addWidget(mpAutoColorSpin,0,1);
  sublayout->addWidget(graylabel,1,0);
  sublayout->addWidget(mpAutoGraySpin,1,1);
  sublayout->addWidget(sizelabel,2,0);
  sublayout->addWidget(mpAutoSizeSpin,2,1);
  sublayout->addMultiCellWidget(autogb,3,3,0,1);
  sublayout->addMultiCellWidget(mpAutoCheckBox,4,4,0,1);
  sublayout->addMultiCellWidget(mpAutoTemplateCheckBox,5,5,0,1);

  mpPagesStack->addWidget(opage,8);
  mpPageListBox->insertItem(tr("Auto-selection"));

//start dialog
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,2,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(1,1);
  Q3GroupBox* gb = new Q3GroupBox(1,Qt::Horizontal,
                                 tr("On next program start"),opage);

  mpAllDevicesRadio = new QRadioButton(tr("List &all devices"),gb);
  mpLocalDevicesRadio = new QRadioButton(tr("List &local devices only"),gb);
  mpLastDeviceRadio = new QRadioButton(tr("List &selected device only"),gb);
  mpSameDeviceRadio = new QRadioButton(tr("&Use selected device, do not show dialog"),gb);

  sublayout->addWidget(gb,0,0);
  mpDeviceButtonGroup = new Q3ButtonGroup(0);
  mpDeviceButtonGroup->insert(mpAllDevicesRadio);
  mpDeviceButtonGroup->insert(mpLocalDevicesRadio);
  mpDeviceButtonGroup->insert(mpLastDeviceRadio);
  mpDeviceButtonGroup->insert(mpSameDeviceRadio);
  mpAllDevicesRadio->setChecked(true);
  mpPagesStack->addWidget(opage,9);
  mpPageListBox->insertItem(tr("Start dialog"));

//filename generation
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,4,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(3,1);
  Q3HBox* fchb = new Q3HBox(opage);
  QLabel* fclabel = new QLabel(tr("Filecounter increment"),fchb);
  mpFileCounterStepSpinBox = new QSpinBox(1,10,1,fchb);
  fchb->setStretchFactor(fclabel,1);
  Q3HBox* pzhb = new Q3HBox(opage);
  pzhb->setSpacing(4);
  mpFilePrependZerosCheckBox = new QCheckBox(tr("&Prepend zeros"),pzhb);
  new QLabel(tr("Counter width (digits)"),pzhb);
  mpFileCounterWidthSpinBox = new QSpinBox(2,10,1,pzhb);
  mpFileCounterWidthSpinBox->setEnabled(false);
  pzhb->setStretchFactor(mpFilePrependZerosCheckBox,1);
  mpFileFillGapCheckBox = new QCheckBox(tr("&Fill gaps"),opage);
  sublayout->addWidget(fchb,0,0);
  sublayout->addWidget(pzhb,1,0);
  sublayout->addWidget(mpFileFillGapCheckBox,2,0);
  connect(mpFilePrependZerosCheckBox,SIGNAL(toggled(bool)),
          mpFileCounterWidthSpinBox,SLOT(setEnabled(bool)));
  mpPagesStack->addWidget(opage,10);
  mpPageListBox->insertItem(tr("Filename generation"));

//Miscelleanous
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,8,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(7,1);
  QLabel* doclabel = new QLabel(tr("Documentation path:"),opage);
  Q3HBox* dochb = new Q3HBox(opage);
  mpEditDocPath = new QLineEdit(dochb);
  mpEditDocPath->setText(xmlConfig->stringValue("HELP_INDEX"));
  mpButtonDocPath = new QToolButton(dochb);
  mpButtonDocPath->setPixmap(*pixmap);
  connect(mpButtonDocPath,SIGNAL(clicked()),this,
          SLOT(slotChangeDocPath()));
  sublayout->addWidget(doclabel,0,0);
  sublayout->addWidget(dochb,1,0);

  QLabel* templabel = new QLabel(tr("Temporary directory:"),opage);
  Q3HBox* temphb = new Q3HBox(opage);
  mpEditTempPath = new QLineEdit(temphb);
  mpEditTempPath->setText(xmlConfig->stringValue("TEMP_PATH","/tmp"));
  mpButtonTempPath = new QToolButton(temphb);
  mpButtonTempPath->setPixmap(*pixmap);
  connect(mpButtonTempPath,SIGNAL(clicked()),this,
          SLOT(slotChangeTempPath()));
  sublayout->addWidget(templabel,2,0);
  sublayout->addWidget(temphb,3,0);
  //enable button
  mpCheckBoxWarnings = new QCheckBox(tr("&Enable optional warnings"),opage);
  mpCheckBoxWarnings->setChecked(true);
  sublayout->addWidget(mpCheckBoxWarnings,4,0);
  //size warning
  Q3HBox* warnhb = new Q3HBox(opage);
  mpWarningLabel = new QLabel(tr("Size warning (in MB)"),warnhb);
  mpWarningSpinBox = new QSpinBox(1,512,1,warnhb);
  warnhb->setStretchFactor(mpWarningLabel,1);
  sublayout->addWidget(warnhb,5,0);
  //filename extensions only in imagebrowser
  mpExtensionOnlyCheckBox = new QCheckBox(tr("Create image-browser contents "
                                          "based on filename extensions"),opage);
  sublayout->addWidget(mpExtensionOnlyCheckBox,6,0);

  mpPagesStack->addWidget(opage,11);
  mpPageListBox->insertItem(tr("Miscellaneous"));

#ifndef QIS_NO_STYLES
//Style
  opage = new QWidget(mpPagesStack);
  sublayout = new Q3GridLayout(opage,7,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(6,1);
  mpRadioWindowsStyle = new QRadioButton(tr("&Windows"),opage);
  mpRadioMotifStyle = new QRadioButton(tr("&Motif"),opage);
  mpRadioMotifPlusStyle = new QRadioButton(tr("Mo&tif plus"),opage);
  mpRadioPlatinumStyle = new QRadioButton(tr("&Platinum"),opage);
  mpRadioSgiStyle = new QRadioButton(tr("&SGI"),opage);
  mpRadioCdeStyle = new QRadioButton(tr("C&DE"),opage);
  sublayout->addWidget(mpRadioWindowsStyle,0,0);
  sublayout->addWidget(mpRadioMotifStyle,1,0);
  sublayout->addWidget(mpRadioMotifPlusStyle,2,0);
  sublayout->addWidget(mpRadioPlatinumStyle,3,0);
  sublayout->addWidget(mpRadioSgiStyle,4,0);
  sublayout->addWidget(mpRadioCdeStyle,5,0);
  mpBGroupStyle = new Q3ButtonGroup(0);
  mpBGroupStyle->insert(mpRadioWindowsStyle);
  mpBGroupStyle->insert(mpRadioMotifStyle);
  mpBGroupStyle->insert(mpRadioMotifPlusStyle);
  mpBGroupStyle->insert(mpRadioPlatinumStyle);
  mpBGroupStyle->insert(mpRadioSgiStyle);
  mpBGroupStyle->insert(mpRadioCdeStyle);
  mpRadioWindowsStyle->setChecked(true);
  mpPagesStack->addWidget(opage,12);
  mpPageListBox->insertItem(tr("Style"));
#endif

  connect(mpPageListBox,SIGNAL(highlighted(int)),
          this,SLOT(slotChangePage(int)));

	mpMainLayout->setRowStretch(3,1);
  mpMainLayout->setColStretch(0,1);
  mpMainLayout->setColStretch(1,1);
  mpMainLayout->activate();
  mpPageListBox->setCurrentItem(0);
  mpPageListBox->setMinimumWidth(mpPageListBox->maxItemWidth()+4);
  mpPagesStack->raiseWidget(0);
  createWhatsThisHelp();
}
/**  */
bool QExtensionWidget::nonBlockingIO()
{
  return mpCheckIoMode->isChecked();
}

/**  */
void QExtensionWidget::loadSettings()
{
  mpBGroupMetricSystem->setButton(xmlConfig->intValue("METRIC_SYSTEM"));
  mMetricSystem = (QIN::MetricSystem) xmlConfig->intValue("METRIC_SYSTEM");
#ifndef QIS_NO_STYLES
  mpBGroupStyle->setButton(xmlConfig->intValue("STYLE"));
#endif
  mpDeviceButtonGroup->setButton(xmlConfig->intValue("DEVICE_QUERY",0));
  mpBGroupLayout->setButton(xmlConfig->intValue("LAYOUT"));
  mpCheckSeparatePreview->setChecked(xmlConfig->boolValue("SEPARATE_PREVIEW"));
  mpSmoothPreviewCheckBox->setChecked(xmlConfig->boolValue("PREVIEW_SMOOTH_SCALING",false));
  mpPreviewUpdateCheckBox->setChecked(xmlConfig->boolValue("PREVIEW_CONTINOUS_UPDATE",true));
  mpLimitPreviewSpin->setValue(xmlConfig->intValue("PREVIEW_SIZE_LIMIT",50));
  mpLimitPreviewCheckBox->setChecked(xmlConfig->boolValue("PREVIEW_DO_LIMIT_SIZE",false));
  mpTranslationsCheckBox->setChecked(xmlConfig->boolValue("ENABLE_BACKEND_TRANSLATIONS",true));
  mpWhatsThisCheckBox->setChecked(xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON",true));
  mpCheckIoMode->setChecked(xmlConfig->boolValue("IO_MODE"));
  mpCheckBoxWarnings->setChecked(false);
  mpTiff8Combo->setCurrentItem(xmlConfig->intValue("TIFF_8BIT_MODE"));
  mpTiffLineartCombo->setCurrentItem(xmlConfig->intValue("TIFF_LINEART_MODE"));
  mpTiffJpegSlider->setValue(xmlConfig->intValue("TIFF_JPEG_QUALITY"));
  mpJpegSlider->setValue(xmlConfig->intValue("JPEG_QUALITY"));
  mpPngSlider->setValue(xmlConfig->intValue("PNG_COMPRESSION"));
  mpCheckBoxHistoryEntries->setChecked(xmlConfig->boolValue("HISTORY_LIMIT_ENTRIES"));
  mpSpinHistoryNumber->setValue(xmlConfig->intValue("HISTORY_MAX_ENTRIES"));
  mpCheckBoxHistoryDelete->setChecked(xmlConfig->boolValue("HISTORY_DELETE_EXIT"));
  mpCheckBoxHistoryPreviews->setChecked(xmlConfig->boolValue("HISTORY_CREATE_PREVIEWS"));
  mpCheckBoxHistory->setChecked(xmlConfig->boolValue("HISTORY_ENABLED"));
  slotEnableHistory(xmlConfig->boolValue("HISTORY_ENABLED"));
  mpUndoSpin->setValue(xmlConfig->intValue("VIEWER_UNDO_STEPS"));
  mpFilterSizeSpin->setValue(xmlConfig->intValue("FILTER_PREVIEW_SIZE"));
  mpDisplaySubsystemCombo->setCurrentIndex(xmlConfig->intValue("DISPLAY_SUBSYSTEM", 0));
  mpEditTransPath->setText(xmlConfig->stringValue("BACKEND_TRANSLATIONS_PATH",QString::null));
  mpAutoColorSpin->setValue(xmlConfig->intValue("AUTOSELECT_COLOR_FACTOR",10));
  mpAutoGraySpin->setValue(xmlConfig->intValue("AUTOSELECT_GRAY_FACTOR",10));
  mpAutoSizeSpin->setValue(xmlConfig->intValue("AUTOSELECT_SIZE",2));
  mpAutoCheckBox->setChecked(xmlConfig->boolValue("AUTOSELECT_ENABLE",false));
  mpAutoTemplateCheckBox->setChecked(xmlConfig->boolValue("AUTOSELECT_TEMPLATE_DISABLE",true));
  mpAutoButtonGroup->setButton(xmlConfig->intValue("AUTOSELECT_BG_TYPE",0));
  mpAutoSmallerSpin->setValue(xmlConfig->intValue("AUTOSELECT_MAXIMAL_GRAY_VALUE",100));
  mpAutoGreaterSpin->setValue(xmlConfig->intValue("AUTOSELECT_MINIMAL_GRAY_VALUE",155));
  mpWarningSpinBox->setValue(xmlConfig->intValue("SCAN_SIZE_WARNING",2));
  mpFileCounterStepSpinBox->setValue(xmlConfig->intValue("FILE_GENERATION_STEP",1));
  mpFilePrependZerosCheckBox->setChecked(xmlConfig->boolValue("FILE_GENERATION_PREPEND_ZEROS",false));
  mpFileCounterWidthSpinBox->setValue(xmlConfig->intValue("FILE_GENERATION_COUNTER_WIDTH",3));
  mpFileFillGapCheckBox->setChecked(xmlConfig->boolValue("FILE_GENERATION_FILL_GAPS",false));
  mpExtensionOnlyCheckBox->setChecked(xmlConfig->boolValue("IMAGEBROWSER_EXTENSION_ONLY",false));
}
/**  */
void QExtensionWidget::slotChangeDocPath()
{
#if 0 //s
  QFileDialogExt fd(QDir::homeDirPath(),QString::null,
                    this,"",true);
  fd.setViewMode((Q3FileDialog::ViewMode)xmlConfig->intValue("SINGLEFILE_VIEW_MODE"));
  fd.setCaption(tr("Select the documentation path"));
  if(fd.exec())
  {
    mpEditDocPath->setText(fd.selectedFile());
  }
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",fd.intViewMode());
#endif
}
/**  */
void QExtensionWidget::slotChangeTempPath()
{
#if 0 //s
  QString old_temp = xmlConfig->stringValue("TEMP_PATH","/tmp");
  QFileDialogExt fd(old_temp,QString::null,this,"",true);
  fd.setMode(Q3FileDialog::DirectoryOnly);
  fd.setCaption(tr("Select a directory"));
  fd.setViewMode((Q3FileDialog::ViewMode)xmlConfig->intValue("SINGLEFILE_VIEW_MODE"));
  if(fd.exec())
  {
    if(checkTempPath(fd.selectedFile()))
      mpEditTempPath->setText(fd.selectedFile());
    else
      mpEditTempPath->setText(old_temp);
  }
  xmlConfig->setIntValue("SINGLEFILE_VIEW_MODE",fd.intViewMode());
#endif
}
/**  */
void QExtensionWidget::slotChangeTransPath()
{
  QString qs;
  qs = Q3FileDialog::getExistingDirectory(xmlConfig->stringValue("BACKEND_TRANSLATIONS_PATH"),
                                        0,0);
  if(!qs.isEmpty())
	{
    mpEditTransPath->setText(qs);
    xmlConfig->setStringValue("BACKEND_TRANSLATIONS_PATH",qs);
  }
}
/**  */
void QExtensionWidget::createWhatsThisHelp()
{
//io mode
  Q3WhatsThis::add(mpCheckIoMode,tr("Activate this check box to enable "
															"non-blocking IO. If you encounter "
															"problems with non-blocking IO, you can "
                              "try to disable this. Be warned that this "
                              "will result in a freeze of the GUI"
                              "while you are scanning."));
//translations
  Q3WhatsThis::add(mpTranslationsCheckBox,tr("Activate this check box to enable "
															"the backend translations. Changing this option will "
                              "have no effect until you restart QuiteInsane."));
//translation path
  Q3WhatsThis::add(mpTransPathCheckBox,tr("If you activate this check box, you can "
															"specify a path to the backend translations. Otherwise "
                              "QuiteInsane will use the default path. Changing this "
                              "option will have no effect until you restart QuiteInsane."));
  Q3WhatsThis::add(mpEditTransPath,tr("Use this lineedit to specify the path to "
															"the backend translations. Changing the path "
                              "will have no effect until you restart QuiteInsane."));
  Q3WhatsThis::add(mpButtonTransPath,tr("Click this button to open a file dialog "
															"which allows you to set the path for the backend "
                              "translations."));
//metric system mm
  Q3WhatsThis::add(mpRadioMM,tr("Display all linear dimensions with unit "
														 "millimetre (mm)."));
//metric system inch
  Q3WhatsThis::add(mpRadioInch,tr("Display all linear dimensions with unit "
														 "inch."));
//metric system cm
  Q3WhatsThis::add(mpRadioCM,tr("Display all linear dimensions with unit "
														 "centimetre (cm)."));
//gocr command
  Q3WhatsThis::add(mpEditOcr,tr("Here you can specify the gocr command."
														 "Leave this field empty, if gocr isn't "
                             "installed on your system."));
//doc path
  Q3WhatsThis::add(mpEditDocPath,tr("Here you can specify the path to "
														 "QuiteInsanes documentation. Clicking "
                             "the home button in the help viewer will "
                             "load this site."));
//temp path
  Q3WhatsThis::add(mpEditTempPath,tr("The directory where QuiteInsane will "
														 "store temporary files."));
//layout scrollview
  Q3WhatsThis::add(mpRadioScrollLayout,tr("If you activate this radio button, "
														   "QuiteInsane will display all options."
                               "in a single scroll view."));
//layout scrollview
  Q3WhatsThis::add(mpRadioTabLayout,tr("If you activate this radio button, "
														   "QuiteInsane will display every option "
                               "group in a seperate page of a tab widget."));
//layout scrollview
  Q3WhatsThis::add(mpRadioMultiWindowLayout,tr("If you activate this radio button, "
														   "QuiteInsane will display the first option "
                               "group in the main window. Further option "
                               "groups are placed in additional windows."));
//layout list
  Q3WhatsThis::add(mpRadioListLayout,tr("If you activate this radio button, "
                               "QuiteInsane will display a listview, "
                               "which shows the available option groups."));
//separate preview
  Q3WhatsThis::add(mpCheckSeparatePreview,tr("If you activate this ckeckbox, "
														   "a separate window is used for the preview."
                               "Otherwise, the preview is integrated in "
                               "the main mindow."));
//button warnings
  Q3WhatsThis::add(mpCheckBoxWarnings,tr("<html>Reactivates all message boxes<br> "
										   "that have been disabled with the<br>"
                       "<b>Don't show this message again</b> checkbox.</html>"));
//history
//enable history
  Q3WhatsThis::add(mpCheckBoxHistory,
                  tr("Select this checkbox to enable the history."));
//delete history
  Q3WhatsThis::add(mpCheckBoxHistoryDelete,
                  tr("If you select this checkbox, the history "
                     "will be deleted when you quit QuiteInsane."));
//entrie history
  Q3WhatsThis::add(mpCheckBoxHistoryEntries,
                  tr("If you select this checkbox, you can limit the "
                     "number of history entries."));
//entries history
  Q3WhatsThis::add(mpSpinHistoryNumber,
                  tr("Use this spinbox to select the maximum number of "
                     "history entries."));
//preview history
  Q3WhatsThis::add(mpCheckBoxHistoryPreviews,
                  tr("Select this checkbox to enable automatic preview "
                     "image creation."));
//viewer undo steps
  Q3WhatsThis::add(mpUndoSpin,
                  tr("Use this spinbox to select the maximum number of "
                     "undo steps. Please note, that every undo step can "
                     "use a huge amount of memory, depending on the "
                     "image size."));
//viewer preview size
  Q3WhatsThis::add(mpFilterSizeSpin,
                  tr("Use this spinbox to adjust the maximum size of "
                     "the preview in the imagefilter dialogs."));
//smooth scaling size
  Q3WhatsThis::add(mpSmoothPreviewCheckBox,
                  tr("Select this checkbox to enable smooth scaling "
                     "for the preview window. This results in better preview images, "
                     "but also slows down resizing of the preview window."));
//whatsthis buttons
  Q3WhatsThis::add(mpWhatsThisCheckBox,
                  tr("Select this checkbox to enable the built-in context help "
                     "buttons (\"What's this\" buttons). Disabling this option "
                     "only makes sense, if the window-manager supplies its own "
                     "context help buttons in the window titlebar."));
//auto-selection color
  Q3WhatsThis::add(mpAutoColorSpin,tr("Use this spinbox to adjust the automatic selection "
                               "of color images. Increase the value, if the detected "
                               "scan-area is bigger then the image size. Decrease the value, "
                               "if the detected scan-area is smaller then the image size."));
//auto-selection gray
  Q3WhatsThis::add(mpAutoGraySpin,tr("Use this spinbox to adjust the automatic selection "
                               "of grayscale images. Increase the value, if the detected "
                               "scan-area is bigger then the image size. Decrease the value, "
                               "if the detected scan-area is smaller then the image size."));
//auto-selection enable
  Q3WhatsThis::add(mpAutoCheckBox,tr("If you select this checkbox, the automatic scan-area "
                               "selection is started after a preview scan. You can also start "
                               "the scan-area selection manually by clicking the \"Automatic "
                               "selection\" button in the preview window."));
//auto-selection
  Q3WhatsThis::add(mpAutoGreaterRadio,tr("Select this radio button, if the scanner cover is white."));
//auto-selection
  Q3WhatsThis::add(mpAutoSmallerRadio,tr("Select this radio button, if the scanner cover is "
                               "black/dark."));
//auto-selection
  Q3WhatsThis::add(mpAutoGreaterSpin,tr("Only lines/rows with an average gray-value greater than "
                               "the selected value are interpreted as background."));
//auto-selection
  Q3WhatsThis::add(mpAutoSmallerSpin,tr("Only lines/rows with an average gray-value smaller than "
                               "the selected value are interpreted as background."));
//auto-selection minimal size
  Q3WhatsThis::add(mpAutoSizeSpin,tr("Use this spinbox to select the minimal size of preview images. "
                               "The value is multiplied with the maximal image width/height. E.g. a "
                               "value of 0.01 means, that the minimal preview size equals one "
                               "percent of the scan-area. This value is used to ensure, that e.g "
                               "small spots (like dust) aren't interpreted as images."));
//size warning
  Q3WhatsThis::add(mpWarningSpinBox,tr("When the expected scan size exceeds the adjusted size, "
                               "then the image information in the upper left corner of the "
                               "scan-dialog is displayed with a red background."));
//preview update
  Q3WhatsThis::add(mpPreviewUpdateCheckBox,tr("When you activate this checkbox, QuiteInsane "
                               "will update the preview window continously during a "
                               "preview scan."));
//limit resolution
  Q3WhatsThis::add(mpLimitPreviewCheckBox,tr("Activate this checkbox, if you want to "
                               "limit the preview resolution which is used during a "
                               "preview scan."));
//limit resolution spin
  Q3WhatsThis::add(mpLimitPreviewSpin,tr("Use this spinbox to adjust the maximal "
                               "preview resolution which is used during a preview scan."));
//file counter increment
  Q3WhatsThis::add(mpFileCounterStepSpinBox,tr("Use this spinbox to specify the value "
                               "by which the filecounter gets incremented."));
//file counter width
  Q3WhatsThis::add(mpFileCounterWidthSpinBox,tr("Use this spinbox to set the width "
                               "of the filecounter."));
//prepend zeros
  Q3WhatsThis::add(mpFilePrependZerosCheckBox,tr("When selected, zeros are prepended "
                               "to the filecounter."));
//prepend zeros
  Q3WhatsThis::add(mpFileFillGapCheckBox,tr("When selected, gaps between existing files "
                               "are filled."));
}
/**  */
void QExtensionWidget::slotChangePage(int index)
{
  mpPagesStack->raiseWidget(index);
  mpTitleLabel->setText("<b>"+mpPageListBox->currentText()+"</b>");
}
/**  */
void QExtensionWidget::accept()
{
  QIN::MetricSystem ms = QIN::Millimetre;
  QIN::Layout nl = QIN::ScrollLayout;
  QString qs;

  xmlConfig->setBoolValue("USE_BACKEND_TRANSLATIONS_PATH",mpTransPathCheckBox->isChecked());
  xmlConfig->setStringValue("BACKEND_TRANSLATIONS_PATH",mpEditTransPath->text());
  if(mpCheckBoxWarnings->isChecked() == true)
  {
    xmlConfig->setBoolValue("WARNING_ADF",true);
    xmlConfig->setBoolValue("WARNING_MULTI",true);
    xmlConfig->setBoolValue("WARNING_PRINTERSETUP",true);
  }
  xmlConfig->setStringValue("GOCR_COMMAND",mpEditOcr->text());
  xmlConfig->setStringValue("HELP_INDEX",mpEditDocPath->text());
  if(checkTempPath(mpEditTempPath->text()))
    xmlConfig->setStringValue("TEMP_PATH",mpEditTempPath->text());
  else
    xmlConfig->setStringValue("TEMP_PATH","/tmp/");
  xmlConfig->setIntValue("VIEWER_UNDO_STEPS",mpUndoSpin->value());
  xmlConfig->setIntValue("FILTER_PREVIEW_SIZE",mpFilterSizeSpin->value());
  xmlConfig->setIntValue("DISPLAY_SUBSYSTEM",mpDisplaySubsystemCombo->currentIndex());

  xmlConfig->setBoolValue("PREVIEW_SMOOTH_SCALING",mpSmoothPreviewCheckBox->isChecked());
  xmlConfig->setBoolValue("PREVIEW_CONTINOUS_UPDATE",mpPreviewUpdateCheckBox->isChecked());
  xmlConfig->setIntValue("PREVIEW_SIZE_LIMIT",mpLimitPreviewSpin->value());
  xmlConfig->setBoolValue("PREVIEW_DO_LIMIT_SIZE",mpLimitPreviewCheckBox->isChecked());
  xmlConfig->setBoolValue("ENABLE_BACKEND_TRANSLATIONS",mpTranslationsCheckBox->isChecked());
  xmlConfig->setBoolValue("ENABLE_WHATSTHIS_BUTTON",mpWhatsThisCheckBox->isChecked());
  if(mpRadioMM->isChecked())
    ms = QIN::Millimetre;
  else if(mpRadioCM->isChecked())
    ms = QIN::Centimetre;
  else if(mpRadioInch->isChecked())
    ms = QIN::Inch;
  xmlConfig->setIntValue("METRIC_SYSTEM",ms);

  if(mpRadioScrollLayout->isChecked())
    nl = QIN::ScrollLayout;
  else if(mpRadioTabLayout->isChecked())
    nl = QIN::TabLayout;
  else if(mpRadioMultiWindowLayout->isChecked())
    nl = QIN::MultiWindowLayout;
  else if(mpRadioListLayout->isChecked())
    nl = QIN::ListLayout;
  xmlConfig->setIntValue("LAYOUT",nl);
  xmlConfig->setBoolValue("SEPARATE_PREVIEW",
                      mpCheckSeparatePreview->isChecked());

  xmlConfig->setIntValue("AUTOSELECT_COLOR_FACTOR",mpAutoColorSpin->value());
  xmlConfig->setIntValue("AUTOSELECT_GRAY_FACTOR",mpAutoGraySpin->value());
  xmlConfig->setIntValue("AUTOSELECT_SIZE",mpAutoSizeSpin->value());
  xmlConfig->setBoolValue("AUTOSELECT_ENABLE",mpAutoCheckBox->isChecked());
  xmlConfig->setBoolValue("AUTOSELECT_TEMPLATE_DISABLE",mpAutoTemplateCheckBox->isChecked());
  if(mpAutoSmallerRadio->isChecked())
    xmlConfig->setIntValue("AUTOSELECT_BG_TYPE",0);
  else
    xmlConfig->setIntValue("AUTOSELECT_BG_TYPE",1);
  xmlConfig->setIntValue("AUTOSELECT_MAXIMAL_GRAY_VALUE",mpAutoSmallerSpin->value());
  xmlConfig->setIntValue("AUTOSELECT_MINIMAL_GRAY_VALUE",mpAutoGreaterSpin->value());
#ifndef QIS_NO_STYLES
  int st;
  st = 0;
  if(mpRadioWindowsStyle->isChecked())
    st = 0;
  else if(mpRadioMotifStyle->isChecked())
    st = 1;
  else if(mpRadioMotifPlusStyle->isChecked())
    st = 2;
  else if(mpRadioPlatinumStyle->isChecked())
    st = 3;
  else if(mpRadioSgiStyle->isChecked())
    st = 4;
  else if(mpRadioCdeStyle->isChecked())
    st = 5;
  xmlConfig->setIntValue("STYLE",st);
#endif
  int db = 0;
  if(mpAllDevicesRadio->isChecked())
    db = 0;
  else if(mpLocalDevicesRadio->isChecked())
    db = 1;
  else if(mpLastDeviceRadio->isChecked())
    db = 2;
  else if(mpSameDeviceRadio->isChecked())
    db = 3;
  xmlConfig->setIntValue("DEVICE_QUERY",db);

  xmlConfig->setBoolValue("IO_MODE",mpCheckIoMode->isChecked());
  xmlConfig->setIntValue("TIFF_8BIT_MODE",mpTiff8Combo->currentItem());
  xmlConfig->setIntValue("TIFF_LINEART_MODE",mpTiffLineartCombo->currentItem());
  xmlConfig->setIntValue("TIFF_JPEG_QUALITY",mpTiffJpegSlider->value());
  xmlConfig->setIntValue("JPEG_QUALITY",mpJpegSlider->value());
  xmlConfig->setIntValue("PNG_COMPRESSION",mpPngSlider->value());

  xmlConfig->setBoolValue("HISTORY_LIMIT_ENTRIES",mpCheckBoxHistoryEntries->isChecked());
  xmlConfig->setIntValue("HISTORY_MAX_ENTRIES",mpSpinHistoryNumber->value());
  xmlConfig->setBoolValue("HISTORY_DELETE_EXIT",mpCheckBoxHistoryDelete->isChecked());
  xmlConfig->setBoolValue("HISTORY_CREATE_PREVIEWS",mpCheckBoxHistoryPreviews->isChecked());
  xmlConfig->setBoolValue("HISTORY_ENABLED",mpCheckBoxHistory->isChecked());
  xmlConfig->setIntValue("SCAN_SIZE_WARNING",mpWarningSpinBox->value());

  if((mpFileCounterStepSpinBox->value() !=
            xmlConfig->intValue("FILE_GENERATION_STEP",1)) ||
     (mpFilePrependZerosCheckBox->isChecked() !=
            xmlConfig->boolValue("FILE_GENERATION_PREPEND_ZEROS",false)) ||
     (mpFileFillGapCheckBox->isChecked() !=
            xmlConfig->boolValue("FILE_GENERATION_FILL_GAPS",false)) ||
     (mpFileCounterWidthSpinBox->value() !=
            xmlConfig->intValue("FILE_GENERATION_COUNTER_WIDTH",2)) )
  {
    mFilenameGenerationChanged = true;
  }
  xmlConfig->setIntValue("FILE_GENERATION_STEP",mpFileCounterStepSpinBox->value());
  xmlConfig->setBoolValue("FILE_GENERATION_PREPEND_ZEROS",mpFilePrependZerosCheckBox->isChecked());
  xmlConfig->setIntValue("FILE_GENERATION_COUNTER_WIDTH",mpFileCounterWidthSpinBox->value());
  xmlConfig->setBoolValue("FILE_GENERATION_FILL_GAPS",mpFileFillGapCheckBox->isChecked());
  xmlConfig->setBoolValue("IMAGEBROWSER_EXTENSION_ONLY",mpExtensionOnlyCheckBox->isChecked());
  QDialog::accept();
}
/**  */
void QExtensionWidget::slotTiffJpegQuality(int value)
{
  QString qs;
  mpTiffJpegLabel->setText(qs.setNum(value));
}
/**  */
void QExtensionWidget::slotJpegQuality(int value)
{
  QString qs;
  mpJpegLabel->setText(qs.setNum(value));
}
/**  */
void QExtensionWidget::slotPngCompression(int value)
{
  QString qs;
  mpPngLabel->setText(qs.setNum(value));
}
/**  */
void QExtensionWidget::setPage(int index)
{
  mpPageListBox->setSelected(index,true);
  slotChangePage(index);
  if(index>-1)
  {
    mpPageListBox->hide();
    mpMainLayout->setColStretch(0,0);
    mpMainLayout->setColStretch(1,1);
    mpMainLayout->activate();
    resize(minimumSizeHint().width(),height());
  }
  else
  {
    mpPageListBox->show();
    mpMainLayout->setColStretch(0,1);
    mpMainLayout->setColStretch(1,1);
  }
}
/**  */
void QExtensionWidget::slotEnableHistory(bool enable)
{
  mpCheckBoxHistoryEntries->setEnabled(enable);
  if(mpCheckBoxHistoryEntries->isChecked() && enable)
    mpSpinHistoryNumber->setEnabled(enable);
  else
    mpSpinHistoryNumber->setEnabled(false);
  mpCheckBoxHistoryDelete->setEnabled(enable);
  mpCheckBoxHistoryPreviews->setEnabled(enable);

}
/**  */
void QExtensionWidget::slotLimitHistory(bool b)
{
  mpSpinHistoryNumber->setEnabled(b);
}
/** No descriptions */
bool QExtensionWidget::checkTempPath(QString path)
{
  QString qs = path;
  QFileInfo fi(qs);
  if(fi.isDir())
  {
    if(fi.isReadable() && fi.isWritable() && fi.isExecutable())
      return true;
  }
  return false;
}
/** No descriptions */
bool QExtensionWidget::filenameGenerationChanged()
{
  return mFilenameGenerationChanged;
}
