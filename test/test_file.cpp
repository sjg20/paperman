#include <QBuffer>
#include <QPainter>
#include <QtConcurrent>
#include <QSet>
#include <QTextStream>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <QStandardPaths>

#include <cstring>

#include "err.h"
#include "file.h"
#include "filejpeg.h"
#include "filemax.h"
#include "fileother.h"
#include "filepdf.h"

#include "op.h"
#include "paperstack.h"

#include "test_file.h"

QString TestFile::copyFixture(const QString &name, const QString &destDir)
{
   const QString src = testSrc + "/" + name;
   const QString dst = destDir + "/" + name;
   if (!QFile::copy(src, dst))
      return QString();
   return dst;
}

void TestFile::testTypeFromName()
{
   QCOMPARE(File::typeFromName("scan.max"), File::Type_max);
   QCOMPARE(File::typeFromName("scan.MAX"), File::Type_max);
   QCOMPARE(File::typeFromName("scan.pdf"), File::Type_pdf);
   QCOMPARE(File::typeFromName("scan.PDF"), File::Type_pdf);
   QCOMPARE(File::typeFromName("photo.jpg"), File::Type_jpeg);
   QCOMPARE(File::typeFromName("photo.JPEG"), File::Type_jpeg);
   QCOMPARE(File::typeFromName("scan.tiff"), File::Type_other);
   QCOMPARE(File::typeFromName("noext"), File::Type_other);
   QCOMPARE(File::typeFromName("/path/with/dots.in.dir/scan.pdf"),
            File::Type_pdf);
}

void TestFile::testTypeNameAndExt()
{
   // Names returned for the display layer
   QCOMPARE(File::typeName(File::Type_other), QStringLiteral("Other"));
   QCOMPARE(File::typeName(File::Type_max), QStringLiteral("Max"));
   QCOMPARE(File::typeName(File::Type_pdf), QStringLiteral("PDF"));
   QCOMPARE(File::typeName(File::Type_jpeg), QStringLiteral("JPEG"));

   // Type_other has no extension; the other three round-trip through
   // typeFromName
   QCOMPARE(File::typeExt(File::Type_other), QString());
   QCOMPARE(File::typeExt(File::Type_max), QStringLiteral(".max"));
   QCOMPARE(File::typeExt(File::Type_pdf), QStringLiteral(".pdf"));
   QCOMPARE(File::typeExt(File::Type_jpeg), QStringLiteral(".jpg"));

   for (int t = File::Type_max; t < File::Type_count; t++) {
      File::e_type type = static_cast<File::e_type>(t);
      QString name = "x" + File::typeExt(type);
      QCOMPARE(File::typeFromName(name), type);
   }
}

void TestFile::testEnvNames()
{
   for (int i = 0; i < File::Env_count; i++) {
      File::e_env env = static_cast<File::e_env>(i);
      QString name = File::envToName(env);
      QVERIFY(!name.isEmpty());
      QCOMPARE(File::envFromName(name), env);
   }
   QCOMPARE(File::envFromName("not-a-real-env"), File::Env_count);
}

void TestFile::testPageNumberCodec()
{
   QString base, ext;
   int page = -1;

   // _p1 maps to page 0 (codec is 1-based on disk, 0-based in memory)
   QVERIFY(File::decodePageNumber("scan_p1.max", base, page, ext));
   QCOMPARE(base, QStringLiteral("scan"));
   QCOMPARE(page, 0);
   QCOMPARE(ext, QStringLiteral("max"));

   QVERIFY(File::decodePageNumber("scan_p42.pdf", base, page, ext));
   QCOMPARE(base, QStringLiteral("scan"));
   QCOMPARE(page, 41);
   QCOMPARE(ext, QStringLiteral("pdf"));

   // No _pN marker
   QVERIFY(!File::decodePageNumber("scan.max", base, page, ext));

   // Out-of-range page numbers are rejected
   QVERIFY(!File::decodePageNumber("scan_p0.max", base, page, ext));
   QVERIFY(!File::decodePageNumber("scan_p99999.max", base, page, ext));

   // Round-trip via a File object (encode needs a type and base)
   File *f = File::createFile("/tmp", "round_p1.pdf", nullptr, File::Type_pdf);
   QVERIFY(f);
   QCOMPARE(f->encodePageNumber("round", 0), QStringLiteral("round_p1.pdf"));
   QCOMPARE(f->encodePageNumber("round", 41), QStringLiteral("round_p42.pdf"));
   delete f;
}

void TestFile::testExtMatchesType()
{
   File *pdf = File::createFile("/tmp", "x.pdf", nullptr, File::Type_pdf);
   QVERIFY(pdf);
   QVERIFY(pdf->extMatchesType("pdf"));
   QVERIFY(pdf->extMatchesType("PDF"));
   QVERIFY(!pdf->extMatchesType("max"));
   delete pdf;

   File *jpg = File::createFile("/tmp", "x.jpg", nullptr, File::Type_jpeg);
   QVERIFY(jpg);
   QVERIFY(jpg->extMatchesType("jpg"));
   QVERIFY(jpg->extMatchesType("jpeg"));
   QVERIFY(!jpg->extMatchesType("pdf"));
   delete jpg;
}

void TestFile::testNotImpl()
{
   err_info *err = File::not_impl();
   QVERIFY(err);
   QCOMPARE(err->errnum, ERR_no_available_for_this_file_type);
}

void TestFile::testCreateFileDispatch()
{
   const QString dir = QStringLiteral("/tmp/");

   struct Case {
      const char *fname;
      File::e_type type;
      const char *cls;
   } cases[] = {
      { "x.max",   File::Type_max,   "Filemax"   },
      { "x.pdf",   File::Type_pdf,   "Filepdf"   },
      { "x.jpg",   File::Type_jpeg,  "Filejpeg"  },
      { "x.bin",   File::Type_other, "Fileother" },
   };

   for (const Case &c : cases) {
      File *f = File::createFile(dir, c.fname, nullptr, c.type);
      QVERIFY2(f, c.fname);
      QCOMPARE(f->type(), c.type);
      QCOMPARE(QString(f->metaObject()->className()), QString(c.cls));

      QCOMPARE(f->filename(), QString(c.fname));
      QCOMPARE(f->pathname(), dir + c.fname);
      QCOMPARE(f->ext(), QString(".") + QFileInfo(c.fname).suffix());

      // .max strips its extension to form the basename; the others keep it
      QString expectBase = (c.type == File::Type_max)
         ? QFileInfo(c.fname).completeBaseName()
         : QString(c.fname);
      QCOMPARE(f->basename(), expectBase);

      delete f;
   }
}

void TestFile::testFixtureMetadata()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   // --- testfile.max ---
   {
      QString path = copyFixture("testfile.max", tmp.path());
      QVERIFY(!path.isEmpty());

      Filemax *f = new Filemax(dir, "testfile.max", nullptr);
      QVERIFY(f->load() == nullptr);
      QVERIFY(f->pagecount() > 0);
      // load() stat()s the file into the inherited _size; the chunk-based
      // getSize() is only meaningful after the file's internal structures
      // have been walked, so don't pin it here.
      QCOMPARE(f->size(), int(QFileInfo(path).size()));

      QString title;
      QVERIFY(f->getPageTitle(0, title) == nullptr);
      // Title is allowed to be empty, but the call must not error.

      QString annot;
      QVERIFY(f->getAnnot(File::Annot_author, annot) == nullptr);

      delete f;
   }

   // --- testpdf.pdf ---
   {
      QString path = copyFixture("testpdf.pdf", tmp.path());
      QVERIFY(!path.isEmpty());

      Filepdf *f = new Filepdf(dir, "testpdf.pdf", nullptr);
      QVERIFY(f->load() == nullptr);
      QVERIFY(f->pagecount() >= 1);

      QString annot;
      QVERIFY(f->getAnnot(File::Annot_author, annot) == nullptr);

      delete f;
   }

   // --- colour_plasma.jpg ---
   {
      QString path = copyFixture("colour_plasma.jpg", tmp.path());
      QVERIFY(!path.isEmpty());

      Filejpeg *f = new Filejpeg(dir, "colour_plasma.jpg", nullptr);
      QVERIFY(f->load() == nullptr);
      QCOMPARE(f->pagecount(), 1);
      QVERIFY(f->getSize() > 0);

      delete f;
   }

   // --- Fileother: any file we don't natively understand ---
   {
      QString path = dir + "stub.bin";
      QFile fp(path);
      QVERIFY(fp.open(QIODevice::WriteOnly));
      fp.write("not really a real file");
      fp.close();

      Fileother *f = new Fileother(dir, "stub.bin", nullptr);
      // Fileother::load returns not_impl unless _valid is already set,
      // and exposes a fixed pagecount/size of 0 — pin both.
      QCOMPARE(f->pagecount(), 1);
      QCOMPARE(f->getSize(), 0);

      QString annot;
      QVERIFY(f->getAnnot(File::Annot_author, annot) == nullptr);
      QVERIFY(annot.isEmpty());

      delete f;
   }
}

