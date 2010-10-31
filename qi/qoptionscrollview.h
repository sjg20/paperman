/***************************************************************************
                          qoptionscrollview.h  -  description
                             -------------------
    begin                : Thu Jul 20 2000
    copyright            : (C) 2000 by M. Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2 as     *
 *   published by the Free Software Foundation.                            *
 *                                                                         *
 ***************************************************************************/

#ifndef QOPTIONSCROLLVIEW_H
#define QOPTIONSCROLLVIEW_H

//#include <q3scrollview.h>
//Added by qt3to4:
#include <QResizeEvent>
#include <Q3VBoxLayout>
#include <QScrollArea>

/**
  *@author M. Herder
  */
//forward declarations
class Q3VBoxLayout;

class QOptionScrollView : public QScrollArea
{
public: 
	QOptionScrollView(QWidget * parent=0, const char * name=0, Qt::WFlags f=0 );
	~QOptionScrollView();
  /**  */
  QWidget* getMainWidget();
  void addWidget(QWidget* qw,int stretch=0);
protected:
  /**  */
  virtual void resizeEvent(QResizeEvent* qre);
//s  virtual void viewportResizeEvent(QResizeEvent* qre);
private: // Private attributes
  /** */
  QWidget* mpMainWidget;
  /** */
  QBoxLayout* mpMainLayout;
};

#endif
