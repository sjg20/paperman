#include <QtTest/QtTest>

#include "test.h"

#include "test_dirmodel.h"
#include "test_ops.h"
#include "test_utils.h"
#include "test_searchserver.h"
#include "test_ocrsearch.h"

//QTEST_MAIN(TestPaperman)

static TestUtils TEST_UTILS("utils");
static TestOps TEST_OPS("ops");
static TestDirmodel TEST_DIRMODEL("dirmodel");
static TestSearchServer TEST_SEARCHSERVER("searchserver");
static TestOcrSearch TEST_OCRSEARCH("ocrsearch");

int test_run(int argc, char **in_argv, QApplication *)
{
   int status = 0;
   auto runTest = [&status, argc, in_argv](QObject* obj) {
       status |= QTest::qExec(obj, argc, in_argv);
   };

   // run suite
   auto &suite = Suite::suite();
   for (auto it = suite.begin(); it != suite.end(); ++it) {
      const Test *test = *it;
      qDebug() << "suite" << test->_name;
      runTest(*it);
   }

   return status;
}
