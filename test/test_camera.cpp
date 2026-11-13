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

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSettings>

#include "../camera.h"
#include "../camerapages.h"
#include "../camerapanel.h"
#include "../mainwidget.h"
#include "../camerascan.h"
#include "../desktopmodel.h"
#include "../desktopview.h"
#include "../desktopwidget.h"
#include "../filemax.h"
#include "../mainwindow.h"
#include "test_camera.h"
#include "test_pagefind.h"

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

   // nothing remembered from the test before
   QSettings().remove("camera");
   QSettings().remove("camerapanel");

   // each picture is its own page, unless a test says otherwise
   QSettings().setValue("camera/pages", Pagefind::Mode_whole);
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

void TestCamera::testPush()
{
   FakeCamera::Spec spec = fakeSpec();

   spec.capture_ms = 100;
   FakeCamera::use({spec});

   QByteArray still = FakeCamera::makeJpeg(QSize(1600, 1200), 40);
   QByteArray after = FakeCamera::makeJpeg(QSize(800, 600), 41);

   // nothing is there to send it
   QVERIFY(!FakeCamera::push("fake0", still));

   QScopedPointer<Camera> cam(Camera::create("fake0"));
   QSignalSpy frames(cam.data(), &Camera::previewFrame);
   QSignalSpy captured(cam.data(), &Camera::captured);
   QSignalSpy pushed(cam.data(), &Camera::stillPushed);
   QSignalSpy followed(cam.data(), &Camera::pushFollowed);

   // nor is one which is not sending small pictures
   QCOMPARE(cam->open(), QString());
   QVERIFY(!FakeCamera::push("fake0", still));
   cam->startPreview(QSize(1600, 1200));
   QTRY_VERIFY(frames.count() >= 1);
   QVERIFY(!FakeCamera::push("other", still));

   // the still arrives as it was sent, and what follows it is not shown
   QVERIFY(FakeCamera::push("fake0", still, after));
   QTRY_COMPARE(pushed.count(), 1);
   QCOMPARE(pushed[0][0].toByteArray(), still);
   QTRY_COMPARE(followed.count(), 1);
   QCOMPARE(followed[0][0].toByteArray(), after);
   QCOMPARE(captured.count(), 0);

   QImage odd = QImage::fromData(after, "JPG");

   frames.clear();
   QTRY_VERIFY(frames.count() >= 2);
   for (int i = 0; i < frames.count(); i++)
      QVERIFY(frames[i][0].value<QImage>() != odd);
   QCOMPARE(followed.count(), 1);

   // one as a camera may send it is made whole, and the next small
   // picture follows it when none is given
   QVERIFY(FakeCamera::push("fake0", withoutTables(still) +
                            QByteArray(20, '\0')));
   QTRY_COMPARE(pushed.count(), 2);
   QCOMPARE(pushed[1][0].toByteArray(), still);
   QTRY_COMPARE(followed.count(), 2);

   // one which did not arrive whole is dropped
   QVERIFY(FakeCamera::push("fake0", still.left(still.size() / 2)));
   QTest::qWait(100);
   QCOMPARE(pushed.count(), 2);
   QCOMPARE(followed.count(), 2);

   // one sent while a still is being taken: both arrive, each once
   cam->capture(QSize(800, 600));
   QVERIFY(FakeCamera::push("fake0", still));
   QTRY_COMPARE(captured.count(), 1);
   QTRY_COMPARE(pushed.count(), 3);
   QVERIFY(captured[0][0].toByteArray() != still);
   QTRY_COMPARE(followed.count(), 3);
   QTest::qWait(100);
   QCOMPARE(captured.count(), 1);
   QCOMPARE(pushed.count(), 3);

   // nothing arrives once it is closed
   QVERIFY(FakeCamera::push("fake0", still));
   cam->close();
   QTest::qWait(100);
   QCOMPARE(pushed.count(), 3);
}

void TestCamera::setupDesk(Mainwindow *me, Desktopmodel *&model,
                           QModelIndex &parent)
{
   Desktopwidget *desktop = me->getDesktop();

   QVERIFY(!desktop->addDir(setupRepo()));
   model = desktop->getModel();
   parent = desktop->getView()->rootIndexSource();
   QVERIFY(parent.isValid());
}

void TestCamera::testFile()
{
   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   int rows = model->rowCount(parent);

   // colour, colour of another size as a camera may send it, and grey
   QImage grey(300, 200, QImage::Format_Grayscale8);
   QByteArray grey_jpeg, bare;
   QBuffer buf(&grey_jpeg);

   grey.fill(180);
   QVERIFY(buf.open(QIODevice::WriteOnly));
   QVERIFY(grey.save(&buf, "JPG"));
   QCOMPARE(cameraStill(withoutTables(FakeCamera::makeJpeg(QSize(400, 600),
                                                           2)), bare),
            QString());

   QList<QByteArray> jpegs = {FakeCamera::makeJpeg(QSize(640, 480), 1),
                              bare, grey_jpeg};
   CameraScan scan(model, parent, "book", "leaf_", 200);

   // nothing is made until there is a page
   QVERIFY(!scan.isOpen());
   QCOMPARE(model->rowCount(parent), rows);
   for (int i = 0; i < jpegs.size(); i++) {
      QVERIFY(!scan.addPage(jpegs[i]));
      QCOMPARE(scan.pageCount(), i + 1);
      QVERIFY(scan.isOpen());
      QCOMPARE(model->rowCount(parent), rows + 1);
   }

   QString fname;
   File *file = nullptr;

   QVERIFY(!scan.finish(&fname, &file));
   QVERIFY(!scan.isOpen());
   QCOMPARE(fname, QString("book.max"));
   QVERIFY(file);
   QCOMPARE(model->rowCount(parent), rows + 1);
   QCOMPARE(file->pagecount(), 3);

   // each page has its name, its size and the resolution given
   const QStringList titles = {"leaf", "leaf_2", "leaf_3"};
   const QList<QSize> sizes = {QSize(640, 480), QSize(400, 600),
                               QSize(300, 200)};
   const QList<int> depths = {24, 24, 8};
   Filemax *max = dynamic_cast<Filemax *>(file);

   QVERIFY(max);
   for (int i = 0; i < 3; i++) {
      QString title;
      QImage image;
      QSize size, true_size, dpi;
      int bpp;

      QVERIFY(!file->getPageTitle(i, title));
      QCOMPARE(title, titles[i]);
      QVERIFY(!file->getImage(i, false, image, size, true_size, bpp,
                              false));
      QCOMPARE(size, sizes[i]);
      QCOMPARE(bpp, depths[i]);
      QVERIFY(!max->getDpi(i, dpi));
      QCOMPARE(dpi, QSize(200, 200));

      // Paperman reads the page as the picture which was given
      QImage want = QImage::fromData(jpegs[i], "JPG");
      QPoint mid(size.width() / 2, size.height() / 2);

      QVERIFY(qAbs(qGray(image.pixel(mid)) - qGray(want.pixel(mid))) < 8);
      QVERIFY(qAbs(qGray(image.pixel(5, 5)) - qGray(want.pixel(5, 5))) < 8);
   }

   // and the file holds each picture, byte for byte
   QFile stored(file->pathname());

   QVERIFY(stored.open(QIODevice::ReadOnly));

   QByteArray all = stored.readAll();
   int at = 0;

   for (const QByteArray &jpeg : jpegs) {
      at = all.indexOf(jpeg, at);
      QVERIFY(at >= 0);
      at += jpeg.size();
   }

   // another stack of the same name gets a name of its own
   CameraScan again(model, parent, "book", "leaf_", 200);

   QVERIFY(!again.addPage(jpegs[0]));
   QVERIFY(!again.finish(&fname, &file));
   QVERIFY(fname != QString("book.max"));
   QCOMPARE(file->pagecount(), 1);
   QCOMPARE(model->rowCount(parent), rows + 2);
}

