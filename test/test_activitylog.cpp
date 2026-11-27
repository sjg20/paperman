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

#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "activitylog.h"
#include "err.h"
#include "test_activitylog.h"

static Activity entry(Activity::e_action action, const QString &path,
                      const QString &from = QString())
{
   Activity act;

   act.action = action;
   act.path = path;
   act.from = from;
   return act;
}

void TestActivityLog::testJson()
{
   Activity act = entry(Activity::A_rename, "bills/power bill.max",
                        "bills/scan1.max");

   act.id = 1234567890123456;
   act.when = QDateTime::fromString("2026-03-04T05:06:07.089Z",
                                    Qt::ISODateWithMs);
   act.who = Activity::W_assistant;

   // only what is set is kept, each under a short key
   QJsonObject obj = act.toJson();
   QCOMPARE(obj.value("do").toString(), QString("rename"));
   QCOMPARE(obj.value("who").toString(), QString("assistant"));
   QCOMPARE(obj.value("t").toString(), QString("2026-03-04T05:06:07.089Z"));
   QCOMPARE(obj.value("path").toString(), QString("bills/power bill.max"));
   QCOMPARE(obj.value("from").toString(), QString("bills/scan1.max"));
   for (const char *key : {"in", "ref", "page", "n", "note", "kept"})
      QVERIFY2(!obj.contains(key), key);

   // and it comes back as it was, large number and all
   Activity back;
   QVERIFY(Activity::fromJson(obj, back));
   QCOMPARE(back, act);

   // with everything set
   act.group = 1234567890123000;
   act.ref = 1234567890123001;
   act.page = 0;
   act.count = 3;
   act.note = "a \"quoted\" caf\u00e9";
   act.kept = "power bill_1.max";
   act.action = Activity::A_scan_page;
   act.who = Activity::W_reader;
   QVERIFY(Activity::fromJson(act.toJson(), back));
   QCOMPARE(back, act);
   QCOMPARE(back.page, 0);

   // an entry needs a number and an action which is known
   obj = act.toJson();
   obj.remove("id");
   QVERIFY(!Activity::fromJson(obj, back));
   obj = act.toJson();
   obj["do"] = "something-new";
   QVERIFY(!Activity::fromJson(obj, back));

   // someone not known of is taken as the user
   obj = act.toJson();
   obj["who"] = "robot";
   QVERIFY(Activity::fromJson(obj, back));
   QCOMPARE(back.who, Activity::W_user);

   // every action has a word of its own
   QSet<QString> names;
   for (int i = Activity::A_none + 1; i < Activity::A_count; i++) {
      QString name = Activity::actionName(Activity::e_action(i));

      QVERIFY2(!name.isEmpty(), qPrintable(QString::number(i)));
      QVERIFY2(!names.contains(name), qPrintable(name));
      names << name;
   }
}

void TestActivityLog::testAddRead()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());

   ActivityLog log(tmp.path());
   QList<Activity> acts;

   // a log which is not there has nothing in it, and that is no error
   QCOMPARE(log.pathname(), tmp.path() + "/.paperlog");
   QVERIFY(!QFile::exists(log.pathname()));
   QVERIFY(!log.read(acts));
   QVERIFY(acts.isEmpty());

   // each entry gets a number, larger than the last, and the time
   Activity scan = entry(Activity::A_scan, "inbox/scan1.max");
   scan.count = 2;
   QVERIFY(!log.add(scan));
   QVERIFY(scan.id > 0);
   QVERIFY(scan.when.isValid());
   QVERIFY(qAbs(scan.when.secsTo(QDateTime::currentDateTimeUtc())) < 60);

   qint64 last = scan.id;
   for (int i = 0; i < 2; i++) {
      Activity page = entry(Activity::A_scan_page, "inbox/scan1.max");

      page.group = scan.id;
      page.page = i;
      QVERIFY(!log.add(page));
      QVERIFY(page.id > last);
      last = page.id;
   }

   // a time which is given is kept
   Activity old = entry(Activity::A_rename, "inbox/bill.max",
                        "inbox/scan1.max");
   old.when = QDateTime::fromString("2020-01-02T03:04:05.000Z",
                                    Qt::ISODateWithMs);
   QVERIFY(!log.add(old));

   // they come back oldest first, as they were
   QVERIFY(!log.read(acts));
   QCOMPARE(int(acts.size()), 4);
   QCOMPARE(acts[0], scan);
   QCOMPARE(acts[1].group, scan.id);
   QCOMPARE(acts[2].page, 1);
   QCOMPARE(acts[3], old);

   // another log on the same repository adds to the same file
   ActivityLog second(tmp.path() + "/");
   Activity more = entry(Activity::A_trash, "inbox/bill.max");
   QVERIFY(!second.add(more));
   QVERIFY(more.id > last);
   QVERIFY(!log.read(acts));
   QCOMPARE(int(acts.size()), 5);

   // only the newest can be asked for
   QVERIFY(!log.read(acts, 2));
   QCOMPARE(int(acts.size()), 2);
   QCOMPARE(acts[0], old);
   QCOMPARE(acts[1], more);

   // the file is one line of JSON for each
   QFile file(log.pathname());
   QVERIFY(file.open(QIODevice::ReadOnly));
   QList<QByteArray> lines = file.readAll().split('\n');
   QCOMPARE(int(lines.size()), 6);
   QVERIFY(lines[5].isEmpty());
   for (int i = 0; i < 5; i++)
      QVERIFY2(QJsonDocument::fromJson(lines[i]).isObject(), lines[i]);
}

