/***************************************************************************
                          qhtmlview.cpp  -  description
                             -------------------
    begin                : Fri Nov 10 2000
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

#include "resource.h"

#include "./pics/forward.xpm"
#include "./pics/backward.xpm"
#include "./pics/home.xpm"
#include "qhtmlview.h"
#include "qxmlconfig.h"

#include <qbrush.h>
#include <qtextbrowser.h>
#include <qwidget.h>
#include <qpopupmenu.h>
#include <qtoolbar.h>
#include <qtoolbutton.h>

#ifdef KDEAPP
#include <kmenubar.h>
#else
#include <qmenubar.h>
#endif

#include <qmessagebox.h>
#include <qfiledialog.h>


QHTMLView::QHTMLView(QWidget * parent,const char * name,WFlags f)
          :QMainWindow(parent,name,f)
{
  setCaption(tr("QuiteInsane - Help viewer"));
  init();
}
QHTMLView::~QHTMLView()
{
}
/**  */
void QHTMLView::init()
{
  QString qs;
  QString qs2;
  mpTextBrowser = new QTextBrowser(this);
  mpTextBrowser->setTextFormat(Qt::RichText);
  setCentralWidget(mpTextBrowser);
  qs = xmlConfig->stringValue("HELP_INDEX");
  qs2 = xmlConfig->stringValue("HELP_LAST_PAGE");
  QFile qf1(qs);
  if(qf1.exists()) mpTextBrowser->setSource(qs);
  if(qs != qs2 )
  {
    QFile qf2(qs2);
    if(qf2.exists()) mpTextBrowser->setSource(qs2);
  }
  QPopupMenu* file = new QPopupMenu( this );
  file->insertItem( tr("&Open File"), this, SLOT( openFile() ), ALT | Key_O );
  file->insertSeparator();
  file->insertItem( tr("&Quit"), this, SLOT( close() ), ALT | Key_Q );

  QPopupMenu* go = new QPopupMenu( this );

  mIdBackward = go->insertItem(tr("&Backward"),mpTextBrowser,
                   SLOT( backward() ),ALT | Key_Left );
  mIdForward = go->insertItem(tr("&Forward"),mpTextBrowser,
                   SLOT( forward() ),ALT | Key_Right );
  go->insertItem(tr("&Home"),mpTextBrowser, SLOT( home() ) );

  QPopupMenu* help = new QPopupMenu( this );
  help->insertItem(tr("&About ..."),this,SLOT(about()));

#ifdef KDEAPP
  KMenuBar* mb = new KMenuBar(this);
#else
  QMenuBar* mb = new QMenuBar(this);
#endif
  mb->insertItem(tr("&File"),file);
  mb->insertItem(tr("&Go"),go);
  mb->insertSeparator();
  mb->insertItem(tr("&Help"),help );

  mb->setItemEnabled( mIdForward, FALSE);
  mb->setItemEnabled( mIdBackward, FALSE);

  connect(mpTextBrowser, SIGNAL( backwardAvailable( bool ) ),
    	    this, SLOT( setBackwardAvailable( bool ) ) );
  connect(mpTextBrowser, SIGNAL( forwardAvailable( bool ) ),
	        this, SLOT( setForwardAvailable( bool ) ) );
  connect(mpTextBrowser,SIGNAL(textChanged() ),this,
          SLOT( slotTextChanged() ) );

  QToolBar* toolbar = new QToolBar( this );
  addToolBar( toolbar);
  QToolButton* tb1;

	mPixForward = QPixmap((const char **)forward_xpm);
	mPixBackward = QPixmap((const char **)backward_xpm);
	mPixHome = QPixmap((const char **)home_xpm);

  tb1 = new QToolButton(mPixBackward, tr("Backward"), "", mpTextBrowser,
                        SLOT(backward()), toolbar );
  connect(mpTextBrowser, SIGNAL( backwardAvailable(bool) ),tb1,
          SLOT( setEnabled(bool) ) );
  tb1->setEnabled( FALSE );

  tb1 = new QToolButton(mPixForward, tr("Forward"), "",mpTextBrowser,
                        SLOT(forward()), toolbar );
  connect(mpTextBrowser,SIGNAL(forwardAvailable(bool) ),tb1,
          SLOT( setEnabled(bool) ) );
  tb1->setEnabled( FALSE );

  tb1 = new QToolButton(mPixHome, tr("Home"), "",this,
                        SLOT(home()), toolbar );
  QWidget* dummy = new QWidget(toolbar);
  toolbar->setStretchableWidget(dummy);
  setRightJustification(true);
}

void QHTMLView::setBackwardAvailable( bool b)
{
  menuBar()->setItemEnabled(mIdBackward, b);
}

void QHTMLView::setForwardAvailable( bool b)
{
  menuBar()->setItemEnabled(mIdForward, b);
}

void QHTMLView::slotTextChanged()
{
  xmlConfig->setStringValue("HELP_LAST_PAGE",mpTextBrowser->source());
  if (mpTextBrowser->documentTitle().isNull())
  	setCaption(mpTextBrowser->context());
  else
	  setCaption(mpTextBrowser->documentTitle()) ;
}

void QHTMLView::about()
{
    QMessageBox::about( this, "QHTMLView",
			tr("<center><b><h2>QHTMLView</h2></b></center><br>"
			"<center>&copy 2000 M. Herder crapsite@gmx.net</center><br>"
			"<center>A very simple help viewer.</center>"));
}

void QHTMLView::openFile()
{
  QString fn = QFileDialog::getOpenFileName(QString::null, QString::null,
                                            this );
  if (!fn.isEmpty())
    mpTextBrowser->setSource(fn);
}

/**  */
void QHTMLView::moveEvent(QMoveEvent* e)
{
  QWidget::moveEvent(e);
  xmlConfig->setIntValue("HELP_POS_X",x());
  xmlConfig->setIntValue("HELP_POS_Y",y());
}
/**  */
void QHTMLView::resizeEvent(QResizeEvent* e)
{
  QWidget::resizeEvent(e);
  //it's possible to get a resize-event, even if the widget
  //isn't visible; this would cause problems when we try to
  //restore the saved size/position
  if(isVisible())
  {
    xmlConfig->setIntValue("HELP_SIZE_X",width());
    xmlConfig->setIntValue("HELP_SIZE_Y",height());
  }
}
/**  */
void QHTMLView::show()
{
  int x;
  int y;
  x = xmlConfig->intValue("HELP_SIZE_X",500);
  y = xmlConfig->intValue("HELP_SIZE_Y",500);
  resize(x,y);
  x = xmlConfig->intValue("HELP_POS_X",0);
  y = xmlConfig->intValue("HELP_POS_Y",0);
  move(x,y);
  QWidget::show();
}
/**  */
void QHTMLView::home()
{
  QString qs;
  qs = xmlConfig->stringValue("HELP_INDEX");
  QFile qf1(qs);
  if(qf1.exists()) mpTextBrowser->setSource(qs);
  mpTextBrowser->home();
}