void TestCamera::testFileNone()
{
   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   int rows = model->rowCount(parent);
   QString dir;
   QByteArray jpeg = FakeCamera::makeJpeg(QSize(640, 480), 1);
   QString fname = "x";
   File *file = nullptr;

   // no pages: nothing to keep
   {
      CameraScan scan(model, parent, "none", "leaf_", 300);

      QVERIFY(!scan.finish(&fname, &file));
      QVERIFY(fname.isEmpty());
      QVERIFY(!file);
      QVERIFY(!scan.cancel());
      QCOMPARE(model->rowCount(parent), rows);
   }

   // a picture which is not one is refused, and makes no stack
   {
      CameraScan scan(model, parent, "bad", "leaf_", 300);

      QVERIFY(scan.addPage("this is not a picture"));
      QVERIFY(scan.addPage(jpeg.left(jpeg.size() / 2)));
      QVERIFY(!scan.isOpen());
      QCOMPARE(scan.pageCount(), 0);
      QCOMPARE(model->rowCount(parent), rows);

      // but a good one after it is kept
      QVERIFY(!scan.addPage(jpeg));
      QVERIFY(!scan.finish(&fname, &file));
      QCOMPARE(fname, QString("bad.max"));
      QCOMPARE(file->pagecount(), 1);
      QCOMPARE(model->rowCount(parent), rows + 1);
      dir = QFileInfo(file->pathname()).path();
   }

   // cancelled with pages: the stack goes
   {
      CameraScan scan(model, parent, "gone", "leaf_", 300);

      QVERIFY(!scan.addPage(jpeg));
      QVERIFY(!scan.addPage(jpeg));
      QCOMPARE(model->rowCount(parent), rows + 2);
      QVERIFY(!scan.cancel());
      QVERIFY(!scan.isOpen());
      QCOMPARE(scan.pageCount(), 0);
      QCOMPARE(model->rowCount(parent), rows + 1);
      QVERIFY(!dir.isEmpty());
      QVERIFY(!QFile::exists(dir + "/gone.max"));
      QVERIFY(QFile::exists(dir + "/bad.max"));
   }
}

Camerapanel *TestCamera::openPanel(Mainwindow *me)
{
   Mainwidget *main = Mainwidget::singleton();
   QAction *act = me->findChild<QAction *>("actionCamera");

   if (!me->isVisible()) {
      me->resize(1600, 1000);
      me->show();
      if (!QTest::qWaitForWindowExposed(me))
         return nullptr;
   }

   // as when the window becomes the active one
   QEvent activate(QEvent::WindowActivate);

   QCoreApplication::sendEvent(me, &activate);
   if (!act || !act->isEnabled())
      return nullptr;
   act->trigger();

   Camerapanel *panel = main->findChild<Camerapanel *>();

   if (!panel || !QTest::qWaitForWindowExposed(panel))
      return nullptr;
   return panel;
}

File *TestCamera::stackWithPages(Desktopmodel *model,
                                 const QModelIndex &parent, int pages)
{
   File *found = nullptr;

   for (int row = 0; row < model->rowCount(parent); row++) {
      File *file = model->getFile(model->index(row, 0, parent));

      if (file && file->pagecount() == pages) {
         if (found)
            return nullptr;
         found = file;
      }
   }
   return found;
}

void TestCamera::testSession()
{
   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   QByteArray jpeg = FakeCamera::makeJpeg(QSize(640, 480), 1);

   // with nothing taken there is nothing to keep or throw away
   QCOMPARE(main->cameraPages(), 0);
   main->cameraDone();
   main->cameraCancel();
   QCOMPARE(model->rowCount(parent), rows);

   // the first page starts a stack, and a bad one is refused
   QCOMPARE(main->cameraAddPage(jpeg), QString());
   QCOMPARE(main->cameraPages(), 1);
   QCOMPARE(model->rowCount(parent), rows + 1);
   QVERIFY(!main->cameraAddPage("not a picture").isEmpty());
   QCOMPARE(main->cameraPages(), 1);
   QCOMPARE(main->cameraAddPage(jpeg), QString());
   QCOMPARE(main->cameraPages(), 2);

   // no scan starts while the stack is open
   main->scan();
   QVERIFY(!main->isScanning());
   QCOMPARE(main->cameraPages(), 2);

   main->cameraDone();
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows + 1);
   QVERIFY(stackWithPages(model, parent, 2));

   // the next is a stack of its own, which can be thrown away
   QCOMPARE(main->cameraAddPage(jpeg), QString());
   QCOMPARE(model->rowCount(parent), rows + 2);
   main->cameraCancel();
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows + 1);
}

