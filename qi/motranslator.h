/***************************************************************************
                          motranslator.h  -  description
                             -------------------
    begin                : Fri Apr 25 2003
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

#ifndef MOTRANSLATOR_H
#define MOTRANSLATOR_H

#include <qstring.h>
#include <qtranslator.h>

/**
  *@author Michael Herder
  */

class MoTranslator : public QTranslator
{
public: 
  MoTranslator(QObject* parent,const char* name=0);
  ~MoTranslator();
  /** No descriptions */
  bool loadMoFile(QString filename,const char* context);
};

#endif
