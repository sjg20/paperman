#ifndef TEST_SEARCHSERVER_H
#define TEST_SEARCHSERVER_H

#include <QObject>

#include "suite.h"
#include "../serverlog.h"

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
   void testLargePdfProgressive();
   void testLargeMaxProgressive();

private:
   // Helper to make HTTP GET request and return response as string
   QString httpGet(const QString& url);

   // Helper to make HTTP GET request and return raw response bytes
   QByteArray httpGetRaw(const QString& url, int timeoutMs = 5000);

   // Helper to create test files
   void createTestFiles(const QString& path);

   // Helper to fetch a page, verify 200/PDF response, and check the log
   void verifyPageFetch(const QString &fileName, int page,
                        ServerLog::Action expectedAction,
                        qint64 *bodySize = nullptr,
                        int timeoutMs = 10000);
};

#endif // TEST_SEARCHSERVER_H
