/***************************************************************************
                          qreadonlyoption.h  -  description
                             -------------------
    begin                : Sat Feb 17 2001
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

#ifndef QREADONLYOPTION_H
#define QREADONLYOPTION_H

#include "qsaneoption.h"
//Added by qt3to4:
#include <QLabel>
/**
  *@author M. Herder
  */
class QLabel;
class QString;

class QReadOnlyOption : public QSaneOption
{
Q_OBJECT
public:
	QReadOnlyOption(QString title,QWidget *parent, const char *name=0);
	~QReadOnlyOption();
  /**  */
  void setText(QString text);
private:
  /**  */
  QLabel* mpValueLabel;
  /**  */
  void initWidget();
  /**  */
  QLabel* mpTitleLabel;
  /**  */
  QString mTitleText;
};

#endif
