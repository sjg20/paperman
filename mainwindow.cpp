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

#include <QtGui>
#include <QSettings>
#include <qvariant.h>


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

#include "config.h"

#include "ui_about.h"
#include "desktopmodel.h"
#include "desktopundo.h"
#include "desktopwidget.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "pagewidget.h"



/*
 *  Constructs a Mainwindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 */
Mainwindow::Mainwindow(QWidget* parent, const char* name, Qt::WindowFlags fl)
    : QMainWindow(parent, name, fl)
{
   _progress = 0;
   _label = 0;
   setupUi(this);
   init();
   _welcome_shown = false;
   _desktop = _main->getDesktop ();
   QSettings qs;

   setWindowState(windowState() ^ Qt::WindowFullScreen);
   setWindowState(windowState() ^ Qt::WindowMaximized);

   restoreGeometry(qs.value("mainwindow/geometry").toByteArray());
   restoreState(qs.value("mainwindow/state").toByteArray());

   // to stop the status bar rising all the time, set its maximum size
   QSize size = QSize (5000, 20);
   statusBar ()->setMaximumSize (size);
   connect (_main->getDesktop (), SIGNAL (updateDone ()),
         this, SLOT (welcome ()));
   connect (_main->getDesktop (), SIGNAL (undoChanged ()),
         this, SLOT (undoChanged ()));
   undoChanged ();
}


/*
 *  Destroys the object and frees any allocated resources
 */
Mainwindow::~Mainwindow()
{
    // no need to delete child widgets, Qt does it all for us
}


void Mainwindow::undoChanged ()
   {
   Desktopundostack *undo = _main->getDesktop ()->getModel ()->getUndoStack ();

   delete actionUndo;
   delete actionRedo;
   actionUndo = undo->createUndoAction (menuEdit);
   actionRedo = undo->createRedoAction (menuEdit);
   actionUndo->setShortcut (tr ("Ctrl+Z"));
   actionRedo->setShortcut (tr ("Ctrl+Shift+Z"));
   menuEdit->insertAction (actionFind, actionUndo);
   menuEdit->insertAction (actionFind, actionRedo);
   }


void Mainwindow::welcome ()
   {
   if (!_welcome_shown)
      {
      statusBar()->showMessage (tr ("Welcome to Maxview, a scanning and filing application to help you manage your paper mountain"), 5000);
      _welcome_shown = true;
      }
   }

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void Mainwindow::languageChange()
{
    retranslateUi(this);
}


void Mainwindow::saveSettings ()
   {
   QSettings settings;

   settings.setValue("mainwindow/geometry", saveGeometry());
   settings.setValue("mainwindow/state", saveState());
   _main->closing ();
   }


void Mainwindow::closeEvent(QCloseEvent *event)
   {
   saveSettings ();
   QMainWindow::closeEvent(event);
   }


void Mainwindow::init ()
{
   connect (_main->getPage (), SIGNAL (newContents (QString)),
            this, SLOT (statusUpdate (QString)));
   connect (_main, SIGNAL (newContents (QString)),
            this, SLOT (statusUpdate (QString)));
   connect (_main->getDesktop (), SIGNAL (newContents (QString)),
            this, SLOT (statusUpdate (QString)));
}


void Mainwindow::on_actionPrint_activated()
{
   _main->print ();
}


void Mainwindow::on_actionExit_activated()
{
   saveSettings ();
   qApp->quit ();
}


void Mainwindow::on_actionAbout_activated()
{
   Ui::About about;
   QDialog *widget = new QDialog;

   about.setupUi(widget);
   about.version->setText (QString ("Version %1: %2, %3").arg (CONFIG_version_str)
      .arg (__DATE__).arg (__TIME__));
   widget->show ();
   widget->exec ();
}

void Mainwindow::on_actionFind_activated()
   {
   getDesktop()->activateFind ();
   }

void Mainwindow::on_actionFullScreen_activated()
{
   setWindowState(windowState() ^ Qt::WindowFullScreen);
}

Desktopwidget * Mainwindow::getDesktop()
{
   return _main->getDesktop ();
}


void Mainwindow::on_actionSwap_activated()
{
   _main->swapDesktop ();
}


void Mainwindow::statusUpdate (QString str)
{
    statusBar()->message (str);
}


void Mainwindow::setProgress (int percent, QString name)
   {
   // initialise
   if (percent == -1)
      {
      statusBar()->clearMessage ();
//       printf ("new op '%s'\n", name.latin1 ());
      _label = new QLabel (name);
      _progress = new QProgressBar ();
      _progress->setRange (0, 100);
      _progress->setTextVisible (false);
      statusBar()->addWidget (_label, false);
      statusBar()->addWidget (_progress, true);
      QSize size = _progress->sizeHint ();
      size = QSize (5000, 20);
      _progress->setMaximumSize (size);
      size = _progress->sizeHint ();
      _progress->show ();
      _label->show ();
      }

   // destroy
   else if (percent == -2)
      {
      delete _progress;
      delete _label;
      _progress = 0;
      }
   else if (_progress)
      _progress->setValue (percent);
   }



void Mainwindow::on_actionScango_activated()
{
   _main->scan ();
}


void Mainwindow::on_actionPscan_activated()
{
   _main->pscan ();
}



void Mainwindow::on_actionSelectall_activated()
{
   _main->selectAll ();
}


void Mainwindow::on_actionByPosition_activated()
{
    _main->arrangeBy (Mainwidget::byPosition);
}


void Mainwindow::on_actionByDate_activated()
{
    _main->arrangeBy (Mainwidget::byDate);
}


void Mainwindow::on_actionByName_activated()
{
    _main->arrangeBy (Mainwidget::byName);
}


void Mainwindow::on_actionResize_all_activated()
{
    _main->resizeAll ();
}


void Mainwindow::on_actionRleft_activated()
{
   _main->rotate (-90);
}


void Mainwindow::on_actionRright_activated()
{
   _main->rotate (90);
}


void Mainwindow::on_actionHflip_activated()
{
   _main->flip (1);
}


void Mainwindow::on_actionVflip_activated()
{
   _main->flip (0);
}



void Mainwindow::on_actionOptions_activated()
{
   _main->options ();
}