void TestCamera::testPanel()
{
   FakeCamera::Spec spec = fakeSpec();

   for (int i = 0; i < 3; i++)
      spec.stills << FakeCamera::makeJpeg(QSize(1600, 1200), 20 + i);
   spec.capture_ms = 150;
   FakeCamera::use({spec});

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);
   QVERIFY(panel->isVisible());

   CameraView *view = panel->findChild<CameraView *>("viewfinder");
   QComboBox *cameras = panel->findChild<QComboBox *>("camera");
   QComboBox *sizes = panel->findChild<QComboBox *>("size");
   QLabel *count = panel->findChild<QLabel *>("count");
   QLabel *status = panel->findChild<QLabel *>("status");
   QPushButton *capture = panel->findChild<QPushButton *>("capture");
   QPushButton *done = panel->findChild<QPushButton *>("done");

   QVERIFY(view && cameras && sizes && count && status && capture && done);

   // there is one camera, so none to choose, and the largest size
   QVERIFY(!cameras->isVisible());
   QCOMPARE(sizes->count(), 4);
   QCOMPARE(sizes->currentData().toSize(), QSize(1600, 1200));
   QVERIFY(sizes->currentText().startsWith("1600 x 1200"));

   // what the camera sees is shown, whole, in its own shape
   QTRY_VERIFY(view->frameCount() >= 2);
   QCOMPARE(view->frame().size(), QSize(800, 600));

   QRect shown = view->frameRect();

   QVERIFY(view->rect().contains(shown));
   QVERIFY(shown.width() == view->width() ||
           shown.height() == view->height());
   QVERIFY(qAbs(shown.width() * 3 - shown.height() * 4) <= 4);

   QCOMPARE(count->text(), QString("0 pages"));
   QVERIFY(capture->isEnabled());
   QCOMPARE(capture->text(), QString("Capture"));

   // nothing is filed until a still is taken
   QCOMPARE(model->rowCount(parent), rows);

   // the button takes one, and shows that it is doing so
   QTest::mouseClick(capture, Qt::LeftButton);
   QVERIFY(panel->busy());
   QVERIFY(!capture->isEnabled());
   QVERIFY(!done->isEnabled());
   QCOMPARE(capture->text(), QString("Capturing..."));

   // pressing again meanwhile takes no more
   QTest::mouseClick(capture, Qt::LeftButton);
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_VERIFY(!panel->busy());
   QCOMPARE(main->cameraPages(), 1);
   QCOMPARE(count->text(), QString("1 page"));
   QCOMPARE(model->rowCount(parent), rows + 1);
   QVERIFY(capture->isEnabled());
   QCOMPARE(capture->text(), QString("Capture"));
   QTest::qWait(300);
   QCOMPARE(main->cameraPages(), 1);

   // then the camera's view carries on
   int seen = view->frameCount();

   QTRY_VERIFY(view->frameCount() > seen);

   // the space bar takes one too
   QTest::keyClick(panel, Qt::Key_Space);
   QVERIFY(panel->busy());
   QTRY_COMPARE(main->cameraPages(), 2);

   // even typed at another button, which it does not press
   done->setFocus();
   QTest::keyClick(done, Qt::Key_Space);
   QVERIFY(panel->isVisible());
   QVERIFY(panel->busy());
   QTRY_COMPARE(main->cameraPages(), 3);
   QCOMPARE(count->text(), QString("3 pages"));
   QCOMPARE(status->text(), QString());
   QCOMPARE(model->rowCount(parent), rows + 1);

   // Done keeps the stack, with the stills as its pages
   QTest::mouseClick(done, Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows + 1);

   File *file = stackWithPages(model, parent, 3);

   QVERIFY(file);

   QFile stored(file->pathname());

   QVERIFY(stored.open(QIODevice::ReadOnly));

   QByteArray all = stored.readAll();
   int at = 0;

   for (const QByteArray &jpeg : spec.stills) {
      at = all.indexOf(jpeg, at);
      QVERIFY(at >= 0);
      at += jpeg.size();
   }

   // the camera is let go of, so nothing more is shown
   QCOMPARE(view->frameCount(), 0);
   QTest::qWait(100);
   QCOMPARE(view->frameCount(), 0);
}

void TestCamera::testPanelCancel()
{
   FakeCamera::use({fakeSpec()});

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);

   CameraView *view = panel->findChild<CameraView *>("viewfinder");
   QPushButton *cancel = panel->findChild<QPushButton *>("cancel");

   QVERIFY(view && cancel);
   QTRY_VERIFY(view->frameCount() >= 1);
   for (int i = 0; i < 2; i++) {
      QTest::keyClick(panel, Qt::Key_Space);
      QTRY_COMPARE(main->cameraPages(), i + 1);
   }
   QCOMPARE(model->rowCount(parent), rows + 1);

   // Cancel throws the stack away
   QTest::mouseClick(cancel, Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows);

   // with nothing taken, Done and Cancel leave nothing
   QCOMPARE(openPanel(&me), panel);
   QTRY_VERIFY(view->frameCount() >= 1);
   QTest::mouseClick(panel->findChild<QPushButton *>("done"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(model->rowCount(parent), rows);

   // the panel opens again on a new stack; closing it keeps the pages
   QCOMPARE(openPanel(&me), panel);
   QCOMPARE(panel->findChild<QLabel *>("count")->text(),
            QString("0 pages"));
   QTRY_VERIFY(view->frameCount() >= 1);
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 1);
   QTest::keyClick(panel, Qt::Key_Escape);
   QVERIFY(!panel->isVisible());
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows + 1);
   QVERIFY(stackWithPages(model, parent, 1));
}

