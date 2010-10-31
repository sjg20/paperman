/***************************************************************************
                          qlistviewitemext.cpp  -  description
                             -------------------
    begin                : Sun Jul 29 2001
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

#include "qlistviewitemext.h"

/** */
QListViewItemExt::QListViewItemExt(Q3ListView* parent)
                 :Q3ListViewItem(parent)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::QListViewItemExt(Q3ListViewItem* parent)
                 :Q3ListViewItem(parent)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
QListViewItemExt::QListViewItemExt(Q3ListView* parent,Q3ListViewItem* after)
              :Q3ListViewItem(parent,after)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::QListViewItemExt(Q3ListViewItem* parent,Q3ListViewItem* after)
              :Q3ListViewItem(parent,after)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::QListViewItemExt(Q3ListView * parent,QString label1,
                             QString label2,QString label3,
                             QString label4,QString label5,
                             QString label6,QString label7,
                             QString label8)
                 :Q3ListViewItem(parent,label1,label2,label3,label4,
			                          label5,label6,label7,label8)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::QListViewItemExt(Q3ListViewItem* parent,QString label1,
                                   QString label2,QString label3,
                                   QString label4,QString label5,
                                   QString label6,QString label7,
                                   QString label8)
                  :Q3ListViewItem(parent,label1,label2,label3,label4,
			                           label5,label6,label7,label8)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::QListViewItemExt(Q3ListView* parent,Q3ListViewItem* after,
                                   QString label1,QString label2,
                                   QString label3,QString label4,
                                   QString label5,QString label6,
                                   QString label7,QString label8)
                 :Q3ListViewItem(parent,after,label1,label2,label3,
                                label4,label5,label6,label7,label8)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::QListViewItemExt(Q3ListViewItem* parent,
                                   Q3ListViewItem* after,QString label1,
                                   QString label2,QString label3,
                                   QString label4,QString label5,
                                   QString label6,QString label7,
                                   QString label8)
                 :Q3ListViewItem(parent,after,label1,label2,label3,
                     			      label4,label5,label6,label7,label8)
{
  mIndex = -1;
  mHiddenText = QString::null;
}
/** */
QListViewItemExt::~QListViewItemExt()
{
}
/**  */
int QListViewItemExt::index()
{
  return mIndex;
}
/**  */
void QListViewItemExt::setIndex(int index)
{
  mIndex = index;
}
/**  */
QString QListViewItemExt::hiddenText()
{
  return mHiddenText;
}
/**  */
void QListViewItemExt::setHiddenText(QString text)
{
  mHiddenText = text;
}
/**  */
QString QListViewItemExt::key(int column,bool ascending) const
{
  ascending = ascending; //s
  QDateTime epoch(QDate(1900,1,1));
  QString qs;
  int i;
  bool ok;

  qs = text(column);
  switch(column)
  {
    //sort by name
    case 0:
    case 1:
      qs = text(1);
      break;
    //sort by byte size
    case 2:
      i = text(2).toInt(&ok);
      if(!ok) i = 0;
      qs.sprintf("%010i",i);
      break;
    case 3:
      i = epoch.secsTo(mDateTime);
      qs.sprintf("%010i",i);
      break;
    default:;
  }
  return qs;
}
/**  */
QDateTime QListViewItemExt::hiddenDateTime()
{
  return mDateTime;
}
/**  */
void QListViewItemExt::setHiddenDateTime(QDateTime dt)
{
  mDateTime = dt;
}
