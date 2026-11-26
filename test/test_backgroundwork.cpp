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
*/

#include <QSignalSpy>
#include <QtTest/QtTest>

#include "backgroundwork.h"
#include "test_backgroundwork.h"

void TestBackgroundwork::testJobs()
{
   Backgroundwork *work = Backgroundwork::instance();
   QCOMPARE(work, Backgroundwork::instance());
   int before = work->jobs().size();
   QSignalSpy changed(work, &Backgroundwork::changed);

   // a job is listed from when it is made, the oldest first
   Backgroundjob *first = new Backgroundjob("Printing");
   QCOMPARE(int(work->jobs().size()), before + 1);
   QCOMPARE(work->jobs().last(), first);
   QCOMPARE(changed.count(), 1);
   {
      Backgroundjob second("Packaging");
      QCOMPARE(int(work->jobs().size()), before + 2);
      QCOMPARE(work->jobs().last(), &second);
      QCOMPARE(work->jobs()[before], first);
   }
   QCOMPARE(int(work->jobs().size()), before + 1);

   // what it says can change, and the list says so only when it does
   changed.clear();
   QCOMPARE(first->title(), QString("Printing"));
   QCOMPARE(first->detail(), QString());
   QCOMPARE(first->percent(), -1);
   first->setTitle("Printing");
   first->setDetail(QString());
   QCOMPARE(changed.count(), 0);
   first->setTitle("Printing 3 stacks");
   first->setDetail("page 2");
   QCOMPARE(first->title(), QString("Printing 3 stacks"));
   QCOMPARE(first->detail(), QString("page 2"));
   QCOMPARE(changed.count(), 2);

   // how far it has got is a percentage once the total is known
   first->setProgress(5, 0);
   QCOMPARE(first->percent(), -1);
   first->setProgress(5, 20);
   QCOMPARE(first->done(), 5);
   QCOMPARE(first->total(), 20);
   QCOMPARE(first->percent(), 25);
   first->setProgress(30, 20);
   QCOMPARE(first->percent(), 100);
   first->setProgress(2000000, 3000000);
   QCOMPARE(first->percent(), 66);
   changed.clear();
   first->setProgress(2000000, 3000000);
   QCOMPARE(changed.count(), 0);

   delete first;
   QCOMPARE(int(work->jobs().size()), before);
   QCOMPARE(changed.count(), 1);
}

void TestBackgroundwork::testStop()
{
   Backgroundjob job("Reading");
   QSignalSpy asked(&job, &Backgroundjob::stopAsked);
   QSignalSpy changed(&job, &Backgroundjob::changed);

   // work which cannot be stopped is not asked to
   QVERIFY(!job.stoppable());
   job.requestStop();
   QVERIFY(!job.stopRequested());
   QCOMPARE(asked.count(), 0);

   job.setStoppable(true);
   QVERIFY(job.stoppable());
   QCOMPARE(changed.count(), 1);
   job.requestStop();
   QVERIFY(job.stopRequested());
   QCOMPARE(asked.count(), 1);
   QCOMPARE(changed.count(), 2);

   // asking again changes nothing
   job.requestStop();
   QCOMPARE(asked.count(), 1);
   QCOMPARE(changed.count(), 2);
}
