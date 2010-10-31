/***************************************************************************
                          fileiosupporter.h  -  description
                             -------------------
    begin                : Thu Nov 21 2002
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

#ifndef FILEIOSUPPORTER_H
#define FILEIOSUPPORTER_H

#include <qstring.h>
#include <qstringlist.h>

/**
  *@author Michael Herder
  */

class FileIOSupporter
{
public: 
  FileIOSupporter();
  ~FileIOSupporter();
  /** No descriptions */
  QString getIncreasingFilename(QString filepath,int step=1,int width=0,bool fill_gap=false);
  /** No descriptions */
  QStringList getAbsPathList(QString dir_path,QString filetemplate,int width,QString ext);
  /** No descriptions */
  bool isValidFilename(QString filepath);
  /** No descriptions */
  QString lastErrorString();
private:
  QString mErrorString;
};

#endif
