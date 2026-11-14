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

#include <cmath>

#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QTest>

#include "../pagefind.h"
#include "test_pagefind.h"

static const QColor PAPER(245, 242, 235);

//! a sheet seen turned a little and from one side, in a 1600x1200 picture
static QPolygonF sheetQuad()
{
   return QPolygonF({QPointF(330, 150), QPointF(1250, 190),
                     QPointF(1300, 1040), QPointF(270, 1000)});
}

//! an open book lying nearly square, in a 1600x1200 picture
static QPolygonF bookQuad()
{
   return QPolygonF({QPointF(200, 250), QPointF(1400, 230),
                     QPointF(1420, 1000), QPointF(190, 1010)});
}

QImage TestPagefind::mat(const QSize &size)
{
   QImage image(size, QImage::Format_RGB32);
   quint32 rand = 99;

   // the same texture every time
   for (int y = 0; y < size.height(); y++) {
      QRgb *line = (QRgb *)image.scanLine(y);

      for (int x = 0; x < size.width(); x++) {
         rand = rand * 1664525u + 1013904223u;

         int v = 24 + ((rand >> 16) % 12);

         line[x] = qRgb(v, v, v + 3);
      }
   }
   return image;
}

void TestPagefind::drawPage(QImage &image, const QPolygonF &quad,
                            const QSize &size, double gutter)
{
   int pw = size.width(), ph = size.height();
   QPolygonF flat({QPointF(0, 0), QPointF(pw, 0), QPointF(pw, ph),
                   QPointF(0, ph)});
   QTransform map;

   QVERIFY(QTransform::quadToQuad(flat, quad, map));

   QPainter p(&image);

   p.setRenderHint(QPainter::Antialiasing);
   p.setTransform(map);
   p.setPen(Qt::NoPen);
   p.fillRect(QRectF(0, 0, pw, ph), PAPER);

   // lines of 'text', in a column on each page if there are two
   int cols = gutter > 0 ? 2 : 1;

   for (int col = 0; col < cols; col++) {
      double from = col ? gutter * pw : 0;
      double to = cols == 2 && !col ? gutter * pw : pw;
      int row = 0;

      for (int y = 130; y < ph - 130; y += 34, row++)
         p.fillRect(QRectF(from + 70, y, to - from - 140 - (row * 37) % 150,
                           11), QColor(40, 40, 45));
   }

   if (gutter > 0) {
      double gx = gutter * pw;
      QLinearGradient shade(gx - 10, 0, gx + 10, 0);

      shade.setColorAt(0, PAPER);
      shade.setColorAt(0.5, QColor(70, 68, 66));
      shade.setColorAt(1, PAPER);
      p.fillRect(QRectF(gx - 10, 0, 20, ph), shade);
   }
   p.fillRect(QRectF(40, 40, 40, 40), QColor(230, 20, 20));
   p.fillRect(QRectF(pw - 80, ph - 80, 40, 40), QColor(20, 20, 230));
}

QPointF TestPagefind::markAt(const QImage &image, bool red)
{
   double sx = 0, sy = 0;
   int count = 0;

   for (int y = 0; y < image.height(); y++)
      for (int x = 0; x < image.width(); x++) {
         QRgb pix = image.pixel(x, y);
         int strong = red ? qRed(pix) : qBlue(pix);
         int weak = red ? qBlue(pix) : qRed(pix);

         if (strong > 170 && weak < 90 && qGreen(pix) < 90) {
            sx += x + 0.5;
            sy += y + 0.5;
            count++;
         }
      }
   return count ? QPointF(sx / count, sy / count) : QPointF(-1, -1);
}

double TestPagefind::cornerError(const QPolygonF &found,
                                 const QPolygonF &want)
{
   double worst = 0;

   if (found.size() != 4)
      return 1e9;
   for (int i = 0; i < 4; i++)
      worst = qMax(worst, QLineF(found[i], want[i]).length());
   return worst;
}

