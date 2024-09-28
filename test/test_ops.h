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

   // Test duplicating a stack and then undoing it
   void testDuplicateUndo();

   // Test duplicating a stack, combining the two and then undoing it
   void testDuplicateStackUndo();

private:
   void duplicate(Mainwindow *me, QModelIndex &repo_ind);
};

#endif // TEST_OPS_H
