/***************************************************************************
                          qoptionscrollview.cpp  -  description
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

#include "err.h"
#include "qoptionscrollview.h"
#include <qsizepolicy.h>
#include <qwidget.h>
#include <qlayout.h>
#include <QResizeEvent>
#include <QLabel>

QOptionScrollView::QOptionScrollView(QWidget * parent, const char * name,Qt::WindowFlags f)
                  :QScrollArea(parent)
{
  UNUSED (f);
  setObjectName (name);
//  setHScrollBarMode(AlwaysOff);
  setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOn);
#if 0
  QPalette palette;
  palette.setBrush(viewport()->backgroundRole(), QPalette::Background);
  viewport()->setPalette(palette);
  viewport()->setBackgroundMode(QPalette::Background);
#endif
  mpMainWidget = new QWidget(/*viewport()*/);
  mpMainLayout = new QBoxLayout(QBoxLayout::TopToBottom, mpMainWidget);
  /* let the scroll area size the content from the layout's minimum hint
     as widgets are added, rather than relying on the layout to push a
     minimum size onto the widget, which does not always happen */
  setWidgetResizable (true);
  /* the content follows the viewport's width, so the options squeeze
     rather than scroll sideways: the layout must not impose its minimum
     width on the widget, and the width is ignored in the size hint */
  mpMainLayout->setSizeConstraint (QLayout::SetNoConstraint);
  mpMainWidget->setSizePolicy (QSizePolicy::Ignored, QSizePolicy::Preferred);
  setWidget (mpMainWidget);
//   mpMainWidget->setLayout (mpMainLayout);
//   setLayout (mpMainLayout);
//   setWidgetResizable (true);
//   mpMainWidget->setFixedWidth(150);
//   mpMainWidget->setFixedHeight(150);
}
QOptionScrollView::~QOptionScrollView()
{
}

#if 0
/**  */
void QOptionScrollView::viewportResizeEvent(QResizeEvent* qre)
{
  Q3ScrollView::viewportResizeEvent(qre);
  if((mpMainWidget->sizeHint().isValid()==true)&&(visibleWidth()>0))
  {
    mpMainWidget->setFixedWidth(visibleWidth());//,m_pMemberBox->height());
  }
}
#endif


void QOptionScrollView::addWidget(QWidget* qw,int stretch)
{
  mpMainLayout->addWidget(qw,stretch);
//   mpMainWidget->updateGeometry ();
//   mpMainWidget->setMinimumSize(mpMainLayout->minimumSize ());
}
