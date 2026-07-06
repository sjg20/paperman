#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstring>

#include "err.h"
#include "file.h"
#include "filejpeg.h"
#include "filemax.h"
#include "fileother.h"
#include "filepdf.h"

#include "op.h"

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
