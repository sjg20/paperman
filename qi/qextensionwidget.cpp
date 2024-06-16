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
#include "quiteinsanenamespace.h"

#include <QButtonGroup>
#include <QFileDialog>
#include <QListWidget>
#include <QStackedWidget>

#include <qapplication.h>
//s #include <qarray.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qcolor.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qpalette.h>
#include <qpixmap.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qstring.h>
#include <qtoolbutton.h>
#include <qwidget.h>

QExtensionWidget::QExtensionWidget(QWidget* parent,const char* name,
                                   bool modal,Qt::WindowFlags f)
                 :QDialog(parent,f)
{
    setObjectName(name);
    setModal(modal);
  setWindowTitle(tr("MaxView - Options"));
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

//  QPixmap* pixmap = new QPixmap((const char **)fileopen);
//the main layout
  mpMainLayout = new QGridLayout(this);
  mpMainLayout->setMargin(8);
  mpMainLayout->setSpacing(5);
//create mpWhatsThisButton in a HBox
	QWidget* wtwidget = new QWidget(this);
  QHBoxLayout* qhbwt = new QHBoxLayout( wtwidget);
  qhbwt->addStretch(1);
  mpMainLayout->addWidget(wtwidget,0,1);
  if(!xmlConfig->boolValue("ENABLE_WHATSTHIS_BUTTON"))
    wtwidget->hide();
//mpPageListBox
  mpPageListBox = new QListWidget(this);
  mpMainLayout->addWidget(mpPageListBox,1,0,5,1);
//mpTitleLabel
  mpTitleLabel = new QLabel(this);
  mpMainLayout->addWidget(mpTitleLabel,1,1);

//horizontal separator
  QFrame* sep1 = new QFrame(this);
  sep1->setFrameStyle(QFrame::HLine|QFrame::Sunken);
  sep1->setLineWidth(2);
  mpMainLayout->addWidget(sep1,2,1);

//mpPagesStack
  mpPagesStack = new QStackedWidget(this);
  mpMainLayout->addWidget(mpPagesStack,3,1);
#if 0
//buttons
  QHBoxLayout* bhb = new QHBoxLayout(this);
	QPushButton* qpb1 = new QPushButton(tr("&OK"),bhb);
  connect(qpb1,SIGNAL(clicked()),this,SLOT(accept()));
  QWidget* dummy = new QWidget(bhb);
  bhb->setStretchFactor(dummy,1);
	QPushButton* qpb2 = new QPushButton(tr("&Cancel"),bhb);
  connect(qpb2,SIGNAL(clicked()),this,SLOT(reject()));
  mpMainLayout->addMultiCellWidget(bhb,5,0,1,2);
#endif
//metric system
  QWidget* opage = new QWidget(mpPagesStack);
  QGridLayout* sublayout = new QGridLayout(opage);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(3,1);
  mpRadioMM = new QRadioButton(tr("&Millimetre"),opage);
  mpRadioCM = new QRadioButton(tr("C&entimetre"),opage);
  mpRadioInch = new QRadioButton(tr("&Inch"),opage);
  sublayout->addWidget(mpRadioMM,0,0);
  sublayout->addWidget(mpRadioCM,1,0);
  sublayout->addWidget(mpRadioInch,2,0);
  mpBGroupMetricSystem = new QButtonGroup(opage);
  mpBGroupMetricSystem->addButton(mpRadioMM, QIN::Millimetre);
  mpBGroupMetricSystem->addButton(mpRadioCM, QIN::Centimetre);
  mpBGroupMetricSystem->addButton(mpRadioInch, QIN::Inch);
  mpRadioMM->setChecked(true);
  mpPagesStack->addWidget(opage);
  mpPageListBox->addItem(tr("Metric system"));

#if 0
//Layout
  opage = new QWidget(mpPagesStack);
  sublayout = new QGridLayout(opage,7,1);
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
  sublayout = new QGridLayout(opage,5,1);
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

  QHBoxLayout* transhb = new QHBoxLayout(opage);
  new QLabel(tr("Translation path:"),transhb);
  mpEditTransPath = new QLineEdit(transhb);
  mpEditTransPath->setText(xmlConfig->stringValue(QString()));
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
  sublayout = new QGridLayout(opage,2,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(1,1);
  QHBoxLayout* ocrhb = new QHBoxLayout(opage);
  new QLabel(tr("OCR command:"),ocrhb);
  mpEditOcr = new QLineEdit(ocrhb);
  mpEditOcr->setText(xmlConfig->stringValue("GOCR_COMMAND"));
  sublayout->addWidget(ocrhb,0,0);
  mpPagesStack->addWidget(opage,3);
  mpPageListBox->insertItem(tr("OCR"));

//Image compression/quality
  opage = new QWidget(mpPagesStack);
  sublayout = new QGridLayout(opage,6,5);
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
  sublayout = new QGridLayout(opage,5,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(4,1);
  mpCheckBoxHistory = new QCheckBox(tr("&Enable history"),opage);
  QHBoxLayout* histhb = new QHBoxLayout(opage);
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
  sublayout = new QGridLayout(opage,3,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(2,1);
  QHBoxLayout* undohb = new QHBoxLayout(opage);
  QLabel* undolabel = new QLabel(tr("Number of undo steps:"),undohb);
  mpUndoSpin = new QSpinBox(undohb);
  mpUndoSpin->setRange(2,25);
  mpUndoSpin->setValue(2);
  undohb->setStretchFactor(undolabel,1);
  sublayout->addWidget(undohb,0,0);

  QHBoxLayout* previewhb = new QHBoxLayout(opage);
  QLabel* previewlabel = new QLabel(tr("Maximal filter preview size:"),previewhb);
  mpFilterSizeSpin = new QSpinBox(previewhb);
  mpFilterSizeSpin->setRange(150,450);
  mpFilterSizeSpin->setValue(150);
  previewhb->setStretchFactor(previewlabel,1);
  sublayout->addWidget(previewhb,1,0);

  QHBoxLayout* b1 = new QHBoxLayout(opage);
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
  sublayout = new QGridLayout(opage,4,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(3,1);
  mpSmoothPreviewCheckBox = new QCheckBox(tr("&Use smooth scaling"),opage);
  sublayout->addWidget(mpSmoothPreviewCheckBox,0,0);
  mpPreviewUpdateCheckBox = new QCheckBox(tr("&Enable continous update"),opage);
  sublayout->addWidget(mpPreviewUpdateCheckBox,1,0);
  QHBoxLayout* pvhbox = new QHBoxLayout(opage);
  mpLimitPreviewCheckBox = new QCheckBox(tr("&Limit preview resolution"),pvhbox);
  mpLimitPreviewSpin = new QSpinBox(25,20000,1,pvhbox);
  pvhbox->setStretchFactor(mpLimitPreviewCheckBox,1);
  sublayout->addWidget(pvhbox,2,0);

  mpPagesStack->addWidget(opage,7);
  mpPageListBox->insertItem(tr("Preview"));
//automatic selection
  opage = new QWidget(mpPagesStack);
  sublayout = new QGridLayout(opage,7,2);
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
  QGroupBox* autogb = new QGroupBox(1,Qt::Horizontal,
                                    tr("Background gray value is"),opage);
  QHBoxLayout* autohb1 = new QHBoxLayout(autogb);
  autohb1->setSpacing(5);
  mpAutoSmallerRadio = new QRadioButton(tr("smaller than (black background):"),autohb1);
  mpAutoSmallerSpin = new QSpinBox(0,255,1,autohb1);
  autohb1->setStretchFactor(mpAutoSmallerRadio,1);

  QHBoxLayout* autohb2 = new QHBoxLayout(autogb);
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
  sublayout = new QGridLayout(opage,2,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(1,1);
  QGroupBox* gb = new QGroupBox(1,Qt::Horizontal,
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
  sublayout = new QGridLayout(opage,4,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(3,1);
  QHBoxLayout* fchb = new QHBoxLayout(opage);
  QLabel* fclabel = new QLabel(tr("Filecounter increment"),fchb);
  mpFileCounterStepSpinBox = new QSpinBox(1,10,1,fchb);
  fchb->setStretchFactor(fclabel,1);
  QHBoxLayout* pzhb = new QHBoxLayout(opage);
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
  sublayout = new QGridLayout(opage,8,1);
  sublayout->setMargin(15);
  sublayout->setSpacing(subspacing);
  sublayout->setRowStretch(7,1);
  QLabel* doclabel = new QLabel(tr("Documentation path:"),opage);
  QHBoxLayout* dochb = new QHBoxLayout(opage);
  mpEditDocPath = new QLineEdit(dochb);
  mpEditDocPath->setText(xmlConfig->stringValue("HELP_INDEX"));
  mpButtonDocPath = new QToolButton(dochb);
  mpButtonDocPath->setPixmap(*pixmap);
  connect(mpButtonDocPath,SIGNAL(clicked()),this,
          SLOT(slotChangeDocPath()));
  sublayout->addWidget(doclabel,0,0);
  sublayout->addWidget(dochb,1,0);

  QLabel* templabel = new QLabel(tr("Temporary directory:"),opage);
  QHBoxLayout* temphb = new QHBoxLayout(opage);
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
  QHBoxLayout* warnhb = new QHBoxLayout(opage);
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
  sublayout = new QGridLayout(opage,7,1);
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
#endif
}
/**  */
bool QExtensionWidget::nonBlockingIO()
{
  return mpCheckIoMode->isChecked();
}

/**  */
void QExtensionWidget::loadSettings()
{
  QAbstractButton *button = mpBGroupMetricSystem->button(
              xmlConfig->intValue("METRIC_SYSTEM"));
  if (button)
      button->setChecked(true);
  mMetricSystem = (QIN::MetricSystem) xmlConfig->intValue("METRIC_SYSTEM");
#if 0
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
  mpEditTransPath->setText(xmlConfig->stringValue("BACKEND_TRANSLATIONS_PATH",QString()));
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
#endif
}
/**  */
void QExtensionWidget::slotChangeDocPath()
{
#if 0 //s
  QFileDialogExt fd(QDir::homePath(),QString(),
                    this,"",true);
  fd.setViewMode((Q3FileDialog::ViewMode)xmlConfig->intValue("SINGLEFILE_VIEW_MODE"));
  fd.setWindowTitle(tr("Select the documentation path"));
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
  QFileDialogExt fd(old_temp,QString(),this,"",true);
  fd.setMode(Q3FileDialog::DirectoryOnly);
  fd.setWindowTitle(tr("Select a directory"));
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
  qs = QFileDialog::getExistingDirectory(0, "Translation Path",
        xmlConfig->stringValue("BACKEND_TRANSLATIONS_PATH"));
  if(!qs.isEmpty())
	{
    mpEditTransPath->setText(qs);
    xmlConfig->setStringValue("BACKEND_TRANSLATIONS_PATH",qs);
  }
}

/**  */
void QExtensionWidget::slotChangePage(int index)
{
  mpPagesStack->setCurrentIndex(index);
  mpTitleLabel->setText("<b>"+mpPageListBox->currentItem()->text()+"</b>");
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
  xmlConfig->setIntValue("TIFF_8BIT_MODE",mpTiff8Combo->currentIndex());
  xmlConfig->setIntValue("TIFF_LINEART_MODE",mpTiffLineartCombo->currentIndex());
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
  mpPageListBox->setCurrentRow(index);
  slotChangePage(index);
  if(index>-1)
  {
    mpPageListBox->hide();
    mpMainLayout->setColumnStretch(0,0);
    mpMainLayout->setColumnStretch(1,1);
    mpMainLayout->activate();
    resize(minimumSizeHint().width(),height());
  }
  else
  {
    mpPageListBox->show();
    mpMainLayout->setColumnStretch(0,1);
    mpMainLayout->setColumnStretch(1,1);
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