void TestPagefind::testSheet()
{
   QImage image = mat(QSize(1600, 1200));
   QPolygonF quad = sheetQuad();

   drawPage(image, quad, QSize(900, 850));

   Pagefind::Result res = Pagefind::find(image, Pagefind::Mode_one);

   QVERIFY(res.found);
   QVERIFY(res.flat);
   QVERIFY(res.confidence > 0.95);
   QVERIFY2(cornerError(res.outline, quad) <= 4,
            qPrintable(QString::number(cornerError(res.outline, quad))));
   QCOMPARE(res.pages.size(), 1);
   QCOMPARE(res.pages[0], res.outline);
   QVERIFY(!res.gutter);

   // left to choose, a sheet is one page
   res = Pagefind::find(image, Pagefind::Mode_auto);
   QVERIFY(res.found);
   QCOMPARE(res.pages.size(), 1);

   // grey works as colour does
   res = Pagefind::find(image.convertToFormat(QImage::Format_Grayscale8),
                        Pagefind::Mode_one);
   QVERIFY(res.found);
   QVERIFY(cornerError(res.outline, quad) <= 4);

   // and so does a large picture, which is looked through at a coarser
   // scale: the corners are as good in proportion
   QImage large = mat(QSize(4800, 3600));
   QPolygonF large_quad;

   for (const QPointF &pt : quad)
      large_quad << pt * 3;
   drawPage(large, large_quad, QSize(2700, 2550));
   res = Pagefind::find(large, Pagefind::Mode_one);
   QVERIFY(res.found);
   QVERIFY(res.flat);
   QVERIFY2(cornerError(res.outline, large_quad) <= 10,
            qPrintable(QString::number(cornerError(res.outline,
                                                   large_quad))));
}

void TestPagefind::testSheetRectified()
{
   QImage image = mat(QSize(1600, 1200));
   QPolygonF quad = sheetQuad();

   drawPage(image, quad, QSize(900, 850));

   Pagefind::Result res;
   QList<QImage> pages = Pagefind::pages(image, Pagefind::Mode_one, &res);

   QVERIFY(res.found);
   QCOMPARE(pages.size(), 1);

   // as wide as the longer of top and bottom, as tall as the longer side
   const QImage &page = pages[0];
   double want_w = qMax(QLineF(quad[0], quad[1]).length(),
                        QLineF(quad[3], quad[2]).length());
   double want_h = qMax(QLineF(quad[0], quad[3]).length(),
                        QLineF(quad[1], quad[2]).length());

   QVERIFY(qAbs(page.width() - want_w) <= 6);
   QVERIFY(qAbs(page.height() - want_h) <= 6);
   QCOMPARE(page.size(), Pagefind::rectifiedSize(res.outline));

   // the marks are where they are on the page, so it is upright and whole
   double sx = page.width() / 900.0, sy = page.height() / 850.0;
   QPointF red = markAt(page, true), blue = markAt(page, false);

   QVERIFY2(QLineF(red, QPointF(60 * sx, 60 * sy)).length() <= 5,
            qPrintable(QString("%1,%2").arg(red.x()).arg(red.y())));
   QVERIFY2(QLineF(blue, QPointF(840 * sx, 790 * sy)).length() <= 5,
            qPrintable(QString("%1,%2").arg(blue.x()).arg(blue.y())));

   // a line of 'text' is level: its left end is as far down as its right
   int row_left = -1, row_right = -1;

   for (int y = 20; y < page.height() / 2 && (row_left < 0 || row_right < 0);
        y++) {
      if (row_left < 0 && qGray(page.pixel(qRound(120 * sx), y)) < 100)
         row_left = y;
      if (row_right < 0 && qGray(page.pixel(qRound(600 * sx), y)) < 100)
         row_right = y;
   }
   QVERIFY(row_left > 0);
   QVERIFY2(qAbs(row_left - row_right) <= 2,
            qPrintable(QString("%1 %2").arg(row_left).arg(row_right)));

   // and none of the mat is left at its corners
   const QList<QPoint> inset = {QPoint(8, 8), QPoint(page.width() - 9, 8),
      QPoint(page.width() - 9, page.height() - 9),
      QPoint(8, page.height() - 9)};

   for (const QPoint &pt : inset)
      QVERIFY(qGray(page.pixel(pt)) > 200);
}

void TestPagefind::testNibbled()
{
   QImage image = mat(QSize(1600, 1200));
   QPolygonF quad = sheetQuad();

   drawPage(image, quad, QSize(900, 850));

   QPainter p(&image);

   p.setRenderHint(QPainter::Antialiasing);
   p.setPen(Qt::NoPen);

   // a shadow which eats into the left edge and one over the top
   p.setBrush(QColor(26, 26, 30));
   p.drawEllipse(QPointF(300, 560), 60, 90);
   p.drawEllipse(QPointF(700, 165), 80, 40);

   // a finger which comes in from the right of the picture to hold it
   p.setBrush(QColor(215, 170, 140));
   p.drawRoundedRect(QRectF(1180, 600, 500, 48), 24, 24);

   // and a turned-up corner
   p.setBrush(QColor(26, 26, 30));
   p.drawPolygon(QPolygonF({quad[2], quad[2] - QPointF(45, 0),
                            quad[2] - QPointF(0, 45)}));
   p.end();

   Pagefind::Result res = Pagefind::find(image, Pagefind::Mode_one);

   QVERIFY(res.found);
   QVERIFY(res.flat);
   QVERIFY2(cornerError(res.outline, quad) <= 6,
            qPrintable(QString::number(cornerError(res.outline, quad))));
}

