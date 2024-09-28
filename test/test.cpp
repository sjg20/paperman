#include <QtTest/QtTest>

#include "test.h"
#include "test_utils.h"

//QTEST_MAIN(TestPaperman)

int test_run(int argc, char **argv, QApplication *app)
{
   TESTLIB_SELFCOVERAGE_START(#TestUtils)
   QT_PREPEND_NAMESPACE(QTest::Internal::callInitMain)<TestUtils>();
   app->setAttribute(Qt::AA_Use96Dpi, true);
   QTEST_DISABLE_KEYPAD_NAVIGATION
   TestUtils tc;
   QTEST_SET_MAIN_SOURCE_PATH
   return QTest::qExec(&tc, argc, argv);
}
