/***************************************************************************
                          qextensionwidget.h  -  description
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

#ifndef QEXTENSIONWIDGET_H
#define QEXTENSIONWIDGET_H


#include <qdialog.h>
#include <qstring.h>
#include "quiteinsanenamespace.h"
//Added by qt3to4:
#include <Q3GridLayout>
#include <QLabel>
/**
  *@author M. Herder
  */

class Q3ButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class Q3GridLayout;
class QLabel;
class QLineEdit;
class Q3ListBox;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QToolButton;
class Q3WidgetStack;

class QExtensionWidget : public QDialog
{
   Q_OBJECT
public:
  QExtensionWidget(QWidget* parent=0,const char* name=0,bool modal = true,
                   Qt::WFlags f=0);
	~QExtensionWidget();
  /**  */
  bool nonBlockingIO();
  /**  */
  void loadSettings();
  /**  */
  void setPage(int index);
  /** No descriptions */
  bool filenameGenerationChanged();
private: // Private attributes
  /**  */
  QDoubleSpinBox* mpAutoColorSpin;
  /**  */
  QDoubleSpinBox* mpAutoGraySpin;
  /**  */
  QDoubleSpinBox* mpAutoSizeSpin;
  /**  */
  QCheckBox* mpAutoCheckBox;
  /**  */
  QCheckBox* mpAutoTemplateCheckBox;
  /**  */
  QRadioButton* mpAutoSmallerRadio;
  /**  */
  QSpinBox* mpAutoSmallerSpin;
  /**  */
  QRadioButton* mpAutoGreaterRadio;
  /**  */
  QSpinBox* mpAutoGreaterSpin;
  /**  */
  Q3ButtonGroup* mpAutoButtonGroup;
  /**  */
  Q3GridLayout* mpMainLayout;
  /**  */
  QCheckBox* mpWhatsThisCheckBox;
  /**  */
  QCheckBox* mpCheckIoMode;
  /**  */
  QCheckBox* mpCheckSeparatePreview;
  /**  */
  QCheckBox* mpSmoothPreviewCheckBox;
  /**  */
  QCheckBox* mpLimitPreviewCheckBox;
  /**  */
  QSpinBox* mpLimitPreviewSpin;
  /**  */
  QLabel* mpWarningLabel;
  /**  */
  QSpinBox* mpWarningSpinBox;
  /**  */
  QCheckBox* mpTranslationsCheckBox;
  /**  */
  QCheckBox* mpPreviewUpdateCheckBox;
  /**  */
  QRadioButton* mpRadioMM;
  /**  */
  QIN::MetricSystem mMetricSystem;
  /**  */
  QRadioButton* mpRadioInch;
  /**  */
  QRadioButton* mpRadioCM;
  /**  */
  QComboBox* mpTiff8Combo;
  /**  */
  QComboBox* mpTiffLineartCombo;
  /**  */
  QSlider* mpJpegSlider;
  /**  */
  QSlider* mpTiffJpegSlider;
  /**  */
  QSlider* mpPngSlider;
  /**  */
  QLabel* mpJpegLabel;
  /**  */
  QLabel* mpTiffJpegLabel;
  /**  */
  QLabel* mpPngLabel;
  /**  */
  QRadioButton* mpRadioScrollLayout;
  /**  */
  QRadioButton* mpRadioTabLayout;
  /**  */
  QRadioButton* mpRadioMultiWindowLayout;
  /**  */
  QRadioButton* mpRadioListLayout;
  /**  */
  QRadioButton* mpRadioWindowsStyle;
  /**  */
  QRadioButton* mpRadioMotifStyle;
  /**  */
  QRadioButton* mpRadioMotifPlusStyle;
  /**  */
  QRadioButton* mpRadioPlatinumStyle;
  /**  */
  QRadioButton* mpRadioSgiStyle;
  /**  */
  QRadioButton* mpRadioCdeStyle;
  /**  */
  QRadioButton* mpAllDevicesRadio;
  /**  */
  QRadioButton* mpLocalDevicesRadio;
  /**  */
  QRadioButton* mpLastDeviceRadio;
  /**  */
  QRadioButton* mpSameDeviceRadio;
  /**  */
  Q3ButtonGroup* mpDeviceButtonGroup;
  /**  */
  QSpinBox* mpFileCounterStepSpinBox;
  /**  */
  QSpinBox* mpFileCounterWidthSpinBox;
  /**  */
  QCheckBox* mpFilePrependZerosCheckBox;
  /**  */
  QCheckBox* mpFileFillGapCheckBox;
  /**  */
  QToolButton* mpButtonDocPath;
  /**  */
  QToolButton* mpButtonTempPath;
  /**  */
  QToolButton* mpButtonTransPath;
  /**  */
  QCheckBox* mpCheckBoxWarnings;
  /**  */
  QToolButton* mpWhatsThisButton;
  /**  */
  QLineEdit* mpEditTransPath;
  /**  */
  QLineEdit* mpEditDocPath;
  /**  */
  QLineEdit* mpEditTempPath;
  /**  */
  QLineEdit* mpEditOcr;
  /**  */
  Q3ButtonGroup* mpBGroupStyle;
  /**  */
  Q3ButtonGroup* mpBGroupDrag;
  /**  */
  Q3ButtonGroup* mpBGroupMetricSystem;
  /**  */
  Q3ButtonGroup* mpBGroupLayout;
  /**  */
  QRadioButton* mpTextRadio;
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  Q3ListBox* mpPageListBox;
  /**  */
  QRadioButton* mpImageRadio;
  /**  */
  Q3WidgetStack* mpPagesStack;
  /**  */
  QCheckBox* mpCheckBoxHistory;
  /**  */
  QCheckBox* mpTransPathCheckBox;
  /**  */
  QCheckBox* mpCheckBoxHistoryEntries;
  /**  */
  QCheckBox* mpCheckBoxHistoryDelete;
  /**  */
  QCheckBox* mpCheckBoxHistoryPreviews;
  /** */
  QSpinBox* mpSpinHistoryNumber;
  /** */
  QSpinBox* mpFilterSizeSpin;
  QComboBox* mpDisplaySubsystemCombo;
  /** */
  QSpinBox* mpUndoSpin;
  /** */
  bool mFilenameGenerationChanged;
  /**  */
  QCheckBox* mpExtensionOnlyCheckBox;
private: //methods
  /**  */
  void createWhatsThisHelp();
  /**  */
  void initWidget();
  /** No descriptions */
  bool checkTempPath(QString path);
private slots: // Private slots
  /**  */
  void slotChangeDocPath();
  /**  */
  void slotChangeTempPath();
  /**  */
  void slotChangeTransPath();
  /**  */
  void slotChangePage(int index);
  /**  */
  void slotTiffJpegQuality(int value);
  /**  */
  void slotJpegQuality(int value);
  /**  */
  void slotPngCompression(int value);
  /**  */
  void slotEnableHistory(bool enable);
  /**  */
  void slotLimitHistory(bool b);
protected slots:
  void accept();
};
#endif
