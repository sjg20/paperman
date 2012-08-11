/***************************************************************************
                          qcombooption.h  -  description
                             -------------------
    begin                : Tue Jul 4 2000
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

#ifndef QCOMBOOPTION_H
#define QCOMBOOPTION_H

#include <qstringlist.h>

#include "qsaneoption.h"
//Added by qt3to4:
#include <QLabel>
/**
  *@author M. Herder
  */
//forward declarations
class QComboBox;
class QCheckBox;
class QLabel;
class QString;

class QComboOption : public QSaneOption
{
Q_OBJECT
public:
	QComboOption(QString title,QWidget* parent, const char * name=0);
	~QComboOption();
  /**  */
  void appendItem(const char* item);
    /**
     * Set the current value of an combo item
     *
     * This works by looking through the list of options and selecting the
     * matching one.
     *
     * @return true if set ok, false if value could not be found
    */
    bool setCurrentValue(const char*item);
  /**  */
  QString getCurrentText();
  /**  */
  bool automatic();
  /**  */
  void enableAutomatic(bool enabled);
  /** No descriptions */
  void setStringList(const QStringList& slist);
private: // Private attributes
  /**  */
  QStringList mStringList;
  /**  */
  QComboBox* mpSelectionCombo;
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  QLabel* mpTipLabel;
  /**  */
  QString mTitleText;
  /**  */
  QString mCurrentText;
  /**  */
  QCheckBox* mpAutoCheckBox;
  /**  */
  bool mAutomatic;
  /**  */
  void initWidget();
private slots: // Private slots
  /** No descriptions */
  void slotChangeTooltip(int index);
  /**  */
  void slotAutoMode(bool automode);
  /**  */
  void slotSelectionChanged(int);
signals: // Signals
  /**  */
  void signalAutomatic(int opt,bool automode);
};

#endif
