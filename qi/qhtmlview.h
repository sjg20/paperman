/***************************************************************************
                          qhtmlview.h  -  description
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

#ifndef QHTMLVIEW_H
#define QHTMLVIEW_H

#include <qmainwindow.h>
#include <qpixmap.h>
/**
  *@author M. Herder
  */
class QTextBrowser;

class QHTMLView : public QMainWindow
{
Q_OBJECT
public:
	QHTMLView(QWidget * parent = 0, const char * name = 0, WFlags f = WType_TopLevel);
	~QHTMLView();
private: // Private methods
  /**  */
  void init();
private: // Private attributes
  /**  */
  QTextBrowser* mpTextBrowser;
  /**  */
  QPixmap mPixBackward;
  /**  */
  QPixmap mPixForward;
  /**  */
  QPixmap mPixHome;
  /**  */
  int mIdBackward;
  /**  */
  int mIdForward;
private slots:
  void setBackwardAvailable( bool b);
  void setForwardAvailable( bool b);
  void slotTextChanged();
  void about();
  void openFile();
public slots: // Public slots
  /**  */
  void show();
  /**  */
  void home();
protected: // Protected methods
  /**  */
  virtual void resizeEvent(QResizeEvent* e);
  /**  */
  virtual void moveEvent(QMoveEvent* e);
};

#endif