void TestPagefind::testOffEdge()
{
   QImage image = mat(QSize(1600, 1200));

   // its foot is below the bottom of the picture
   QPolygonF quad({QPointF(330, 150), QPointF(1250, 190),
                   QPointF(1300, 1340), QPointF(270, 1300)});

   drawPage(image, quad, QSize(900, 1150));

   Pagefind::Result res = Pagefind::find(image, Pagefind::Mode_one);

   QVERIFY(res.found);
   QVERIFY(res.flat);

   // the top is found as ever
   QVERIFY(QLineF(res.outline[0], quad[0]).length() <= 4);
   QVERIFY(QLineF(res.outline[1], quad[1]).length() <= 4);

   // and the sides run on down to the picture's edge
   QPointF left, right;
   QLineF foot(0, 1200, 1600, 1200);

   QLineF(quad[0], quad[3]).intersects(foot, &left);
   QLineF(quad[1], quad[2]).intersects(foot, &right);
   QVERIFY2(QLineF(res.outline[3], left).length() <= 5,
            qPrintable(QString("%1,%2").arg(res.outline[3].x())
                       .arg(res.outline[3].y())));
   QVERIFY(QLineF(res.outline[2], right).length() <= 5);
}

void TestPagefind::testBook()
{
   QImage image = mat(QSize(1600, 1200));
   QPolygonF quad = bookQuad();

   // the gutter is not quite in the middle
   drawPage(image, quad, QSize(1200, 760), 0.46);

   Pagefind::Result res;
   QList<QImage> pages = Pagefind::pages(image, Pagefind::Mode_auto, &res);

   QVERIFY(res.found);
   QVERIFY(res.flat);
   QVERIFY(cornerError(res.outline, quad) <= 4);
   QVERIFY(res.gutter);
   QVERIFY2(qAbs(res.gutter_top - 0.46) <= 0.01,
            qPrintable(QString::number(res.gutter_top)));
   QVERIFY2(qAbs(res.gutter_bottom - 0.46) <= 0.01,
            qPrintable(QString::number(res.gutter_bottom)));

   // two pages which meet at the gutter, the left one first
   QCOMPARE(res.pages.size(), 2);
   QCOMPARE(res.pages[0][0], res.outline[0]);
   QCOMPARE(res.pages[0][3], res.outline[3]);
   QCOMPARE(res.pages[1][1], res.outline[1]);
   QCOMPARE(res.pages[1][2], res.outline[2]);
   QCOMPARE(res.pages[0][1], res.pages[1][0]);
   QCOMPARE(res.pages[0][2], res.pages[1][3]);

   QPointF top = quad[0] + (quad[1] - quad[0]) * 0.46;

   QVERIFY(QLineF(res.pages[0][1], top).length() <= 14);

   // each comes out at its share of the width, with its own mark
   QCOMPARE(pages.size(), 2);

   double whole = Pagefind::rectifiedSize(res.outline).width();

   QVERIFY(qAbs(pages[0].width() - whole * 0.46) <= 16);
   QVERIFY(qAbs(pages[1].width() - whole * 0.54) <= 16);
   QVERIFY(qAbs(pages[0].height() - pages[1].height()) <= 16);
   QVERIFY(markAt(pages[0], true).x() > 0);
   QVERIFY(markAt(pages[0], false).x() < 0);
   QVERIFY(markAt(pages[1], false).x() > 0);
   QVERIFY(markAt(pages[1], true).x() < 0);

   QPointF red = markAt(pages[0], true);
   double scale = pages[0].height() / 760.0;

   QVERIFY2(QLineF(red, QPointF(60 * scale, 60 * scale)).length() <= 6,
            qPrintable(QString("%1,%2").arg(red.x()).arg(red.y())));
}

