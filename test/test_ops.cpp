#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "mainwindow.h"
#include "test_ops.h"

const char TEST_DIR[] = "test/files";

void TestOps::testStartup()
{
   Mainwindow *me;

   me = new Mainwindow();
   delete me;
}
