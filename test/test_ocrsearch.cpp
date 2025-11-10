#include "test_ocrsearch.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "err.h"
#include "searchindex.h"

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

   // Test searching for "PDF" - should find it on page 1
   QList<SearchResult> results;
   err = searchIndex.search("PDF", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);
   QCOMPARE(results[0].filename, filename);
   QCOMPARE(results[0].pagenum, 0);  // Page 1 (0-indexed)
   QVERIFY(results[0].snippet.contains("PDF") || results[0].snippet.contains("pdf"));

   // Test searching for "three" - should find it on page 1
   results.clear();
   err = searchIndex.search("three", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);
   QCOMPARE(results[0].pagenum, 0);  // Page 1

   // Test searching for "second" - should find it on page 2
   results.clear();
   err = searchIndex.search("second", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);
   QCOMPARE(results[0].pagenum, 1);  // Page 2 (0-indexed)

   // Test searching for "final" - should find it on page 3
   results.clear();
   err = searchIndex.search("final", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);
   QCOMPARE(results[0].pagenum, 2);  // Page 3 (0-indexed)

   // Test searching for "quick" - should find it on page 2
   results.clear();
   err = searchIndex.search("quick", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);
   QCOMPARE(results[0].pagenum, 1);  // Page 2
   QVERIFY(results[0].snippet.contains("quick"));

   // Test multi-word phrase search
   results.clear();
   err = searchIndex.search("testing PDF Object", results, 100);
   QVERIFY2(!err, err ? err->errstr : "");
   QVERIFY(results.size() > 0);

   qDebug() << "Real document test passed - tested multi-page indexing and searching";
}
