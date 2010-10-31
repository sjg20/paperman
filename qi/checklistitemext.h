/***************************************************************************
                          checklistitemext.h  -  description
                             -------------------
    begin                : Thu Jan 24 2002
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

#ifndef CHECKLISTITEMEXT_H
#define CHECKLISTITEMEXT_H

#include <qcolor.h>
#include <q3listview.h>
//Added by qt3to4:
#include <QPixmap>

/**
  *@author Michael Herder
  */

class CheckListItemExt : public Q3CheckListItem
{
public:
  CheckListItemExt(Q3ListView* parent,const QString& text,Type tt=Controller);
  CheckListItemExt(Q3ListView* parent,const QString& text,const QPixmap& p);
  CheckListItemExt(Q3ListViewItem* parent,const QString& text,Type tt=Controller);
  CheckListItemExt(Q3ListViewItem* parent,const QString& text,const QPixmap & p);
	~CheckListItemExt();
  /** No descriptions */
  QRgb bgColor();
  /** No descriptions */
  void setBgColor(QRgb bg_color);
  /** No descriptions */
  QRgb fgColor();
  /** No descriptions */
  void setFgColor(QRgb fg_color);
  /** No descriptions */
  void updatePixmap();
  /** No descriptions */
  int number();
  /** No descriptions */
  void setNumber(int i);
private:
  int mNumber;
  QRgb mBgColor;
  QRgb mFgColor;
};

#endif
