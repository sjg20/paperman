/***************************************************************************
                          qfiledialogext.h  -  description
                             -------------------
    begin                : Sun Sep 16 2001
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

#ifndef QFILEDIALOGEXT_H
#define QFILEDIALOGEXT_H

#include <qfiledialog.h>
#include <qstring.h>

/**This class is needed to work around some
bugs/limitations in QFileDialog.
  *@author Michael Herder
  */

class QFileDialogExt : public QFileDialog
{
public: 
	QFileDialogExt(QWidget* parent=0,const char* name = 0,bool modal = true);
  QFileDialogExt(const QString& dirName,const QString& filter=QString::null,
                 QWidget* parent=0,const char* name=0,bool modal=true);
	~QFileDialogExt();
  int intViewMode();
private:
  int mViewMode;
protected slots:
  virtual void reject();
  virtual void accept();
};

#endif
