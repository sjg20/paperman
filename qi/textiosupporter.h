/***************************************************************************
                          textiosupporter.h  -  description
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

#ifndef TEXTIOSUPPORTER_H
#define TEXTIOSUPPORTER_H

#include <qstring.h>

/**
  *@author Michael Herder
  */

class TextIOSupporter
{
public:
  TextIOSupporter();
  ~TextIOSupporter();
  /** No descriptions */
  QString validateExtension(QString file_path,QString format=QString::null);
  /** No descriptions */
  QString lastErrorString();
private:
  QString mErrorString;
};

#endif
