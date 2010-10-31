/***************************************************************************
                          directorylistview.h  -  description
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

#ifndef DIRECTORYLISTVIEW_H
#define DIRECTORYLISTVIEW_H

#include <qlistview.h>
#include <qdir.h>
#include <qstring.h>

/**
  *@author Michael Herder
  */

class DirectoryListView : public QListView
{
Q_OBJECT
public:
	DirectoryListView(QWidget* parent=0,const char* name=0);
	~DirectoryListView();
  /** No descriptions */
  QString directory();
  /** No descriptions */
  void setDirectory(QString path);
private: // Private methods
  /** No descriptions */
  void createContents();
private:
  QString mCurrentDirPath;
  QDir mCurrentDir;
private slots: // Private slots
  /** No descriptions */
  void slotItemClicked(QListViewItem* li);
signals:
  /** No descriptions */
  void signalDirectoryChanged(QString path);
};

#endif
