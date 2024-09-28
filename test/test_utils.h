#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <QObject>

class QTemporaryDir;

class TreeItem;

class TestUtils: public QObject
{
   Q_OBJECT
private slots:
   void testDetectYear();
   void testDetectMonth();
   void testDetectMatches();
   void testScanDir();
private:
   // Create files in a temporary directory structure used for testing
   void createDirStructure(QTemporaryDir& tmp);

   // Create an empty file in a directory
   void touch(const QString& dirpath, QString fname);

   // Compare two trees recursively
   void compare_trees(TreeItem *node1, TreeItem *node2);
};

#endif // TEST_UTILS_H
