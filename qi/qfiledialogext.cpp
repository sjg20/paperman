/***************************************************************************
                          qfiledialogext.cpp  -  description
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

#include "resource.h"
#include "qfiledialogext.h"

QFileDialogExt::QFileDialogExt(QWidget* parent,const char* name,bool modal)
               :QFileDialog(parent,name,modal)
{
}
QFileDialogExt::QFileDialogExt(const QString& dirName,const QString& filter,QWidget* parent,const char* name,bool modal)
               :QFileDialog (dirName,filter,parent,name,modal)
{
}
QFileDialogExt::~QFileDialogExt()
{
}
void QFileDialogExt::reject()
{
  mViewMode = int(viewMode());
  //What the hell is this? Not enough, that viewMode() only works, as
  //long as the dialog is visible, it also returns the wrong values
  //(0 when it should return 1 and vice versa)
  //At least, this stupid behaviour is consistent in all versions
  //of Qt2.2.x. If this is fixed in Qt3, this class can  be removed
  //completely. But since we still support Qt2, we better keep it.
#ifndef USE_QT3
  if(mViewMode == 0)
    mViewMode = 1;
  else if(mViewMode == 1)
    mViewMode = 0;
#endif
  QDialog::reject();
}

void QFileDialogExt::accept()
{
  mViewMode = int(viewMode());
  //see reject() for stupid hack description
#ifndef USE_QT3
  if(mViewMode == 0)
    mViewMode = 1;
  else if(mViewMode == 1)
    mViewMode = 0;
#endif
  QDialog::accept();
}
/** Return the view mode as an int value. This is needed, since
    the implementation of viewMode() in QFileDialog only works,
    as long as the dialog is visible.
*/
int QFileDialogExt::intViewMode()
{
  return mViewMode;
}