void TestCamera::testPanelNoCamera()
{
   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   QAction *act = me.findChild<QAction *>("actionCamera");

   // the button is greyed out, and says why
   QVERIFY(act);
   QVERIFY(!openPanel(&me));
   QVERIFY(!act->isEnabled());
   QVERIFY(act->toolTip().contains("No camera"));
   QVERIFY(!main->findChild<Camerapanel *>());

   // plugging one in brings it back
   FakeCamera::use({fakeSpec()});

   QEvent activate(QEvent::WindowActivate);

   QCoreApplication::sendEvent(&me, &activate);
   QVERIFY(act->isEnabled());
   QVERIFY(act->toolTip().contains("F7"));

   // a camera which goes before the panel opens leaves it with nothing
   FakeCamera::use({});
   main->camera();

   Camerapanel *panel = main->findChild<Camerapanel *>();

   QVERIFY(panel);
   QVERIFY(panel->isVisible());

   QPushButton *capture = panel->findChild<QPushButton *>("capture");

   QVERIFY(!capture->isEnabled());
   QCOMPARE(panel->findChild<QLabel *>("status")->text(),
            QString("No camera found"));
   QCOMPARE(panel->findChild<QComboBox *>("size")->count(), 0);
   QTest::keyClick(panel, Qt::Key_Space);
   QVERIFY(!panel->busy());
   QTest::mouseClick(panel->findChild<QPushButton *>("done"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(model->rowCount(parent), rows);
}

void TestCamera::testPanelChoices()
{
   FakeCamera::Spec other = fakeSpec("fake1");

   other.sizes = {QSize(2000, 1500), QSize(1280, 720), QSize(640, 480)};
   FakeCamera::use({fakeSpec("fake0"), other});

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);

   CameraView *view = panel->findChild<CameraView *>("viewfinder");
   QComboBox *cameras = panel->findChild<QComboBox *>("camera");
   QComboBox *sizes = panel->findChild<QComboBox *>("size");

   // the first camera, at its largest
   QVERIFY(cameras->isVisible());
   QCOMPARE(cameras->count(), 2);
   QCOMPARE(cameras->currentText(), QString("Fake camera fake0"));
   QCOMPARE(sizes->currentData().toSize(), QSize(1600, 1200));
   QTRY_VERIFY(view->frameCount() >= 1);

   // choose the other, which has its own sizes
   cameras->setCurrentIndex(1);
   emit cameras->activated(1);
   QCOMPARE(sizes->count(), 3);
   QCOMPARE(sizes->currentData().toSize(), QSize(2000, 1500));
   QTRY_VERIFY(view->frameCount() >= 1);
   QCOMPARE(view->frame().size(), QSize(640, 480));

   // and a smaller size of it, whose shape the camera's view takes
   sizes->setCurrentIndex(1);
   emit sizes->activated(1);
   QTRY_COMPARE(view->frame().size(), QSize(1280, 720));

   // a still is of the size chosen
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 1);
   QTest::mouseClick(panel->findChild<QPushButton *>("done"),
                     Qt::LeftButton);

   File *file = stackWithPages(model, parent, 1);
   QImage image;
   QSize size, true_size;
   int bpp;

   QVERIFY(file);
   QVERIFY(!file->getImage(0, false, image, size, true_size, bpp, false));
   QCOMPARE(size, QSize(1280, 720));

   // both are remembered for next time
   QCOMPARE(openPanel(&me), panel);
   QCOMPARE(cameras->currentText(), QString("Fake camera fake1"));
   QCOMPARE(sizes->currentData().toSize(), QSize(1280, 720));
   QTRY_VERIFY(view->frameCount() >= 1);
   QCOMPARE(view->frame().size(), QSize(1280, 720));
   QTest::mouseClick(panel->findChild<QPushButton *>("cancel"),
                     Qt::LeftButton);
}

void TestCamera::testPanelFailure()
{
   FakeCamera::Spec spec = fakeSpec();

   spec.capture_err = "The lens cap is on";
   spec.capture_ms = 100;
   FakeCamera::use({spec});

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);

   QLabel *status = panel->findChild<QLabel *>("status");
   QPushButton *capture = panel->findChild<QPushButton *>("capture");

   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_VERIFY(!panel->busy());
   QCOMPARE(status->text(), QString("The lens cap is on"));
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows);
   QVERIFY(capture->isEnabled());

   // it can be tried again
   QTest::keyClick(panel, Qt::Key_Space);
   QVERIFY(panel->busy());
   QCOMPARE(status->text(), QString());
   QTRY_VERIFY(!panel->busy());
   QCOMPARE(status->text(), QString("The lens cap is on"));

   QTest::mouseClick(panel->findChild<QPushButton *>("done"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(model->rowCount(parent), rows);
}

void TestCamera::testPanelPush()
{
   FakeCamera::Spec spec = fakeSpec();

   spec.capture_ms = 150;
   FakeCamera::use({spec});

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);

   // a size the panel does not offer, as the camera chooses its own
   QByteArray first = FakeCamera::makeJpeg(QSize(2000, 1500), 50);
   QByteArray second = FakeCamera::makeJpeg(QSize(2000, 1500), 51);
   QByteArray odd = FakeCamera::makeJpeg(QSize(800, 600), 52);

   // with the panel closed there is nothing to take it
   QVERIFY(!FakeCamera::push("fake0", first));

   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);

   CameraView *view = panel->findChild<CameraView *>("viewfinder");
   QLabel *count = panel->findChild<QLabel *>("count");
   QLabel *status = panel->findChild<QLabel *>("status");
   QPushButton *capture = panel->findChild<QPushButton *>("capture");
   QPushButton *done = panel->findChild<QPushButton *>("done");

   QTRY_VERIFY(view->frameCount() >= 1);
   QCOMPARE(panel->findChild<QComboBox *>("size")->currentData().toSize(),
            QSize(1600, 1200));

   // the camera's own still starts the stack, as Capture would
   QVERIFY(FakeCamera::push("fake0", first, odd));
   QTRY_COMPARE(main->cameraPages(), 1);
   QCOMPARE(count->text(), QString("1 page"));
   QCOMPARE(model->rowCount(parent), rows + 1);
   QVERIFY(!panel->busy());
   QVERIFY(capture->isEnabled());

   // the picture after it is not shown, and the view carries on
   QImage odd_image = QImage::fromData(odd, "JPG");
   int seen = view->frameCount();

   QTRY_VERIFY(view->frameCount() > seen + 2);
   QVERIFY(view->frame() != odd_image);

   /* one sent while Capture's still is on its way is a page of its
      own, as is the still which was asked for, each kept once */
   QTest::keyClick(panel, Qt::Key_Space);
   QVERIFY(panel->busy());
   QVERIFY(FakeCamera::push("fake0", second));
   QTRY_COMPARE(main->cameraPages(), 3);
   QVERIFY(!panel->busy());
   QCOMPARE(count->text(), QString("3 pages"));
   QCOMPARE(status->text(), QString());
   QTest::qWait(200);
   QCOMPARE(main->cameraPages(), 3);
   QTest::mouseClick(done, Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(model->rowCount(parent), rows + 1);

   // the pages are the stills as sent, at their own size, in order
   File *file = stackWithPages(model, parent, 3);
   QImage image;
   QSize size, true_size;
   int bpp;

   QVERIFY(file);
   QVERIFY(!file->getImage(0, false, image, size, true_size, bpp, false));
   QCOMPARE(size, QSize(2000, 1500));
   QVERIFY(!file->getImage(1, false, image, size, true_size, bpp, false));
   QCOMPARE(size, QSize(2000, 1500));
   QVERIFY(!file->getImage(2, false, image, size, true_size, bpp, false));
   QCOMPARE(size, QSize(1600, 1200));

   QFile stored(file->pathname());

   QVERIFY(stored.open(QIODevice::ReadOnly));

   QByteArray all = stored.readAll();
   int at = all.indexOf(first);

   QVERIFY(at >= 0);
   QVERIFY(all.indexOf(second, at) > at);

   // once the panel has closed, nothing takes one
   QVERIFY(!FakeCamera::push("fake0", first));
   QCOMPARE(main->cameraPages(), 0);
}

