#ifndef TEST_OPS_H
#define TEST_OPS_H

#include <QObject>

#include "suite.h"

class QTemporaryDir;

class Mainwindow;
class TreeItem;

class TestOps: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testStartup();
   void testAddRepos();

   // Test duplicating a stack
   void testDuplicate();

private:
   void duplicate(Mainwindow *me, QModelIndex &repo_ind);
};

#endif // TEST_OPS_H
