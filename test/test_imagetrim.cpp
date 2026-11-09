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
#include "test_imagetrim.h"

//! a white page with a black block of print where given
static QImage page_with_print(QSize size, QRect print)
{
   QImage image(size, QImage::Format_RGB32);

   image.fill(Qt::white);
   QPainter(&image).fillRect(print, Qt::black);
   return image;
}

void TestImagetrim::testMargins()
{
   QImage image = page_with_print(QSize(800, 1000),
                                  QRect(100, 200, 500, 400));
   QRect rect = Imagetrim::contentBounds(image);

   // padding is 1% of the longer side, so 10 pixels
   QVERIFY2(qAbs(rect.left() - 90) <= 1,
            qPrintable(QString::number(rect.left())));
   QVERIFY(qAbs(rect.top() - 190) <= 1);
   QVERIFY(qAbs(rect.right() - 609) <= 1);
   QVERIFY(qAbs(rect.bottom() - 609) <= 1);
}

void TestImagetrim::testBackground()
{
   // black surround on all sides, with print well inside the sheet
   QImage image(800, 1000, QImage::Format_RGB32);

   image.fill(Qt::black);
   {
      QPainter paint(&image);

      paint.fillRect(QRect(50, 40, 700, 900), Qt::white);
      paint.fillRect(QRect(200, 300, 300, 200), Qt::black);
   }

   // the white margins of the sheet are kept: only the surround goes
   QCOMPARE(Imagetrim::contentBounds(image), QRect(50, 40, 700, 900));

   // a band on one side only leaves the other sides fitting the print
   image.fill(Qt::white);
   {
      QPainter paint(&image);

      paint.fillRect(QRect(0, 0, 60, 1000), Qt::black);
      paint.fillRect(QRect(300, 300, 200, 200), Qt::black);
   }
   QRect rect = Imagetrim::contentBounds(image);

   QCOMPARE(rect.left(), 60);
   QVERIFY(qAbs(rect.top() - 290) <= 1);
   QVERIFY(qAbs(rect.right() - 509) <= 1);
}

void TestImagetrim::testColourPage()
{
   // the print covers most of the page, in a mid-tone
   QImage image(800, 1000, QImage::Format_RGB32);

   image.fill(Qt::white);
   QPainter(&image).fillRect(QRect(60, 80, 680, 840), QColor(100, 60, 140));

   QRect rect = Imagetrim::contentBounds(image);

   QVERIFY(rect.contains(QRect(60, 80, 680, 840)));
   QVERIFY(60 - rect.left() <= 12);
   QVERIFY(rect.bottom() - 919 <= 12);
}

void TestImagetrim::testBlank()
{
   QImage image(400, 500, QImage::Format_RGB32);

   image.fill(Qt::white);
   QVERIFY(Imagetrim::contentBounds(image).isNull());
   QVERIFY(Imagetrim::contentBounds(QImage()).isNull());
}

void TestImagetrim::testDust()
{
   QImage image = page_with_print(QSize(800, 1000),
                                  QRect(300, 300, 200, 200));

   // single specks out in the margins
   image.setPixel(10, 10, qRgb(0, 0, 0));
   image.setPixel(780, 950, qRgb(0, 0, 0));

   QRect rect = Imagetrim::contentBounds(image);

   QVERIFY(rect.left() > 250);
   QVERIFY(rect.bottom() < 550);
}

void TestImagetrim::testLargePage()
{
   QImage image = page_with_print(QSize(2480, 3508),
                                  QRect(400, 600, 1600, 2300));
   QRect rect = Imagetrim::contentBounds(image);

   // the print is all kept, with a margin no more than twice the padding
   QVERIFY(rect.contains(QRect(400, 600, 1600, 2300)));
   QVERIFY(400 - rect.left() <= 70);
   QVERIFY(rect.bottom() - 2899 <= 70);
}

void TestImagetrim::testMono()
{
   QImage image(800, 1000, QImage::Format_Mono);

   image.setColor(0, qRgb(0, 0, 0));
   image.setColor(1, qRgb(255, 255, 255));
   image.fill(1);
   for (int y = 300; y < 500; y++)
      for (int x = 200; x < 400; x++)
         image.setPixel(x, y, 0);

   QRect rect = Imagetrim::contentBounds(image);

   QVERIFY(rect.contains(QRect(200, 300, 200, 200)));
   QVERIFY(rect.right() < 450);
}

void TestImagetrim::testAreaToRect()
{
   QSize size(1000, 2000);

   QCOMPARE(Imagetrim::areaToRect(size, QRectF(0.1, 0.25, 0.5, 0.5)),
            QRect(100, 500, 500, 1000));

   // rounds outwards
   QCOMPARE(Imagetrim::areaToRect(size, QRectF(0.1005, 0, 0.1, 1)),
            QRect(100, 0, 101, 2000));

   // never leaves the page or vanishes
   QCOMPARE(Imagetrim::areaToRect(size, QRectF(1, 1, 0, 0)),
            QRect(999, 1999, 1, 1));

   // and comes back again
   QRect rect(100, 500, 500, 1000);
   QCOMPARE(Imagetrim::areaToRect(size, Imagetrim::rectToArea(size, rect)),
            rect);
}

void TestImagetrim::testAreaValid()
{
   QVERIFY(Imagetrim::areaValid(QRectF(0, 0, 1, 1)));
   QVERIFY(Imagetrim::areaValid(QRectF(0.2, 0.2, 0.5, 0.5)));
   QVERIFY(!Imagetrim::areaValid(QRectF(0.2, 0.2, 0, 0.5)));
   QVERIFY(!Imagetrim::areaValid(QRectF(-0.1, 0, 0.5, 0.5)));
   QVERIFY(!Imagetrim::areaValid(QRectF(0.8, 0, 0.5, 0.5)));
}

void TestImagetrim::testCropRectNoChange()
{
   QImage image = page_with_print(QSize(400, 500), QRect(100, 100, 200, 200));

   QVERIFY(Imagetrim::cropRect(image, QRectF(0, 0, 1, 1)).isNull());
   QCOMPARE(Imagetrim::cropRect(image, QRectF(0, 0, 0.5, 0.5)),
            QRect(0, 0, 200, 250));
   QVERIFY(!Imagetrim::cropRect(image, QRectF()).isNull());

   image.fill(Qt::white);
   QVERIFY(Imagetrim::cropRect(image, QRectF()).isNull());
}
