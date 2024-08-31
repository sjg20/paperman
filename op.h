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

#ifndef OP_H
#define OP_H

class QWidget;


#include "qprogressdialog.h"


class Operation : public QObject
   {
   Q_OBJECT

public:
   /** start a new operation

      \param name     operation name
      \param count    number of steps
      \param parent   parent widget */
   Operation (QString name, int count, QWidget *parent);
   ~Operation ();

   /** set the progress value of the dialog

      \returns true if operation was cancelled */
   bool setProgress (int upto);

   /** indicate that progress has moved on by a given number of steps
   
      \param by      number of steps to increase progress by
      \return true to continue, false if the user wants to cancel
   */
   bool incProgress (int by);

   /** set the number of steps in the operation */
   void setCount (int count);

   static void setMainWidget (QWidget *widget);

   /** @brief State of the operation */
   enum state_t {
      init,       // called when the operation starts
      running,    // called with progress percentage
      uninit,     // called when operation is destroyed
   };

signals:
   /**
    * @brief Report progress of an operation
    * @param percent   Percentage complete
    * @param name      Operation name (empty string)
    *
    * This is emitted whenever setProgress() is called
    */
   void operationProgress(enum Operation::state_t state, int percent,
                          QString name);

private:
   int _maximum;    //!< maximum progress count
   int _upto;       //!< what we are currently up to
   };

#endif /* OP_H */