void TestFile::testEncodeDecode()
{
   File *f = File::createFile("/tmp/", "round.pdf", nullptr, File::Type_pdf);
   QVERIFY(f);

   f->setPos(QPoint(123, 456));
   f->setPagenum(2);
   f->setPreviewMaxsize(QSize(80, 100));
   f->setTitleMaxsize(QSize(40, 12));
   f->setPagenameMaxsize(QSize(50, 14));

   QString encoded;
   {
      QTextStream out(&encoded);
      f->encodeFile(out);
   }
   QVERIFY(encoded.contains("round.pdf="));

   // Decode the comma-separated payload (everything after the '=')
   int eq = encoded.indexOf('=');
   QVERIFY(eq > 0);
   QString line = encoded.mid(eq + 1).trimmed();

   File *g = File::createFile("/tmp/", "round.pdf", nullptr, File::Type_pdf);
   QVERIFY(g);
   g->decodeFile(line, true);

   QCOMPARE(g->pos(), QPoint(123, 456));
   QCOMPARE(g->pagenum(), 2);
   QCOMPARE(g->previewMaxsize(), QSize(80, 100));
   QCOMPARE(g->titleMaxsize(), QSize(40, 12));
   QCOMPARE(g->pagenameMaxsize(), QSize(50, 14));

   delete f;
   delete g;
}

void TestFile::testGetImage()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   // --- Filemax ---
   {
      QVERIFY(!copyFixture("testfile.max", tmp.path()).isEmpty());
      Filemax *f = new Filemax(dir, "testfile.max", nullptr);
      QVERIFY(f->load() == nullptr);

      QImage img;
      QSize size, trueSize;
      int bpp = 0;
      QVERIFY(f->getImage(0, false, img, size, trueSize, bpp, false) == nullptr);
      QVERIFY(!img.isNull());
      QVERIFY(size.width() > 0 && size.height() > 0);
      QCOMPARE(img.size(), trueSize);
      QVERIFY(bpp == 1 || bpp == 8 || bpp == 24);
      delete f;
   }

   // --- Filepdf ---
   {
      QVERIFY(!copyFixture("testpdf.pdf", tmp.path()).isEmpty());
      Filepdf *f = new Filepdf(dir, "testpdf.pdf", nullptr);
      QVERIFY(f->load() == nullptr);

      QImage img;
      QSize size, trueSize;
      int bpp = 0;
      QVERIFY(f->getImage(0, false, img, size, trueSize, bpp, false) == nullptr);
      QVERIFY(!img.isNull());
      QVERIFY(size.width() > 0 && size.height() > 0);
      QCOMPARE(img.size(), size);
      QCOMPARE(size, trueSize);
      delete f;
   }

   // --- Filejpeg colour ---
   {
      QVERIFY(!copyFixture("colour_plasma.jpg", tmp.path()).isEmpty());
      Filejpeg *f = new Filejpeg(dir, "colour_plasma.jpg", nullptr);
      QVERIFY(f->load() == nullptr);

      QImage img;
      QSize size, trueSize;
      int bpp = 0;
      QVERIFY(f->getImage(0, false, img, size, trueSize, bpp, false) == nullptr);
      QCOMPARE(img.size(), QSize(2400, 3300));
      QCOMPARE(size, QSize(2400, 3300));
      QCOMPARE(trueSize, size);
      // Loaded as a colour image — Qt may pick 24 or 32 bpp
      QVERIFY(bpp == 24 || bpp == 32);
      delete f;
   }

   // --- Filejpeg greyscale ---
   {
      QVERIFY(!copyFixture("greyscale_gradient.jpg", tmp.path()).isEmpty());
      Filejpeg *f = new Filejpeg(dir, "greyscale_gradient.jpg", nullptr);
      QVERIFY(f->load() == nullptr);

      QImage img;
      QSize size, trueSize;
      int bpp = 0;
      QVERIFY(f->getImage(0, false, img, size, trueSize, bpp, false) == nullptr);
      QCOMPARE(img.size(), QSize(2400, 3300));
      QVERIFY(bpp == 8 || bpp == 24 || bpp == 32);
      delete f;
   }

   // --- Fileother: getImage is not implemented ---
   {
      QFile fp(dir + "stub.bin");
      QVERIFY(fp.open(QIODevice::WriteOnly));
      fp.write("hi");
      fp.close();

      Fileother *f = new Fileother(dir, "stub.bin", nullptr);
      QImage img;
      QSize size, trueSize;
      int bpp = 0;
      err_info *err = f->getImage(0, false, img, size, trueSize, bpp, false);
      QVERIFY(err);
      QCOMPARE(err->errnum, ERR_no_available_for_this_file_type);
      delete f;
   }
}

void TestFile::testGetPreviewPixmap()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   // Filejpeg downscales by /24
   {
      QVERIFY(!copyFixture("colour_plasma.jpg", tmp.path()).isEmpty());
      Filejpeg *f = new Filejpeg(dir, "colour_plasma.jpg", nullptr);
      QVERIFY(f->load() == nullptr);

      QPixmap pix;
      QVERIFY(f->getPreviewPixmap(0, pix, false) == nullptr);
      QVERIFY(!pix.isNull());
      // /24 on the 2400x3300 fixture (height rounds up under KeepAspectRatio)
      QCOMPARE(pix.size(), QSize(100, 138));
      delete f;
   }

   // Filepdf renders at DPI/24 so the preview is much smaller than the full
   // page; pin only that it's non-null and notably smaller than the full image
   {
      QVERIFY(!copyFixture("testpdf.pdf", tmp.path()).isEmpty());
      Filepdf *f = new Filepdf(dir, "testpdf.pdf", nullptr);
      QVERIFY(f->load() == nullptr);

      QImage full;
      QSize fullSize, fullTrue;
      int bpp = 0;
      QVERIFY(f->getImage(0, false, full, fullSize, fullTrue, bpp, false)
              == nullptr);

      QPixmap pix;
      QVERIFY(f->getPreviewPixmap(0, pix, false) == nullptr);
      QVERIFY(!pix.isNull());
      QVERIFY(pix.width() < fullSize.width());
      QVERIFY(pix.height() < fullSize.height());
      delete f;
   }
}

void TestFile::testFileotherSetThumbnail()
{
   Fileother *f = new Fileother("/tmp/", "stub.bin", nullptr);

   // No thumbnail yet → generic placeholder is returned, but the call must
   // succeed.
   QPixmap placeholder = f->pixmap(false);
   QVERIFY(!placeholder.isNull());

   // Inject a recognisable pixmap and verify pixmap() returns it verbatim.
   QPixmap inject(32, 16);
   inject.fill(Qt::red);
   f->setThumbnail(inject);

   QPixmap got = f->pixmap(false);
   QCOMPARE(got.size(), QSize(32, 16));
   QCOMPARE(got.toImage(), inject.toImage());

   delete f;
}

void TestFile::testFileotherMutationsNotImpl()
{
   Fileother *f = new Fileother("/tmp/", "stub.bin", nullptr);

   QBitArray bits(1);
   QByteArray del;
   int count = 0;

   // addPage, removePages, restorePages, unstackPages, stackStack all return
   // not_impl with the canonical errnum
   err_info *err = f->addPage(nullptr, false);
   QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);

   err = f->removePages(bits, del, count);
   QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);

   err = f->restorePages(bits, del, count);
   QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);

   err = f->unstackPages(0, 1, false, nullptr);
   QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);

   err = f->stackStack(nullptr);
   QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);

   // duplicate signals 'not supported' rather than erroring — pin both.
   // Operation has a static receiver that earlier suites (Dirview) may have
   // pointed at a now-destroyed widget; clear it so emit-on-construct does
   // not crash here.
   Operation::setReceiver(nullptr);
   File *fnew = nullptr;
   bool supported = true;
   Operation op("duptest", 1, nullptr);
   QString uniq = "x";
   err = f->duplicate(fnew, File::Type_other, uniq, 0, op, supported);
   QVERIFY(err == nullptr);
   QVERIFY(!supported);

   delete f;
}