void TestPagefind::testModes()
{
   QImage book = mat(QSize(1600, 1200));
   QImage sheet = mat(QSize(1600, 1200));
   QImage tall = mat(QSize(1200, 1600));

   drawPage(book, bookQuad(), QSize(1200, 760), 0.5);
   drawPage(sheet, bookQuad(), QSize(1200, 760));

   // a page which is taller than it is wide, with a line down it
   drawPage(tall, QPolygonF({QPointF(250, 200), QPointF(950, 210),
                             QPointF(960, 1400), QPointF(240, 1390)}),
            QSize(700, 1190), 0.5);

   // told it is one page, a book is left whole, but its gutter is noted
   Pagefind::Result res = Pagefind::find(book, Pagefind::Mode_one);

   QVERIFY(res.found);
   QCOMPARE(res.pages.size(), 1);
   QVERIFY(res.gutter);

   // told there are two, a sheet with no gutter is cut down the middle
   res = Pagefind::find(sheet, Pagefind::Mode_two);
   QVERIFY(res.found);
   QVERIFY(!res.gutter);
   QCOMPARE(res.pages.size(), 2);
   QCOMPARE(res.gutter_top, 0.5);
   QVERIFY(qAbs(Pagefind::rectifiedSize(res.pages[0]).width()
                - Pagefind::rectifiedSize(res.pages[1]).width()) <= 2);

   // left to choose, a wide sheet with no gutter is one page
   res = Pagefind::find(sheet, Pagefind::Mode_auto);
   QVERIFY(res.found);
   QCOMPARE(res.pages.size(), 1);

   // and so is a tall one, whatever runs down it
   res = Pagefind::find(tall, Pagefind::Mode_auto);
   QVERIFY(res.found);
   QCOMPARE(res.pages.size(), 1);
   res = Pagefind::find(tall, Pagefind::Mode_two);
   QCOMPARE(res.pages.size(), 2);

   // asked for the whole picture, nothing is looked for
   res = Pagefind::find(book, Pagefind::Mode_whole);
   QVERIFY(!res.found);
   QVERIFY(res.pages.isEmpty());
   QVERIFY(Pagefind::pages(book, Pagefind::Mode_whole).isEmpty());
}

void TestPagefind::testThinGutter()
{
   // a book held flat, in a picture big enough to show a thin line
   QSize size(3600, 2280);
   QPolygonF quad;

   for (const QPointF &pt : bookQuad())
      quad << pt * 3;

   for (int pass = 0; pass < 2; pass++) {
      QImage image = mat(QSize(4800, 3600));
      QPolygonF flat({QPointF(0, 0), QPointF(size.width(), 0),
                      QPointF(size.width(), size.height()),
                      QPointF(0, size.height())});
      QTransform map;

      drawPage(image, quad, size);
      QVERIFY(QTransform::quadToQuad(flat, quad, map));

      QPainter p(&image);
      quint32 rand = 7;

      p.setTransform(map);

      // the pages' inner margins, clear of the lines of 'text'
      p.fillRect(QRectF(0.47 * size.width() - 150, 90, 300,
                        size.height() - 180), PAPER);

      // upright strokes, as print has, all over the middle of the page
      for (int i = 0; i < 400; i++) {
         rand = rand * 1664525u + 1013904223u;

         int x = 1000 + (rand >> 8) % 1600;

         rand = rand * 1664525u + 1013904223u;
         p.fillRect(QRectF(x, 150 + (rand >> 8) % 1900, 5, 36),
                    QColor(40, 40, 45));
      }

      /* the line where the pages meet, which leans and has gaps: too
         thin and too faint to stand out in a small copy of the picture */
      if (!pass) {
         QPen pen(QColor(190, 188, 184), 2);

         pen.setDashPattern({12, 4});
         p.setPen(pen);
         p.drawLine(QPointF(0.47 * size.width() - 18, 0),
                    QPointF(0.47 * size.width() + 18, size.height()));
      }
      p.end();

      Pagefind::Result res = Pagefind::find(image, Pagefind::Mode_auto);

      QVERIFY(res.found);
      if (pass) {
         // print alone does not make a gutter
         QVERIFY(!res.gutter);
         QCOMPARE(res.pages.size(), 1);
         continue;
      }
      QVERIFY(res.gutter);
      QVERIFY2(qAbs(res.gutter_top - 0.465) <= 0.002,
               qPrintable(QString::number(res.gutter_top)));
      QVERIFY2(qAbs(res.gutter_bottom - 0.475) <= 0.002,
               qPrintable(QString::number(res.gutter_bottom)));
      QCOMPARE(res.pages.size(), 2);
   }
}

