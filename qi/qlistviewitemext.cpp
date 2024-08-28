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

#include<QTableWidgetItem>

#include "qlistviewitemext.h"
#include "err.h"

/** */
QListViewItemExt::QListViewItemExt(QTableWidget* parent)
                 :QTableWidgetItem()
{
    mParent = parent;
  mRow = parent->rowCount();

  parent->insertRow(mRow);
  parent->setItem(mRow, 0, this);
  mIndex = -1;
  mHiddenText = QString();
}
/** */
QListViewItemExt::QListViewItemExt(QTableWidgetItem* item)
                 :QTableWidgetItem()
{
  QTableWidget* parent = item->tableWidget();
  mParent = parent;
  mRow = parent->rowCount();

  parent->insertRow(mRow);
  parent->setItem(mRow, 0, this);
  mIndex = -1;
  mHiddenText = QString();
}
QListViewItemExt::QListViewItemExt(QTableWidget* parent,QTableWidgetItem* after)
              :QTableWidgetItem()
{
    mParent = parent;
    mRow = parent->row(after);

  parent->insertRow(mRow);
  parent->setItem(mRow, 0, this);
  mIndex = -1;
  mHiddenText = QString();
}
/** */
QListViewItemExt::QListViewItemExt(QTableWidgetItem* item,QTableWidgetItem* after)
              :QTableWidgetItem()
{
  QTableWidget* parent = item->tableWidget();
  mParent = parent;
  mRow = parent->row(after);

  parent->insertRow(mRow);
  parent->setItem(mRow, 0, this);
  mIndex = -1;
  mHiddenText = QString();
}

void QListViewItemExt::addWidgets(QTableWidget *parent, QString label1,
                             QString label2,QString label3,
                             QString label4,QString label5,
                             QString label6,QString label7,
                             QString label8)
{
   QTableWidgetItem *item;

   item = new QTableWidgetItem(label1);
   parent->setItem(mRow, 0, item);
   item = new QTableWidgetItem(label2);
   parent->setItem(mRow, 1, item);
   item = new QTableWidgetItem(label3);
   parent->setItem(mRow, 2, item);
   item = new QTableWidgetItem(label4);
   parent->setItem(mRow, 3, item);
   item = new QTableWidgetItem(label5);
   parent->setItem(mRow, 4, item);
   item = new QTableWidgetItem(label6);
   parent->setItem(mRow, 5, item);
   item = new QTableWidgetItem(label7);
   parent->setItem(mRow, 6, item);
   item = new QTableWidgetItem(label8);
   parent->setItem(mRow, 7, item);
}

/** */
QListViewItemExt::QListViewItemExt(QTableWidget * parent,QString label1,
                             QString label2,QString label3,
                             QString label4,QString label5,
                             QString label6,QString label7,
                             QString label8)
{
    mParent = parent;
  mIndex = -1;
  mHiddenText = QString();
  mRow = parent->rowCount();

  parent->insertRow(mRow);
  parent->setColumnCount(8);
  addWidgets(parent, label1, label2, label3, label4, label5, label6,
             label7, label8);
}
/** */
QListViewItemExt::QListViewItemExt(QTableWidgetItem* item,QString label1,
                                   QString label2,QString label3,
                                   QString label4,QString label5,
                                   QString label6,QString label7,
                                   QString label8)
{
    QTableWidget* parent = item->tableWidget();
    mParent = parent;
    mIndex = -1;
    mHiddenText = QString();
    mRow = parent->rowCount();

    parent->insertRow(mRow);
    parent->setColumnCount(8);
    addWidgets(parent, label1, label2, label3, label4, label5, label6,
               label7, label8);
  mIndex = -1;
  mHiddenText = QString();
}
/** */
QListViewItemExt::QListViewItemExt(QTableWidget* parent,QTableWidgetItem* after,
                                   QString label1,QString label2,
                                   QString label3,QString label4,
                                   QString label5,QString label6,
                                   QString label7,QString label8)
{
    mParent = parent;
    mIndex = -1;
    mHiddenText = QString();
    mRow = parent->row(after);

    parent->insertRow(mRow);
    parent->setColumnCount(8);
    addWidgets(parent, label1, label2, label3, label4, label5, label6,
               label7, label8);
  mIndex = -1;
  mHiddenText = QString();
}
/** */
QListViewItemExt::QListViewItemExt(QTableWidgetItem* item,
                                   QTableWidgetItem* after,QString label1,
                                   QString label2,QString label3,
                                   QString label4,QString label5,
                                   QString label6,QString label7,
                                   QString label8)
{
    QTableWidget* parent = item->tableWidget();
    mParent = parent;
    mIndex = -1;
    mHiddenText = QString();
    mRow = parent->row(after);

    parent->insertRow(mRow);
    parent->setColumnCount(8);
    addWidgets(parent, label1, label2, label3, label4, label5, label6,
               label7, label8);
  mIndex = -1;
  mHiddenText = QString();
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
  UNUSED(ascending);
  QDateTime epoch;
  QString qs;
  int i;
  bool ok;

  epoch.setDate(QDate(1900, 1, 1));
  qs = mParent->item(mRow, column)->text();
  switch(column)
  {
    //sort by name
    case 0:
    case 1:
      qs = mParent->item(mRow, 2)->text();
      break;
    //sort by byte size
    case 2:
      i = mParent->item(mRow, 2)->text().toInt(&ok);
      if(!ok) i = 0;
      qs.asprintf("%010i",i);
      break;
    case 3:
      i = epoch.secsTo(mDateTime);
      qs.asprintf("%010i",i);
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
