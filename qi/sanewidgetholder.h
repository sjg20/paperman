/***************************************************************************
                          sanewidgetholder.h  -  description
                             -------------------
    begin                : Wed Feb 26 2003
    copyright            : (C) 2003 by Michael Herder
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

#ifndef SANEWIDGETHOLDER_H
#define SANEWIDGETHOLDER_H

#include <qwidget.h>

/**
  *@author Michael Herder
  */

class QGridLayout;
class QSaneOption;

class SaneWidgetHolder : public QWidget
{
public:
  SaneWidgetHolder(QWidget* parent=0,const char* name=0,Qt::WindowFlags f=Qt::WindowFlags());
  ~SaneWidgetHolder();
  /** No descriptions */
  void addWidget(QSaneOption* widget);
  /** No descriptions */
  void replaceWidget(QSaneOption* widget);
  /** No descriptions */
  QSaneOption* saneOption();

  /** check if a change is pending. These are saved up until sane is no longer busy

     \returns true if a change is pending */
  bool getPending (void);

  /** set whether a change is pending

     \param pending     true if pending */
  void setPending (bool pending);

private:
  QSaneOption* mpSaneOption;
  QGridLayout* mpGridLayout;
  bool mPending;  //!< pending change has been made while sane is busy
};

#endif