void TestActivityLog::testDamaged()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());

   ActivityLog log(tmp.path());
   Activity first = entry(Activity::A_scan, "a.max");
   Activity second = entry(Activity::A_turn, "a.max");

   QVERIFY(!log.add(first));

   /* a line cut short, one which is not JSON, one from something newer
      and a blank one */
   QFile file(log.pathname());
   QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Append));
   file.write("{\"id\":5,\"do\":\"sc\n");
   file.write("not json at all\n");
   file.write("{\"id\":6,\"do\":\"teleport\",\"path\":\"a.max\"}\n");
   file.write("\n");
   file.close();
   QVERIFY(!log.add(second));

   QList<Activity> acts;
   QVERIFY(!log.read(acts));
   QCOMPARE(int(acts.size()), 2);
   QCOMPARE(acts[0], first);
   QCOMPARE(acts[1], second);
}

void TestActivityLog::testDescribe()
{
   Activity act = entry(Activity::A_rename, "bills/power bill.max",
                        "bills/scan1.max");

   QCOMPARE(act.describe(),
            QString("Renamed 'scan1' to 'power bill'"));

   act = entry(Activity::A_move, "bills/2026/power.max", "power.max");
   QCOMPARE(act.describe(),
            QString("Moved 'power' from the top folder to bills/2026"));

   act = entry(Activity::A_scan, "inbox/scan1.max");
   act.count = 1;
   QCOMPARE(act.describe(), QString("Scanned 'scan1' (1 page)"));
   act.count = 12;
   QCOMPARE(act.describe(), QString("Scanned 'scan1' (12 pages)"));

   // pages are numbered from 1 for the user
   act = entry(Activity::A_turn, "a.max");
   act.page = 0;
   QCOMPARE(act.describe(), QString("Turned page 1 of 'a'"));
   act.page = -1;
   QCOMPARE(act.describe(), QString("Turned the pages of 'a'"));

   act = entry(Activity::A_read, "a.max");
   act.count = 7;
   QCOMPARE(act.describe(), QString("Read the text of 7 pages of 'a'"));
   act.page = 2;
   QCOMPARE(act.describe(), QString("Read the text of page 3 of 'a'"));

   // a PDF keeps its extension, as it does on the desk
   act = entry(Activity::A_trash, "bills/statement.PDF");
   QCOMPARE(act.describe(), QString("Deleted 'statement.PDF'"));
   act = entry(Activity::A_restore, "bills/statement.PDF");
   QCOMPARE(act.describe(),
            QString("Put back 'statement.PDF', which had been deleted"));
   QCOMPARE(Activity::displayName("a/b/power bill.max"),
            QString("power bill"));
   QCOMPARE(Activity::displayName("3.5 inch.MAX"), QString("3.5 inch"));
   QCOMPARE(Activity::displayName("scan.pdf"), QString("scan.pdf"));

   act = entry(Activity::A_group, "bills");
   act.note = "Name stacks (3)";
   QCOMPARE(act.describe(), QString("Name stacks (3)"));

   act = entry(Activity::A_undo, "a.max");
   act.note = "Renamed 'a' to 'b'";
   QCOMPARE(act.describe(), QString("Undid: Renamed 'a' to 'b'"));

   // a folder is named by its whole path, and renamed or moved
   act = entry(Activity::A_folder_new, "bills/2027");
   QCOMPARE(act.describe(), QString("Made the folder bills/2027"));
   QVERIFY(act.isFolder());
   act = entry(Activity::A_folder_move, "bills/2026 old", "bills/2026");
   QCOMPARE(act.describe(),
            QString("Renamed the folder bills/2026 to '2026 old'"));
   act = entry(Activity::A_folder_move, "archive/2026", "bills/2026");
   QCOMPARE(act.describe(),
            QString("Moved the folder bills/2026 to archive/2026"));
   QVERIFY(!entry(Activity::A_rename, "a.max", "b.max").isFolder());

   // every action says something
   for (int i = Activity::A_none + 1; i < Activity::A_count; i++) {
      act = entry(Activity::e_action(i), "dir/a.max", "dir/b.max");
      act.note = "note";
      QVERIFY2(!act.describe().isEmpty(),
               qPrintable(Activity::actionName(act.action)));
   }
}

