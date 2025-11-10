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
};

#endif // TEST_OCRSEARCH_H