void TestFile::testSupportsJpeg()
{
   File *pdf = File::createFile("/tmp/", "x.pdf", nullptr, File::Type_pdf);
   File *max = File::createFile("/tmp/", "x.max", nullptr, File::Type_max);
   File *jpg = File::createFile("/tmp/", "x.jpg", nullptr, File::Type_jpeg);
   File *oth = File::createFile("/tmp/", "x.bin", nullptr, File::Type_other);

   QVERIFY(pdf->supportsJpeg());
   QVERIFY(!max->supportsJpeg());
   QVERIFY(!jpg->supportsJpeg());
   QVERIFY(!oth->supportsJpeg());

   // The base addPageJpeg returns not_impl; subclasses that don't override
   // inherit that.  Use Filemax to prove the fallback path is wired up.
   err_info *err = max->addPageJpeg(QByteArray(), 1, 1, false);
   QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);

   delete pdf;
   delete max;
   delete jpg;
   delete oth;
}

void TestFile::testStackItemTypeMismatch()
{
   // stackItem is the public funnel for stacking: it must reject mixed
   // types before calling into the subclass stackStack (which would not
   // know how to consume foreign Filepages).  All the desk-level stack
   // operations go through this check.
   File *pdf = File::createFile("/tmp/", "x.pdf", nullptr, File::Type_pdf);
   File *jpg = File::createFile("/tmp/", "x.jpg", nullptr, File::Type_jpeg);

   err_info *err = pdf->stackItem(jpg);
   QVERIFY(err);
   QCOMPARE(err->errnum, ERR_cannot_stack_type_onto_type2);

   delete pdf;
   delete jpg;
}

void TestFile::testCacheBoundaryReads()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";
   QString path = copyFixture("testfile.max", tmp.path());
   QVERIFY(!path.isEmpty());

   QFile raw(path);
   QVERIFY(raw.open(QIODevice::ReadOnly));
   QByteArray bytes = raw.readAll();
   QVERIFY(bytes.size() > 3 * 4096 + 4200);

   Filemax f(dir, "testfile.max", nullptr);
   QVERIFY(f.load() == nullptr);
   QVERIFY(f.ensure_open() == nullptr);

   auto hw_at = [&bytes](int pos) {
      return int(quint8(bytes[pos]) | quint8(bytes[pos + 1]) << 8);
   };
   auto word_at = [&bytes](int pos) {
      return int(quint32(quint8(bytes[pos]))
                 | quint32(quint8(bytes[pos + 1])) << 8
                 | quint32(quint8(bytes[pos + 2])) << 16
                 | quint32(quint8(bytes[pos + 3])) << 24);
   };

   /* getword() and gethw() read through a 4KB cache window which has a
      few bytes of padding on the end. A read which straddles the end
      of the window's valid data must reload the cache rather than
      returning padding bytes, which are not file data. Seed the window
      at a known start, then read across its boundary, checking every
      value against the raw file bytes */
   for (int window = 0; window <= 2 * 4096; window += 4096)
      for (int off = 4090; off <= 4096; off++) {
         int pos = window + off;

         f.gethw(window);   // make the cache window start at 'window'
         QCOMPARE(f.getword(pos), word_at(pos));

         f.gethw(window);
         QCOMPARE(f.gethw(pos), hw_at(pos));
      }

   // a halfword at the very end of the file can still be read
   QCOMPARE(f.gethw(bytes.size() - 2), hw_at(bytes.size() - 2));
}

//! Check two images are the same size and nearly identical in content,
//! allowing for JPEG recompression loss
static bool nearlySame(const QImage &a, const QImage &b)
{
   if (a.size() != b.size())
      return false;

   QImage ga = a.convertToFormat(QImage::Format_Grayscale8);
   QImage gb = b.convertToFormat(QImage::Format_Grayscale8);
   qint64 total = 0;
   int count = 0;

   for (int y = 0; y < ga.height(); y += 13) {
      const uchar *pa = ga.constScanLine(y);
      const uchar *pb = gb.constScanLine(y);
      for (int x = 0; x < ga.width(); x += 13, count++)
         total += qAbs(int(pa[x]) - int(pb[x]));
   }
   return count && total / count < 8;
}

void TestFile::testTransformPage()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   // --- max: rotate and mirror are applied and are reversible ---
   {
      copyFixture("testfile.max", tmp.path());
      Filemax max(dir, "testfile.max", nullptr);
      QVERIFY(max.load() == nullptr);

      QImage orig, image;
      QSize size, trueSize;
      int bpp;
      QVERIFY(!max.getImage(0, false, orig, size, trueSize, bpp, false));
      QString title_before;
      QVERIFY(!max.getPageTitle(0, title_before));

      // rotating swaps the dimensions
      QVERIFY(!max.transformPage(0, File::Transform_rotate90));
      QVERIFY(!max.getImage(0, false, image, size, trueSize, bpp, false));
      QCOMPARE(image.width(), orig.height());
      QCOMPARE(image.height(), orig.width());

      // the page keeps its title
      QString title;
      QVERIFY(!max.getPageTitle(0, title));
      QCOMPARE(title, title_before);

      // rotating back restores the original content
      QVERIFY(!max.transformPage(0, File::Transform_rotate270));
      QVERIFY(!max.getImage(0, false, image, size, trueSize, bpp, false));

      /* rotating back restores the original content. The pages are
         JPEG compressed so allow for recompression loss */
      QVERIFY(nearlySame(image, orig));

      /* mirroring changes the image (the page has horizontal colour
         bars, so flip vertically) and mirroring again restores it */
      QVERIFY(!max.transformPage(0, File::Transform_vflip));
      QVERIFY(!max.getImage(0, false, image, size, trueSize, bpp, false));
      QVERIFY(!nearlySame(image, orig));
      QVERIFY(!max.transformPage(0, File::Transform_vflip));
      QVERIFY(!max.getImage(0, false, image, size, trueSize, bpp, false));
      QVERIFY(nearlySame(image, orig));

      // the other pages are untouched
      QCOMPARE(max.pagecount(), 5);
   }

   // --- max: a 1-bit text page survives rotation ---
   {
      Filemax max(dir, "testfile.max", nullptr);
      QVERIFY(max.load() == nullptr);

      QImage orig, image;
      QSize size, trueSize;
      int bpp;
      QVERIFY(!max.getImage(3, false, orig, size, trueSize, bpp, false));
      qDebug() << "page 3 bpp" << bpp << "format" << orig.format();

      QVERIFY(!max.transformPage(3, File::Transform_rotate90));
      QVERIFY(!max.getImage(3, false, image, size, trueSize, bpp, false));

      // 1-bit images are stored with the width padded to 32 pixels
      QVERIFY(image.width() >= orig.height());
      QVERIFY(image.width() < orig.height() + 32);

      /* the page must not come back blank: it has text, so a fair
         number of pixels are dark */
      QImage grey = image.convertToFormat(QImage::Format_Grayscale8);
      int dark = 0;
      for (int y = 0; y < grey.height(); y += 4) {
         const uchar *p = grey.constScanLine(y);
         for (int x = 0; x < grey.width(); x += 4)
            if (p[x] < 128)
               dark++;
      }
      qDebug() << "dark pixels" << dark;
      QVERIFY(dark > 100);

      /* a fresh object reading the file from disk must see the rotated
         page too, and its preview must not be blank */
      {
         Filemax fresh(dir, "testfile.max", nullptr);
         QVERIFY(fresh.load() == nullptr);
         QImage fimage;
         QVERIFY(!fresh.getImage(3, false, fimage, size, trueSize, bpp,
                                 false));
         qDebug() << "fresh size" << fimage.size();
         QVERIFY(nearlySame(fimage, image));

         QPixmap pixmap;
         QVERIFY(!fresh.getPreviewPixmap(3, pixmap, false));
         QImage pgrey =
            pixmap.toImage().convertToFormat(QImage::Format_Grayscale8);
         int pdark = 0;
         for (int y = 0; y < pgrey.height(); y++) {
            const uchar *p = pgrey.constScanLine(y);
            for (int x = 0; x < pgrey.width(); x++)
               if (p[x] < 128)
                  pdark++;
         }
         qDebug() << "preview size" << pixmap.size() << "dark" << pdark;
         QVERIFY(pdark > 20);

         /* the preview of a 1-bit page holds greyscale, so text scaled
            down to a twenty-fourth comes out as a range of greys, not
            the four levels the 2bpp preview MaxView uses can hold */
         QSet<int> levels;
         for (int y = 0; y < pgrey.height(); y++) {
            const uchar *p = pgrey.constScanLine(y);
            for (int x = 0; x < pgrey.width(); x++)
               levels.insert(p[x]);
         }
         qDebug() << "preview levels" << levels.size();
         QVERIFY(levels.size() > 4);
      }
   }

   // --- pdf: rotation works, mirroring is not available ---
   {
      copyFixture("testpdf.pdf", tmp.path());
      Filepdf pdf(dir, "testpdf.pdf", nullptr);
      QVERIFY(pdf.load() == nullptr);

      QImage orig, image;
      QSize size, trueSize;
      int bpp;
      QVERIFY(!pdf.getImage(0, false, orig, size, trueSize, bpp, false));

      QVERIFY(!pdf.transformPage(0, File::Transform_rotate90));
      QVERIFY(!pdf.getImage(0, false, image, size, trueSize, bpp, false));
      QCOMPARE(image.width(), orig.height());
      QCOMPARE(image.height(), orig.width());

      QVERIFY(!pdf.transformPage(0, File::Transform_rotate270));
      QVERIFY(!pdf.getImage(0, false, image, size, trueSize, bpp, false));
      QCOMPARE(image.size(), orig.size());

      err_info *err = pdf.transformPage(0, File::Transform_hflip);
      QVERIFY(err && err->errnum == ERR_no_available_for_this_file_type);
   }

   // --- jpeg: rotation is applied to the page file ---
   {
      copyFixture("colour_plasma.jpg", tmp.path());
      Filejpeg jpg(dir, "colour_plasma.jpg", nullptr);
      QVERIFY(jpg.load() == nullptr);

      QImage orig, image;
      QSize size, trueSize;
      int bpp;
      QVERIFY(!jpg.getImage(0, false, orig, size, trueSize, bpp, false));

      QVERIFY(!jpg.transformPage(0, File::Transform_rotate90));
      QVERIFY(!jpg.getImage(0, false, image, size, trueSize, bpp, false));
      QCOMPARE(image.width(), orig.height());
      QCOMPARE(image.height(), orig.width());

      // the change is on disk, not just in memory
      QImage ondisk(dir + "colour_plasma.jpg");
      QCOMPARE(ondisk.width(), orig.height());
   }

   // --- the inverse helper undoes each transform ---
   QCOMPARE(File::transformInverse(File::Transform_rotate90),
            File::Transform_rotate270);
   QCOMPARE(File::transformInverse(File::Transform_rotate270),
            File::Transform_rotate90);
   QCOMPARE(File::transformInverse(File::Transform_rotate180),
            File::Transform_rotate180);
   QCOMPARE(File::transformInverse(File::Transform_hflip),
            File::Transform_hflip);
}

