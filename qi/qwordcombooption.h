/***************************************************************************
                          qwordcombooption.h  -  description
                             -------------------
    begin                : Tue Nov 19 2000
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

#ifndef QWORDCOMBOOPTION_H
#define QWORDCOMBOOPTION_H

#include "qsaneoption.h"
//Added by qt3to4:
#include <QLabel>
//s #include <qarray.h>
extern "C"
{
#include <sane/sane.h>
}
/**
  *@author M. Herder
  */
//forward declarations
class QCheckBox;
class QComboBox;
class QLabel;
class QString;

class QWordComboOption : public QSaneOption
{
Q_OBJECT
public:
	QWordComboOption(QString title,QWidget * parent,
                   SANE_Value_Type type,const char * name=0);
	~QWordComboOption();
  /**  */
  void appendArray(QVector <SANE_Word> qa);
  /**  */
  SANE_Word getCurrentValue();
  SANE_Value_Type getSaneType();
  /**  */
  void setValue(SANE_Word val);
  /**  */
  void enableAutomatic(bool b);
  /**  */
  bool automatic();
private: // Private attributes
  /** */
  QVector <SANE_Word> mValueArray;
  /**  */
  QComboBox* mpSelectionCombo;
  /**  */
  QCheckBox* mpAutoCheckBox;
  /**  */
  QLabel * mpTitleLabel;
  /** */
  SANE_Value_Type mSaneValueType;
  /**  */
  bool mAutomatic;
private: //methods
  /**  */
  void initWidget();
private slots: // Private slots
  /**Turn automatic mode on or off.  */
  void slotAutoMode(bool automode);
  void slotValueChanged(int);
signals: // Signals
  /**  */
  void signalAutomatic(int opt,bool automode);
};

#endif
