#ifndef TEST_OPS_H
#define TEST_OPS_H

#include <QObject>

#include "suite.h"

class QTemporaryDir;

class TreeItem;

class TestOps: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testStartup();
};

#endif // TEST_OPS_H
