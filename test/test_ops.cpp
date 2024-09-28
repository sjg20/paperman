#include <QtTest/QtTest>

#include "../utils.h"
#include "test.h"

#include "desktopwidget.h"
#include "mainwindow.h"
#include "test_ops.h"

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
   auto path = setupRepo();
   err_info *err = desktop->addDir(path);
   Q_ASSERT(!err);
}
