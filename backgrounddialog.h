/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
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

#pragma once

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;

class OcrReader;

/** Shows the work which Paperman is doing in the background, and lets it
    be held back or given up.

    For now that is the reading of the text of scanned pages: the stacks
    waiting are listed in the order they are taken, with how many pages
    each has left. Reading can be paused, one stack or several dropped
    from the list, or all of it stopped. The window follows the reader as
    it goes, and stays open while Paperman is used */
class Backgrounddialog : public QDialog
   {
   Q_OBJECT

public:
   /**
      \param reader  the reader to show and control
      \param parent  parent widget */
   explicit Backgrounddialog (OcrReader *reader, QWidget *parent = nullptr);

   //! the full path of each stack listed, in order
   QStringList listed () const;

   //! what the line above the list says
   QString summary () const;

   //! select the stack in a row of the list, from 0
   void selectRow (int row);

   /** what is left to read, as "12 pages in 2 stacks"

      \param reader  the reader
      \returns the text; "1 stack" alone when no page count is known */
   static QString leftText (const OcrReader *reader);

public slots:
   //! pause the reading, or carry on with it
   void togglePaused ();

   //! drop the stacks selected from the list
   void removeSelected ();

   //! drop every stack from the list
   void stopAll ();

private slots:
   //! show the reader as it is now
   void refresh ();

private:
   OcrReader *_reader;
   QLabel *_summary;
   QTableWidget *_table;
   QPushButton *_pause, *_remove, *_stop;
   };