void TestPagefind::testCurvedBook()
{
   QImage image = mat(QSize(1600, 1200));
   QPainterPath book;

   // each page rises from the gutter and falls away to its outer edge
   book.moveTo(250, 260);
   book.quadTo(520, 120, 800, 250);
   book.quadTo(1080, 150, 1350, 280);
   book.lineTo(1370, 1010);
   book.quadTo(1080, 930, 800, 1040);
   book.quadTo(520, 900, 230, 1000);
   book.closeSubpath();

   QPainter p(&image);

   p.setRenderHint(QPainter::Antialiasing);
   p.setPen(Qt::NoPen);
   p.fillPath(book, PAPER);
   for (int y = 330; y < 880; y += 34) {
      p.fillRect(QRectF(320, y, 400, 11), QColor(40, 40, 45));
      p.fillRect(QRectF(880, y, 400, 11), QColor(40, 40, 45));
   }
   p.fillRect(QRectF(795, 250, 10, 790), QColor(70, 68, 66));
   p.end();

   Pagefind::Result res = Pagefind::find(image, Pagefind::Mode_auto);

   QVERIFY(res.found);

   /* there are no four corners to find, so it gets a box which is square
      to the picture and holds all of it */
   QVERIFY(!res.flat);

   QRectF bounds = book.boundingRect();
   QRectF box = res.outline.boundingRect();

   QCOMPARE(res.outline, QPolygonF({box.topLeft(), box.topRight(),
                                    box.bottomRight(), box.bottomLeft()}));
   QVERIFY2(qAbs(box.left() - bounds.left()) <= 6
            && qAbs(box.right() - bounds.right()) <= 6
            && qAbs(box.top() - bounds.top()) <= 6
            && qAbs(box.bottom() - bounds.bottom()) <= 6,
            qPrintable(QString("%1,%2 %3x%4").arg(box.x()).arg(box.y())
                       .arg(box.width()).arg(box.height())));

   // and is still split where the gutter is
   QVERIFY(res.gutter);
   QCOMPARE(res.pages.size(), 2);
   QVERIFY2(qAbs(res.pages[0][1].x() - 800) <= 8,
            qPrintable(QString::number(res.pages[0][1].x())));
   QVERIFY(qAbs(res.pages[0][2].x() - 800) <= 8);

   // cutting a box out trims the picture and no more
   QImage left = Pagefind::rectify(image, res.pages[0]);

   QVERIFY(qAbs(left.height() - box.height()) <= 1);
   QVERIFY(qAbs(left.width() - (800 - box.left())) <= 9);
}

double TestPagefind::curvedBook(QImage &image, const Book &book)
{
   int w = image.width(), h = image.height();
   double cx = w / 2.0, cy = h / 2.0;

   /* Work in the picture's pixels as they are at the mat, which puts the
      camera as far above it as the focal length, in pixels */
   double z = book.focal * w;
   int wide = book.width;
   double half = book.height / 2.0;

   // how high the page is, this far from the gutter
   auto high = [&](double x) {
      double t = qAbs(x) / wide;

      return book.rise * 4 * t * (1 - t);
   };

   // how far along its surface each point across it is
   QVector<double> along(wide + 2, 0);

   for (int i = 1; i < along.size(); i++)
      along[i] = along[i - 1] + std::hypot(1.0, high(i) - high(i - 1));

   for (int y = 0; y < h; y++) {
      QRgb *line = (QRgb *)image.scanLine(y);

      for (int x = 0; x < w; x++) {
         // find the point of the book which is seen here
         double px = x + 0.5 - cx, at = px, up = 0;

         for (int i = 0; i < 12; i++) {
            up = qAbs(at) <= wide ? high(at) : 0;
            at = px * (z - up) / z;
         }

         double down = (y + 0.5 - cy) * (z - up) / z - book.lower;

         if (qAbs(at) > wide) {
            double beside = qAbs(px) - wide;
            double from_mid = y + 0.5 - cy - book.lower;

            /* the pages beneath lie level with the page, as long as it
               is and a little darker, with the lines of their edges */
            if (beside <= book.block && qAbs(from_mid) <= half) {
               int v = (int)beside % 4 ? 0 : 12;

               line[x] = qRgb(214 - v, 211 - v, 205 - v);
               continue;
            }

            // the flap lies on the mat, a little shorter than the page
            if (book.flap && beside <= (book.level ? 120 : 90) &&
                from_mid <= half - 45 &&
                from_mid >= (book.level ? -half : 45 - half))
               line[x] = book.level ? PAPER.rgb() : qRgb(228, 224, 210);
            continue;
         }
         if (qAbs(down) > half)
            continue;

         int n = (int)qAbs(at);
         double s = along[n] + (along[n + 1] - along[n]) * (qAbs(at) - n);
         double mark = qAbs(down) / book.height;
         bool rule = std::fmod(s + 3, RULE) < 6 && s > RULE / 2 &&
            (mark < 0.02 || qAbs(mark - 0.36) < 0.02);
         bool across = qAbs(qAbs(down) - half / 2) < 3;

         if (qAbs(at) < 5)
            line[x] = qRgb(70, 68, 66);
         else if (rule || across)
            line[x] = qRgb(40, 40, 45);
         else
            line[x] = PAPER.rgb();
      }
   }
   return along[wide];
}