QByteArray TestCamera::bookJpeg(double gutter)
{
   QImage image = TestPagefind::mat(QSize(1600, 1200));
   QByteArray jpeg;
   QBuffer buf(&jpeg);

   TestPagefind::drawPage(image,
      QPolygonF({QPointF(200, 250), QPointF(1400, 230),
                 QPointF(1420, 1000), QPointF(190, 1010)}),
      QSize(1200, 760), gutter);
   buf.open(QIODevice::WriteOnly);
   image.save(&buf, "JPG", 92);
   return jpeg;
}

void TestCamera::choose(Camerapanel *panel, const QString &list,
                        const QString &text)
{
   QComboBox *combo = panel->findChild<QComboBox *>(list);

   QVERIFY(combo);

   int index = combo->findText(text);

   QVERIFY2(index >= 0, qPrintable(text));
   combo->setCurrentIndex(index);
   emit combo->activated(index);
}

void TestCamera::testPagesCurved()
{
   QImage image = TestPagefind::mat(QSize(1600, 1200));
   TestPagefind::Book book;
   double surface = TestPagefind::curvedBook(image, book);
   QByteArray jpeg;
   QBuffer buf(&jpeg);

   QVERIFY(buf.open(QIODevice::WriteOnly));
   QVERIFY(image.save(&buf, "JPG", 90));

   // each page comes out as wide for its height as its surface is
   bool found = false;
   QByteArrayList pages = CameraPages::process(jpeg, Pagefind::Mode_auto,
                                               90, found, book.focal);

   QVERIFY(found);
   QCOMPARE(pages.size(), 2);

   QImage left = QImage::fromData(pages[0], "JPG");
   double shape = surface / book.height;

   QVERIFY(!left.isNull());
   QVERIFY2(qAbs((double)left.width() / left.height() - shape) <=
            shape * 0.03,
            qPrintable(QString("%1x%2").arg(left.width())
                       .arg(left.height())));

   // which depends on the focal length it is told
   pages = CameraPages::process(jpeg, Pagefind::Mode_auto, 90, found,
                                book.focal * 3);
   QVERIFY(found);
   QCOMPARE(pages.size(), 2);
   QVERIFY(QImage::fromData(pages[0], "JPG").width() >
           left.width() * 1.1);

   // in the camera's view each page is outlined along its own edges
   CameraView view;

   view.setPages(Pagefind::Mode_auto);
   view.setFrame(image.scaled(800, 600));
   QCOMPARE(view.outlines().size(), 2);
   QCOMPARE(view.outlines()[0].size(), 2 * Pagefind::CurvePoints);

   // the top of a page is higher in the middle than at its ends
   QPolygonF outline = view.outlines()[0];

   QVERIFY(outline[Pagefind::CurvePoints / 2].y() < outline[0].y() - 8);
}

