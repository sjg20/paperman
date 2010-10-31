/***************************************************************************
                          qbooloption.h  -  description
                             -------------------
    begin                : Thu Sep 14 2000
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

#ifndef QBOOLOPTION_H
#define QBOOLOPTION_H

#include "qsaneoption.h"
extern "C"
{
#include <sane/sane.h>
}

/**
  *@author M. Herder
  */
class QString;
class QCheckBox;

class QBoolOption : public QSaneOption
{
   Q_OBJECT
public:
	QBoolOption(QString title,QWidget *parent, const char *name=0);
	~QBoolOption();
  /**  */
  SANE_Bool state();
  /**  */
  void setState(SANE_Bool sb);
  /**  */
  void enableAutomatic(bool b);
  /**  */
  bool automatic();
private:
  /**  */
  void initWidget();
  /**  */
  QCheckBox* mpOptionCheckBox;
  /**  */
  QCheckBox* mpAutoCheckBox;
  /**  */
  bool mAutomatic;
private slots: // Private slots
  /**Turn automatic mode on or off.  */
  void slotAutoMode(bool automode);
signals: // Signals
  /**  */
  void signalAutomatic(int opt,bool automode);
};

#endif
