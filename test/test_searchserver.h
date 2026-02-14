#ifndef TEST_SEARCHSERVER_H
#define TEST_SEARCHSERVER_H

#include <QObject>

#include "suite.h"

class SearchServer;

class TestSearchServer: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testServerStartStop();
   void testStatusEndpoint();
   void testSearchEndpoint();
   void testListEndpoint();
   void testInvalidEndpoint();
   void testMissingSearchParameter();
   void testReposEndpoint();
   void testSearchWithRepo();
   void testFileEndpoint();
   void testFilePageCount();
   void testFilePageExtract();

private:
   // Helper to make HTTP GET request and return response as string
   QString httpGet(const QString& url);

   // Helper to make HTTP GET request and return raw response bytes
   QByteArray httpGetRaw(const QString& url);

   // Helper to create test files
   void createTestFiles(const QString& path);
};

#endif // TEST_SEARCHSERVER_H