/* Check that PDF pages render correctly after rotation. A 1-bit page
   falls back to poppler because PoDoFo 0.9.8 refuses to decode the
   streams of images with under 8 bits per pixel (its predictor checks
   misread the image's BitsPerComponent as predictor parameters). A
   colour page goes through PoDoFo's image extraction, which returns
   the raw scan, so the render must apply the page's /Rotate itself */
void TestFile::testPdfMonoRender()
{
   QTemporaryDir tmp;
   QString dir = tmp.path() + "/";

   copyFixture("testfile.max", tmp.path());
   Filemax max(dir, "testfile.max", nullptr);
   QVERIFY(max.load() == nullptr);

   File *pdf = File::createFile(dir, "converted.pdf", nullptr,
                                File::Type_pdf);
   QVERIFY(pdf);
   QVERIFY(pdf->create() == nullptr);
   Operation op("Convert file", 0, 0);
   QVERIFY(max.copyTo(pdf, 3, op, false) == nullptr);
   QCOMPARE(pdf->pagecount(), max.pagecount());

   // every page must render, including the 1-bit one
   for (int page = 0; page < pdf->pagecount(); page++) {
      QImage image;
      QSize size, trueSize;
      int bpp;
      err_info *e = pdf->getImage(page, false, image, size, trueSize, bpp,
                                  false);
      QVERIFY2(!e, qPrintable(QString("page %1: %2").arg(page)
                              .arg(e ? e->errstr : "")));
      QVERIFY(!image.isNull());
   }

   /* rotating the 1-bit page and re-rendering follows the same path as
      the page view's refresh after a rotation */
   QVERIFY(!pdf->transformPage(3, File::Transform_rotate90));
   QImage image;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!pdf->getImage(3, false, image, size, trueSize, bpp, false));
   QVERIFY(!image.isNull());

   /* the colour page 0 goes through PoDoFo's image extraction, which
      returns the raw scan; the render must still reflect the page's
      rotation, so a rotated page renders rotated rather than upright */
   QImage colour;
   QVERIFY(!pdf->getImage(0, false, colour, size, trueSize, bpp, false));
   QVERIFY(colour.width() != colour.height());

   QVERIFY(!pdf->transformPage(0, File::Transform_rotate90));
   QVERIFY(!pdf->getImage(0, false, image, size, trueSize, bpp, false));
   QCOMPARE(image.width(), colour.height());
   QCOMPARE(image.height(), colour.width());
   QImage want = File::transformImage(colour, File::Transform_rotate90);
   QVERIFY(nearlySame(image, want));
   delete pdf;
}

/* Compare a page rendered after transformPage() against the original
   render transformed in memory. The stored page may be wider than the
   reference because of format padding, so compare the reference-sized
   region; lossless pages must match exactly, JPEG pages nearly */
static void checkTransformedRender(Filemax &max, int pagenum,
                                   File::e_transform op, bool lossless)
{
   QImage a, b;
   QSize size, trueSize;
   int bpp;

   QVERIFY(!max.getImage(pagenum, false, a, size, trueSize, bpp, false));

   /* crop the decode padding so the reference matches what
      transformPage() stores */
   if (a.size() != size)
      a = a.copy(QRect(QPoint(0, 0), size));

   QVERIFY(!max.transformPage(pagenum, op));
   QVERIFY(!max.getImage(pagenum, false, b, size, trueSize, bpp, false));

   QImage expected = File::transformImage(a, op);
   QVERIFY(b.width() >= expected.width());
   QVERIFY(b.width() < expected.width() + 32);
   QVERIFY(b.height() >= expected.height());
   QVERIFY(b.height() < expected.height() + 32);

   QImage got = b.copy(0, 0, expected.width(), expected.height())
                   .convertToFormat(QImage::Format_Grayscale8);
   QImage want = expected.convertToFormat(QImage::Format_Grayscale8);
   QCOMPARE(got.size(), want.size());

   if (lossless) {
      /* the two routes must produce identical bytes: compare each row
         directly (the rows are compared individually because the
         scanline padding bytes are not meaningful) */
      for (int y = 0; y < want.height(); y++)
         QVERIFY2(!memcmp(got.constScanLine(y), want.constScanLine(y),
                          want.width()),
                  qPrintable(QString("row %1 differs").arg(y)));
   } else {
      // JPEG tiles are recompressed, so allow a small mean difference
      qint64 total = 0;
      for (int y = 0; y < want.height(); y++) {
         const uchar *pg = got.constScanLine(y);
         const uchar *pw = want.constScanLine(y);
         for (int x = 0; x < want.width(); x++)
            total += qAbs(int(pg[x]) - int(pw[x]));
      }
      qint64 mean = total / (qint64(want.width()) * want.height());
      QVERIFY2(mean < 4, qPrintable(QString("mean pixel diff %1")
                                    .arg(mean)));
   }

   // put the page back for the next check
   QVERIFY(!max.transformPage(pagenum, File::transformInverse(op)));
}

