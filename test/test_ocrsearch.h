#ifndef TEST_OCRSEARCH_H
#define TEST_OCRSEARCH_H

#include <QObject>

#include "suite.h"

class TestOcrSearch: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   //! Test OCR indexing a file
   void testOcrIndexing();

   //! Test searching indexed OCR text
   void testOcrSearch();

   //! Test re-indexing existing OCR text
   void testReindexing();

   //! Test search with no results
   void testSearchNoResults();

   //! Test with realistic sample document text
   void testRealDocument();

   //! the words, lines and paragraphs read from tesseract's TSV output
   void testOcrPageFromTsv();

   //! a page's words are kept and read back, in any script
   void testOcrPageBytes();

   //! tesseract reads a page's words, with where each is
   void testOcrPageTesseract();
};

#endif // TEST_OCRSEARCH_H
