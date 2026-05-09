#include <QtTest/QtTest>

#include "pagemodel.h"
#include "test_pageinfo.h"


void TestPageinfo::testUpdatePixmapAfterDisconnect()
{
   Pageinfo info;

   // Simulate the race where invalidate() runs before a queued scanDone()
   // arrives from the scanner thread. After invalidate() the model pointer
   // is null, but scanDone() then sets _rescale = true. updatePixmap()
   // must not dereference the null model.
   info.invalidate();
   info.scanDone("100", false);

   QCOMPARE(info.updatePixmap(), false);
}
