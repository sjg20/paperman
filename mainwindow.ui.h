/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/
/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

#include "qstatusbar.h"
#include "qstring.h"

#include "about.h"
#include "desktopviewer.h"
#include "desktopwidget.h"
#include "mainwidget.h"
#include "pagewidget.h"


void Mainwindow::init ()
{
   connect (_main->getPage (), SIGNAL (newContents (const char *)),
            this, SLOT (statusUpdate (const char *)));
   connect (_main->getViewer (), SIGNAL (newContents (const char *)),
            this, SLOT (statusUpdate (const char *)));
   connect (_main, SIGNAL (newContents (const char *)),
            this, SLOT (statusUpdate (const char *)));
   viewSmoothingAction->setOn (_main->isSmoothing ());
   connect (match, SIGNAL (returnPressed()),
	    this, SLOT (matchUpdate ()));
   connect (reset, SIGNAL (pressed()),
	    this, SLOT (matchReset ()));
   connect (match, SIGNAL (textChanged(const QString&)),
	    this, SLOT (matchChange (const QString &)));
}


void Mainwindow::filePrint()
{
   _main->print ();
}


void Mainwindow::fileExit()
{
    exit (0);
}


void Mainwindow::editFind()
{

}


void Mainwindow::helpAbout()
{
   About about;

   about.exec ();
}




Desktopwidget * Mainwindow::getDesktop()
{
   return _main->getDesktop ();
}


void Mainwindow::page_clicked()
{
   _main->swapDesktop ();
}


void Mainwindow::left_clicked()
{
   _main->pageLeft ();
}


void Mainwindow::right_clicked()
{
   _main->pageRight ();
}


void Mainwindow::statusUpdate( const char *str )
{
    QString s = QString (str);

    statusBar()->message (s);
}


void Mainwindow::scan_clicked()
{
   _main->scan ();
}


void Mainwindow::pscan_clicked()
{
   _main->pscan ();
}



void Mainwindow::selectAll()
{
   _main->selectAll ();
}


void Mainwindow::toolsArrangeby_PositionAction_activated()
{
    _main->arrangeBy (Mainwidget::byPosition);
}


void Mainwindow::toolsArrangeby_DateAction_activated()
{
    _main->arrangeBy (Mainwidget::byDate);
}


void Mainwindow::toolsArrangeby_NameAction_activated()
{
    _main->arrangeBy (Mainwidget::byName);
}


void Mainwindow::rotate_left_clicked()
{
   _main->rotate (-90);
}


void Mainwindow::rotate_right_clicked()
{
   _main->rotate (90);
}


void Mainwindow::hflip_clicked()
{
   _main->flip (1);
}


void Mainwindow::vflip_clicked()
{
   _main->flip (0);
}


void Mainwindow::viewSmoothingAction_toggled( bool b )
{
   _main->setSmoothing (b);
}




void Mainwindow::matchUpdate()
{
   _main->matchUpdate (match->text (), global->isChecked ());
}

void Mainwindow::matchReset()
{
    // reset the match
    _main->matchUpdate (QString::null, false);
}


void Mainwindow::matchChange( const QString & )
{
    if (!global->isChecked ())
      _main->matchUpdate (match->text (), false);
}


void Mainwindow::pleft_clicked()
{
   _main->stackLeft ();
}


void Mainwindow::pright_clicked()
{
   _main->stackRight ();
}



void Mainwindow::go_clicked()
{
   matchUpdate ();
}


void Mainwindow::options_clicked()
{
   _main->options ();
}
