/***************************************************************************
                          unknownprogressdialog.h  -  description
                             -------------------
    begin                : Sat Mar 16 2002
    copyright            : (C) 2002 by Michael Herder
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

#ifndef UNKNOWNPROGRESSDIALOG_H
#define UNKNOWNPROGRESSDIALOG_H

#include <qdialog.h>
#include <qstring.h>

/**
  *@author Michael Herder
  */

class QLabel;
class QUnknownProgressWidget;

class UnknownProgressDialog : public QDialog
{
public: 
	UnknownProgressDialog(QString label=QString::null,QWidget* parent=0,const char* name=0);
	~UnknownProgressDialog();
private:
  QString mLabelText;
  QLabel* mpLabel;
  QUnknownProgressWidget* mpUnknownProgress;
private: // Private methods
  /** No descriptions */
  void initDialog();
};

#endif
