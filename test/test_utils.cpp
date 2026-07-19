#include <QtTest/QtTest>

#include "../config.h"

#include <pwd.h>
#include <unistd.h>

#include "../filemax.h"
#include "../imageadjust.h"
#include "../utils.h"

extern "C" {
   #define HAVE_STDINT_H 1
   #include "../md5.h"
   };

#include <QBuffer>
#include <QPainter>
#include "test.h"

#include "test_utils.h"

void TestUtils::testDetectYear()
{
   int pos;

   QCOMPARE(utilDetectYear("bills/2024/03mar/fred.max", pos), 2024);
   QCOMPARE(pos, 6);
   QCOMPARE(utilDetectYear("bills/tax/2023/fred.max", pos), 2023);
   QCOMPARE(pos, 10);
   QCOMPARE(utilDetectYear("bills/tax/1899/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/20201/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/12020/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/a2020/fred.max", pos), 2020);
   QCOMPARE(pos, 11);
   QCOMPARE(utilDetectYear("bills/tax/12020b/fred.max", pos), 0);
   QCOMPARE(utilDetectYear("bills/tax/a2020b/fred.max", pos), 2020);
   QCOMPARE(pos, 11);
   QCOMPARE(utilDetectYear("2024/fred.max", pos), 2024);
   QCOMPARE(pos, 0);
   QCOMPARE(utilDetectYear("2023", pos), 2023);
   QCOMPARE(pos, 0);
   QCOMPARE(utilDetectYear("01jan", pos), 0);
}

