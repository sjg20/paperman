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

#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "docfacts.h"
#include "err.h"
#include "filemax.h"
#include "filepdf.h"
#include "test_docfacts.h"

// copy one of the test files into a directory
static bool copyFixture(const QString &name, const QString &dir)
{
   return QFile::copy(Test::testSrc + "/" + name, dir + "/" + name);
}

static Docfacts bill()
{
   Docfacts facts;

   facts.type = "bill";
   facts.from = "Northern Power";
   facts.date = "2026-03-03";
   facts.period = "Feb 2026";
   facts.amount = "184.20";
   facts.currency = "NZD";
   facts.summary = "Electricity bill for February 2026";
   return facts;
}

void TestDocfacts::testClean()
{
   Docfacts facts;

   QVERIFY(facts.isEmpty());
   facts.clean();
   QVERIFY(facts.isEmpty());

   // what is already fit to keep is left as it is
   facts = bill();
   facts.clean();
   QCOMPARE(facts, bill());
   QVERIFY(!facts.isEmpty());

   // each is one line, of no great length
   facts.from = "  Northern\n Power\t Ltd ";
   facts.type = QString(Docfacts::max_chars + 20, 'a');
   facts.summary = QString(Docfacts::max_summary_chars + 20, 'b');
   facts.clean();
   QCOMPARE(facts.from, QString("Northern Power Ltd"));
   QCOMPARE(int(facts.type.size()), int(Docfacts::max_chars));
   QCOMPARE(int(facts.summary.size()), int(Docfacts::max_summary_chars));

   // a date may give less than the day, but must be a date
   for (const QString &date : {"2026", "2026-03", "2026-03-14"}) {
      facts.date = date;
      facts.clean();
      QCOMPARE(facts.date, date);
   }
   for (const QString &date : {"14/03/2026", "March 2026", "2026-13",
                               "2026-03-32", "2026-3-4", "26-03-14"}) {
      facts.date = date;
      facts.clean();
      QVERIFY2(facts.date.isEmpty(), qPrintable(date));
   }

   // an amount is a plain number, and a currency three letters
   facts.amount = "1,184.20";
   facts.currency = "nzd";
   facts.clean();
   QCOMPARE(facts.amount, QString("1184.20"));
   QCOMPARE(facts.currency, QString("NZD"));

   facts.amount = "-12";
   facts.clean();
   QCOMPARE(facts.amount, QString("-12"));

   for (const QString &amount : {"$184.20", "184.20 NZD", "about 100",
                                 "1.2.3", "."}) {
      facts.amount = amount;
      facts.clean();
      QVERIFY2(facts.amount.isEmpty(), qPrintable(amount));
   }
   for (const QString &currency : {"$", "dollars", "NZ"}) {
      facts.currency = currency;
      facts.clean();
      QVERIFY2(facts.currency.isEmpty(), qPrintable(currency));
   }
}

void TestDocfacts::testProps()
{
   QMap<QString, QString> props;

   // each fact has its own key, and other properties are left alone
   props["other"] = "kept";
   bill().toProps(props);
   QCOMPARE(int(props.size()), 8);
   QCOMPARE(props.value("doc.from"), QString("Northern Power"));
   QCOMPARE(props.value("doc.date"), QString("2026-03-03"));
   QCOMPARE(props.value("other"), QString("kept"));
   QCOMPARE(Docfacts::fromProps(props), bill());

   // a fact which is not known is not kept, and replaces one which is
   Docfacts less = bill();

   less.amount.clear();
   less.currency.clear();
   less.toProps(props);
   QVERIFY(!props.contains("doc.amount"));
   QVERIFY(!props.contains("doc.currency"));
   QCOMPARE(int(props.size()), 6);
   QCOMPARE(Docfacts::fromProps(props), less);

   // nothing known takes them all out
   Docfacts().toProps(props);
   QCOMPARE(int(props.size()), 1);
   QVERIFY(Docfacts::fromProps(props).isEmpty());

   // what is read is made fit, whatever was kept
   props["doc.date"] = "sometime";
   props["doc.from"] = "Kiwi  Trust Bank";
   Docfacts got = Docfacts::fromProps(props);
   QVERIFY(got.date.isEmpty());
   QCOMPARE(got.from, QString("Kiwi Trust Bank"));
}

