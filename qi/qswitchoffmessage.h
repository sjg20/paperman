/***************************************************************************
                          qswitchoffmessage.h  -  description
                             -------------------
    begin                : Mon Mar 12 2001
    copyright            : (C) 2001 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QSWITCHOFFMESSAGE_H
#define QSWITCHOFFMESSAGE_H

#include <qdialog.h>

/**
  *@author Michael Herder
  */
class QCheckBox;

class QSwitchOffMessage : public QDialog
{
Q_OBJECT
public:
  enum Type
  {
    Type_AdfWarning,
    Type_MultiWarning,
    Type_PrinterSetupWarning
  };
	QSwitchOffMessage(Type t,QWidget* parent=0,const char* name=0);
	~QSwitchOffMessage();
  /**  */
  bool dontShow();
private:
  Type mType;
  QCheckBox* mpDontShowCheckBox;
private: //methods
  void initDlg();
private slots: // Private slots
  /**  */
  void slotButton2();
  /**  */
  void slotDontShow(bool);
};

#endif
