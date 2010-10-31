/***************************************************************************
                          qlistviewitemext.h  -  description
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

#ifndef QLISTVIEWITEMEXT_H
#define QLISTVIEWITEMEXT_H

#include <qdatetime.h>
#include <q3listview.h>
#include <qstring.h>
/**
  *@author Michael Herder
  */
class QListViewItemExt : public Q3ListViewItem
{
public: 
  //constructors
  QListViewItemExt(Q3ListView* parent);
  /** */
  QListViewItemExt(Q3ListViewItem* parent);
  /** */
  QListViewItemExt(Q3ListView* parent,Q3ListViewItem* after);
  /** */
  QListViewItemExt(Q3ListViewItem* parent,Q3ListViewItem* after);
  /** */
  QListViewItemExt(Q3ListView* parent,QString,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null);
  /** */
  QListViewItemExt(Q3ListViewItem* parent,QString,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null);
  /** */
  QListViewItemExt(Q3ListView* parent,Q3ListViewItem* after,QString,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString = QString::null);
  /** */
  QListViewItemExt(Q3ListViewItem* parent,Q3ListViewItem* after,QString,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null,QString=QString::null,
                   QString=QString::null);
  //destructor
	virtual ~QListViewItemExt();
  /**  */
  void setHiddenText(QString text);
  /**  */
  QString hiddenText();
  /**  */
  void setIndex(int index);
  /**  */
  int index();
  /**  */
  void setHiddenDateTime(QDateTime dt);
  /**  */
  QDateTime hiddenDateTime();
  /**  */
  QString key(int column,bool ascending) const;
private: // Private attributes
  /**  */
  int mIndex;
  /**  */
  QString mHiddenText;
  /**  */
  QDateTime mDateTime;
};

#endif
