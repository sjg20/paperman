#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <cstring>

#include "test.h"

#include "test_desktopui.h"
#include "test_dirmodel.h"
#include "test_dirview.h"
#include "test_file.h"
#include "test_ops.h"
#include "test_pageinfo.h"
#include "test_pagewidget.h"
#include "test_qscanner.h"
#include "test_utils.h"
#include "test_searchserver.h"
#include "test_clientconf.h"
#include "test_fakescan.h"
#include "test_localbackend.h"
#include "test_ocrsearch.h"

//QTEST_MAIN(TestPaperman)

static TestUtils TEST_UTILS("utils");
static TestOps TEST_OPS("ops");
static TestDesktopUi TEST_DESKTOPUI("desktopui");
static TestDirmodel TEST_DIRMODEL("dirmodel");
static TestDirview TEST_DIRVIEW("dirview");
static TestFile TEST_FILE("file");
static TestPageinfo TEST_PAGEINFO("pageinfo");
static TestPagewidget TEST_PAGEWIDGET("pagewidget");
static TestQscanner TEST_QSCANNER("qscanner");
static TestSearchServer TEST_SEARCHSERVER("searchserver");
static TestOcrSearch TEST_OCRSEARCH("ocrsearch");
static TestLocalBackend TEST_LOCALBACKEND("localbackend");
static TestClientConf TEST_CLIENTCONF("clientconf");
static TestFakescan TEST_FAKESCAN("fakescan");

int test_run(int, char **in_argv, QApplication *,
             const char *filter)
{
   int status = 0;
   const char *className = filter;
   const char *funcName = nullptr;

   /* Keep the tests away from the user's own settings. The widgets read
    * the saved splitter sizes and repository list from QSettings and
    * write them back on close, so without this the results depend on
    * the machine and a test run scribbles on the real configuration */
   QTemporaryDir settings_dir;
   if (!settings_dir.isValid()) {
      fprintf(stderr, "Cannot create a temporary settings directory\n");
      return 1;
   }
   QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                      settings_dir.path());
#ifdef Q_OS_MACOS
   /* the native settings on macOS are preferences, kept by a service
      which takes no notice of setPath(), so use files there instead */
   QSettings::setDefaultFormat(QSettings::IniFormat);
   QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                      settings_dir.path());
#endif

   /* Likewise keep them away from the user's scanners: libsane is told
    * about the fake one in test/fakescan and nothing else. This has to
    * happen before anything asks libsane for a scanner */
   Fakescan::setup();

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
