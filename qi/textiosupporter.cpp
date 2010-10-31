/***************************************************************************
                          textiosupporter.cpp  -  description
                             -------------------
    begin                : Fri Mar 1 2002
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

#include "textiosupporter.h"

#include <qdir.h>
#include <qfile.h>
#include <qimage.h>
#include <qobject.h>

//will be extended in a following release
//at the moment, it's only used to generate filenames for text output

TextIOSupporter::TextIOSupporter()
{
  mErrorString = QString::null;
}
TextIOSupporter::~TextIOSupporter()
{
}
/** No descriptions */
QString TextIOSupporter::validateExtension(QString file_path,QString format)
{
  QString new_filepath;
  QString ext;
  QFileInfo fi(file_path);
qDebug("TextIOSupporter: input file_path %s",file_path.latin1());
  if(!(fi.extension(false).isEmpty()))
  {
    ext = "." + fi.extension(false);
    if(format == "TXT")
    {
      if(".txt" != ext)
        ext = QString::null;
    }
  }
qDebug("TextIOSupporter: ext %s",ext.latin1());

  if(".txt" == ext) //valid filename extension ?
  {
    new_filepath = file_path;//valid
  }
  else
  {
    if(format.isEmpty())
    if(format.isEmpty())
    {
      mErrorString = QObject::tr("No valid filename extension.") + "\n";
      return QString::null;//no extension, no format -> can't create filename
    }
    if(format == "TXT")
    {
      ext = ".txt";
      new_filepath = file_path + ext;
    }
    else
    {
      mErrorString = QObject::tr("No valid filename extension.") + "\n";
      return QString::null;//no extension, no format -> can't create filename
    }
  }
qDebug("TextIOSupporter:return new_filepath %s",new_filepath.latin1());
  return new_filepath;
}
/** No descriptions */
QString TextIOSupporter::lastErrorString()
{
  return mErrorString;
}
