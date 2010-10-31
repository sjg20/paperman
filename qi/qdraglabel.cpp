/***************************************************************************
                          qdraglabel.cpp  -  description
                             -------------------
    begin                : Thu Feb 22 2001
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

#include "qdraglabel.h"
#include "qxmlconfig.h"

#include <qcstring.h>
#include <qdragobject.h>
#include <qevent.h>
#include <qstrlist.h>


QDragLabel::QDragLabel(QWidget* parent,const char* name,WFlags f)
           :QLabel(parent,name,f)
{
  mFileNames.clear();
}
QDragLabel::~QDragLabel()
{
}
/**  */
void QDragLabel::mouseMoveEvent(QMouseEvent* me)
{
  QUriDrag* d;

  if(me->state() == Qt::LeftButton)
  {
    d = new QUriDrag(this);
    d->setFilenames(mFileNames);
    d->dragCopy();
  }
}
/**Clear the list of filenames and add fn as the only entry.*/
void QDragLabel::setFilename(QString fn)
{
  mFileNames.clear();
  mFileNames.append(fn);//QUriDrag::localFileToUri(fn));
}
/**Add fn to the list of filenames.*/
void QDragLabel::addFilename(QString fn)
{
  mFileNames.append(fn);
}
/** Clear the list of filenames */
void QDragLabel::clearFilenameList()
{
  mFileNames.clear();
}
