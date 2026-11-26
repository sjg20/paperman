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

#include "backgroundwork.h"


Backgroundjob::Backgroundjob (const QString &title, QObject *parent)
   : QObject (parent), _title (title)
   {
   Backgroundwork::instance ()->add (this);
   }


Backgroundjob::~Backgroundjob ()
   {
   Backgroundwork::instance ()->remove (this);
   }


void Backgroundjob::setTitle (const QString &title)
   {
   if (title == _title)
      return;
   _title = title;
   emit changed ();
   }


void Backgroundjob::setDetail (const QString &detail)
   {
   if (detail == _detail)
      return;
   _detail = detail;
   emit changed ();
   }


void Backgroundjob::setProgress (int done, int total)
   {
   if (done == _done && total == _total)
      return;
   _done = done;
   _total = total;
   emit changed ();
   }


int Backgroundjob::percent () const
   {
   if (_total <= 0)
      return -1;
   return qBound (0, int (qint64 (_done) * 100 / _total), 100);
   }


void Backgroundjob::setStoppable (bool stoppable)
   {
   if (stoppable == _stoppable)
      return;
   _stoppable = stoppable;
   emit changed ();
   }


void Backgroundjob::requestStop ()
   {
   if (!_stoppable || _stop)
      return;
   _stop = true;
   emit stopAsked ();
   emit changed ();
   }


Backgroundwork *Backgroundwork::instance ()
   {
   static Backgroundwork *work = new Backgroundwork ();

   return work;
   }


void Backgroundwork::add (Backgroundjob *job)
   {
   _jobs << job;
   connect (job, &Backgroundjob::changed, this, &Backgroundwork::changed);
   emit changed ();
   }


void Backgroundwork::remove (Backgroundjob *job)
   {
   _jobs.removeAll (job);
   emit changed ();
   }
