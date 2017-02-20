/***************************************************************************
                          qdevicesettings.h  -  description
                             -------------------
    begin                : Mon Mar 05 2001
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

#ifndef QDEVICESETTINGS_H
#define QDEVICESETTINGS_H

#include <qdialog.h>
#include <qmap.h>
#include <qpoint.h>
#include <qstring.h>
#include <QLabel>
/**
  *@author M. Herder
  */
//forward declarations
class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QScanner;
class QToolButton;

class QDeviceSettings : public QDialog
{
   Q_OBJECT
public:
	QDeviceSettings(QScanner* s,QWidget *parent=0,const char *name=0,bool modal=true,Qt::WFlags f=0);
	~QDeviceSettings();
  /**  */
  void createContents();
  /**  */
  QString getIncreasingFilename();
  /**  */
  int quality();
  /**  */
  QString imageType();
  /**  */
  bool saveDeviceSettings(QString uname = QString::null);
private slots: // Private slots
  /**  */
  void slotClearList();
  /**  */
  void slotDelete();
  /**  */
  void slotSelectionChanged();
  /**  */
  void slotLoad();
  /**  */
  void slotSave();
  /**  */
  void slotNew();
  /**  */
  void slotDoubleClicked(QListWidgetItem*);
private: // Private attributes
  /** */
  QScanner* mpScanner;
  /**  */
  QListWidget* mpListWidget;
  /**  */
  QPushButton* mpButtonDelete;
  /**  */
  QPushButton* mpButtonLoad;
  /**  */
  QPushButton* mpButtonNew;
  /**  */
  QPushButton* mpButtonSave;
  /**  */
  QPushButton* mpButtonCancel;
  /** */
  QMap<QString,QString> mOptionMap;
  /**  */
  void initWidget();
  /**  */
  void createWhatsThisHelp();
};

#endif