void TestFile::testTransformMatchesReference()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";
   copyFixture("testfile.max", tmp.path());

   Filemax max(dir, "testfile.max", nullptr);
   QVERIFY(max.load() == nullptr);

   // page 3 is a 1-bit text page, compressed losslessly
   checkTransformedRender(max, 3, File::Transform_rotate90, true);
   checkTransformedRender(max, 3, File::Transform_rotate180, true);
   checkTransformedRender(max, 3, File::Transform_vflip, true);

   // page 0 is a colour page with JPEG tiles, so allow encoding loss
   checkTransformedRender(max, 0, File::Transform_rotate90, false);
   checkTransformedRender(max, 0, File::Transform_rotate180, false);
}

void TestFile::testTransformEmptyStack()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   /* create an empty stack, as the app does for a new stack which has
      not been scanned into yet */
   Filemax max(dir, "empty.max", nullptr);
   QVERIFY(max.create() == nullptr);
   QVERIFY(max.flush() == nullptr);
   QCOMPARE(max.pagecount(), 0);

   qint64 size_before = QFileInfo(dir + "empty.max").size();
   QVERIFY(size_before > 0);

   // there is nothing to rotate, so this must fail rather than crash
   err_info *err = max.transformPage(0, File::Transform_rotate90);
   QVERIFY(err != nullptr);

   // and the file must be untouched and still loadable
   QCOMPARE(QFileInfo(dir + "empty.max").size(), size_before);
   Filemax fresh(dir, "empty.max", nullptr);
   QVERIFY(fresh.load() == nullptr);
   QCOMPARE(fresh.pagecount(), 0);
}

void TestFile::testTransformImageCache()
{
   QTemporaryDir tmp;
   QString dir = tmp.path() + "/";
   QVERIFY(!copyFixture("testfile.max", tmp.path()).isEmpty());

   Filemax max(dir, "testfile.max", nullptr);
   QVERIFY(max.load() == nullptr);

   QImage before, cached, ondisk, again;
   QSize size, trueSize;
   int bpp;

   // page 0 is colour, so its rotated image is cached for reuse
   QVERIFY(!max.getImage(0, false, before, size, trueSize, bpp, false));
   QVERIFY(bpp > 1);

   // after rotating, page 0's image is cached and the next read returns
   // the rotated page
   QVERIFY(!max.transformPage(0, File::Transform_rotate90));
   QCOMPARE(max._xform_page, 0);
   QVERIFY(!max.getImage(0, false, cached, size, trueSize, bpp, false));
   QCOMPARE(cached.width(), before.height());
   QCOMPARE(cached.height(), before.width());

   // the cached image agrees with a fresh decode from the file, so the
   // file was written correctly and the cache is not stale
   {
      Filemax fresh(dir, "testfile.max", nullptr);
      QVERIFY(fresh.load() == nullptr);
      QVERIFY(!fresh.getImage(0, false, ondisk, size, trueSize, bpp, false));
   }
   QCOMPARE(cached.size(), ondisk.size());
   QVERIFY(nearlySame(cached, ondisk));

   // any flush (e.g. from another edit) invalidates the cache, so a
   // later read cannot return a stale image
   QVERIFY(max.flush() == nullptr);
   QCOMPARE(max._xform_page, -1);
   QVERIFY(!max.getImage(0, false, again, size, trueSize, bpp, false));
   QVERIFY(nearlySame(again, ondisk));
}

void TestFile::testRemoveRestorePages()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";
   QVERIFY(!copyFixture("testfile.max", tmp.path()).isEmpty());

   Filemax max(dir, "testfile.max", nullptr);
   QVERIFY(max.load() == nullptr);
   int orig = max.pagecount();
   QVERIFY(orig > 1);

   // capture the first page so we can check it survives the round-trip
   QImage before, restored;
   QSize size, trueSize;
   int bpp;
   QVERIFY(!max.getImage(0, false, before, size, trueSize, bpp, false));

   // mark the first page for deletion
   QBitArray pages(orig);
   pages.setBit(0);
   QByteArray del_info;
   int count = 1;

   QVERIFY(max.removePages(pages, del_info, count) == nullptr);
   QCOMPARE(max.pagecount(), orig - 1);

   // undo the delete: this reads the deleted page's chunks back from disc,
   // which used to crash because the file was left closed
   QVERIFY(max.restorePages(pages, del_info, count) == nullptr);
   QCOMPARE(max.pagecount(), orig);

   // the restored page still decodes to the same image
   QVERIFY(!max.getImage(0, false, restored, size, trueSize, bpp, false));
   QCOMPARE(restored.size(), before.size());
}


void TestFile::testJpegAnnotations()
{
   if (QStandardPaths::findExecutable("exiftool").isEmpty())
      QSKIP("exiftool is not installed");

   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   QImage img(80, 60, QImage::Format_RGB32);
   img.fill(Qt::white);
   QVERIFY(img.save(dir + "photo.jpg", "JPG"));

   {
      Filejpeg jf(dir, "photo.jpg", nullptr);
      QVERIFY(jf.load() == nullptr);

      QHash<int, QString> updates;
      updates[File::Annot_author] = "An Author";
      updates[File::Annot_title] = "A Title";
      updates[File::Annot_ocr] = "line one\nline two";
      QVERIFY(jf.putAnnot(updates) == nullptr);
   }

   // a fresh object reads the values back from the file itself
   Filejpeg again(dir, "photo.jpg", nullptr);
   QVERIFY(again.load() == nullptr);

   QString text;
   QVERIFY(again.getAnnot(File::Annot_author, text) == nullptr);
   QCOMPARE(text, QString("An Author"));
   QVERIFY(again.getAnnot(File::Annot_title, text) == nullptr);
   QCOMPARE(text, QString("A Title"));

   /* the ocr text spans lines; it used to be dropped entirely
      because it had no exif tag to go to */
   QVERIFY(again.getAnnot(File::Annot_ocr, text) == nullptr);
   QVERIFY2(text.contains("line one"), qPrintable(text));
   QVERIFY2(text.contains("line two"), qPrintable(text));
}


void TestFile::testJpegTransform()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";

   /* left half black, right half white so a flip is visible */
   QImage img(60, 40, QImage::Format_RGB32);
   img.fill(Qt::white);
   {
      QPainter paint(&img);
      paint.fillRect(0, 0, 30, 40, Qt::black);
   }
   QVERIFY(img.save(dir + "photo.jpg", "JPG"));

   {
      Filejpeg jf(dir, "photo.jpg", nullptr);
      QVERIFY(jf.load() == nullptr);
      QVERIFY(jf.transformPage(0, File::Transform_rotate90) == nullptr);
   }

   // the rotation lands on disk with swapped dimensions
   QImage turned(dir + "photo.jpg");
   QCOMPARE(turned.size(), QSize(40, 60));

   {
      Filejpeg jf(dir, "photo.jpg", nullptr);
      QVERIFY(jf.load() == nullptr);
      QVERIFY(jf.transformPage(0, File::Transform_rotate270) == nullptr);
   }
   QImage back(dir + "photo.jpg");
   QCOMPARE(back.size(), QSize(60, 40));

   // a horizontal flip swaps the dark and light halves
   {
      Filejpeg jf(dir, "photo.jpg", nullptr);
      QVERIFY(jf.load() == nullptr);
      QVERIFY(jf.transformPage(0, File::Transform_hflip) == nullptr);
   }
   QImage flipped(dir + "photo.jpg");
   QVERIFY(qGray(flipped.pixel(5, 20)) > 128);    // now light on the left
   QVERIFY(qGray(flipped.pixel(55, 20)) < 128);   // and dark on the right
}


/* Feed a synthetic 24-bit page through a Paperstack with auto colour on
   and return the depth it was stored at, with the coverage string */
static int scanSynthetic (const QByteArray &rgb, int width, int height,
                          QString &coverage, int chunk = 0,
                          Paperstack::t_sideways sideways = Paperstack::Sideways_no,
                          bool front = true, Filepage **mpp = NULL,
                          bool auto_size = true)
{
   Paperstack stack ("stack", "page", false);
   QMutex mutex;
   Filepage *mp = NULL;

   stack.setAutoColour (true);
   stack.setAutoSize (auto_size);
   stack.setSideways (sideways);
   stack.addImage (width, height, 24, width * 3, front, false);
   /* feed the page in chunks of the given size, as a back end delivering
      raw data does, or all at once */
   if (!chunk)
      chunk = rgb.size ();
   for (int pos = 0; pos < rgb.size (); pos += chunk)
      stack.addImageBytes ((unsigned char *)rgb.data () + pos,
                           qMin (chunk, rgb.size () - pos));
   coverage = stack.coverageStr ();
   err_info *err = stack.confirmImage (mp, mutex);
   if (err || !mp)
      return -1;
   int depth = mp->_depth;
   if (mpp)
      *mpp = mp;
   else
      delete mp;
   return depth;
}