QString TestPagefind::checkFlat(const QImage &page, bool left,
                                double surface, const Book &book,
                                double within)
{
   int w = page.width(), h = page.height();
   double scale = w / surface;

   // it is as wide for its height as the page's surface is
   if (qAbs((double)w / h - surface / book.height) >
       within * surface / book.height)
      return QString("size %1x%2, surface %3").arg(w).arg(h).arg(surface);

   // the marks across it are where they are on the surface
   for (int band = 0; band < 3; band++) {
      int y = h * (band ? band == 1 ? 50 : 86 : 14) / 100;
      QVector<double> found;
      int start = -1;

      for (int x = 0; x <= w; x++) {
         bool dark = x < w && qGray(page.pixel(x, y)) < 128;

         if (dark && start < 0)
            start = x;
         if (!dark && start >= 0) {
            found << (start + x) / 2.0;
            start = -1;
         }
      }

      int count = 0;

      for (double s = RULE; s < surface - RULE; s += RULE) {
         double want = left ? w - s * scale : s * scale;
         double best = w;

         for (double x : found)
            best = qMin(best, qAbs(x - want));
         if (best > within * w)
            return QString("row %1: mark at %2 is %3 out, of %4")
               .arg(y).arg(want).arg(best).arg(w);
         count++;
      }
      if (count < 8)
         return QString("only %1 marks").arg(count);
   }

   // and the lines across it are level
   for (int n = 0; n < 2; n++) {
      double want = h * (n ? 0.75 : 0.25);

      for (int col = 1; col < 12; col++) {
         int x = w * col / 12 + RULE * scale / 2;
         double sum = 0;
         int count = 0;

         // between the marks
         if (x >= w)
            continue;
         for (int y = qMax(0, (int)(want - h * 0.08));
              y < qMin(h, (int)(want + h * 0.08)); y++)
            if (qGray(page.pixel(x, y)) < 128) {
               sum += y;
               count++;
            }
         if (count > h * 0.04)
            continue;
         if (!count || qAbs(sum / count - want) > within * h)
            return QString("column %1: line at %2 is at %3, of %4")
               .arg(x).arg(want).arg(count ? sum / count : -1).arg(h);
      }
   }
   return QString();
}

void TestPagefind::testFlatten()
{
   QImage image = mat(QSize(1600, 1200));
   Book book;

   book.flap = true;

   double surface = curvedBook(image, book);

   // the pages are a good deal wider than they look
   QVERIFY(surface > book.width * 1.05);

   Pagefind::Result res;
   QList<QImage> pages = Pagefind::pages(image, Pagefind::Mode_auto, &res,
                                         book.focal);

   QVERIFY(res.found);
   QVERIFY(!res.flat);
   QVERIFY(res.gutter);
   QCOMPARE(res.pages.size(), 2);
   QCOMPARE(res.curves.size(), 2);
   QCOMPARE(pages.size(), 2);
   for (int i = 0; i < 2; i++) {
      const Pagefind::Curve &curve = res.curves[i];

      QVERIFY(curve.traced);
      QCOMPARE(curve.top.size(), (int)Pagefind::CurvePoints);
      QCOMPARE(curve.bottom.size(), (int)Pagefind::CurvePoints);

      // the edges run from the page's outer edge to the gutter, or back
      double from = i ? 800 : 800 - book.width;

      QVERIFY2(qAbs(curve.top.first().x() - from) <= 8,
               qPrintable(QString::number(curve.top.first().x())));
      QVERIFY2(qAbs(curve.bottom.last().x() - from - book.width) <= 8,
               qPrintable(QString::number(curve.bottom.last().x())));

      // the shape to draw goes round the page, not round a box
      QCOMPARE(res.shape(i).size(), 2 * Pagefind::CurvePoints);
      QVERIFY(res.shape(i) != res.pages[i]);

      QString problem = checkFlat(pages[i], !i, surface, book, 0.03);

      QVERIFY2(problem.isEmpty(), qPrintable(problem));
   }

   /* How much larger the high part of a page looks says how high it
      is against how far away the camera is. So with the camera taken to
      be further away than it is the page is taken to rise further, and
      comes out wider; with it taken to be nearer, narrower */
   QList<QImage> far_off = Pagefind::pages(image, Pagefind::Mode_auto,
                                           nullptr, book.focal * 3);
   QList<QImage> close_by = Pagefind::pages(image, Pagefind::Mode_auto,
                                            nullptr, book.focal / 3);

   QCOMPARE(far_off.size(), 2);
   QCOMPARE(close_by.size(), 2);
   QVERIFY(far_off[0].width() > pages[0].width() * 1.1);
   QVERIFY(close_by[0].width() < pages[0].width() * 0.97);
   QVERIFY(!checkFlat(far_off[0], true, surface, book, 0.03).isEmpty());
}

