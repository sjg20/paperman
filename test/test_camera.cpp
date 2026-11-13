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

#include <QBuffer>
#include <QSignalSpy>
#include <QTest>

#include "../camera.h"
#include "test_camera.h"

static FakeCamera::Spec fakeSpec(const QString &id = "fake0")
{
   FakeCamera::Spec spec;

   spec.id = id;
   spec.name = "Fake camera " + id;
   spec.sizes = {QSize(320, 240), QSize(1600, 1200), QSize(800, 600),
                 QSize(1280, 720)};
   return spec;
}

QByteArray TestCamera::withoutTables(const QByteArray &jpeg)
{
   QByteArray out = jpeg.left(2);
   int pos = 2;

   // copy each segment up to the picture's data, but for the tables
   while (pos + 4 <= jpeg.size()) {
      int marker = (unsigned char)jpeg[pos + 1];
      int len = 2 + ((unsigned char)jpeg[pos + 2] << 8 |
                     (unsigned char)jpeg[pos + 3]);

      if (marker == 0xda)
         break;
      if (marker != 0xc4)
         out += jpeg.mid(pos, len);
      pos += len;
   }
   return out + jpeg.mid(pos);
}

void TestCamera::init()
{
   FakeCamera::use({});
}

void TestCamera::cleanup()
{
   FakeCamera::use({});
}

void TestCamera::testJpegInfo()
{
   QByteArray jpeg = FakeCamera::makeJpeg(QSize(200, 120), 1);
   QSize size;
   int depth;
   bool has_dht;

   QCOMPARE(cameraJpegInfo(jpeg, size, depth, has_dht), QString());
   QCOMPARE(size, QSize(200, 120));
   QCOMPARE(depth, 24);
   QVERIFY(has_dht);

   // a grey picture
   QImage grey(64, 48, QImage::Format_Grayscale8);
   QByteArray grey_jpeg;
   QBuffer buf(&grey_jpeg);

   grey.fill(200);
   QVERIFY(buf.open(QIODevice::WriteOnly));
   QVERIFY(grey.save(&buf, "JPG"));
   QCOMPARE(cameraJpegInfo(grey_jpeg, size, depth, has_dht), QString());
   QCOMPARE(size, QSize(64, 48));
   QCOMPARE(depth, 8);

   // not a JPEG, nothing at all, and ones which stop early
   QVERIFY(!cameraJpegInfo("not a jpeg at all", size, depth,
                           has_dht).isEmpty());
   QVERIFY(!cameraJpegInfo(QByteArray(), size, depth, has_dht).isEmpty());
   QVERIFY(!cameraJpegInfo(jpeg.left(40), size, depth, has_dht).isEmpty());
   QVERIFY(!cameraJpegInfo(jpeg.left(jpeg.size() - 2), size, depth,
                           has_dht).isEmpty());
   QVERIFY(!cameraJpegInfo(jpeg.left(jpeg.size() / 2), size, depth,
                           has_dht).isEmpty());

   // just the start and end markers
   QVERIFY(!cameraJpegInfo(QByteArray::fromHex("ffd8ffd9"), size, depth,
                           has_dht).isEmpty());
}

void TestCamera::testStillAsSent()
{
   QByteArray jpeg = FakeCamera::makeJpeg(QSize(200, 120), 2);
   QByteArray out;

   QCOMPARE(cameraStill(jpeg, out), QString());
   QCOMPARE(out, jpeg);

   // the buffer it arrives in may be longer than the picture
   QCOMPARE(cameraStill(jpeg + QByteArray(100, '\0'), out), QString());
   QCOMPARE(out, jpeg);

   // one which is cut short is refused, and nothing is given
   QVERIFY(!cameraStill(jpeg.left(jpeg.size() - 10), out).isEmpty());
   QVERIFY(out.isEmpty());
}

