/***************************************************************************
                          iconviewitemext.cpp  -  description
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

#include "iconviewitemext.h"

#include <qpainter.h>

IconViewItemExt::IconViewItemExt(QIconView* parent)
                :QIconViewItem(parent)
{
}
IconViewItemExt::IconViewItemExt(QIconView* parent,QIconViewItem* after)
                :QIconViewItem(parent,after)
{
}
IconViewItemExt::IconViewItemExt(QIconView* parent,const QString& text)
                :QIconViewItem(parent,text)
{
}
IconViewItemExt::IconViewItemExt(QIconView* parent,QIconViewItem* after,const QString& text)
                :QIconViewItem(parent,after,text)
{
}
IconViewItemExt::IconViewItemExt(QIconView* parent,const QString& text,const QPixmap& icon)
                :QIconViewItem(parent,text,icon)
{
}
IconViewItemExt::IconViewItemExt(QIconView* parent,QIconViewItem* after,
                                 const QString& text,const QPixmap& icon)
                :QIconViewItem(parent,after,text,icon)
{
}
IconViewItemExt::~IconViewItemExt()
{
}
/**  */
QString IconViewItemExt::hiddenText()
{
  return mHiddenText;
}
/**  */
void IconViewItemExt::setHiddenText(QString text)
{
  mHiddenText = text;
}
/**  */
void IconViewItemExt::paintItem(QPainter* p,const QColorGroup& cg)
{
  if(isSelected())
  {
    p->setBrush(cg.highlight());
    p->drawRect(textRect(false));
    p->setPen(QPen(cg.highlightedText()));
    p->setBackgroundColor(cg.highlight());
    p->setBackgroundMode(Qt::OpaqueMode);
    p->drawText(textRect(false),Qt::AlignHCenter|Qt::WordBreak,text());
    p->setPen(QPen(cg.dark(),4));
    p->drawRect(pixmapRect(false));
    p->drawPixmap(pixmapRect(false).x(),pixmapRect(false).y(),*pixmap());
  }
  else
  {
    QIconViewItem::paintItem ( p,cg);
  }
}