void TestUtils::testDetectMonth()
{
   int pos;

   QCOMPARE(utilDetectMonth("bills/2024/03mar/fred.max", pos), 3);
   QCOMPARE(pos, 11);
   QCOMPARE(utilDetectMonth("bills/2024/04-mar/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("bills/2024/07mar/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("bills/2024/07-marc/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("bills/2024/07-mmar/fred.max", pos), 0);
   QCOMPARE(utilDetectMonth("03mar/fred.max", pos), 3);
   QCOMPARE(pos, 0);
   QCOMPARE(utilDetectMonth("08aug", pos), 8);
   QCOMPARE(pos, 0);
}

void TestUtils::testDetectMatches()
{
   QStringList matches, final, missing;
   QDate date = QDate(2024, 9, 1);

   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 0);

   // Should ignore a month if it isn't preceeded by a year
   matches << "bills/03mar";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 0);

   matches << "bills/2024/03mar";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 0);

   // Check it can detect a month
   matches << "bills/2024/09sep";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2024/09sep");
   QCOMPARE(missing.size(), 0);

   // Check it can detect a year
   matches << "bills/2024";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 2);
   QCOMPARE(final[0], "bills/2024/09sep");
   QCOMPARE(final[1], "bills/2024");
   QCOMPARE(missing.size(), 0);

   // Advance to Oct 2024; check it can suggest adding a month
   date = QDate(2024, 10, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2024");
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2024/10oct");

   // Make sure it only suggests this if the year is right
   date = QDate(2023, 10, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);

   // Advance to Jan 2025; check it doesn't suggest adding January, since Dec
   // isn't there. But it should suggest adding 2025
   date = QDate(2025, 1, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025");

   // Advance to Feb 2025; check that it suggests adding 2025
   date = QDate(2025, 2, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025");

   // Make to Jan 2025; now add Dec24 and check that it suggests adding Jan25
   date = QDate(2025, 1, 15);
   matches << "bills/2024/12dec";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025/01jan");

   // ...but not if Jan25 already exists
   matches << "bills/2025/01jan";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 1);
   QCOMPARE(final[0], "bills/2025/01jan");
   QCOMPARE(missing.size(), 0);
if (0) {
   // See that it suggests Dec24 as well
   date = QDate(2025, 1, 15);
   matches << "bills/2024/12dec";
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2025/01jan");
}
   // Now move to 2026 and make sure it suggests to add that
   matches << "bills/2025";
   date = QDate(2026, 1, 1);
   final = utilDetectMatches(date, matches, missing);
   QCOMPARE(final.size(), 0);
   QCOMPARE(missing.size(), 1);
   QCOMPARE(missing[0], "bills/2026");
}

void TestUtils::compare_trees(TreeItem *node1, TreeItem *node2)
{
   if (!node1 || !node2) {
      QCOMPARE(node1, node2);
      return;
   }
   QCOMPARE(node1->dirName(), node2->dirName());
   QCOMPARE(node1->childCount(), node2->childCount());
   for (int i = 0; i < node1->childCount(); i++)
      compare_trees(node1->child(i), node2->child(i));
}

void TestUtils::touch(const QString& dirpath, QString fname)
{
   QFile file(dirpath + "/" + fname);
   file.open(QIODevice::WriteOnly);
}

void TestUtils::createDirStructure(QTemporaryDir& tmp)
{
   QVERIFY(tmp.isValid());

   QString root = tmp.path();
   QDir dir(root);
   dir.mkpath(root + "/dir2");
   dir.mkpath(root + "/somedir/more-subdir");
   touch(root, "1");
   touch(root, "2");
   touch(root, "3");
   touch(root, "asc2");
   touch(root + "/somedir", "somefile");
   touch(root + "/somedir/more-subdir", "another-file");
   touch(root + "/dir2", "4");
}

void TestUtils::testScanDir()
{
   QTemporaryDir tmp;
   TreeItem *root, *chk;

   createDirStructure(tmp);
   root = utilScanDir(tmp.path(), nullptr);
   QString fname = cacheFile(tmp.path());
   utilWriteTree(fname, root);
   chk = utilReadTree(fname, tmp.path());
   compare_trees(root, chk);
   TreeItem::freeTree(chk);
   QCOMPARE(root->dirName(), tmp.path());
   QCOMPARE(root->childCount(), 6);
   TreeItem *child = root->child("2");
   QCOMPARE(child->dirName(), "2");

   QByteArray ba;
   QTextStream stream(&ba);
   root->write(stream, 0);
   stream.flush();
   stream.seek(0);
   QCOMPARE(stream.readLine(), " - 1");
   QCOMPARE(stream.readLine(), " - 2");
   QCOMPARE(stream.readLine(), " - 3");
   QCOMPARE(stream.readLine(), " - asc2");
   QCOMPARE(stream.readLine(), " + dir2");
   QCOMPARE(stream.readLine(), "  - 4");
   QCOMPARE(stream.readLine(), " + somedir");
   QCOMPARE(stream.readLine(), "  + more-subdir");
   QCOMPARE(stream.readLine(), "   - another-file");
   QCOMPARE(stream.readLine(), "  - somefile");
   QVERIFY(stream.atEnd());

   TreeItem::freeTree(root);
}

void TestUtils::testAdopt()
{
    QTemporaryDir tmp;
    TreeItem *root, *chk;

    createDirStructure(tmp);
    root = utilScanDir(tmp.path(), nullptr);

    QCOMPARE(root->childCount(), 6);

    chk = new TreeItem({"wibble"});
    chk->adopt(root);

    QCOMPARE(root->childCount(), 0);
    delete root;

    QCOMPARE(chk->childCount(), 6);
    QCOMPARE(chk->child(0)->data(0).toString(), "1");
    QCOMPARE(chk->child(1)->data(0).toString(), "2");
    QCOMPARE(chk->child(2)->data(0).toString(), "3");
    QCOMPARE(chk->child(3)->data(0).toString(), "asc2");
    QCOMPARE(chk->child(4)->data(0).toString(), "dir2");
    QCOMPARE(chk->child(5)->data(0).toString(), "somedir");

    QCOMPARE(chk->child(0)->childCount(), 0);
    QCOMPARE(chk->child(1)->childCount(), 0);
    QCOMPARE(chk->child(2)->childCount(), 0);
    QCOMPARE(chk->child(3)->childCount(), 0);
    QCOMPARE(chk->child(4)->childCount(), 1);
    QCOMPARE(chk->child(5)->childCount(), 2);
}

void TestUtils::testImageDepth()
{
   // Pure black image should be detected as 1bpp
   QImage black(10, 10, QImage::Format_ARGB32);
   black.fill(QColor(0, 0, 0));
   QCOMPARE(utilImageDepth(black), 1);

   // Mid-grey image should be detected as 8bpp
   QImage grey(10, 10, QImage::Format_ARGB32);
   grey.fill(QColor(128, 128, 128));
   QCOMPARE(utilImageDepth(grey), 8);

   // Colour image should be detected as 24bpp
   QImage colour(10, 10, QImage::Format_ARGB32);
   colour.fill(QColor(255, 0, 0));
   QCOMPARE(utilImageDepth(colour), 24);

   // Mostly black with one grey pixel should be 8bpp
   QImage mixGrey(10, 10, QImage::Format_ARGB32);
   mixGrey.fill(QColor(0, 0, 0));
   mixGrey.setPixelColor(5, 5, QColor(128, 128, 128));
   QCOMPARE(utilImageDepth(mixGrey), 8);

   // Mostly grey with one colour pixel should be 24bpp
   QImage mixColour(10, 10, QImage::Format_ARGB32);
   mixColour.fill(QColor(128, 128, 128));
   mixColour.setPixelColor(5, 5, QColor(255, 0, 0));
   QCOMPARE(utilImageDepth(mixColour), 24);

   // Colour noise within tolerance (channel diff <= 30) is greyscale
   QImage noise(10, 10, QImage::Format_ARGB32);
   noise.fill(QColor(128, 128, 128));
   noise.setPixelColor(3, 3, QColor(130, 125, 135));  // diff = 10
   QCOMPARE(utilImageDepth(noise), 8);

   // Small number of colour pixels below 0.5% threshold is greyscale
   QImage noisy(10, 10, QImage::Format_ARGB32);
   noisy.fill(QColor(128, 128, 128));
   noisy.setPixelColor(3, 3, QColor(200, 100, 135));  // diff > 30
   QCOMPARE(utilImageDepth(noisy), 24);

   // 8-bit indexed greyscale image returns 8 directly
   QImage indexed(10, 10, QImage::Format_Indexed8);
   QVector<QRgb> table;
   for (int i = 0; i < 256; i++)
      table << qRgb(i, i, i);
   indexed.setColorTable(table);
   indexed.fill(128);
   QCOMPARE(utilImageDepth(indexed), 8);
}

void TestUtils::testFindItem()
{
    const TreeItem *chk;
    QTemporaryDir tmp;
    TreeItem *root, *chkw;

    createDirStructure(tmp);
    root = utilScanDir(tmp.path(), nullptr);

    chk = root->findItem("");
    Q_ASSERT(chk != nullptr);
    QCOMPARE(chk, root);

    chk = root->findItem("3");
    Q_ASSERT(chk != nullptr);
    Q_ASSERT(chk != root);
    QCOMPARE(chk->data(0).toString(), "3");
    chkw = root->findItemW("3");
    QCOMPARE(chk, chkw);

    chk = root->findItem("dir2/4");
    Q_ASSERT(chk != nullptr);
    Q_ASSERT(chk != root);
    QCOMPARE(chk->data(0).toString(), "4");
    chkw = root->findItemW("dir2/4");
    QCOMPARE(chk, chkw);

    chk = root->findItem("somedir/more-subdir/another-file");
    Q_ASSERT(chk != nullptr);
    Q_ASSERT(chk != root);
    QCOMPARE(chk->data(0).toString(), "another-file");
    chkw = root->findItemW("somedir/more-subdir/another-file");
    QCOMPARE(chk, chkw);
}

void TestUtils::testPreview8bppRoundtrip()
{
   // Create a test pattern with varying grey levels (image convention:
   // 0=black, 255=white)
   const int width = 100;
   const int height = 50;
   const int size = width * height;
   byte raw[size];

   for (int y = 0; y < height; y++)
      for (int x = 0; x < width; x++)
         raw[y * width + x] = (x * 255) / (width - 1);

   // Encode
   byte encoded[size];
   int enc_len = encode_8bpp_preview(raw, size, encoded);

   QVERIFY(enc_len > 0);
   QVERIFY(enc_len <= size);

   // Decode: the decoder inverts the polarity (image 0=black becomes
   // preview 255=black) so the expected value is 255 - raw
   byte decoded[size];

   memset(decoded, 0, size);
   int dec_len = decode_8bpp_preview(encoded, encoded + enc_len,
                                     decoded, decoded + size);
   QCOMPARE(dec_len, size);

   // Check each pixel against the expected inverted value, within
   // quantisation tolerance (nibble reduces 256 levels to 16)
   int max_err = 0;

   for (int i = 0; i < size; i++) {
      int expected = 255 - raw[i];
      int diff = abs(expected - (int)decoded[i]);

      if (diff > max_err)
         max_err = diff;
   }

   // Nibble quantisation can produce up to ~17 error per pixel
   QVERIFY2(max_err <= 17,
            qPrintable(QString("max pixel error %1 exceeds tolerance 17")
                       .arg(max_err)));

   // Verify the decoded data is not all one value
   int sum = 0;

   for (int i = 0; i < size; i++)
      sum += decoded[i];
   QVERIFY2(sum > 0, "decoded preview is all-zero");
   QVERIFY2(sum < size * 255, "decoded preview is all-255");
}

void TestUtils::testPreviewFromJpeg()
{
   // Load the deterministic greyscale test image
   QImage img(testSrc + "/greyscale_gradient.jpg");

   QVERIFY2(!img.isNull(), "failed to load greyscale_gradient.jpg");

   // Convert to 8bpp greyscale if needed
   if (img.format() != QImage::Format_Indexed8)
      img = img.convertToFormat(QImage::Format_Indexed8);

   int pw = img.width() / 24;
   int ph = img.height() / 24;
   int pwidth = (pw + 3) & ~3;  // word-aligned
   int psize = pwidth * ph;

   // Scale down to preview size (matching scale_8bpp logic)
   QByteArray preview(psize, 0);

   for (int y = 0; y < ph; y++) {
      for (int x = 0; x < pw; x++) {
         int sum = 0;
         int count = 0;

         for (int sy = 0; sy < 24 && y * 24 + sy < img.height(); sy++) {
            const uchar *line = img.scanLine(y * 24 + sy);

            for (int sx = 0; sx < 24; sx++)
               sum += line[x * 24 + sx];
            count += 24;
         }
         preview[y * pwidth + x] = sum / count;
      }
   }

   // Encode
   QByteArray encoded(psize, 0);
   int enc_len = encode_8bpp_preview((byte *)preview.data(), psize,
                                     (byte *)encoded.data());
   QVERIFY(enc_len > 0);

   // Decode
   byte *enc = (byte *)encoded.data();
   QByteArray decoded(psize, 0);

   decode_8bpp_preview(enc, enc + enc_len,
                       (byte *)decoded.data(),
                       (byte *)decoded.data() + psize);

   // Compress and check deterministic size
   QByteArray compressed = qCompress(decoded);
   int csize = compressed.size();

   QVERIFY2(csize > 50, qPrintable(
      QString("compressed preview too small: %1").arg(csize)));
   QCOMPARE(csize, 1412);
}

void TestUtils::testUserName()
{
   /* the user name must be found even with no controlling terminal
      (tests, cron, IDE launches). If it silently comes back empty the
      per-user .papertree cache filename loses its suffix and a stale,
      shared cache file is read instead */
   struct passwd *pw = getpwuid(getuid());
   QVERIFY(pw != nullptr);
   QCOMPARE(utilUserName(), QString(pw->pw_name));
}


/* a colour JPEG with enough rows that /24 previews are a few pixels */
static QByteArray makeTestJpeg(int width, int height, bool grey)
{
   QImage img(width, height,
              grey ? QImage::Format_Grayscale8 : QImage::Format_RGB32);
   img.fill(Qt::white);
   QPainter paint(&img);
   paint.fillRect(0, 0, width / 2, height, Qt::black);
   paint.end();

   QByteArray jpeg;
   QBuffer buf(&jpeg);
   buf.open(QIODevice::WriteOnly);
   img.save(&buf, "JPG");
   return jpeg;
}

void TestUtils::testJpegThumbnailCorrupt()
{
   /* A scanner can deliver short or misframed JPEG data (e.g. when the
      scan window does not match the paper); the thumbnailer must cope
      rather than writing through an unallocated buffer. */
   QByteArray good = makeTestJpeg(480, 720, false);
   byte *dest;
   int destSize;
   cpoint size;

   // sanity: the intact picture thumbnails fine
   dest = nullptr;
   destSize = 0;
   QVERIFY(jpeg_thumbnail((byte *)good.data(), good.size(), &dest,
                          &destSize, &size) != 0);
   QVERIFY(dest != nullptr);
   free(dest);

   /* truncated mid-stream: decode stops early and the preview shrinks,
      but whatever comes back must be consistent */
   QByteArray cut = good.left(good.size() / 2);
   dest = nullptr;
   destSize = 0;
   jpeg_thumbnail((byte *)cut.data(), cut.size(), &dest, &destSize, &size);
   if (dest)
      free(dest);

   /* A stray start-of-image marker in the stream: libjpeg warns while
      the header is still being parsed, which used to divide by zero in
      the warning handler (the decode scale is not set up yet).  The
      decode either fails cleanly with no output buffer, or recovers
      and produces a thumbnail; either way it must not crash. */
   QByteArray twoSoi = good;
   int insert = twoSoi.indexOf((char)0xda);   // just before scan data
   if (insert < 0)
      insert = twoSoi.size() / 4;
   twoSoi.insert(insert + 100, "\xff\xd8", 2);
   dest = nullptr;
   destSize = 0;
   int rc = jpeg_thumbnail((byte *)twoSoi.data(), twoSoi.size(), &dest,
                           &destSize, &size);
   if (rc == 0)
      QVERIFY(dest == nullptr);
   if (dest)
      free(dest);

   /* the same again for a greyscale picture, whose copy-out path pads
      each row and must not read past the decoded data */
   QByteArray greyCut = makeTestJpeg(444, 720, true);
   greyCut.truncate(greyCut.size() / 2);
   dest = nullptr;
   destSize = 0;
   jpeg_thumbnail((byte *)greyCut.data(), greyCut.size(), &dest,
                  &destSize, &size);
   if (dest)
      free(dest);
}


void TestUtils::testMd5()
{
   unsigned char digest[16];

   // RFC 1321 appendix test vectors
   md5_buffer("", 0, digest);
   QCOMPARE(QByteArray((const char *)digest, 16).toHex(),
            QByteArray("d41d8cd98f00b204e9800998ecf8427e"));

   md5_buffer("abc", 3, digest);
   QCOMPARE(QByteArray((const char *)digest, 16).toHex(),
            QByteArray("900150983cd24fb0d6963f7d28e17f72"));

   // long enough to exercise the block loop (> 64 bytes)
   QByteArray many(1000, 'a');
   md5_buffer(many.constData(), many.size(), digest);
   QCOMPARE(QByteArray((const char *)digest, 16).toHex(),
            QByteArray("cabe45dcc9ae5b66ba86600cca6b8ba8"));
}


void TestUtils::testJpegThumbnail()
{
   /* a picture large enough that /CONFIG_preview_scale is still a
      few pixels across */
   QImage img(480, 240, QImage::Format_RGB32);
   img.fill(Qt::white);
   QPainter paint(&img);
   paint.fillRect(0, 0, 240, 240, Qt::black);
   paint.end();

   QByteArray jpeg;
   QBuffer buf(&jpeg);
   QVERIFY(buf.open(QIODevice::WriteOnly));
   QVERIFY(img.save(&buf, "JPG"));

   byte *dest = nullptr;
   int destSize = 0;
   cpoint size;
   int valid = jpeg_thumbnail((byte *)jpeg.data(), jpeg.size(), &dest,
                              &destSize, &size);
   QVERIFY(valid != 0);
   QVERIFY(dest != nullptr);
   QCOMPARE(size.x, 480 / CONFIG_preview_scale);
   QCOMPARE(size.y, 240 / CONFIG_preview_scale);
   QVERIFY(destSize > 0);
   free(dest);

   // garbage input is rejected rather than crashing
   QByteArray junk(200, 'x');
   dest = nullptr;
   QCOMPARE(jpeg_thumbnail((byte *)junk.data(), junk.size(), &dest,
                           &destSize, &size), 0);
}


void TestUtils::testImageAdjustWhiten()
{
   /* a "scan" with a dingy grey background and dark text strokes */
   QImage img(200, 100, QImage::Format_RGB32);
   img.fill(QColor(200, 198, 190));
   QPainter paint(&img);
   paint.setPen(QPen(QColor(25, 25, 25), 2));
   for (int i = 0; i < 8; i++)
      paint.drawLine(10, 12 + i * 10, 190, 12 + i * 10);
   paint.end();

   ImageAdjust::apply(img, ImageAdjust::Adjust_whiten);

   // the background is stretched to (near) white...
   QVERIFY(qGray(img.pixel(5, 5)) > 240);
   QVERIFY(qGray(img.pixel(195, 95)) > 240);
   // ...while the text stays dark
   QVERIFY(qGray(img.pixel(100, 12)) < 100);

   QCOMPARE(ImageAdjust::name(ImageAdjust::Adjust_whiten),
            QString("Whiten background"));
   QCOMPARE(ImageAdjust::suffix(ImageAdjust::Adjust_whiten),
            QString("_white"));

   // formats with nothing to do are left alone
   QImage mono(50, 50, QImage::Format_Mono);
   mono.fill(1);
   QImage before = mono;
   ImageAdjust::apply(mono, ImageAdjust::Adjust_whiten);
   QCOMPARE(mono, before);
}
