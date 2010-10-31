/***************************************************************************
                          qdraglabel.h  -  description
                             -------------------
    begin                : Thu Feb 22 2001
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

#ifndef QDRAGLABEL_H
#define QDRAGLABEL_H

#include <qlabel.h>
#include <qstringlist.h>


/**@author Michael Herder */

/**This label allows dragging a list of files to other
applications.  */

class QMouseMoveEvent;

class QDragLabel : public QLabel
{
Q_OBJECT
public:
	QDragLabel(QWidget* parent,const char* name=0,WFlags f=0);
	~QDragLabel();
  /**Clear the list of filenames and add fn as the only entry.*/
  void setFilename(QString fn);
  /**Add fn to the list of filenames.*/
  void addFilename(QString fn);
  /** Clear the list of filenames */
  void clearFilenameList();
private: // Private attributes
  /**  */
  QStringList mFileNames;
protected: // Protected methods
  /**  */
  void mouseMoveEvent(QMouseEvent* me);
};

#endif
