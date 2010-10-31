/***************************************************************************
                          qbuttonoption.h  -  description
                             -------------------
    begin                : Thu Sep 7 2000
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

#ifndef QBUTTONOPTION_H
#define QBUTTONOPTION_H

#include "qsaneoption.h"
//Added by qt3to4:
#include <QLabel>
/**
  *@author M. Herder
  */
class QPushButton;
class QLabel;
class QString;

class QButtonOption : public QSaneOption
{
   Q_OBJECT
public:
	QButtonOption(QString title,QWidget *parent, const char *name=0);
	~QButtonOption();
private:
  /**  */
  QPushButton* mpOptionButton;
  /**  */
  void initWidget();
  /**  */
  QLabel* mpTitleLabel;
};

#endif