void TestCamera::testPages()
{
   QByteArray book = bookJpeg(0.46);
   QByteArray plain = FakeCamera::makeJpeg(QSize(800, 600), 3);
   bool found = false;

   // an open book gives its two pages, the left one first
   QByteArrayList pages = CameraPages::process(book, Pagefind::Mode_auto,
                                               90, found);

   QVERIFY(found);
   QCOMPARE(pages.size(), 2);

   QImage left = QImage::fromData(pages[0], "JPG");
   QImage right = QImage::fromData(pages[1], "JPG");

   QVERIFY(!left.isNull() && !right.isNull());
   QVERIFY(left.width() < right.width());
   QVERIFY(qAbs(left.width() + right.width() - 1225) <= 25);
   QVERIFY(qAbs(left.height() - 770) <= 20);
   QVERIFY(TestPagefind::markAt(left, true).x() > 0);
   QVERIFY(TestPagefind::markAt(right, false).x() > 0);
   QVERIFY(TestPagefind::markAt(right, true).x() < 0);

   // or the one, if it is said to be one
   pages = CameraPages::process(book, Pagefind::Mode_one, 90, found);
   QVERIFY(found);
   QCOMPARE(pages.size(), 1);
   QVERIFY(qAbs(QImage::fromData(pages[0], "JPG").width() - 1225) <= 25);

   // a lower quality makes a smaller page
   QVERIFY(CameraPages::process(book, Pagefind::Mode_one, 40, found)[0]
           .size() < pages[0].size());

   // a picture kept whole, or with no page to find, is itself
   pages = CameraPages::process(book, Pagefind::Mode_whole, 90, found);
   QVERIFY(!found);
   QCOMPARE(pages, QByteArrayList({book}));
   pages = CameraPages::process(plain, Pagefind::Mode_auto, 90, found);
   QVERIFY(!found);
   QCOMPARE(pages, QByteArrayList({plain}));
   pages = CameraPages::process("junk", Pagefind::Mode_auto, 90, found);
   QVERIFY(!found);
   QCOMPARE(pages, QByteArrayList({QByteArray("junk")}));

   // a grey picture gives grey pages
   QImage grey = QImage::fromData(book, "JPG")
                    .convertToFormat(QImage::Format_Grayscale8);
   QByteArray grey_jpeg;
   QBuffer buf(&grey_jpeg);

   QVERIFY(buf.open(QIODevice::WriteOnly));
   QVERIFY(grey.save(&buf, "JPG", 92));
   pages = CameraPages::process(grey_jpeg, Pagefind::Mode_auto, 90, found);
   QVERIFY(found);
   QCOMPARE(pages.size(), 2);
   QVERIFY(QImage::fromData(pages[0], "JPG").isGrayscale());

   // given several, their pages arrive in the order they were given
   CameraPages maker;
   QSignalSpy done(&maker, &CameraPages::processed);

   QCOMPARE(maker.pending(), 0);
   maker.add(book, Pagefind::Mode_auto, 90);
   maker.add(plain, Pagefind::Mode_auto, 90);
   maker.add(book, Pagefind::Mode_whole, 90);
   maker.add(book, Pagefind::Mode_one, 90);
   QCOMPARE(maker.pending(), 4);
   QCOMPARE(done.count(), 0);
   QTRY_COMPARE(done.count(), 4);
   QCOMPARE(maker.pending(), 0);
   QCOMPARE(done[0][0].value<QByteArrayList>().size(), 2);
   QVERIFY(done[0][1].toBool());
   QCOMPARE(done[1][0].value<QByteArrayList>(), QByteArrayList({plain}));
   QVERIFY(!done[1][1].toBool());
   QCOMPARE(done[2][0].value<QByteArrayList>(), QByteArrayList({book}));
   QCOMPARE(done[3][0].value<QByteArrayList>().size(), 1);
   QVERIFY(done[3][1].toBool());

   // those forgotten never arrive, and it carries on with the next
   done.clear();
   for (int i = 0; i < 5; i++)
      maker.add(book, Pagefind::Mode_auto, 90);
   maker.abandon();
   QCOMPARE(maker.pending(), 0);
   maker.add(plain, Pagefind::Mode_auto, 90);
   QCOMPARE(maker.pending(), 1);
   QTRY_COMPARE(done.count(), 1);
   QCOMPARE(done[0][0].value<QByteArrayList>(), QByteArrayList({plain}));
   QTest::qWait(300);
   QCOMPARE(done.count(), 1);
   QCOMPARE(maker.pending(), 0);
}

void TestCamera::testKeep()
{
   QString dir = setupRepo();
   QByteArray first = FakeCamera::makeJpeg(QSize(640, 480), 1);
   QByteArray second = FakeCamera::makeJpeg(QSize(800, 600), 2);

   // nothing is made until there is a picture
   CameraKeep keep(dir, "book_camera", "leaf_", 200);

   QVERIFY(keep.filename().isEmpty());
   QCOMPARE(keep.dir(), dir + "/");
   QVERIFY(!QFile::exists(dir + "/book_camera.max"));

   // one which is not a picture is refused, and makes nothing
   QVERIFY(keep.add("junk"));
   QVERIFY(!QFile::exists(dir + "/book_camera.max"));

   // each picture is on the disk as soon as it is added
   QVERIFY(!keep.add(first));
   QCOMPARE(keep.filename(), QString("book_camera.max"));
   QCOMPARE(keep.count(), 1);
   {
      Filemax max(dir + "/", "book_camera.max", nullptr);

      QVERIFY(!max.load());
      QCOMPARE(max.pagecount(), 1);
   }
   QVERIFY(!keep.add(second));
   QCOMPARE(keep.count(), 2);
   keep.finish();

   // each as it was given, with its name and the resolution
   Filemax max(dir + "/", "book_camera.max", nullptr);
   QString title;
   QSize dpi;

   QVERIFY(!max.load());
   QCOMPARE(max.pagecount(), 2);
   QVERIFY(!max.getPageTitle(1, title));
   QCOMPARE(title, QString("leaf_2"));
   QVERIFY(!max.getDpi(0, dpi));
   QCOMPARE(dpi, QSize(200, 200));

   QFile stored(dir + "/book_camera.max");

   QVERIFY(stored.open(QIODevice::ReadOnly));

   QByteArray all = stored.readAll();
   int at = all.indexOf(first);

   QVERIFY(at >= 0);
   QVERIFY(all.indexOf(second, at) > at);
   stored.close();

   // another of the same name gets a name of its own, and can be removed
   CameraKeep again(dir, "book_camera", "leaf_", 200);

   QVERIFY(!again.add(first));
   QVERIFY(again.filename() != QString("book_camera.max"));

   QString other = dir + "/" + again.filename();

   QVERIFY(QFile::exists(other));
   QVERIFY(!again.cancel());
   QVERIFY(!QFile::exists(other));
   QCOMPARE(again.count(), 0);
   QVERIFY(QFile::exists(dir + "/book_camera.max"));

   // with nothing added there is nothing to remove
   CameraKeep none(dir, "none", "leaf_", 200);

   QVERIFY(!none.cancel());
}

