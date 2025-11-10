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
