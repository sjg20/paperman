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
#include "options.h"

#include <qvariant.h>
#include <qimage.h>
#include <qpixmap.h>

#include "qxmlconfig.h"
#include "sliderspin.h"

#include "ui_printopt.h"


/*
 *  Constructs a Options as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
Options::Options(QWidget* parent, const char* name, bool modal, Qt::WindowFlags fl)
    : QDialog(parent, fl)
{
    setObjectName(name);
    setModal(modal);
    setupUi(this);

    init();
}

/*
 *  Destroys the object and frees any allocated resources
 */
Options::~Options()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void Options::languageChange()
{
    retranslateUi(this);
}


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


void Options::setMainwidget( Mainwidget *main )
{
   _main = main;
}


void Options::init()
{
   int single_val = xmlConfig->intValue("SCAN_SINGLE");
   int stack_val = xmlConfig->intValue("SCAN_STACK_COUNT");

   jpeg->setChecked (xmlConfig->boolValue ("SCAN_USE_JPEG"));

   single->setRange (1, 999);
   limit->setChecked (single_val > 0);
   single->setEnabled (single_val > 0);
   single->setValue (single_val);

   stackCount->setRange (1, 999);
   stackLimit->setChecked (stack_val > 0);
   stackCount->setEnabled (stack_val > 0);
   stackCount->setValue (stack_val);

   blank->setCurrentIndex (xmlConfig->intValue("SCAN_BLANK"));
   threshold->setRange (10, 5000);
   threshold->setTitle ("Blank threshold n:1");
   threshold->setValue (xmlConfig->intValue("SCAN_BLANK_THRESHOLD"));

   connect (threshold, SIGNAL (signalValueChanged(int)), this, SLOT (updateThreshold (int)));
   connect (limit, SIGNAL (toggled(bool)), single, SLOT (setEnabled(bool)));
   connect (stackLimit, SIGNAL (toggled(bool)), stackCount, SLOT (setEnabled(bool)));

   _main = 0;
}


void Options::ok_clicked()
{
   xmlConfig->setBoolValue("SCAN_USE_JPEG" ,jpeg->isChecked ());

   int single_val = limit->isChecked () ? single->value () : 0;
   int stack_val = stackLimit->isChecked () ? stackCount->value () : 0;
   xmlConfig->setIntValue("SCAN_SINGLE", single_val);
   xmlConfig->setIntValue("SCAN_STACK_COUNT", stack_val);
   xmlConfig->setIntValue("SCAN_BLANK", blank->currentIndex ());
   xmlConfig->setIntValue("SCAN_BLANK_THRESHOLD", threshold->value ());
   close ();
}


void Options::cancel_clicked()
{
   close ();
}


void Options::updateThreshold( int value)
{
   double cov;

   // calculate coverage
   if (value)
      cov = 1.0 / value;
   else
      cov = 1;
   coverageStr->setText (QString ("%1%").arg (cov * 100, 0, 'f', 1));
}


