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

private:
   // Helper to make HTTP GET request and return response
   QString httpGet(const QString& url);

   // Helper to create test files
   void createTestFiles(const QString& path);
};

#endif // TEST_SEARCHSERVER_H