void TestDocfacts::testJson()
{
   QJsonObject obj = bill().toJson();

   QCOMPARE(int(obj.size()), 7);
   QCOMPARE(obj.value("from").toString(), QString("Northern Power"));
   QCOMPARE(Docfacts::fromJson(obj), bill());

   // a fact which is not known is left out, and may be left out
   Docfacts less;

   less.type = "letter";
   obj = less.toJson();
   QCOMPARE(int(obj.size()), 1);
   QCOMPARE(Docfacts::fromJson(obj), less);
   QVERIFY(Docfacts::fromJson(QJsonObject()).isEmpty());

   // a model may give the amount as a number, and facts not asked for
   obj = QJsonObject();
   obj["amount"] = 184.2;
   obj["colour"] = "blue";
   obj["date"] = "3 March";
   Docfacts got = Docfacts::fromJson(obj);
   QCOMPARE(got.amount, QString("184.20"));
   QVERIFY(got.date.isEmpty());
   QCOMPARE(int(got.toJson().size()), 1);
}

void TestDocfacts::testSchema()
{
   QJsonObject schema = Docfacts::schema();
   QJsonObject props = schema.value("properties").toObject();

   QCOMPARE(schema.value("type").toString(), QString("object"));
   QVERIFY(!schema.contains("required"));

   // every fact which is kept is asked for, with what it is
   for (const QString &key : bill().toJson().keys()) {
      QJsonObject field = props.value(key).toObject();

      QVERIFY2(!field.isEmpty(), qPrintable(key));
      QCOMPARE(field.value("type").toString(), QString("string"));
      QVERIFY(!field.value("description").toString().isEmpty());
   }
   QCOMPARE(int(props.size()), int(bill().toJson().size()));
}

void TestDocfacts::testFile()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";
   QVERIFY(copyFixture("testfile.max", tmp.path()));

   Docfacts facts;

   // a stack has none at first, and a new file sees those it is given
   {
      Filemax max(dir, "testfile.max", nullptr);
      QVERIFY(!max.load());
      QVERIFY(!Docfacts::read(&max, facts));
      QVERIFY(facts.isEmpty());

      QMap<QString, QString> props;
      props["other"] = "kept";
      QVERIFY(!max.putStackProps(props));
      QVERIFY(!Docfacts::write(&max, bill()));
   }
   {
      Filemax max(dir, "testfile.max", nullptr);
      QVERIFY(!max.load());
      QVERIFY(!Docfacts::read(&max, facts));
      QCOMPARE(facts, bill());

      // they sit beside the stack's other properties
      QMap<QString, QString> props;
      QVERIFY(!max.getStackProps(props));
      QCOMPARE(props.value("other"), QString("kept"));
      QCOMPARE(props.value("doc.type"), QString("bill"));

      // and removing them leaves the others
      QVERIFY(!Docfacts::write(&max, Docfacts()));
      QVERIFY(!Docfacts::read(&max, facts));
      QVERIFY(facts.isEmpty());
      QVERIFY(!max.getStackProps(props));
      QCOMPARE(int(props.size()), 1);
   }

   // a file type with nowhere to keep them says so, and gives none
   QVERIFY(copyFixture("testpdf.pdf", tmp.path()));
   Filepdf pdf(dir, "testpdf.pdf", nullptr);
   QVERIFY(!pdf.load());
   facts = bill();
   QVERIFY(Docfacts::read(&pdf, facts));
   QVERIFY(facts.isEmpty());
   QVERIFY(Docfacts::write(&pdf, bill()));
}
