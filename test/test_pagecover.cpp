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

#include <QPainter>
#include <QTest>

#include "../imagetrim.h"
#include "../pagecover.h"
#include "test_pagecover.h"

//! the cover in some text, which must be one
static Pagecover parsed(const QString &str)
{
   Pagecover cover;

   if (!Pagecover::parse(str, cover))
      qWarning() << "not a cover:" << str;
   return cover;
}

void TestPagecover::testParse()
{
   Pagecover cover;

   QVERIFY(Pagecover::parse("page", cover));
   QVERIFY(cover.isWholePage());
   QCOMPARE(cover.box(), QRectF(0, 0, 1, 1));

   QVERIFY(Pagecover::parse("0.1000,0.2000,0.3000,0.4000", cover));
   QVERIFY(cover.isBox());
   QCOMPARE(cover.box(), QRectF(0.1, 0.2, 0.3, 0.4));

   // fewer places are fine, and more are rounded to four
   QVERIFY(Pagecover::parse("0.5,0,0.5,1", cover));
   QCOMPARE(cover.toString(), QString("0.5000,0.0000,0.5000,1.0000"));
   QVERIFY(Pagecover::parse("0.12344,0.5,0.25,0.25", cover));
   QCOMPARE(cover.toString(), QString("0.1234,0.5000,0.2500,0.2500"));

   // a box over all of the page is the whole page
   QVERIFY(Pagecover::parse("0.0000,0.0000,1.0000,1.0000", cover));
   QVERIFY(cover.isWholePage());

   const QStringList bad = {
      "", "front", "Page", "0.5,0,0.5", "0.5,0,0.5,1,0", "a,b,c,d",
      "-0.1,0,0.5,0.5", "0,0,0,0.5", "0,0,0.5,0", "0.6,0,0.5,1",
      "0,0.6,1,0.5", "0,0,1.5,1", "nan,0,0.5,0.5", "0,0,inf,0.5",
      "0,0,0.00001,0.5",
   };
   for (const QString &str : bad) {
      cover = Pagecover::wholePage();
      QVERIFY2(!Pagecover::parse(str, cover), qPrintable(str));
      QVERIFY(cover.isNull());
   }
}

void TestPagecover::testFormat()
{
   QCOMPARE(Pagecover().toString(), QString());
   QVERIFY(Pagecover().isNull());
   QCOMPARE(Pagecover().box(), QRectF());
   QCOMPARE(Pagecover::wholePage().toString(), QString("page"));

   Pagecover cover = Pagecover::fromBox(QRectF(0.25, 0.125, 0.5, 0.375));
   QCOMPARE(cover.toString(), QString("0.2500,0.1250,0.5000,0.3750"));
   QCOMPARE(parsed(cover.toString()), cover);

   // the box is kept to the page, and rounded
   cover = Pagecover::fromBox(QRectF(0.5, 0.5, 1, 1));
   QCOMPARE(cover.toString(), QString("0.5000,0.5000,0.5000,0.5000"));
   cover = Pagecover::fromBox(QRectF(1.0 / 3, 0, 1.0 / 3, 1));
   QCOMPARE(cover.toString(), QString("0.3333,0.0000,0.3334,1.0000"));

   // a box given backwards is the same box
   QCOMPARE(Pagecover::fromBox(QRectF(0.75, 0.5, -0.5, -0.25)),
            Pagecover::fromBox(QRectF(0.25, 0.25, 0.5, 0.25)));

   // the whole page, or more, is the whole page; nothing is no cover
   QVERIFY(Pagecover::fromBox(QRectF(0, 0, 1, 1)).isWholePage());
   QVERIFY(Pagecover::fromBox(QRectF(-1, -1, 3, 3)).isWholePage());
   QVERIFY(Pagecover::fromBox(QRectF()).isNull());
   QVERIFY(Pagecover::fromBox(QRectF(0.5, 0.5, 0.00001, 0.5)).isNull());
   QVERIFY(Pagecover::fromBox(QRectF(2, 0, 1, 1)).isNull());

   QVERIFY(Pagecover::wholePage() != Pagecover());
   QVERIFY(cover != Pagecover::fromBox(QRectF(0.5, 0.5, 0.25, 0.5)));
}

void TestPagecover::testProps()
{
   QMap<QString, QString> props;
   props["name"] = "jacket";

   QVERIFY(Pagecover::fromProps(props).isNull());

   Pagecover cover = Pagecover::fromBox(QRectF(0.5, 0, 0.5, 1));
   cover.toProps(props);
   QCOMPARE(props.value("cover"), QString("0.5000,0.0000,0.5000,1.0000"));
   QCOMPARE(Pagecover::fromProps(props), cover);
   QCOMPARE(props.value("name"), QString("jacket"));

   // one which cannot be read is no cover
   props["cover"] = "front";
   QVERIFY(Pagecover::fromProps(props).isNull());

   // no cover takes it out, leaving the others
   Pagecover().toProps(props);
   QVERIFY(!props.contains("cover"));
   QCOMPARE(props.size(), 1);
}

