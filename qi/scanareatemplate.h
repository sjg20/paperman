/***************************************************************************
                          scanareatemplate.h  -  description
                             -------------------
    begin                : Sat Nov 30 2002
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

#ifndef SCANAREATEMPLATE_H
#define SCANAREATEMPLATE_H

#include <QVector>
#include "scanarea.h"

#include <qstring.h>

/**
  *@author Michael Herder
  */

class ScanAreaTemplate
{
public: 
  ScanAreaTemplate(QString name = QString::null);
  ~ScanAreaTemplate();
  /** No descriptions */
  QVector <ScanArea*> rects();  //s
  /** No descriptions */
  QString name();
  /** No descriptions */
  void setName(QString name);
  /** No descriptions */
  void addRects(QVector <ScanArea*> rects);  //s
private:
  QVector <ScanArea*> mRects;
  QString mName;
};

#endif