void TestCamera::testSessionPages()
{
   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   QSignalSpy filed(main, &Mainwidget::cameraFiled);
   int rows = model->rowCount(parent);
   QByteArray book = bookJpeg(0.46);
   QByteArray plain = FakeCamera::makeJpeg(QSize(800, 600), 3);

   // kept whole with nothing ahead of it, a picture is filed at once
   QCOMPARE(main->cameraAddPicture(plain, Pagefind::Mode_whole, true),
            QString());
   QCOMPARE(main->cameraPages(), 1);
   QCOMPARE(main->cameraPending(), 0);
   QCOMPARE(filed.count(), 0);

   // a book's pages take a moment, and what follows waits its turn
   QCOMPARE(main->cameraAddPicture(book, Pagefind::Mode_auto, true),
            QString());
   QCOMPARE(main->cameraAddPicture(plain, Pagefind::Mode_whole, true),
            QString());
   QCOMPARE(main->cameraAddPicture(plain, Pagefind::Mode_one, true),
            QString());
   QCOMPARE(main->cameraPending(), 3);
   QCOMPARE(main->cameraPages(), 1);
   QTRY_COMPARE(filed.count(), 3);
   QCOMPARE(main->cameraPending(), 0);
   QCOMPARE(main->cameraPages(), 5);
   for (int i = 0; i < 3; i++)
      QCOMPARE(filed[i][0].toString(), QString());

   // one which is not a picture is reported, and nothing is filed
   QVERIFY(!main->cameraAddPicture("junk", Pagefind::Mode_auto, true)
           .isEmpty());
   QTRY_COMPARE(filed.count(), 4);
   QVERIFY(!filed[3][0].toString().isEmpty());
   QCOMPARE(main->cameraPages(), 5);

   // only the pages are on the desk until it is done
   QCOMPARE(model->rowCount(parent), rows + 1);

   // Done waits for what is still being made
   QCOMPARE(main->cameraAddPicture(book, Pagefind::Mode_two, true),
            QString());
   QCOMPARE(main->cameraPending(), 1);
   main->cameraDone();
   QCOMPARE(main->cameraPending(), 0);
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows + 2);

   // the pages: the picture, the book's two, two pictures, the book's two
   File *file = stackWithPages(model, parent, 7);

   QVERIFY(file);

   // the pictures kept are those which pages were looked for in
   QString kept_name = QFileInfo(file->filename()).completeBaseName()
         + "_camera.max";
   QModelIndex kept_ind = model->index(kept_name, parent);

   QVERIFY(kept_ind.isValid());

   File *kept = model->getFile(kept_ind);

   QVERIFY(kept);
   QTRY_COMPARE(kept->pagecount(), 3);

   QFile kept_file(kept->pathname());

   QVERIFY(kept_file.open(QIODevice::ReadOnly));

   QByteArray all = kept_file.readAll();
   int at = all.indexOf(book);

   QVERIFY(at >= 0);
   at = all.indexOf(plain, at + book.size());
   QVERIFY(at >= 0);
   QVERIFY(all.indexOf(book, at + plain.size()) > at);

   // asked not to keep them, there is the one stack
   QCOMPARE(main->cameraAddPicture(book, Pagefind::Mode_two, false),
            QString());
   main->cameraDone();
   QCOMPARE(model->rowCount(parent), rows + 3);

   // thrown away while pages are being made, nothing is left
   for (int i = 0; i < 3; i++)
      QCOMPARE(main->cameraAddPicture(book, Pagefind::Mode_two, true),
               QString());
   QCOMPARE(model->rowCount(parent), rows + 4);
   main->cameraCancel();
   QCOMPARE(main->cameraPending(), 0);
   QCOMPARE(model->rowCount(parent), rows + 3);
   filed.clear();
   QTest::qWait(500);
   QCOMPARE(filed.count(), 0);
   QCOMPARE(main->cameraPages(), 0);
   QCOMPARE(model->rowCount(parent), rows + 3);

   // and none of its pictures are left in the folder
   QDir dir(QFileInfo(file->pathname()).path());

   QCOMPARE(dir.entryList({"*_camera*.max"}).size(), 1);
}