void TestPagecover::testTransform()
{
   const Pagecover cover = parsed("0.1000,0.2000,0.3000,0.4000");

   QCOMPARE(cover.transformed(File::Transform_rotate90).toString(),
            QString("0.4000,0.1000,0.4000,0.3000"));
   QCOMPARE(cover.transformed(File::Transform_rotate180).toString(),
            QString("0.6000,0.4000,0.3000,0.4000"));
   QCOMPARE(cover.transformed(File::Transform_rotate270).toString(),
            QString("0.2000,0.6000,0.4000,0.3000"));
   QCOMPARE(cover.transformed(File::Transform_hflip).toString(),
            QString("0.6000,0.2000,0.3000,0.4000"));
   QCOMPARE(cover.transformed(File::Transform_vflip).toString(),
            QString("0.1000,0.4000,0.3000,0.4000"));

   // each is undone exactly by its opposite, and four turns go round
   const Pagecover odd = Pagecover::fromBox(QRectF(1.0 / 3, 1.0 / 7,
                                                   0.2, 0.6));
   for (File::e_transform op : {File::Transform_rotate90,
                                File::Transform_rotate180,
                                File::Transform_rotate270,
                                File::Transform_hflip,
                                File::Transform_vflip}) {
      QCOMPARE(odd.transformed(op).transformed(File::transformInverse(op)),
               odd);
   }
   Pagecover turned = odd;
   for (int i = 0; i < 4; i++)
      turned = turned.transformed(File::Transform_rotate90);
   QCOMPARE(turned, odd);

   // the whole page, and no cover, stay as they are
   QVERIFY(Pagecover::wholePage().transformed(File::Transform_rotate90)
              .isWholePage());
   QVERIFY(Pagecover().transformed(File::Transform_hflip).isNull());
}

//! the bounds of what is black on a page
static QRect black_bounds(const QImage &image)
{
   QRect bounds;

   for (int y = 0; y < image.height(); y++)
      for (int x = 0; x < image.width(); x++)
         if (qGray(image.pixel(x, y)) < 128)
            bounds |= QRect(x, y, 1, 1);
   return bounds;
}

void TestPagecover::testTransformImage()
{
   // a wide page, with the box blacked out
   QImage image(200, 100, QImage::Format_RGB32);
   const Pagecover cover = parsed("0.1000,0.2000,0.3000,0.4000");
   const QRect rect = Imagetrim::areaToRect(image.size(), cover.box());

   QCOMPARE(rect, QRect(20, 20, 60, 40));
   image.fill(Qt::white);
   QPainter(&image).fillRect(rect, Qt::black);

   for (File::e_transform op : {File::Transform_rotate90,
                                File::Transform_rotate180,
                                File::Transform_rotate270,
                                File::Transform_hflip,
                                File::Transform_vflip}) {
      QImage out = File::transformImage(image, op);
      QRect expect = Imagetrim::areaToRect(out.size(),
                                           cover.transformed(op).box());

      QVERIFY2(black_bounds(out) == expect,
               qPrintable(File::transformName(op)));
   }
}

void TestPagecover::testCrop()
{
   const Pagecover cover = parsed("0.2500,0.2500,0.5000,0.5000");

   // keeping the left half keeps the left half of the box
   QCOMPARE(cover.cropped(QRectF(0, 0, 0.5, 1)).toString(),
            QString("0.5000,0.2500,0.5000,0.5000"));

   // keeping the middle of the page makes the box bigger on it
   QCOMPARE(cover.cropped(QRectF(0.125, 0.125, 0.75, 0.75)).toString(),
            QString("0.1667,0.1667,0.6666,0.6666"));

   // a jacket's front cover, on the right, cut down to the cover
   const Pagecover front = parsed("0.5500,0.0000,0.4500,1.0000");
   QVERIFY(front.cropped(QRectF(0.55, 0, 0.45, 1)).isWholePage());
   QVERIFY(front.cropped(QRectF(0.5, 0, 0.5, 1)).isBox());

   // a box all outside what is kept, or just touching it, goes
   QVERIFY(front.cropped(QRectF(0, 0, 0.5, 1)).isNull());
   QVERIFY(front.cropped(QRectF(0, 0, 0.55, 1)).isNull());
   QVERIFY(cover.cropped(QRectF(0, 0.8, 1, 0.2)).isNull());

   // the whole page stays the whole page, and no cover stays none
   QVERIFY(Pagecover::wholePage().cropped(QRectF(0, 0, 0.5, 0.5))
              .isWholePage());
   QVERIFY(Pagecover().cropped(QRectF(0, 0, 0.5, 0.5)).isNull());

   // an empty area leaves it alone
   QCOMPARE(cover.cropped(QRectF()), cover);
}

void TestPagecover::testUncrop()
{
   const Pagecover cover = parsed("0.2500,0.2500,0.5000,0.5000");
   const QRectF middle(0.125, 0.125, 0.75, 0.75);

   // all of the box was kept, so it comes back as it was
   Pagecover back = cover.cropped(middle).uncropped(middle);
   QCOMPARE(back.toString(), QString("0.2500,0.2500,0.5000,0.5000"));

   // only what was kept of it comes back
   const QRectF left(0, 0, 0.5, 1);
   back = cover.cropped(left).uncropped(left);
   QCOMPARE(back.toString(), QString("0.2500,0.2500,0.2500,0.5000"));

   // the whole of a page cropped is the part kept, once uncropped
   QVERIFY(Pagecover::wholePage().uncropped(left).isWholePage());
   QVERIFY(Pagecover().uncropped(left).isNull());
}
