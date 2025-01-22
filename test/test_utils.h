#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <QObject>

#include "suite.h"

class QTemporaryDir;

class TreeItem;

class TestUtils: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testDetectYear();
   void testDetectMonth();
   void testDetectMatches();
   void testScanDir();
   void testAdopt();
   void testFindItem();
private:
   // Create files in a temporary directory structure used for testing
   void createDirStructure(QTemporaryDir& tmp);

   // Create an empty file in a directory
   void touch(const QString& dirpath, QString fname);

   // Compare two trees recursively
   void compare_trees(TreeItem *node1, TreeItem *node2);
};

#endif // TEST_UTILS_H
