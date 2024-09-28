#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "desktopwidget.h"
#include "mainwindow.h"
#include "test_ops.h"

const char TEST_DIR[] = "test/files";

void TestOps::testStartup()
{
   Mainwindow *me;

   me = new Mainwindow();
   delete me;
}

void TestOps::testAddRepos()
{
   Mainwindow me;

   Desktopwidget *desktop = me.getDesktop ();

   // Add our test repo
   err_info *err = desktop->addDir(TEST_DIR);
   Q_ASSERT(!err);
}