//! whether the given pixel of a stored mono page is black
static bool monoPixel (const Filepage *mp, int x, int y)
{
   const unsigned char *data = (const unsigned char *)mp->_data.constData ();

   return data [y * mp->_stride + (x >> 3)] & (0x80 >> (x & 7));
}

/* Sheets fed sideways are turned upright as they are stored: the front a
   quarter turn one way and the back, seen from the other side of the
   sheet, the other way */
/* The scanner scans the full width of its window. Fed sideways the page's
   height lies along that width and nothing trims it, so a page shorter
   than the window leaves the backing showing past the sheet's edge. The
   sheet is paper-bright somewhere along every line of it; the backing
   never is, so the page is cut off where the brightness stops */
void TestFile::testSidewaysCrop()
{
   const int width = 400, height = 400, paper = 300;
   QByteArray page (width * height * 3, (char)255);
   unsigned char *p = (unsigned char *)page.data ();
   QString cov;
   Filepage *mp;

   /* the sheet fills the first 'paper' columns; the grey backing shows
      beyond it, never as bright as the paper */
   for (int y = 0; y < height; y++)
      for (int x = paper; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 215;
         }

   // a line of text on the sheet, so it is not a blank page
   for (int y = 40; y < 44; y++)
      for (int x = 20; x < 280; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         }

   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_right, true, &mp), 1);
   /* turned upright the scan's width becomes the height, and it should
      stop at the sheet rather than run on to the end of the window,
      give or take the margin left for skew */
   QCOMPARE (mp->_width, height);
   QVERIFY (mp->_height >= paper);
   QVERIFY (mp->_height <= paper + width / 25);
   delete mp;

   /* a blank sheet has no content to find, but it is still paper: it
      must be cut in the same place, not thrown away */
   page.fill ((char)255);
   for (int y = 0; y < height; y++)
      for (int x = paper; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 215;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_right, true, &mp), 1);
   QVERIFY (mp->_height >= paper);
   QVERIFY (mp->_height <= paper + width / 25);
   delete mp;

   /* a scan with nothing paper-bright anywhere gives nothing to cut
      on, so the window is kept whole rather than guessed at */
   page.fill ((char)240);
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_right, true, &mp), 1);
   QCOMPARE (mp->_height, width);
   delete mp;

   /* with auto-size off the user has asked for the page size they set,
      so the sheet is stored as wide as the window */
   page.fill ((char)255);
   for (int y = 0; y < height; y++)
      for (int x = paper; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 215;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_right, true, &mp, false),
             1);
   QCOMPARE (mp->_height, width);
   delete mp;
}


void TestFile::testSideways()
{
   const int width = 200, height = 100;
   QByteArray page (width * height * 3, (char)255);
   unsigned char *p = (unsigned char *)page.data ();
   QString cov;
   Filepage *mp;

   // a black square in the top-left corner of the scan
   for (int y = 0; y < 20; y++)
      for (int x = 0; x < 20; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         }

   /* the top of the page at the left of the front: a quarter turn
      clockwise puts the square top right */
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_left, true, &mp), 1);
   QCOMPARE (mp->_width, height);
   QCOMPARE (mp->_height, width);
   QVERIFY (monoPixel (mp, 90, 10));
   QVERIFY (!monoPixel (mp, 10, 10));
   QVERIFY (!monoPixel (mp, 10, 190));
   delete mp;

   // the back of that sheet turns the other way: the square goes bottom left
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_left, false, &mp), 1);
   QCOMPARE (mp->_width, height);
   QVERIFY (monoPixel (mp, 10, 190));
   QVERIFY (!monoPixel (mp, 90, 10));
   delete mp;

   // with the top at the right, the front turns anticlockwise instead
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_top_right, true, &mp), 1);
   QVERIFY (monoPixel (mp, 10, 190));
   delete mp;

   // fed upright, the page is stored as scanned
   QCOMPARE (scanSynthetic (page, width, height, cov, 0,
                            Paperstack::Sideways_no, true, &mp), 1);
   QCOMPARE (mp->_width, width);
   QVERIFY (monoPixel (mp, 10, 10));
   delete mp;
}

/* The render thread decodes a page of a stack while the GUI thread adds
   pages to the same stack, which is what happens all through a scan.
   Adding a page grows the chunk list, moving it in memory, so a decode
   walking it must not be left holding where it used to be: that crashed
   the page renderer in decode_tiledata() */

void TestFile::testAddWhileRendering()
{
   QTemporaryDir tmp;
   QVERIFY (tmp.isValid ());

   QString dir = tmp.path () + "/";

   QVERIFY (!copyFixture ("testfile.max", tmp.path ()).isEmpty ());

   Filemax max (dir, "testfile.max", nullptr);

   QVERIFY (!max.load ());
   QVERIFY (max.pagecount () > 0);

   // a page to add over and over
   const int width = 200, height = 200;
   QByteArray rgb (width * height * 3, (char)255);
   unsigned char *p = (unsigned char *)rgb.data ();

   for (int y = 20; y < height - 20; y += 8)
      for (int x = 10; x < width - 10; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;

         px [0] = px [1] = px [2] = 0;
         }

   /* a page can only be added once, since the file takes over the
      memory it holds, so make one for each */
   const int adds = 40;
   QString cov;
   QList<Filepage *> page;

   for (int i = 0; i < adds; i++)
      {
      Filepage *mp = NULL;

      QCOMPARE (scanSynthetic (rgb, width, height, cov, 0,
                               Paperstack::Sideways_no, true, &mp), 1);
      QVERIFY (mp);
      page << mp;
      }

   int pages = max.pagecount ();
   QAtomicInt decoded (0), stop (0);
   QFuture<void> render = QtConcurrent::run ([&] ()
      {
      while (!stop.loadAcquire ())
         {
         QImage image;
         QSize size, trueSize;
         int bpp;

         if (max.getImage (0, false, image, size, trueSize, bpp, false))
            break;
         decoded.fetchAndAddRelaxed (1);
         }
      });

   for (int i = 0; i < adds; i++)
      QVERIFY (!max.addPage (page [i], false));
   stop.storeRelease (1);
   render.waitForFinished ();

   QVERIFY (decoded.loadRelaxed () > 0);
   QCOMPARE (max.pagecount (), pages + adds);
   QVERIFY (!max.flush ());
   qDeleteAll (page);
}


/* Real pages from the scanner, with what each should be stored as, so
   that a change made for one kind of page cannot quietly change what
   happens to another. test/files/corpus/corpus.txt says where they came
   from and how to add one */

void TestFile::testCorpus()
{
   QFile manifest (QString ("test/corpus/corpus.txt"));

   QVERIFY (manifest.open (QIODevice::ReadOnly | QIODevice::Text));

   QTextStream in (&manifest);
   int pages = 0;

   while (!in.atEnd ())
      {
      QString line = in.readLine ().trimmed ();

      if (line.isEmpty () || line.startsWith ('#'))
         continue;

      QStringList field = line.split (' ', Qt::SkipEmptyParts);

      QVERIFY2 (field.size () >= 4, qPrintable (line));

      QString name = field [0];
      Paperstack::t_sideways sideways =
         field [1] == "left" ? Paperstack::Sideways_top_left
            : field [1] == "right" ? Paperstack::Sideways_top_right
            : Paperstack::Sideways_no;
      int depth = field [2].toInt ();
      int height = field [3].toInt ();

      QImage im (QString ("test/corpus/") + name);

      QVERIFY2 (!im.isNull (), qPrintable (name));
      im = im.convertToFormat (QImage::Format_RGB888);

      QByteArray rgb;

      for (int y = 0; y < im.height (); y++)
         rgb.append ((const char *)im.constScanLine (y), im.width () * 3);

      QString cov;
      Filepage *mp = NULL;
      int got = scanSynthetic (rgb, im.width (), im.height (), cov, 0,
                               sideways, true, &mp);

      QVERIFY2 (mp, qPrintable (name));
      QVERIFY2 (got == depth,
                qPrintable (QString ("%1: stored at %2 bpp, expected %3 (%4)")
                            .arg (name).arg (got).arg (depth).arg (cov)));
      if (height)
         {
         int slack = height / 50;

         QVERIFY2 (qAbs (mp->_height - height) <= slack,
                   qPrintable (QString ("%1: stored %2 high, expected %3")
                               .arg (name).arg (mp->_height).arg (height)));
         }
      delete mp;
      pages++;
      }
   QVERIFY (pages >= 5);
}


