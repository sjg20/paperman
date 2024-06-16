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

#include <QTableWidgetItem>

#include <qdatetime.h>
#include <qstring.h>
/**
  *@author Michael Herder
  */
class QListViewItemExt : public QTableWidgetItem
{
public: 
  //constructors
  QListViewItemExt(QTableWidget* parent);
  /** */
  QListViewItemExt(QTableWidgetItem* parent);
  /** */
  QListViewItemExt(QTableWidget* parent,QTableWidgetItem* after);
  /** */
  QListViewItemExt(QTableWidgetItem* parent,QTableWidgetItem* after);
  /** */
  QListViewItemExt(QTableWidget* parent,QString,QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString());
  /** */
  QListViewItemExt(QTableWidgetItem* parent,QString,QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString());
  /** */
  QListViewItemExt(QTableWidget* parent,QTableWidgetItem* after,QString,
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString(),
                   QString = QString());
  /** */
  QListViewItemExt(QTableWidgetItem* parent,QTableWidgetItem* after,QString,
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString(),QString=QString(),
                   QString=QString());

private:
  void addWidgets(QTableWidget *parent, QString label1,
                               QString label2,QString label3,
                               QString label4,QString label5,
                               QString label6,QString label7,
                               QString label8);
public:
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
  int mRow;
  QTableWidget *mParent;
};

#endif
