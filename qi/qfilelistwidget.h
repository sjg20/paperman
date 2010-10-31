/***************************************************************************
                          qfilelistwidget.h  -  description
                             -------------------
    begin                : Sat Aug 26 2000
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

#ifndef QFILELISTWIDGET_H
#define QFILELISTWIDGET_H

#include <qgroupbox.h>
#include <qpoint.h>
/**
  *@author M. Herder
  */
//forward declarations
class QDragListBox;
class QListViewItem;
class QLabel;
class QLineEdit;
class QPushButton;
class QComboBox;
class QRadioButton;
class QSlider;
class QToolButton;

class QFileListWidget : public QGroupBox
{
   Q_OBJECT
public:
	QFileListWidget(bool tm=false,QWidget *parent=0, const char *name=0);
	~QFileListWidget();
  /**  */
  void createContents();
  /**  */
  QString getFilename();
  /**  */
  void addFilename(QString fn);
  /**  */
  QString format();
private: // Private attributes
  /** */
  int mFileCnt;
  /**  */
  QDragListBox* mpListBox;
  /**  */
  QString mFileNameTemplate;
  /**  */
  QString mFolderName;
  /**  */
  QString mFileType;
  /**  */
  QLineEdit* mpListDirLineEdit;
  /**  */
  QToolButton* mpButtonListDir;
  /**  */
  QToolButton* mpFileGenerationButton;
  /**  */
  QPushButton* mpButtonDelete;
  /**  */
  QPushButton* mpButtonTemplate;
  /**  */
  QPushButton* mpButtonClose;
  /**  */
  QComboBox* mpComboFileType;
  /**  */
  QLineEdit* mpTemplateLineEdit;
  /**  */
  bool mTextMode;
  /**  */
  int mLastItem;
private: //methods
  /**  */
  void initWidget();
  /**  */
  void createWhatsThisHelp();
  /**  */
  void loadSettings();
private slots: // Private slots
  /**  */
  void slotClearList();
  /**  */
  void slotDisplayImage(QListViewItem* lvi);
  /**  */
  void slotChangeFolder();
  /**  */
  void slotDeleteFile();
  /**  */
  void slotTypeChanged(int);
  /**  */
  void slotNewFileTemplate();
  /**  */
  void slotDisplayText(QListViewItem* lvi);
  /**  */
  void slotSelectionChanged();
  /**  */
  void slotStartDrag(QListViewItem*);
  /**  */
  void slotImageSettings();
  /**  */
  void slotFilenameGenerationSettings();
};

#endif
