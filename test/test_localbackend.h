#ifndef TEST_LOCALBACKEND_H
#define TEST_LOCALBACKEND_H

#include <QObject>

#include "suite.h"

/** Tests for LocalBackend, the in-process data source the server (and
    local repositories) read from */
class TestLocalBackend : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! Repository listing reports existing and missing roots
   void testRepositories();

   //! Directory browsing lists dirs first, hides dotfiles and
   //! reports unknown paths
   void testBrowse();

   //! Whole-file reads return the bytes, with 404-style errors for
   //! missing files
   void testReadFile();

   //! Content types derive from the filename extension
   void testContentType();

   //! The file cache builds from a directory scan
   void testFileCache();
};

#endif // TEST_LOCALBACKEND_H
