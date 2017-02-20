/***************************************************************************
                          directorylistview.cpp  -  description
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

#include "directorylistview.h"

#include <qdir.h>

DirectoryListView::DirectoryListView(QWidget* parent,const char* name)
                  :QListView(parent,name)
{
  addColumn(tr("Directory"));
  mCurrentDirPath = QDir::homePath();
  mCurrentDir.setPath(mCurrentDirPath);
  createContents();
  connect(this,SIGNAL(clicked(QListViewItem*)),
          this,SLOT(slotItemClicked(QListViewItem*)));
}
DirectoryListView::~DirectoryListView()
{
}
/** No descriptions */
void DirectoryListView::setDirectory(QString path)
{
  mCurrentDir.setPath(path);
  createContents();
}
/** No descriptions */
QString DirectoryListView::directory()
{
  return mCurrentDirPath;
}
/** No descriptions */
void DirectoryListView::createContents()
{
  clear();
	setUpdatesEnabled(false);
	const QFileInfoList* files = mCurrentDir.entryInfoList();
	if(files)
  {
	  QFileInfoListIterator it( *files );
	  QFileInfo * fi;
	  while( (fi=it.current()) != 0 )
    {
		  ++it;
		  if ((fi->fileName() == "." || fi->fileName() == ".." || fi->isDir()) &&
           fi->isReadable())
      {
		    QListViewItem *item;
			  item = new QListViewItem(this,fi->fileName());
      }
		}
	}
	setUpdatesEnabled(true);
}
/** No descriptions */
void DirectoryListView::slotItemClicked(QListViewItem* li)
{
  if(!li)
    return;

  QString qs;
  qs = mCurrentDir.absPath();
  if(qs.right(1) != "/")
    qs += "/";
  qs += li->text(0);
  QFileInfo fi(qs);

  if(fi.isDir() && fi.isReadable())
  {
    qs = QDir::cleanDirPath(qs);
    mCurrentDir.setPath(qs);
    createContents();
    emit signalDirectoryChanged(qs);
  }
}