void TestFile::testAutoColour()
{
   const int width = 200, height = 200;
   QByteArray page (width * height * 3, (char)255);
   unsigned char *p = (unsigned char *)page.data ();
   QString cov;

   /* black text-like strokes on white with soft edges, and some print
      showing through from the back of the sheet: mono, as text is */
   for (int y = 10; y < height - 10; y += 10)
      for (int x = 0; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         px [-3 * width] = px [1 - 3 * width] = px [2 - 3 * width] = 150;
         px [3 * width] = px [1 + 3 * width] = px [2 + 3 * width] = 200;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 1);
   QVERIFY (cov.endsWith (" mono"));

   /* the same page in chunks that split pixels between them, as raw
      data from a back end arrives: the pixels must stay in step, or the
      stroke edges read as colour */
   QCOMPARE (scanSynthetic (page, width, height, cov, 1001), 1);
   QVERIFY (cov.endsWith (" mono"));

   /* add a bold black heading and the mid-grey shadow of the paper edge
      that the scanner leaves along the top and bottom: still mono */
   for (int y = 40; y < 60; y++)
      for (int x = 20; x < 180; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 10;
         }
   for (int y = 0; y < height; y++)
      if (y < 8 || y >= height - 12)
         for (int x = 0; x < width; x++)
            {
            unsigned char *px = p + (y * width + x) * 3;
            px [0] = px [1] = px [2] = 160;
            }
   QCOMPARE (scanSynthetic (page, width, height, cov), 1);
   QVERIFY (cov.endsWith (" mono"));

   // the same page with a dark red mark on it: enough colour to keep
   for (int y = 80; y < 100; y++)
      for (int x = 20; x < 60; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = 90; px [1] = 30; px [2] = 30;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 24);
   QVERIFY (!cov.endsWith (" mono") && !cov.endsWith (" grey"));

   /* a page of text with a note written on it in pencil: the note is
      mid-toned all through, which mono would throw away, so the page is
      kept as grey although the print on it is as black as ever */
   page.fill ((char)255);
   for (int y = 10; y < height - 10; y += 10)
      for (int x = 0; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 1);
   QVERIFY (cov.endsWith (" mono"));

   for (int y = 101; y < 110; y++)
      for (int x = 40; x < 140; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 150;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 8);
   QVERIFY (cov.endsWith (" grey"));

   /* a page of text with a small stamp in blue ink on it: by area the
      stamp is half the colour a page needs to be called colour, but it
      is coloured through and through, which the fringes along the edges
      of black print are not */
   page.fill ((char)255);
   for (int y = 10; y < height - 10; y += 10)
      for (int x = 0; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         }
   for (int y = 100; y < 115; y++)
      for (int x = 60; x < 75; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = 40; px [1] = 60; px [2] = 190;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 24);
   QVERIFY (!cov.endsWith (" mono") && !cov.endsWith (" grey"));

   /* a page of text with a picture on it that is ink and paper with no
      mid-tones at all, as an engraving is: the tones say nothing, but
      no print fills a block of the page solidly */
   page.fill ((char)255);
   for (int y = 10; y < height - 10; y += 10)
      for (int x = 0; x < width; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         }
   for (int y = 20; y < 100; y++)
      for (int x = 20; x < 100; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 0;
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 8);
   QVERIFY (cov.endsWith (" grey"));

   /* a blank page with a small dark photograph on it, a twentieth of
      the page in the darker mid-tones: grey */
   page.fill ((char)255);
   for (int y = 80; y < 120; y++)
      for (int x = 50; x < 100; x++)
         {
         unsigned char *px = p + (y * width + x) * 3;
         px [0] = px [1] = px [2] = 70 + ((x * 7 + y * 3) % 60);
         }
   QCOMPARE (scanSynthetic (page, width, height, cov), 8);
   QVERIFY (cov.endsWith (" grey"));
}


/* Feed a page to the scan path as the back end delivers it and say
   whether it came out marked blank

   \param rgb    the page, as RGB pixels
   \param w, h   its size
   \param jpeg   true to hand it over as JPEG, as a colour scan arrives */
static bool scanIsBlank (const QByteArray &rgb, int w, int h, bool jpeg)
{
   Paperstack stack ("stack", "page", jpeg);
   QMutex mutex;
   Filepage *mp = NULL;
   QByteArray data = rgb;

   if (jpeg)
      {
      QImage im ((const uchar *)rgb.constData (), w, h, w * 3,
                 QImage::Format_RGB888);
      QBuffer buf (&data);

      data.clear ();
      buf.open (QIODevice::WriteOnly);
      im.save (&buf, "JPEG", 90);
      }
   stack.setBlankPolicy (Paperstack::ignore, 500);
   stack.addImage (w, h, 24, w * 3, true, jpeg);
   stack.addImageBytes ((unsigned char *)data.data (), data.size ());
   if (stack.confirmImage (mp, mutex) || !mp)
      return false;

   bool blank = mp->_mark_blank;

   delete mp;
   return blank;
}


/* The scanner sends a colour page as JPEG, so blank detection has to
   work on that as well as on raw pixels: a colour scan is the only kind
   which arrives compressed, and a blank sheet in the middle of one used
   to be kept as a page like any other */
void TestFile::testBlankJpeg()
{
   const int w = 400, h = 600;
   QByteArray blank (w * h * 3, (char)255);
   QByteArray printed = blank;
   unsigned char *p = (unsigned char *)printed.data ();

   /* something on the page, well over the 1-in-500 pixels which the
      blank threshold allows */
   for (int y = 100; y < 160; y++)
      for (int x = 100; x < 300; x++)
         p [(y * w + x) * 3] = p [(y * w + x) * 3 + 1] =
            p [(y * w + x) * 3 + 2] = 0;

   QVERIFY2 (scanIsBlank (blank, w, h, false), "raw blank page");
   QVERIFY2 (!scanIsBlank (printed, w, h, false), "raw printed page");
   QVERIFY2 (scanIsBlank (blank, w, h, true), "blank page sent as JPEG");
   QVERIFY2 (!scanIsBlank (printed, w, h, true), "printed page sent as JPEG");
}


/* A scanner told to stop at the foot of the sheet ends the page early,
   and what it said about the page beforehand stands: the JPEG header
   promises the lines of the whole window while the data stops short of
   them. The page must be stored at the length which arrived, or what is
   in the file cannot be read back out of it */
void TestFile::testShortJpegPage()
{
   const int w = 64, h = 400;
   QImage im (w, h, QImage::Format_RGB888);

   for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
         im.setPixel (x, y, qRgb ((x * 37) & 0xff, (y * 53) & 0xff,
                                  ((x + y) * 17) & 0xff));

   QByteArray data;
   QBuffer buf (&data);

   QVERIFY (buf.open (QIODevice::WriteOnly));
   QVERIFY (im.save (&buf, "JPEG", 90));
   buf.close ();
   data.truncate (data.size () / 4);

   Paperstack stack ("stack", "page", true);
   QMutex mutex;
   Filepage *mp = NULL;

   stack.addImage (w, h, 24, w * 3, true, true);
   stack.addImageBytes ((unsigned char *)data.data (), data.size ());
   QVERIFY (!stack.confirmImage (mp, mutex));
   QVERIFY (mp);
   QVERIFY2 (mp->_height > 0 && mp->_height < h,
             qPrintable (QString ("stored %1 lines of a page whose data "
                                  "holds fewer than %2").arg (mp->_height)
                         .arg (h)));
   delete mp;
}


/* A sheet narrower than the window the scanner was given leaves the
   backing showing down both sides of the page. With auto-size on it is
   cut off at the edge of the sheet, and for a page kept as the
   scanner's JPEG that is done by moving the JPEG's blocks about, so
   what is left is still what the scanner sent */
