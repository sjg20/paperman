#include <QtTest/QtTest>
#include <cstring>

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

int test_run(int argc, char **in_argv, QApplication *,
             const char *filter)
{
   int status = 0;
   const char *className = filter;
   const char *funcName = nullptr;

   // Split "Class::function" into class filter and function filter
   static char filterBuf[256];
   if (filter) {
      const char *sep = strstr(filter, "::");
      if (sep) {
         size_t len = sep - filter;
         if (len >= sizeof(filterBuf))
            len = sizeof(filterBuf) - 1;
         memcpy(filterBuf, filter, len);
         filterBuf[len] = '\0';
         className = filterBuf;
         funcName = sep + 2;
      }
   }

   // Build argv for QTest, optionally including function name
   char *qt_argv[3] = { in_argv[0], nullptr, nullptr };
   int qt_argc = 1;
   if (funcName) {
      qt_argv[qt_argc++] = const_cast<char *>(funcName);
   }

   auto runTest = [&status, qt_argc, &qt_argv](QObject* obj) {
       status |= QTest::qExec(obj, qt_argc, qt_argv);
   };

   auto &suite = Suite::suite();

   // List available suites and exit
   if (filter && !strcmp(filter, "list")) {
      for (auto it = suite.begin(); it != suite.end(); ++it) {
         const Test *test = *it;
         printf("  %s\n", test->metaObject()->className());
      }
      return 0;
   }

   // Run suites
   for (auto it = suite.begin(); it != suite.end(); ++it) {
      const Test *test = *it;
      if (className && strcmp(test->metaObject()->className(), className))
         continue;
      qDebug() << "suite" << test->_name;
      runTest(*it);
   }

   return status;
}
