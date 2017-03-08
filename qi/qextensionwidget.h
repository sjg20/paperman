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
#include <QLabel>
/**
  *@author M. Herder
  */

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QToolButton;
class QStackedWidget;

class QExtensionWidget : public QDialog
{
   Q_OBJECT
public:
  QExtensionWidget(QWidget* parent=0,const char* name=0,bool modal = true,
                   Qt::WindowFlags f=0);
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
  QButtonGroup* mpAutoButtonGroup;
  /**  */
  QGridLayout* mpMainLayout;
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
  QButtonGroup* mpDeviceButtonGroup;
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
  QLineEdit* mpEditTransPath;
  /**  */
  QLineEdit* mpEditDocPath;
  /**  */
  QLineEdit* mpEditTempPath;
  /**  */
  QLineEdit* mpEditOcr;
  /**  */
  QButtonGroup* mpBGroupStyle;
  /**  */
  QButtonGroup* mpBGroupDrag;
  /**  */
  QButtonGroup* mpBGroupMetricSystem;
  /**  */
  QButtonGroup* mpBGroupLayout;
  /**  */
  QRadioButton* mpTextRadio;
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  QListWidget* mpPageListBox;
  /**  */
  QRadioButton* mpImageRadio;
  /**  */
  QStackedWidget* mpPagesStack;
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