void TestFile::testNarrowSheetCrop()
{
   const int w = 640, h = 480, edge = 160;
   QImage im (w, h, QImage::Format_RGB888);

   /* the sheet in the middle with a line of print down it, the
      scanner's backing either side */
   for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
         {
         /* the backing is a light grey, as the scanner's is: lighter
            than ink and darker than paper */
         bool sheet = x >= edge && x < w - edge;
         int v = sheet ? 254 : 225;

         if (sheet && (y % 40) < 8 && x > edge + 20 && x < w - edge - 20)
            v = 0;
         im.setPixel (x, y, qRgb (v, v, v));
         }

   QByteArray data;
   QBuffer buf (&data);

   QVERIFY (buf.open (QIODevice::WriteOnly));
   QVERIFY (im.save (&buf, "JPEG", 90));
   buf.close ();

   Paperstack stack ("stack", "page", true);
   QMutex mutex;
   Filepage *mp = NULL;

   stack.setAutoSize (true);
   stack.addImage (w, h, 24, w * 3, true, true);
   stack.addImageBytes ((unsigned char *)data.data (), data.size ());
   QVERIFY (!stack.confirmImage (mp, mutex));
   QVERIFY (mp);

   /* the sheet is 320 wide, and a margin is left round it; the cut can
      only fall on a JPEG block boundary, so a little more is kept */
   QVERIFY2 (mp->_width >= w - 2 * edge && mp->_width < w - edge,
             qPrintable (QString ("stored %1 wide for a sheet %2 wide in "
                                  "a window %3 wide").arg (mp->_width)
                         .arg (w - 2 * edge).arg (w)));
   QCOMPARE (mp->_height, h);
   delete mp;
}


/* A sheet which went through the feeder a little askew has its corners
   out beyond the rest of it. The columns they lie in are mostly
   backing, so a rule which asks for half a column of paper puts the
   edge of the sheet inside them, and cutting the page there takes the
   corners off along with the backing */
void TestFile::testSkewedSheetCrop()
{
   const int w = 800, h = 600, sheet = 500, lean = 40;
   QByteArray page (w * h * 3, (char)0);
   unsigned char *p = (unsigned char *)page.data ();

   /* the sheet leans across the window by `lean' columns from top to
      bottom, as a sheet fed a degree or two off square does */
   for (int y = 0; y < h; y++)
      {
      int at = (w - sheet) / 2 - lean / 2 + lean * y / h;

      for (int x = 0; x < w; x++)
         {
         bool on_sheet = x >= at && x < at + sheet;
         int v = on_sheet ? 254 : 225;

         // a line of print, well inside the sheet
         if (on_sheet && (y % 50) < 6 && x > at + 40 && x < at + sheet - 40)
            v = 0;
         p [(y * w + x) * 3] = p [(y * w + x) * 3 + 1] =
            p [(y * w + x) * 3 + 2] = v;
         }
      }

   QString cov;
   Filepage *mp = NULL;
   int depth = scanSynthetic (page, w, h, cov, 0, Paperstack::Sideways_no,
                              true, &mp, true);

   QVERIFY (depth > 0);
   QVERIFY (mp);

   /* the sheet reaches from the leftmost point of its top edge to the
      rightmost point of its foot, which is `sheet' plus the lean; the
      page must be at least that wide, and not the whole window */
   QVERIFY2 (mp->_width >= sheet + lean,
             qPrintable (QString ("stored %1 wide, but the sheet covers "
                                  "%2 columns of the window")
                         .arg (mp->_width).arg (sheet + lean)));
   QVERIFY2 (mp->_width < w,
             qPrintable (QString ("stored %1 wide: the backing was kept")
                         .arg (mp->_width)));
   delete mp;
}


/* Which pages to remove is decided by the page view, whose list of
   pages is not always the stack's: it can hold pages which never
   reached the file, or belong to another stack. That used to stop the
   program with an assertion at the end of a scan, taking the pages
   which had just been scanned with it */
void TestFile::testRemovePagesMismatch()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   const QString dir = tmp.path() + "/";
   QVERIFY(!copyFixture("testfile.max", tmp.path()).isEmpty());

   Filemax max(dir, "testfile.max", nullptr);
   QVERIFY(max.load() == nullptr);
   int orig = max.pagecount();
   QVERIFY(orig > 1);

   /* a list longer than the stack, asking for a page past the end of
      it as well as one it has */
   QBitArray pages(orig + 3);
   pages.setBit(0);
   pages.setBit(orig + 2);
   QByteArray del_info;
   int count = 2;

   QVERIFY(max.removePages(pages, del_info, count) == nullptr);

   // the page the stack has is gone, and the one it does not have is
   // reported as not removed
   QCOMPARE(max.pagecount(), orig - 1);
   QCOMPARE(count, 1);

   /* asking for every page that is left, which is what a scan of
      nothing but blank sheets comes to, empties the stack rather than
      stopping the program */
   int left = max.pagecount();
   QBitArray all(left);

   all.fill(true);
   count = left;
   QVERIFY(max.removePages(all, del_info, count) == nullptr);
   QCOMPARE(max.pagecount(), 0);
}


/* The scanner's JPEG arrives in pieces, and the decoder is fed each
   piece as it comes. A marker in it can say to skip over more than has
   arrived so far, which leaves the decoder to take the rest of that
   skip out of the pieces which follow: get that wrong and it starts
   again in the middle of the marker.

   This checks that a page put together from small pieces, with a
   marker across several of them, still comes out whole. A decoder
   which loses its place here usually finds it again at the next
   marker, so this is a check that the pieces are handled at all
   rather than a test of what happens when it does not */
void TestFile::testJpegInChunks()
{
   const int w = 256, h = 400;
   QImage im (w, h, QImage::Format_RGB888);

   for (int y = 0; y < h; y++)
      for (int x = 0; x < w; x++)
         im.setPixel (x, y, qRgb ((x * 5) & 0xff, (y * 3) & 0xff,
                                  ((x + y) * 7) & 0xff));

   QByteArray data;
   QBuffer buf (&data);

   QVERIFY (buf.open (QIODevice::WriteOnly));
   QVERIFY (im.save (&buf, "JPEG", 85));
   buf.close ();

   /* a comment holding more than one piece of the data, so the skip
      over it must carry from one piece to the next, as the scanner's
      own markers do */
   QByteArray marker ("\xff\xfe", 2);
   const int len = 5000;

   marker.append ((char)((len + 2) >> 8));
   marker.append ((char)((len + 2) & 0xff));

   /* what is inside it looks like the start of a picture, so a decoder
      which comes back in the middle of it goes off making one rather
      than quietly finding its way again */
   for (int i = 0; i < len; i += 4)
      marker.append ("\xff\xd8\xff\xc4", 4);
   marker.truncate (4 + len);
   /* put it where the picture starts, so that a decoder which comes
      back in the wrong place lands in the middle of the picture data
      rather than in the headers, where it would find its feet again */
   int sos = data.indexOf (QByteArray ("\xff\xda", 2));

   QVERIFY (sos > 0);
   data.insert (sos, marker);

   QString cov;
   Filepage *mp = NULL;
   Paperstack stack ("stack", "page", true);
   QMutex mutex;

   stack.addImage (w, h, 24, w * 3, true, true);
   for (int pos = 0; pos < data.size (); pos += 1024)
      stack.addImageBytes ((unsigned char *)data.data () + pos,
                           qMin (1024, data.size () - pos));
   cov = stack.coverageStr ();
   QVERIFY (!stack.confirmImage (mp, mutex));
   QVERIFY (mp);
   QCOMPARE (mp->_height, h);

   /* every line of the page is there: a decode which lost its way
      leaves the rest of the page as a flat fill */
   QImage out;
   QVERIFY (out.loadFromData (mp->_data, "JPEG"));
   out = out.convertToFormat (QImage::Format_RGB888);
   QCOMPARE (out.height (), h);

   int last = 0;

   for (int y = 0; y < out.height (); y++)
      {
      QRgb first = out.pixel (0, y);
      bool flat = true;

      for (int x = 1; x < out.width () && flat; x++)
         flat = out.pixel (x, y) == first;
      if (!flat)
         last = y + 1;
      }
   QVERIFY2 (last == h,
             qPrintable (QString ("the page has picture down to line %1 "
                                  "of %2").arg (last).arg (h)));
   delete mp;
}