void TestActivityLog::testWhereNow()
{
   QList<Activity> acts;
   qint64 id = 100;
   auto add = [&](Activity::e_action action, const QString &path,
                  const QString &from = QString(), qint64 ref = 0,
                  qint64 group = 0) {
      Activity act = entry(action, path, from);

      act.id = id++;
      act.ref = ref;
      act.group = group;
      acts << act;
      return act.id;
   };
   bool gone;

   // a stack is scanned, named, turned, moved and named again
   add(Activity::A_scan, "inbox/scan1.max");                         // 0
   qint64 named = add(Activity::A_rename, "inbox/bill.max",
                      "inbox/scan1.max");                            // 1
   add(Activity::A_turn, "inbox/bill.max");                          // 2
   add(Activity::A_rename, "inbox/other.max", "inbox/scan2.max");    // 3
   qint64 moved = add(Activity::A_move, "bills/bill.max",
                      "inbox/bill.max");                             // 4
   add(Activity::A_rename, "bills/power.max", "bills/bill.max");     // 5

   // from any entry about it, it is where the last one leaves it
   for (int at : {0, 1, 2, 4, 5}) {
      QCOMPARE(ActivityLog::whereNow(acts, at, gone),
               QString("bills/power.max"));
      QVERIFY(!gone);
   }

   // another stack is not confused with it
   QCOMPARE(ActivityLog::whereNow(acts, 3, gone), QString("inbox/other.max"));

   // an entry which is not there gives nothing
   QCOMPARE(ActivityLog::whereNow(acts, 99, gone), QString());
   QCOMPARE(ActivityLog::whereNow(acts, -1, gone), QString());

   // undoing the last rename, then the move, takes it back
   qint64 last = acts[5].id;
   add(Activity::A_undo, "", "", last);                              // 6
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/bill.max"));
   add(Activity::A_undo, "", "", moved);                             // 7
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("inbox/bill.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 4, gone), QString("inbox/bill.max"));

   // and redoing the move puts it there again
   add(Activity::A_redo, "", "", moved);                             // 8
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/bill.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 1, gone), QString("bills/bill.max"));
   Q_UNUSED(named);

   // it is deleted, which undo brings back
   qint64 trashed = add(Activity::A_trash, "bills/bill.max");        // 9
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/bill.max"));
   QVERIFY(gone);
   QCOMPARE(ActivityLog::whereNow(acts, 9, gone), QString("bills/bill.max"));
   QVERIFY(gone);
   add(Activity::A_undo, "", "", trashed);                           // 10
   ActivityLog::whereNow(acts, 0, gone);
   QVERIFY(!gone);
   ActivityLog::whereNow(acts, 9, gone);
   QVERIFY(!gone);

   /* deleted again, it is put back from the trash, long after: it is
      there again, until that is undone, and done again */
   add(Activity::A_redo, "", "", trashed);
   ActivityLog::whereNow(acts, 0, gone);
   QVERIFY(gone);
   qint64 back = add(Activity::A_restore, "bills/bill.max", "", trashed);
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/bill.max"));
   QVERIFY(!gone);
   QCOMPARE(ActivityLog::whereNow(acts, 9, gone), QString("bills/bill.max"));
   QVERIFY(!gone);
   add(Activity::A_undo, "", "", back);
   ActivityLog::whereNow(acts, 9, gone);
   QVERIFY(gone);
   add(Activity::A_redo, "", "", back);
   ActivityLog::whereNow(acts, 9, gone);
   QVERIFY(!gone);

   // several named together are a group, which is undone as one
   qint64 group = add(Activity::A_group, "bills");                   // 11
   add(Activity::A_rename, "bills/gas.max", "bills/bill.max", 0, group);
   add(Activity::A_rename, "bills/water.max", "inbox/other.max", 0, group);
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/gas.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 3, gone), QString("bills/water.max"));
   add(Activity::A_undo, "", "", group);
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/bill.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 3, gone), QString("inbox/other.max"));
   add(Activity::A_redo, "", "", group);
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/gas.max"));

   // a stack put onto another is gone, and the other stays where it is
   add(Activity::A_stack, "bills/gas.max", "bills/water.max");
   ActivityLog::whereNow(acts, 3, gone);
   QVERIFY(gone);
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/gas.max"));
   QVERIFY(!gone);
}

