#ifndef TEST_OPS_H
#define TEST_OPS_H

#include <QObject>

#include "suite.h"

class QTemporaryDir;

class Desktopmodel;
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
   void testUnstackPage();

   //! Duplicate a PDF as a Max file and then that as a PDF
   void testDuplicateMax();

   //! Test duplication of odd and even pages
   void testDuplicateEvenOdd();

   //! Test deleting of one or more stacks
   void testDeleteStacks();

   //! Test unstacking stacks
   void testUnstackStacks();

   //! Test renaming stacks
   void testRenameStack();

   //! Test renaming a page
   void testRenamePage();

   //! Test creating a dir
   void testCreateDir();

   //! Test creating a dir inside a directory not in the cache
   void testCreateDirInNewParent();

   //! Test moving a file to a different directory
   void testMoveToDir();

   //! Test changing to a different directory
   void testChangeDir();

   //! Test deleting an empty directory
   void testDeleteDir();

   //! Test renaming a directory
   void testRenameDir();

   //! Test that findFolders suggests the next month directory
   void testFindFoldersSuggestsMonth();

   //! Test that findFolders via the desktop path suggests the next month
   void testFindFoldersSuggestsMonthViaDesktop();

   //! Test that findFolders works after a cache refresh
   void testFindFoldersSuggestsMonthAfterRefresh();

   //! Test that the print options count pages correctly
   void testPrintCountPages();

   //! Test printing the selected stacks to a PDF file
   void testPrintToPdf();

   //! Test the options dialog loads and saves scan settings
   void testOptionsDialog();

private:
   //! Setup the test repo and return its model and root index
   void getTestRepo(Mainwindow *me, Desktopmodel*& model,
                     QModelIndex& repo_ind);

   void duplicate(Mainwindow *me, QModelIndex &repo_ind);

   /**
    * @brief Get ready to duplicate a stack
    * @param me       Mainwindow
    * @param repo_ind Returns index of the repo being used
    * @param max_ind  Returns index of the max file
    */
   void prepareDuplicate(Mainwindow *me, QModelIndex &repo_ind,
                         QModelIndex &max_ind);

};

#endif // TEST_OPS_H