void TestCamera::testPanelPages()
{
   FakeCamera::Spec spec = fakeSpec();
   QByteArray whole = FakeCamera::makeJpeg(QSize(1600, 1200), 7);

   // two pictures of a book, then one with no page to find in it
   spec.stills = {bookJpeg(0.46), bookJpeg(0.5), whole};
   FakeCamera::use({spec});
   QSettings().remove("camera/pages");

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);

   CameraView *view = panel->findChild<CameraView *>("viewfinder");
   QComboBox *pages = panel->findChild<QComboBox *>("pages");
   QCheckBox *keep = panel->findChild<QCheckBox *>("keep");
   QLabel *count = panel->findChild<QLabel *>("count");
   QLabel *status = panel->findChild<QLabel *>("status");

   QVERIFY(view && pages && keep && count && status);

   // left to itself it chooses, and keeps the camera's pictures
   QCOMPARE(pages->currentText(), QString("Auto"));
   QVERIFY(keep->isChecked());
   QVERIFY(keep->isEnabled());

   // the pages which would be cut out are marked in the camera's view
   QTRY_COMPARE(view->outlines().size(), 2);
   QVERIFY(view->outlines()[0].boundingRect().right()
           <= view->outlines()[1].boundingRect().left() + 2);
   QVERIFY(QRectF(view->frame().rect()).adjusted(-2, -2, 2, 2)
           .contains(view->outlines()[0].boundingRect()));

   // with the whole picture kept there is nothing to mark, or to keep
   choose(panel, "pages", "Whole picture");
   QVERIFY(view->outlines().isEmpty());
   QVERIFY(!keep->isEnabled());
   choose(panel, "pages", "One page");
   QTRY_COMPARE(view->outlines().size(), 1);
   QVERIFY(keep->isEnabled());
   choose(panel, "pages", "Two pages");
   QTRY_COMPARE(view->outlines().size(), 2);

   // each picture of the book gives two pages
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 2);
   QCOMPARE(count->text(), QString("2 pages"));
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 4);
   QTRY_COMPARE(main->cameraPending(), 0);
   QCOMPARE(status->text(), QString());

   // only the stack of pages is on the desk so far
   QCOMPARE(model->rowCount(parent), rows + 1);

   // a picture with no page to be found is kept whole
   choose(panel, "pages", "Auto");
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 5);
   QCOMPARE(count->text(), QString("5 pages"));

   // and a book is split without being told that it is one
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 7);

   QTest::mouseClick(panel->findChild<QPushButton *>("done"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(main->cameraPages(), 0);

   // now there are two stacks: the pages, and the camera's pictures
   QCOMPARE(model->rowCount(parent), rows + 2);

   File *file = stackWithPages(model, parent, 7);
   File *kept = stackWithPages(model, parent, 4);

   QVERIFY(file);
   QVERIFY(kept);
   QCOMPARE(kept->filename(),
            QFileInfo(file->filename()).completeBaseName()
            + "_camera.max");
   QCOMPARE(QFileInfo(kept->pathname()).path(),
            QFileInfo(file->pathname()).path());

   // the pages are in order, the left of each pair before the right
   const QList<double> gutters = {0.46, 0.5, 0, 0.46};

   for (int i = 0; i < 8; i++) {
      // the picture kept whole comes between them
      if (i / 2 == 2)
         continue;

      QImage image;
      QSize size, true_size;
      int bpp;

      QVERIFY(!file->getImage(i < 4 ? i : i - 1, false, image, size,
                              true_size, bpp, false));
      image = image.copy(QRect(QPoint(0, 0), size));

      double share = i % 2 ? 1 - gutters[i / 2] : gutters[i / 2];

      QVERIFY2(qAbs(size.width() - 1225 * share) <= 25,
               qPrintable(QString("page %1 is %2 wide").arg(i)
                          .arg(size.width())));
      QVERIFY(qAbs(size.height() - 770) <= 20);
      QVERIFY((TestPagefind::markAt(image, true).x() > 0) == !(i % 2));
      QVERIFY((TestPagefind::markAt(image, false).x() > 0) == (i % 2));
   }

   // and the one between is the picture itself
   QFile stored(file->pathname());

   QVERIFY(stored.open(QIODevice::ReadOnly));
   QVERIFY(stored.readAll().contains(whole));

   // the pictures kept are those the camera sent, byte for byte
   QFile kept_file(kept->pathname());

   QVERIFY(kept_file.open(QIODevice::ReadOnly));

   QByteArray all = kept_file.readAll();
   int at = 0;

   for (int i = 0; i < 4; i++) {
      const QByteArray &jpeg = spec.stills[i % 3];

      at = all.indexOf(jpeg, at);
      QVERIFY(at >= 0);
      at += jpeg.size();
   }

   // what was chosen is remembered
   choose(panel, "pages", "One page");
   keep->setChecked(false);
   QCOMPARE(openPanel(&me), panel);
   QCOMPARE(pages->currentText(), QString("One page"));
   QVERIFY(!keep->isChecked());
   QCOMPARE(QSettings().value("camera/pages").toInt(),
            (int)Pagefind::Mode_one);
   QVERIFY(!QSettings().value("camera/keep").toBool());

   // without the pictures kept there is the one stack
   QTRY_VERIFY(view->frameCount() >= 1);
   QTest::keyClick(panel, Qt::Key_Space);
   QTRY_COMPARE(main->cameraPages(), 1);
   QTest::mouseClick(panel->findChild<QPushButton *>("done"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(model->rowCount(parent), rows + 3);
}

void TestCamera::testPanelPagesWait()
{
   FakeCamera::use({fakeSpec()});
   QSettings().setValue("camera/pages", Pagefind::Mode_two);

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);

   QLabel *status = panel->findChild<QLabel *>("status");
   QPushButton *capture = panel->findChild<QPushButton *>("capture");
   QPushButton *done = panel->findChild<QPushButton *>("done");
   QByteArray book = bookJpeg(0.5);

   // pictures arrive faster than their pages can be made
   for (int i = 0; i < 3; i++)
      QCOMPARE(main->cameraAddPicture(book, Pagefind::Mode_two, true),
               QString());
   QCOMPARE(main->cameraPending(), 3);
   QCOMPARE(main->cameraPages(), 0);

   // the stack is there already, and the pictures are on the disk
   QCOMPARE(model->rowCount(parent), rows + 1);

   // Done waits for them, and says so
   QTest::mouseClick(done, Qt::LeftButton);
   QVERIFY(panel->isVisible());
   QCOMPARE(status->text(),
            QString("Finishing: 3 pictures still to do"));
   QVERIFY(!done->isEnabled());
   QVERIFY(!capture->isEnabled());
   QVERIFY(!panel->findChild<QComboBox *>("pages")->isEnabled());

   // then closes of its own accord, with every page kept
   QTRY_VERIFY(!panel->isVisible());
   QCOMPARE(main->cameraPending(), 0);
   QCOMPARE(model->rowCount(parent), rows + 2);
   QVERIFY(stackWithPages(model, parent, 6));
   QVERIFY(stackWithPages(model, parent, 3));

   // Cancel while it waits throws them all away
   QCOMPARE(openPanel(&me), panel);
   for (int i = 0; i < 3; i++)
      QCOMPARE(main->cameraAddPicture(book, Pagefind::Mode_two, true),
               QString());
   QTest::mouseClick(done, Qt::LeftButton);
   QVERIFY(panel->isVisible());
   QTest::mouseClick(panel->findChild<QPushButton *>("cancel"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(main->cameraPending(), 0);
   QCOMPARE(model->rowCount(parent), rows + 2);
   QTest::qWait(500);
   QCOMPARE(model->rowCount(parent), rows + 2);
   QCOMPARE(main->cameraPages(), 0);

   // and the panel is ready for the next stack
   QCOMPARE(openPanel(&me), panel);
   QVERIFY(done->isEnabled());
   QTRY_VERIFY(capture->isEnabled());
   QCOMPARE(status->text(), QString());
   QTest::mouseClick(done, Qt::LeftButton);
   QVERIFY(!panel->isVisible());
}

void TestCamera::testPanelPagesCancel()
{
   FakeCamera::Spec spec = fakeSpec();

   spec.stills = {bookJpeg(0.5)};
   FakeCamera::use({spec});
   QSettings().setValue("camera/pages", Pagefind::Mode_two);

   Mainwindow me;
   Desktopmodel *model = nullptr;
   QModelIndex parent;

   setupDesk(&me, model, parent);
   QVERIFY(model);

   Mainwidget *main = Mainwidget::singleton();
   int rows = model->rowCount(parent);
   Camerapanel *panel = openPanel(&me);

   QVERIFY(panel);
   for (int i = 0; i < 2; i++) {
      QTest::keyClick(panel, Qt::Key_Space);
      QTRY_COMPARE(main->cameraPages(), (i + 1) * 2);
   }
   QCOMPARE(model->rowCount(parent), rows + 1);

   // the pictures are being kept beside the stack
   File *file = stackWithPages(model, parent, 4);

   QVERIFY(file);

   QString kept = QFileInfo(file->pathname()).path() + "/"
         + QFileInfo(file->filename()).completeBaseName() + "_camera.max";
   QString pages_path = file->pathname();

   QVERIFY(QFile::exists(kept));

   // Cancel leaves neither
   QTest::mouseClick(panel->findChild<QPushButton *>("cancel"),
                     Qt::LeftButton);
   QVERIFY(!panel->isVisible());
   QCOMPARE(model->rowCount(parent), rows);
   QVERIFY(!QFile::exists(kept));
   QVERIFY(!QFile::exists(pages_path));
   QCOMPARE(main->cameraPages(), 0);
}
