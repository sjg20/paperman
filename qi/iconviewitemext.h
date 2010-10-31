/***************************************************************************
                          iconviewitemext.h  -  description
                             -------------------
    begin                : Wed Feb 6 2002
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

#ifndef ICONVIEWITEMEXT_H
#define ICONVIEWITEMEXT_H

#include <qiconview.h>

/**
  *@author Michael Herder
  */

class IconViewItemExt : public QIconViewItem
{
public: 
  /**  */
  IconViewItemExt(QIconView* parent);
  /**  */
  IconViewItemExt(QIconView* parent,QIconViewItem *after);
  /**  */
  IconViewItemExt(QIconView* parent,const QString &text);
  /**  */
  IconViewItemExt(QIconView* parent,QIconViewItem *after,const QString &text);
  /**  */
  IconViewItemExt(QIconView* parent,const QString &text,const QPixmap &icon);
  /**  */
  IconViewItemExt(QIconView* parent,QIconViewItem *after,const QString &text,const QPixmap &icon);
  /**  */
	~IconViewItemExt();
  /**  */
  QString hiddenText();
  /**  */
  void setHiddenText(QString text);
protected:
  void paintItem(QPainter* p,const QColorGroup& cg);
private:
  /**  */
  QString mHiddenText;
};

#endif
