#include <QTemporaryDir>
#include <QtTest/QtTest>

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