void TestCamera::testStillTables()
{
   QByteArray jpeg = FakeCamera::makeJpeg(QSize(200, 120), 3);
   QByteArray bare = withoutTables(jpeg);
   QByteArray out;
   QSize size;
   int depth;
   bool has_dht;

   QVERIFY(bare.size() < jpeg.size());
   QCOMPARE(cameraJpegInfo(bare, size, depth, has_dht), QString());
   QCOMPARE(size, QSize(200, 120));
   QVERIFY(!has_dht);

   /* The JPEG was written with the standard tables, as an encoder does
      unless asked to work out its own, so putting them back gives the
      file there was */
   QCOMPARE(cameraStill(bare, out), QString());
   QCOMPARE(out, jpeg);
   QCOMPARE(cameraJpegInfo(out, size, depth, has_dht), QString());
   QVERIFY(has_dht);

   QImage image = QImage::fromData(out, "JPG");

   QCOMPARE(image.size(), QSize(200, 120));
   QCOMPARE(image, QImage::fromData(jpeg, "JPG"));

   // the picture's own data is untouched
   QVERIFY(out.endsWith(bare.right(bare.size() - 700)));
}

void TestCamera::testPreviewSize()
{
   QList<QSize> sizes = fakeSpec().sizes;

   // the smallest of the shape which is wide enough
   QCOMPARE(Camera::previewSize(sizes, QSize(1600, 1200)),
            QSize(800, 600));
   QCOMPARE(Camera::previewSize(sizes, QSize(1280, 720)),
            QSize(1280, 720));

   // none of that shape, so the smallest which is wide enough
   QCOMPARE(Camera::previewSize(sizes, QSize(1000, 1000)),
            QSize(800, 600));
   QCOMPARE(Camera::previewSize(sizes, QSize()), QSize(800, 600));

   // none wide enough, so the largest
   QCOMPARE(Camera::previewSize({QSize(160, 120), QSize(320, 240)},
                                QSize(320, 240)), QSize(320, 240));
   QVERIFY(!Camera::previewSize({}, QSize(320, 240)).isValid());
}

void TestCamera::testList()
{
   // with the fake cameras in use and none offered, there are none
   QVERIFY(FakeCamera::inUse());
   QVERIFY(Camera::list().isEmpty());
   QVERIFY(!Camera::present());
   QVERIFY(!Camera::create("fake0"));

   FakeCamera::use({fakeSpec("fake0"), fakeSpec("fake1")});

   QList<CameraInfo> list = Camera::list();

   QCOMPARE(list.size(), 2);
   QVERIFY(Camera::present());
   QCOMPARE(list[0].id, QString("fake0"));
   QCOMPARE(list[1].name, QString("Fake camera fake1"));
   QVERIFY(!Camera::create("other"));

   QScopedPointer<Camera> cam(Camera::create("fake1"));

   QVERIFY(cam);
   QVERIFY(!cam->isOpen());
   QCOMPARE(cam->open(), QString());
   QVERIFY(cam->isOpen());

   // largest first
   QCOMPARE(cam->stillSizes(),
            QList<QSize>({QSize(1600, 1200), QSize(1280, 720),
                          QSize(800, 600), QSize(320, 240)}));
   cam->close();
   QVERIFY(!cam->isOpen());
}

void TestCamera::testPreview()
{
   FakeCamera::use({fakeSpec()});

   QScopedPointer<Camera> cam(Camera::create("fake0"));
   QSignalSpy frames(cam.data(), &Camera::previewFrame);
   QSignalSpy failed(cam.data(), &Camera::failed);

   // not until it is open
   cam->startPreview(QSize(1600, 1200));
   QCOMPARE(failed.count(), 1);
   failed.clear();

   QCOMPARE(cam->open(), QString());
   cam->startPreview(QSize(1600, 1200));
   QTRY_VERIFY(frames.count() >= 3);

   QImage image = frames[0][0].value<QImage>();

   QCOMPARE(image.size(), QSize(800, 600));
   QVERIFY(image != frames[1][0].value<QImage>());

   // a still of another shape changes the shape of them
   cam->startPreview(QSize(1280, 720));
   frames.clear();
   QTRY_VERIFY(frames.count() >= 1);
   QCOMPARE(frames[0][0].value<QImage>().size(), QSize(1280, 720));

   cam->stopPreview();
   frames.clear();
   QTest::qWait(100);
   QCOMPARE(frames.count(), 0);
   QCOMPARE(failed.count(), 0);
}

