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


#include <QDebug>

#include "err.h"
#include "qapplication.h"

#include "op.h"


//! the main widget which needs to know about operation progress
static QWidget *main_widget;



Operation::Operation (QString name, int count, QWidget *parent)
   {
   UNUSED (parent);
//    setMinimumDuration (200);
//    setFocusPolicy (Qt::NoFocus);
   _maximum = count;
   qDebug () << "operation max" << _maximum;
   connect(this, SIGNAL(operationProgress(int, QString)),
           main_widget, SLOT(setProgress(int, QString)));
   emit operationProgress(-1, name);
   _upto = 0;
   }


Operation::~Operation ()
   {
   emit operationProgress(-2, QString());
   }


void Operation::setCount (int count)
   {
   _maximum = count < 1 ? 1 : count;
//    setMaximum (count);
   }


bool Operation::setProgress (int upto)
   {
   _upto = upto;
   int pc = int ((float)upto / _maximum * 100);
   emit operationProgress(pc, QString());
//   printf ("progress %d\n", pc);
//    QProgressDialog::setValue (upto);
   qApp->processEvents ();
//    if (wasCanceled ())
//       printf ("cancelled\n");
//    return wasCanceled ();
   return false;
   }


bool Operation::incProgress (int by)
   {
   setProgress (_upto + by);

   return true;
   }
   

void Operation::setMainWidget (QWidget *widget)
   {
   main_widget = widget;
   }


