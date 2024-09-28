#include <QtTest/QtTest>

#include "test.h"
#include "test_utils.h"

//QTEST_MAIN(TestPaperman)

static TestUtils TEST_UTILS("utils");

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