void TestPagefind::testFlattenOffEdge()
{
   QImage image = mat(QSize(1600, 1000));
   Book book;

   /* the foot of each page is off the picture where the page is high,
      and seen near the gutter and the outer edge where it is low */
   book.rise = 130;
   book.lower = 500 - book.height / 2 - 35;

   double surface = curvedBook(image, book);
   Pagefind::Result res;
   QList<QImage> pages = Pagefind::pages(image, Pagefind::Mode_auto, &res,
                                         book.focal);

   QVERIFY(res.found);
   QCOMPARE(res.curves.size(), 2);
   QCOMPARE(pages.size(), 2);
   for (int i = 0; i < 2; i++) {
      const Pagefind::Curve &curve = res.curves[i];

      QVERIFY(curve.traced);

      // the foot is worked out to be below the picture's own
      double lowest = 0;

      for (const QPointF &pt : curve.bottom)
         lowest = qMax(lowest, pt.y());
      QVERIFY2(lowest > 1008 && lowest < 1035,
               qPrintable(QString::number(lowest)));

      QString problem = checkFlat(pages[i], !i, surface, book, 0.03);

      QVERIFY2(problem.isEmpty(), qPrintable(problem));
   }

   // a book which runs off the picture at head and foot is only cut out
   QImage tall = mat(QSize(1600, 1200));

   book.lower = 0;
   book.rise = 100;
   book.height = 1190;
   curvedBook(tall, book);
   pages = Pagefind::pages(tall, Pagefind::Mode_two, &res, book.focal);
   QVERIFY(res.found);
   QCOMPARE(res.curves.size(), 2);
   QVERIFY(!res.curves[0].traced);
   QVERIFY(!res.curves[1].traced);
   QCOMPARE(res.shape(0), res.pages[0]);
   QCOMPARE(pages.size(), 2);
   QCOMPARE(pages[0].size(), Pagefind::rectifiedSize(res.pages[0]));
}

void TestPagefind::testSheetEdge()
{
   for (int pass = 0; pass < 2; pass++) {
      QImage image = mat(QSize(1600, 1200));
      Book book;

      /* First the block of pages beneath, which the page's edges run on
         along at both its head and its foot. Then a flap as light as the
         page, which its head runs on along but its foot does not */
      if (pass) {
         book.flap = true;
         book.level = true;
      } else {
         book.block = 60;
      }

      double surface = curvedBook(image, book);
      Pagefind::Result res;
      QList<QImage> pages = Pagefind::pages(image, Pagefind::Mode_auto,
                                            &res, book.focal);

      QVERIFY(res.found);
      QCOMPARE(res.curves.size(), 2);
      QCOMPARE(pages.size(), 2);
      for (int i = 0; i < 2; i++) {
         const Pagefind::Curve &curve = res.curves[i];
         double outer = i ? 800 + book.width : 800 - book.width;

         QVERIFY(curve.traced);

         // both edges end at the sheet
         const QPointF &top = i ? curve.top.last() : curve.top.first();
         const QPointF &bottom = i ? curve.bottom.last()
            : curve.bottom.first();

         QVERIFY2(qAbs(top.x() - outer) <= 8,
                  qPrintable(QString("pass %1 page %2 top %3").arg(pass)
                             .arg(i).arg(top.x())));
         QVERIFY2(qAbs(bottom.x() - outer) <= 8,
                  qPrintable(QString("pass %1 page %2 bottom %3").arg(pass)
                             .arg(i).arg(bottom.x())));

         QString problem = checkFlat(pages[i], !i, surface, book, 0.03);

         QVERIFY2(problem.isEmpty(), qPrintable(problem));
      }
   }
}

void TestPagefind::testFlattenBad()
{
   QImage image = mat(QSize(400, 300));
   Pagefind::Curve curve;

   // not traced
   QVERIFY(Pagefind::flatten(image, curve).isNull());

   curve.traced = true;
   for (int i = 0; i < 8; i++) {
      curve.top << QPointF(50 + i * 40, 40);
      curve.bottom << QPointF(50 + i * 40, 260);
   }
   QVERIFY(Pagefind::flatten(QImage(), curve).isNull());
   QVERIFY(Pagefind::flatten(image, curve, 0).isNull());

   // a page which does not rise comes out as it is
   QImage page = Pagefind::flatten(image, curve);

   QCOMPARE(page.size(), QSize(280, 220));

   // grey stays grey
   page = Pagefind::flatten(image.convertToFormat(QImage::Format_Grayscale8),
                            curve);
   QCOMPARE(page.format(), QImage::Format_Grayscale8);

   // the edges must match, and have some height between them
   curve.bottom.removeLast();
   QVERIFY(Pagefind::flatten(image, curve).isNull());
   curve.bottom = curve.top;
   QVERIFY(Pagefind::flatten(image, curve).isNull());
}

