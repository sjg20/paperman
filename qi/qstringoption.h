/***************************************************************************
                          qstringoption.h  -  description
                             -------------------
    begin                : Fri Sep 15 2000
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

#ifndef QSTRINGOPTION_H
#define QSTRINGOPTION_H

#include "qsaneoption.h"
/**
  *@author M. Herder
  */
class QString;
class QLineEdit;
class QPushButton;

class QStringOption : public QSaneOption
{
   Q_OBJECT
public: 
	QStringOption(QString title,QWidget *parent, const char *name=0);
	~QStringOption();
  /**  */
  QString text();
  /**  */
  void setText(const char* text);
  /** No descriptions */
  void setMaxLength(int length);
private: // Private methods
  /**  */
  void initWidget();
private: // Private attributes
  /**  */
  QLineEdit* mpOptionLineEdit;
  /**  */
  QString mCurrentText;
private slots: // Private slots
  /**  */
  void slotTextChanged(const QString& qs);
};

#endif
