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

    At the top is whatever is listed in Backgroundwork: printing,
    converting, indexing and the like, each with how far it has got, and
    a button to stop the one selected if it can be stopped.

    Below that is the reading of the text of scanned pages: the stacks
    waiting are listed in the order they are taken, with how many pages
    each has left. Reading can be paused, one stack or several dropped
    from the list, or all of it stopped. The window follows the work as
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

   //! what the list of other work shows, a row for each job as
   //! "title: how far it has got"
   QStringList jobsListed () const;

   //! select the job in a row of the list of other work, from 0
   void selectJob (int row);

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

   //! ask the job selected to stop
   void stopJob ();

private slots:
   //! show the reader as it is now
   void refresh ();

   //! show the other work as it is now
   void refreshJobs ();

private:
   //! how often the list of other work is brought up to date, at most
   static constexpr int jobs_refresh_ms = 200;

   OcrReader *_reader;
   QLabel *_summary;
   QTableWidget *_table;
   QPushButton *_pause, *_remove, *_stop;
   QTableWidget *_jobs;       //!< the other work going on
   QPushButton *_stop_job;
   bool _jobs_due = false;    //!< refreshJobs() is on its way
   };
