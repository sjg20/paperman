#include <QLabel>
/***************************************************************************
                          qsanestatusmessage.h  -  description
                             -------------------
    begin                : Thu Jun 7 2001
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

#ifndef QSANESTATUSMESSAGE_H
#define QSANESTATUSMESSAGE_H

extern "C"
{
#include <sane/sane.h>
}
#include <qmessagebox.h>

/**
  *@author Michael Herder
  */


class QSaneStatusMessage : public QMessageBox
{
Q_OBJECT
public:
	QSaneStatusMessage(SANE_Status status, QWidget* parent);
	~QSaneStatusMessage();
private:
  SANE_Status mSaneStatus;
  QLabel* mMessageLabel;
  void setMessage();
};

#endif
