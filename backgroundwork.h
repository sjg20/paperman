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

#include <QList>
#include <QObject>
#include <QString>

/** A piece of work which takes a while, for the user to watch and
    perhaps stop.

    Whatever does the work makes a job, keeps what it says up to date
    and deletes it when the work is done. The job is in the list of
    Backgroundwork for as long as it lives. Stopping is a request: the
    job notes it and says so, and the work stops when it next looks */
class Backgroundjob : public QObject
   {
   Q_OBJECT

public:
   /**
      \param title   what the work is, for the user: "Printing"
      \param parent  parent object */
   explicit Backgroundjob (const QString &title, QObject *parent = nullptr);
   ~Backgroundjob ();

   QString title () const { return _title; }
   void setTitle (const QString &title);

   //! more about how it is going, if there is any: "12 pages in 2 stacks"
   QString detail () const { return _detail; }
   void setDetail (const QString &detail);

   /** say how far the work has got

      \param done   how much is done
      \param total  how much there is, or 0 if that is not known */
   void setProgress (int done, int total);
   int done () const { return _done; }
   int total () const { return _total; }

   //! how far it has got as a percentage, or -1 if that is not known
   int percent () const;

   //! whether the work can be stopped, which it cannot unless this is set
   bool stoppable () const { return _stoppable; }
   void setStoppable (bool stoppable);

   //! ask for the work to stop, if it can be; it is asked only once
   void requestStop ();

   //! true once the work has been asked to stop
   bool stopRequested () const { return _stop; }

signals:
   //! what the job says has changed
   void changed ();

   //! the work has been asked to stop
   void stopAsked ();

private:
   QString _title;
   QString _detail;
   int _done = 0;
   int _total = 0;
   bool _stoppable = false;
   bool _stop = false;
   };


/** The work going on at present which takes a while: one list for the
    whole program, which a window can show */
class Backgroundwork : public QObject
   {
   Q_OBJECT

public:
   //! the one list
   static Backgroundwork *instance ();

   //! the jobs under way, the oldest first
   QList<Backgroundjob *> jobs () const { return _jobs; }

signals:
   //! a job has started, finished, or changed what it says
   void changed ();

private:
   friend class Backgroundjob;

   Backgroundwork () {}
   void add (Backgroundjob *job);
   void remove (Backgroundjob *job);

   QList<Backgroundjob *> _jobs;
   };
