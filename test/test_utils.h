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
   //! Test that the user name is found without a controlling terminal
   void testUserName();

   void testDetectYear();
   void testDetectMonth();
   void testDetectMatches();
   void testScanDir();
   void testAdopt();
   void testFindItem();
   void testImageDepth();

   //! Test 8bpp preview encode/decode roundtrip
   void testPreview8bppRoundtrip();

   //! Test preview encode/decode against the greyscale test image
   void testPreviewFromJpeg();

   //! jpeg_thumbnail() survives truncated and corrupt JPEG data
   void testJpegThumbnailCorrupt();
private:
   // Create files in a temporary directory structure used for testing
   void createDirStructure(QTemporaryDir& tmp);

   // Create an empty file in a directory
   void touch(const QString& dirpath, QString fname);

   // Compare two trees recursively
   void compare_trees(TreeItem *node1, TreeItem *node2);
};

#endif // TEST_UTILS_H