void TestPagefind::testNotFound()
{
   // no picture
   QVERIFY(!Pagefind::find(QImage(), Pagefind::Mode_auto).found);

   // a bare mat
   QImage bare = mat(QSize(1600, 1200));

   QVERIFY(!Pagefind::find(bare, Pagefind::Mode_auto).found);
   QVERIFY(Pagefind::pages(bare, Pagefind::Mode_auto).isEmpty());

   // one colour all over
   QImage plain(800, 600, QImage::Format_RGB32);

   plain.fill(Qt::white);
   QVERIFY(!Pagefind::find(plain, Pagefind::Mode_one).found);

   // a page which fills the picture, so that no edge of it shows
   QImage full = mat(QSize(1600, 1200));

   drawPage(full, QPolygonF({QPointF(-200, -150), QPointF(1800, -160),
                             QPointF(1820, 1400), QPointF(-210, 1380)}),
            QSize(2000, 1550));
   QVERIFY(!Pagefind::find(full, Pagefind::Mode_one).found);

   // something too small to be a page
   QImage speck = mat(QSize(1600, 1200));

   drawPage(speck, QPolygonF({QPointF(700, 500), QPointF(900, 500),
                              QPointF(900, 640), QPointF(700, 640)}),
            QSize(200, 140));
   QVERIFY(!Pagefind::find(speck, Pagefind::Mode_one).found);
}

void TestPagefind::testRectify()
{
   QImage image(400, 300, QImage::Format_RGB32);

   for (int y = 0; y < 300; y++)
      for (int x = 0; x < 400; x++)
         image.setPixel(x, y, qRgb(x * 255 / 399, y * 255 / 299, 40));

   // the whole picture comes out as it went in
   QPolygonF whole({QPointF(0, 0), QPointF(400, 0), QPointF(400, 300),
                    QPointF(0, 300)});

   QCOMPARE(Pagefind::rectifiedSize(whole), QSize(400, 300));
   QCOMPARE(Pagefind::rectify(image, whole), image);

   // a part of it, at its own size
   QPolygonF part({QPointF(100, 50), QPointF(300, 50), QPointF(300, 250),
                   QPointF(100, 250)});
   QImage out = Pagefind::rectify(image, part);

   QCOMPARE(out.size(), QSize(200, 200));
   QCOMPARE(out.pixel(0, 0), image.pixel(100, 50));
   QCOMPARE(out.pixel(199, 199), image.pixel(299, 249));
   QCOMPARE(out, image.copy(100, 50, 200, 200));

   // a shape which narrows: as wide as its longer end
   QPolygonF narrowing({QPointF(100, 50), QPointF(300, 50),
                        QPointF(350, 250), QPointF(50, 250)});

   out = Pagefind::rectify(image, narrowing);
   QCOMPARE(out.width(), 300);
   QVERIFY(qAbs(out.height() - 206) <= 1);

   // its corners are the shape's corners
   QVERIFY(qAbs(qRed(out.pixel(0, 0)) - qRed(image.pixel(100, 50))) <= 2);
   QVERIFY(qAbs(qRed(out.pixel(299, 0)) - qRed(image.pixel(299, 50))) <= 2);
   QVERIFY(qAbs(qRed(out.pixel(0, out.height() - 1))
                - qRed(image.pixel(50, 249))) <= 2);

   // what is beyond the picture is white
   QPolygonF beyond({QPointF(-100, 0), QPointF(100, 0), QPointF(100, 100),
                     QPointF(-100, 100)});

   out = Pagefind::rectify(image, beyond);
   QCOMPARE(out.size(), QSize(200, 100));
   QCOMPARE(out.pixel(10, 50), qRgb(255, 255, 255));
   QCOMPARE(out.pixel(150, 50), image.pixel(50, 50));

   // grey stays grey
   QImage grey = image.convertToFormat(QImage::Format_Grayscale8);

   out = Pagefind::rectify(grey, part);
   QCOMPARE(out.format(), QImage::Format_Grayscale8);
   QCOMPARE(out, grey.copy(100, 50, 200, 200));

   // and another kind of picture is done as colour
   out = Pagefind::rectify(image.convertToFormat(QImage::Format_RGB888),
                           part);
   QCOMPARE(out.format(), QImage::Format_RGB32);
   QCOMPARE(out.pixel(20, 30), image.pixel(120, 80));

   // a shape which is not one gives nothing
   QVERIFY(Pagefind::rectify(image, QPolygonF({QPointF(0, 0),
                                               QPointF(10, 0),
                                               QPointF(10, 10)})).isNull());
   QVERIFY(Pagefind::rectify(image, QPolygonF({QPointF(0, 0),
      QPointF(100, 100), QPointF(100, 0), QPointF(0, 100)})).isNull());
   QVERIFY(Pagefind::rectify(QImage(), whole).isNull());
   QVERIFY(!Pagefind::rectifiedSize(QPolygonF()).isValid());
}