/* A stack is followed when the folder it is in, or one above that, is
   renamed or moved, and so is a folder */
void TestActivityLog::testWhereNowFolders()
{
   QList<Activity> acts;
   qint64 id = 100;
   auto add = [&](Activity::e_action action, const QString &path,
                  const QString &from = QString(), qint64 ref = 0) {
      Activity act = entry(action, path, from);

      act.id = id++;
      act.ref = ref;
      acts << act;
      return act.id;
   };
   bool gone;

   add(Activity::A_folder_new, "bills/2026");                        // 0
   add(Activity::A_scan, "bills/2026/03mar/power.max");              // 1
   add(Activity::A_scan, "bills/2026x/other.max");                   // 2
   add(Activity::A_folder_move, "bills/year 2026", "bills/2026");    // 3

   // what is in the folder goes with it, and so does the folder
   QCOMPARE(ActivityLog::whereNow(acts, 1, gone),
            QString("bills/year 2026/03mar/power.max"));
   QVERIFY(!gone);
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone), QString("bills/year 2026"));
   QCOMPARE(ActivityLog::whereNow(acts, 3, gone), QString("bills/year 2026"));

   // a folder whose name only starts the same is left alone
   QCOMPARE(ActivityLog::whereNow(acts, 2, gone),
            QString("bills/2026x/other.max"));

   // a folder above it is moved, and then the stack is renamed
   add(Activity::A_folder_move, "archive/bills", "bills");           // 4
   QCOMPARE(ActivityLog::whereNow(acts, 1, gone),
            QString("archive/bills/year 2026/03mar/power.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 2, gone),
            QString("archive/bills/2026x/other.max"));
   add(Activity::A_rename, "archive/bills/year 2026/03mar/electric.max",
       "archive/bills/year 2026/03mar/power.max");                   // 5
   QCOMPARE(ActivityLog::whereNow(acts, 1, gone),
            QString("archive/bills/year 2026/03mar/electric.max"));
   QCOMPARE(ActivityLog::whereNow(acts, 0, gone),
            QString("archive/bills/year 2026"));

   /* a folder is followed by itself too, from any entry: this is where
      a stack would be put back to, if it left the folder at that entry */
   QCOMPARE(ActivityLog::folderNow(acts, 1, "bills/2026/03mar"),
            QString("archive/bills/year 2026/03mar"));
   QCOMPARE(ActivityLog::folderNow(acts, 3, "bills/year 2026"),
            QString("archive/bills/year 2026"));
   QCOMPARE(ActivityLog::folderNow(acts, 4, "archive/bills"),
            QString("archive/bills"));
   QCOMPARE(ActivityLog::folderNow(acts, 0, ""), QString(""));
   QCOMPARE(ActivityLog::folderNow(acts, 0, "bills/2026x"),
            QString("archive/bills/2026x"));

   // an entry made after the moves starts from where things then are
   QCOMPARE(ActivityLog::whereNow(acts, 5, gone),
            QString("archive/bills/year 2026/03mar/electric.max"));
}

void TestActivityLog::testDisabled()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());

   ActivityLog log(tmp.path());
   Activity act = entry(Activity::A_scan, "a.max");

   QVERIFY(ActivityLog::enabled());
   ActivityLog::setEnabled(false);
   auto restore = qScopeGuard([] { ActivityLog::setEnabled(true); });

   // the entry still gets its number, for others to name
   QVERIFY(!log.add(act));
   QVERIFY(act.id > 0);
   QVERIFY(!QFile::exists(log.pathname()));

   ActivityLog::setEnabled(true);
   QVERIFY(!log.add(act));
   QVERIFY(QFile::exists(log.pathname()));
}
