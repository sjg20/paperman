#include "test_ocrsearch.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "err.h"
#include "searchindex.h"
#include "ocr.h"
#include "ocrpage.h"
#include "ocrreader.h"
#include "filemax.h"
#include <QBitArray>
#include <QSignalSpy>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>

void TestOcrSearch::testOcrIndexing()
{
   // Create a temporary directory for the test
   QTemporaryDir tempDir;
   QVERIFY(tempDir.isValid());

   QString dirPath = tempDir.path() + "/";

   // Initialize search index
   SearchIndex searchIndex;
   err_info *err = searchIndex.init(dirPath);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(searchIndex.isOpen());

   // Add some test OCR text
   QString testText = "Hello World Test Document";
   QString filename = "test-ocr.max";

   err = searchIndex.addPage(dirPath + filename, filename, 0, testText);
   QVERIFY2(!err, err ? err->errstr : "");

   // Verify the index file was created
   QString indexPath = dirPath + ".paperindex";
   QVERIFY(QFile::exists(indexPath));

   qDebug() << "Index file created:" << indexPath;
}

void TestOcrSearch::testOcrSearch()
{
   // Create a temporary directory for the test
   QTemporaryDir tempDir;
   QVERIFY(tempDir.isValid());

   QString dirPath = tempDir.path() + "/";

   // Initialize search index
   SearchIndex searchIndex;
   err_info *err = searchIndex.init(dirPath);
   QVERIFY2(!err, err ? err->errstr : "");

   // Add test OCR text with searchable content
   QString testText = "Testing Search Feature Find This Text";
   QString filename = "search-test.max";

   err = searchIndex.addPage(dirPath + filename, filename, 0, testText);
   QVERIFY2(!err, err ? err->errstr : "");

   // Search for text
   QList<SearchResult> results;
   err = searchIndex.search("Search", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");

   // Verify we got results
   QVERIFY(results.size() > 0);
   QCOMPARE(results[0].filename, filename);
   QCOMPARE(results[0].pagenum, 0);
   QVERIFY(results[0].snippet.contains("Search") || results[0].snippet.contains("search"));

   qDebug() << "Search found:" << results.size() << "results";
   qDebug() << "Snippet:" << results[0].snippet;
}

void TestOcrSearch::testReindexing()
{
   // Create a temporary directory for the test
   QTemporaryDir tempDir;
   QVERIFY(tempDir.isValid());

   QString dirPath = tempDir.path() + "/";

   QString filename = "reindex-test.max";
   QString testText = "Reindex Test Document";

   // Index the text initially
   SearchIndex searchIndex;
   err_info *err = searchIndex.init(dirPath);
   QVERIFY2(!err, err ? err->errstr : "");

   err = searchIndex.addPage(dirPath + filename, filename, 0, testText);
   QVERIFY2(!err, err ? err->errstr : "");

   // Re-index the same file (simulating re-indexing existing OCR)
   err = searchIndex.addPage(dirPath + filename, filename, 0, testText);
   QVERIFY2(!err, err ? err->errstr : "");

   // Verify we can search the re-indexed text
   QList<SearchResult> results;
   err = searchIndex.search("Reindex", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);

   qDebug() << "Re-indexing test passed";
}

void TestOcrSearch::testSearchNoResults()
{
   // Create a temporary directory for the test
   QTemporaryDir tempDir;
   QVERIFY(tempDir.isValid());

   QString dirPath = tempDir.path() + "/";

   // Create and index a file
   QString filename = "noresults-test.max";
   QString testText = "Sample Document Text";

   SearchIndex searchIndex;
   err_info *err = searchIndex.init(dirPath);
   QVERIFY2(!err, err ? err->errstr : "");

   err = searchIndex.addPage(dirPath + filename, filename, 0, testText);
   QVERIFY2(!err, err ? err->errstr : "");

   // Search for text that doesn't exist
   QList<SearchResult> results;
   err = searchIndex.search("NonexistentWord12345", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");

   // Verify no results
   QCOMPARE(results.size(), 0);

   qDebug() << "No results test passed";
}

void TestOcrSearch::testRealDocument()
{
   // Create a temporary directory for the test
   QTemporaryDir tempDir;
   QVERIFY(tempDir.isValid());

   QString dirPath = tempDir.path() + "/";

   // Initialize search index
   SearchIndex searchIndex;
   err_info *err = searchIndex.init(dirPath);
   QVERIFY2(!err, err ? err->errstr : "");

   // Simulate realistic document text (based on actual sample.max content)
   // Page 1
   QString page1Text =
      "Sample PDF\n"
      "\n"
      "Created for testing PDF Object\n"
      "\n"
      "This PDF is three pages long. Three long pages. Or three short pages,\n"
      "if you prefer. Anyway, it has three pages.";

   // Page 2
   QString page2Text =
      "This is the second page. Not much to see here. Just text and more text.\n"
      "The quick brown fox jumps over the lazy dog. Pack my box with five dozen\n"
      "liquor jugs. How vexingly quick daft zebras jump!";

   // Page 3
   QString page3Text =
      "This is the third and final page. We've made it to the end!\n"
      "Page three of three. That's all, folks.";

   QString filename = "sample.max";

   // Add all three pages to the index
   err = searchIndex.addPage(dirPath + filename, filename, 0, page1Text);
   QVERIFY2(!err, err ? err->errstr : "");

   err = searchIndex.addPage(dirPath + filename, filename, 1, page2Text);
   QVERIFY2(!err, err ? err->errstr : "");

   err = searchIndex.addPage(dirPath + filename, filename, 2, page3Text);
   QVERIFY2(!err, err ? err->errstr : "");

   // Test searching for "PDF" - should find it exactly once on page 1
   QList<SearchResult> results;
   err = searchIndex.search("PDF", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 1);  // Should find exactly 1 result
   QCOMPARE(results[0].filename, filename);
   QCOMPARE(results[0].pagenum, 0);  // Page 1 (0-indexed)
   QVERIFY(results[0].snippet.contains("PDF") || results[0].snippet.contains("pdf"));

   // Test searching for "three" - should find it on page 1 and page 3
   results.clear();
   err = searchIndex.search("three", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 2);  // Should find exactly 2 results (page 1 and page 3)
   QCOMPARE(results[0].pagenum, 0);  // Page 1 (should be first due to more occurrences)

   // Test searching for "second" - should find it exactly once on page 2
   results.clear();
   err = searchIndex.search("second", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 1);  // Should find exactly 1 result
   QCOMPARE(results[0].pagenum, 1);  // Page 2 (0-indexed)

   // Test searching for "final" - should find it exactly once on page 3
   results.clear();
   err = searchIndex.search("final", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 1);  // Should find exactly 1 result
   QCOMPARE(results[0].pagenum, 2);  // Page 3 (0-indexed)

   // Test searching for "quick" - should find it twice on page 2
   results.clear();
   err = searchIndex.search("quick", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 1);  // Should find 1 result (same page, indexed once)
   QCOMPARE(results[0].pagenum, 1);  // Page 2
   QVERIFY(results[0].snippet.contains("quick"));

   // Test multi-word phrase search
   results.clear();
   err = searchIndex.search("testing PDF Object", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 1);  // Should find exactly 1 result

   // Test searching for word that doesn't appear anywhere
   results.clear();
   err = searchIndex.search("xylophone", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 0);  // Should find no results

   // Test searching for another non-existent word
   results.clear();
   err = searchIndex.search("dinosaur", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QCOMPARE(results.size(), 0);  // Should find no results

   qDebug() << "Real document test passed - tested multi-page indexing and searching";
}


void TestOcrSearch::testOcrPageFromTsv()
{
   /* two paragraphs, the first of two lines, the second in another
      block; an empty word and the rows above word level are left out */
   QByteArray tsv =
      "level\tpage_num\tblock_num\tpar_num\tline_num\tword_num\tleft\ttop\twidth\theight\tconf\ttext\n"
      "1\t1\t0\t0\t0\t0\t0\t0\t1000\t800\t-1\t\n"
      "2\t1\t1\t0\t0\t0\t10\t10\t500\t100\t-1\t\n"
      "5\t1\t1\t1\t1\t1\t10\t10\t80\t20\t96.5\tInvoice\n"
      "5\t1\t1\t1\t1\t2\t100\t10\t60\t20\t91\t12345\n"
      "5\t1\t1\t1\t2\t1\t10\t40\t70\t20\t88\tDue\n"
      "5\t1\t1\t1\t2\t2\t90\t40\t30\t20\t12\t \n"
      "5\t1\t2\t1\t1\t1\t10\t200\t90\t20\t95\tThanks\n";
   OcrPage page;

   QVERIFY (OcrPage::fromTsv (tsv, QSize (1000, 800), page));
   QCOMPARE (page.size, QSize (1000, 800));
   QCOMPARE (page.words.size (), 4);
   QCOMPARE (page.words[0].box, QRect (10, 10, 80, 20));
   QCOMPARE (page.words[0].conf, 97);
   QCOMPARE (page.text (), QString ("Invoice 12345\nDue\n\nThanks"));
   QCOMPARE (page.words[2].line, 1);
   QCOMPARE (page.words[3].para, 1);

   QVERIFY (!OcrPage::fromTsv ("not tesseract", QSize (), page));
}

void TestOcrSearch::testOcrPageBytes()
{
   OcrPage page, back;

   page.size = QSize (2480, 3508);
   page.words.append ({QRect (100, 200, 300, 40), "Grüße", 93, 0, 0});
   page.words.append ({QRect (420, 200, 180, 40), "東京", 81, 0, 0});
   page.words.append ({QRect (100, 260, 260, 40), "Łódź", 77, 1, 0});

   QByteArray data = page.toBytes ();
   QVERIFY (!data.isEmpty ());
   QVERIFY (OcrPage::fromBytes (data, back));
   QCOMPARE (back, page);
   QCOMPARE (back.text (), QString ("Grüße 東京\nŁódź"));

   // nothing is kept for a page with no words
   QVERIFY (OcrPage ().toBytes ().isEmpty ());
   QVERIFY (!OcrPage::fromBytes ("{not json", back));
   QVERIFY (back.isEmpty ());
}

void TestOcrSearch::testOcrPageTesseract()
{
   if (QStandardPaths::findExecutable ("tesseract").isEmpty ())
      QSKIP ("tesseract is not installed");

   // a page at 300dpi with two lines of large, clear type
   QImage image (1240, 700, QImage::Format_RGB32);
   image.fill (Qt::white);
   QPainter painter (&image);
   QFont font ("DejaVu Sans");
   font.setPixelSize (60);
   painter.setFont (font);
   painter.setPen (Qt::black);
   painter.drawText (100, 200, "Invoice number 4821");
   painter.drawText (100, 400, "Payment due Friday");
   painter.end ();

   err_info *err = nullptr;
   Ocr *ocr = Ocr::getOcr (err);
   QVERIFY (ocr && !err);

   OcrPage page;
   QVERIFY (!ocr->imageToPage (image, page));
   QCOMPARE (page.size, image.size ());
   QString text = page.text ();
   QVERIFY2 (text.contains ("Invoice") && text.contains ("4821")
             && text.contains ("Friday"), qPrintable (text));
   QVERIFY2 (text.contains ('\n'), qPrintable (text));

   // each word is where it was drawn: the first line near y 150-200
   for (const OcrWord &word : page.words)
      if (word.text == "Invoice")
         {
         QVERIFY2 (word.box.top () > 120 && word.box.bottom () < 220,
                   qPrintable (QString ("box %1,%2 %3x%4")
                               .arg (word.box.x ()).arg (word.box.y ())
                               .arg (word.box.width ())
                               .arg (word.box.height ())));
         QVERIFY (word.box.left () >= 90 && word.box.left () < 130);
         }

   // imageToText() gives the same text
   QString plain;
   QVERIFY (!ocr->imageToText (image, plain));
   QCOMPARE (plain, text);
}


/* Copy the test stack into a directory and open it */
static Filemax *openStack(const QString &src, QTemporaryDir &tmp)
{
   if (!QFile::copy(src + "/testfile.max", tmp.path() + "/testfile.max"))
      return nullptr;

   Filemax *max = new Filemax(tmp.path() + "/", "testfile.max", nullptr);

   if (max->load()) {
      delete max;
      return nullptr;
   }
   return max;
}


// a word naming an image by its pixels
static QString imageName(const QImage &image)
{
   return QString("p%1").arg(qHashBits(image.constBits(),
                                       image.sizeInBytes()));
}


/* An engine which names each page by its pixels, so that what is stored
   can be matched to the page it came from */
static QString fakeRead(QImage &image, OcrPage &page)
{
   page = OcrPage();
   page.size = image.size();
   page.words << OcrWord{QRect(1, 2, 3, 4), imageName(image), 90, 0, 0};
   return QString();
}


static QString pageWord(File *f, int pagenum)
{
   OcrPage ocr;

   if (f->getPageOcr(pagenum, ocr) || ocr.isEmpty())
      return QString();
   return ocr.words[0].text;
}


static QString pixelsWord(File *f, int pagenum)
{
   QImage image;
   QSize size, trueSize;
   int bpp;

   if (f->getImage(pagenum, false, image, size, trueSize, bpp, false))
      return QString();
   return imageName(image);
}


void TestOcrSearch::testReaderReadsPages()
{
   QTemporaryDir tmp;
   QScopedPointer<Filemax> max(openStack(testSrc, tmp));
   QVERIFY(max);
   int count = max->pagecount();

   // one page has been read already, and is left alone
   OcrPage done;
   done.size = QSize(10, 10);
   done.words << OcrWord{QRect(), "already", 100, 0, 0};
   QVERIFY(!max->putPageOcr(1, done));

   OcrReader reader;
   QAtomicInt calls = 0;
   reader.setEngine([&calls](QImage &image, OcrPage &page) {
      calls++;
      return fakeRead(image, page);
   });
   QSignalSpy read(&reader, &OcrReader::pageRead);
   QSignalSpy idle(&reader, &OcrReader::idle);

   reader.addFile(max.data());
   QVERIFY(reader.isBusy());
   QVERIFY(idle.wait(10000));
   QVERIFY(!reader.isBusy());
   QCOMPARE(int(calls), count - 1);
   QCOMPARE(read.count(), count - 1);

   for (int i = 0; i < count; i++)
      QCOMPARE(pageWord(max.data(), i),
               i == 1 ? QString("already") : pixelsWord(max.data(), i));

   // queueing it again reads nothing more
   reader.addFile(max.data());
   QVERIFY(idle.count() == 2 || idle.wait(10000));
   QCOMPARE(int(calls), count - 1);
}


void TestOcrSearch::testReaderPageMoves()
{
   QTemporaryDir tmp;
   QScopedPointer<Filemax> max(openStack(testSrc, tmp));
   QVERIFY(max);
   int count = max->pagecount();
   QVERIFY(count >= 2);

   /* delete the first page while it is being read, so that the page now
      first is not the one which was read */
   OcrReader reader;
   QSemaphore started, go;
   reader.setEngine([&](QImage &image, OcrPage &page) {
      started.release();
      go.acquire();
      return fakeRead(image, page);
   });
   QSignalSpy idle(&reader, &OcrReader::idle);

   reader.addFile(max.data());
   QVERIFY(started.tryAcquire(1, 10000));

   QBitArray pages(count);
   pages.setBit(0);
   QByteArray del_info;
   int del_count = 1;
   QVERIFY(!max->removePages(pages, del_info, del_count));
   go.release(count * 2);

   QVERIFY(idle.wait(10000));
   QCOMPARE(max->pagecount(), count - 1);
   for (int i = 0; i < count - 1; i++)
      QCOMPARE(pageWord(max.data(), i), pixelsWord(max.data(), i));
}


void TestOcrSearch::testReaderStackGone()
{
   QTemporaryDir tmp;
   Filemax *max = openStack(testSrc, tmp);
   QVERIFY(max);

   OcrReader reader;
   QSemaphore started, go;
   reader.setEngine([&](QImage &image, OcrPage &page) {
      started.release();
      go.acquire();
      return fakeRead(image, page);
   });
   QSignalSpy read(&reader, &OcrReader::pageRead);
   QSignalSpy idle(&reader, &OcrReader::idle);

   reader.addFile(max);
   QVERIFY(started.tryAcquire(1, 10000));
   delete max;
   go.release(100);

   QVERIFY(idle.wait(10000));
   QCOMPARE(read.count(), 0);
   QVERIFY(!reader.isBusy());
}
