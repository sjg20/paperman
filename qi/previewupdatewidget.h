/***************************************************************************
                          previewupdatewidget.h  -  description
                             -------------------
    begin                : Mon May 13 2002
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

#ifndef PREVIEWUPDATEWIDGET_H
#define PREVIEWUPDATEWIDGET_H

#include <qimage.h>
#include <qpixmap.h>
#include <qwidget.h>
//Added by qt3to4:
#include <QPaintEvent>

/**
  *@author Michael Herder
  */

class PreviewUpdateWidget : public QWidget
{
Q_OBJECT
public: 
  PreviewUpdateWidget(QWidget* parent=0,const char* name=0);
  ~PreviewUpdateWidget();
  /** No descriptions */
  void setData(QByteArray& data);
  /** No descriptions */
  void clearWidget();
  /** No descriptions */
  void initPixmap(int rw, int rh);
protected: // Protected methods
  /** No descriptions */
  void paintEvent(QPaintEvent* e);
private:
 /** */
 int mBegin;
 /** */
 int mRealWidth;
 /** */
 int mRealHeight;
 /** */
 QPixmap mPixmap;
};

#endif
