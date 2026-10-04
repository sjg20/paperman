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

   //! the background reader reads each page not yet read, once
   void testReaderReadsPages();

   //! a page which moves while it is read has its words stored with it
   void testReaderPageMoves();

   //! a stack which goes away while a page is read is let go
   void testReaderStackGone();

   //! what the user types is always a valid query, matching word starts
   void testMatchQuery();

   //! the index follows the stacks as their text changes and they go
   void testIndexSync();

   //! an index kept away from the stacks can be searched while it is
   //! being written, and brought up to date a stack at a time
   void testIndexElsewhere();

   //! text in a stack's OCR annotation, where OCR used to put it, is
   //! found on the stack's first page
   void testIndexOcrAnnotation();

   //! an index built by an older version, which lacks text now found,
   //! is built again
   void testIndexVersion();

   //! the server's reader reads the unread pages of every stack in a
   //! repository but the hidden ones, and puts the words in each in one
   //! step, keeping the stack's permissions
   void testRepoReaderReads();

   //! what has been read, or found to have nothing on it, is not read
   //! again until it changes, even after a restart
   void testRepoReaderResumes();

   //! a stack which changes while it is being read is read again as it
   //! is now, and one which arrives is read at once
   void testRepoReaderChanges();
};

#endif // TEST_OCRSEARCH_H
