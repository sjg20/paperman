/***************************************************************************
                          scanareatemplate.cpp  -  description
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

#include "scanareatemplate.h"

ScanAreaTemplate::ScanAreaTemplate(QString name)
{
  mName = name;
  mRects.resize(0);
}
ScanAreaTemplate::~ScanAreaTemplate()
{
}
/** No descriptions */
void ScanAreaTemplate::addRects(QVector <ScanArea*> rects)
{
  mRects = rects;
}
/** No descriptions */
void ScanAreaTemplate::setName(QString name)
{
  mName = name;
}
/** No descriptions */
QString ScanAreaTemplate::name()
{
  return mName;
}
/** No descriptions */
QVector <ScanArea*> ScanAreaTemplate::rects()
{
  return mRects;
}