void TestCamera::testCapture()
{
   FakeCamera::Spec spec = fakeSpec();
   QByteArray first = FakeCamera::makeJpeg(QSize(800, 600), 10);
   QByteArray second = FakeCamera::makeJpeg(QSize(800, 600), 11);

   // the second arrives as a camera may send it
   spec.stills = {first, withoutTables(second) + QByteArray(50, '\0')};
   spec.capture_ms = 100;
   FakeCamera::use({spec});

   QScopedPointer<Camera> cam(Camera::create("fake0"));
   QSignalSpy frames(cam.data(), &Camera::previewFrame);
   QSignalSpy captured(cam.data(), &Camera::captured);
   QSignalSpy failed(cam.data(), &Camera::failed);

   QCOMPARE(cam->open(), QString());
   cam->startPreview(QSize(800, 600));
   QTRY_VERIFY(frames.count() >= 1);

   // nothing else arrives while a still is taken
   int before = -1;

   connect(cam.data(), &Camera::captured, this, [&]() {
      if (before < 0)
         before = frames.count();
   });

   // asking again while one is being taken gives just the one
   frames.clear();
   cam->capture(QSize(800, 600));
   cam->capture(QSize(800, 600));
   QTRY_COMPARE(captured.count(), 1);
   QCOMPARE(before, 0);
   QCOMPARE(captured[0][0].toByteArray(), first);
   QTest::qWait(150);
   QCOMPARE(captured.count(), 1);

   // then the small pictures carry on
   QTRY_VERIFY(frames.count() >= 1);

   cam->capture(QSize(800, 600));
   QTRY_COMPARE(captured.count(), 2);
   QCOMPARE(captured[1][0].toByteArray(), second);

   // and round again
   cam->capture(QSize(800, 600));
   QTRY_COMPARE(captured.count(), 3);
   QCOMPARE(captured[2][0].toByteArray(), first);
   QCOMPARE(failed.count(), 0);

   // closing it while a still is on its way means none arrives
   cam->capture(QSize(800, 600));
   cam->close();
   QTest::qWait(200);
   QCOMPARE(captured.count(), 3);

   // with no stills given, it makes them at the size asked
   spec.stills.clear();
   spec.capture_ms = 0;
   FakeCamera::use({spec});
   cam.reset(Camera::create("fake0"));

   QSignalSpy made(cam.data(), &Camera::captured);
   QSize size;
   int depth;
   bool has_dht;

   QCOMPARE(cam->open(), QString());
   cam->capture(QSize(1280, 720));
   QTRY_COMPARE(made.count(), 1);
   QCOMPARE(cameraJpegInfo(made[0][0].toByteArray(), size, depth,
                           has_dht), QString());
   QCOMPARE(size, QSize(1280, 720));
}

void TestCamera::testFailures()
{
   FakeCamera::Spec spec = fakeSpec();

   spec.open_err = "It is in a drawer";
   FakeCamera::use({spec});

   QScopedPointer<Camera> cam(Camera::create("fake0"));

   QCOMPARE(cam->open(), QString("It is in a drawer"));
   QVERIFY(!cam->isOpen());

   // a still which cannot be taken
   spec.open_err.clear();
   spec.capture_err = "The lens cap is on";
   FakeCamera::use({spec});
   cam.reset(Camera::create("fake0"));

   QSignalSpy captured(cam.data(), &Camera::captured);
   QSignalSpy failed(cam.data(), &Camera::failed);

   QCOMPARE(cam->open(), QString());
   cam->capture(QSize(800, 600));
   QTRY_COMPARE(failed.count(), 1);
   QCOMPARE(failed[0][0].toString(), QString("The lens cap is on"));

   // and one which arrives damaged
   spec.capture_err.clear();
   spec.stills = {FakeCamera::makeJpeg(QSize(800, 600), 1).left(5000)};
   FakeCamera::use({spec});
   cam.reset(Camera::create("fake0"));

   QSignalSpy captured2(cam.data(), &Camera::captured);
   QSignalSpy failed2(cam.data(), &Camera::failed);

   QCOMPARE(cam->open(), QString());
   cam->capture(QSize(800, 600));
   QTRY_COMPARE(failed2.count(), 1);
   QCOMPARE(captured2.count(), 0);
   QCOMPARE(captured.count(), 0);
}
