/*
License: GPL-2
*/

#ifndef TEST_ASYNC_H
#define TEST_ASYNC_H

#include <QModelIndex>
#include <QObject>
#include <QTemporaryDir>

#include <memory>

#include "suite.h"

class Desktopwidget;
class Mainwindow;
class ServerThread;

/** Checks that the desktop does not wait for a server

    Each test drives the desktop against a server on its own thread
    which is slow to reply, and checks that the main thread neither
    waited for the server nor stopped handling events while the desktop
    did what was asked.  Flows which still wait are marked as expected
    failures, until they are made asynchronous. */
class TestAsync : public Suite
{
   Q_OBJECT
public:
   // defined where the members' types are complete
   explicit TestAsync (QString name);
   ~TestAsync () override;

private slots:
   void initTestCase ();
   void cleanupTestCase ();
   void init ();

   //! also fails the test if the main thread waited for the server
   void cleanup ();

   //! the checks catch a wait for the server, a stall and a slow reply
   void testHarness ();

   //! attaching a server
   void testConnect ();

   //! attaching a server which never answers, then closing the window
   void testConnectHung ();

   //! attaching a server named by its host at startup
   void testConnectAtStartup ();

   //! opening a folder on the server
   void testOpenFolder ();

   //! opening a folder on a server which never answers
   void testOpenFolderHung ();

   //! leaving a folder whose layout has changed, which saves it
   void testLeaveFolder ();

   //! another client changing a stack in the folder shown
   void testOtherClient ();

   //! locating a stack in a folder not yet listed selects it once it is
   void testLocate ();
   void testDirIndexLater ();
   void testChooseFolderPath ();

   /** opening folders in the tree when the server takes longer to list
       them than a request is normally allowed */
   void testSlowFolders ();

   //! a stack going while its thumbnail is on its way
   void testStaleThumbnail ();
   void testWaitingThumbnail ();
   void testConnectionState ();
   void testConnectionIndicator ();
   void testConnectionDetails ();

   //! showing a stack's pages
   void testPreviewStack ();

   //! showing another page of a stack, which has not come yet
   void testPreviewPage ();

   //! showing a stack whose server never answers
   void testPreviewHung ();

   //! showing a stack which comes whole, not a page at a time
   void testPreviewWhole ();

   //! turning a stack's page in the folder, before it is fetched
   void testTurnStackPage ();

   /** drawing a stack's pages again after the server has changed it,
       before its new structure has come */
   void testRepaintChanged ();

   //! renaming a stack
   void testRename ();

   //! rotating a page
   void testRotate ();

   //! moving a stack to the trash
   void testTrash ();

   //! undoing a rename before the server has answered it
   void testUndoRename ();

   //! undoing a move to the trash before the server has answered it
   void testUndoTrash ();

   //! renaming a page
   void testRenamePage ();

   //! changing a stack's annotations
   void testAnnotations ();

   //! renaming a stack on a server which never answers
   void testRenameHung ();

   //! deleting a page, and undoing that before the server has answered
   void testDeletePages ();

   //! unstacking a page, and undoing that before the server has answered
   void testUnstackPage ();

   //! unstacking a stack, and undoing that before the server has answered
   void testUnstackStacks ();

   //! duplicating a stack, and undoing that before the server has answered
   void testDuplicate ();

   //! converting a stack to PDF, and undoing that
   void testConvert ();

   //! unfolding a booklet, and undoing that
   void testUnfold ();

   //! reading a page's text
   void testOcr ();

   //! asking the server to read a stack's pages next
   void testReadStack ();

   //! searching for text on the stacks' pages
   void testSearch ();
   void testSearchName ();
   void testMoveKey ();
   void testCopyKey ();
   void testMoveToNewFolder ();
   void testNewSubdirectory ();

   //! scanning a stack into a folder on the server
   void testScan ();

   //! closing the window with a scanned stack still to be uploaded
   void testScanClose ();
   void testScanSlowServer ();

   //! fetching every page of a stack, as for printing it
   void testFetchStacks ();

   /** converting and unfolding a stack on a server which cannot, so the
       desktop does it */
   void testConvertHere ();

   //! copying a stack on the server to the clipboard
   void testCopyFiles ();

private:
   /** restart the server so that it indexes the text on its stacks and
       reads their pages */
   void indexServer ();

   //! attach the server, waiting until it is there
   void connectServer ();

   /** Make the folder @p path, within the repository, through the server,
       as a user would; one made on the disk instead is not listed until
       the server lists its folders again */
   void makeFolder (const QString &path);

   //! open the folder holding the test stack, waiting until it is shown
   void openFolder ();

   //! the test stack, as an index in the desktop model
   QModelIndex stackIndex ();

   //! click the test stack, once it has settled in place
   void clickStack ();

   //! scan a one-page stack called "scanned" into the folder shown
   void scanStack ();

   //! have the server's backend say it cannot convert stacks
   void dropConvert ();

   /** check that the main thread has not waited for the server, then
       close the window, which waits for queued changes and saves the
       folder's layout; @p destroy deletes it as well */
   void closeWindow (bool destroy);

   //! the path of a file in the server's folder
   QString serverPath (const QString &fname) const;

   //! the server's repository in the folder tree, as a path
   QString repoPath () const;

   //! the number of pages in a stack on the server's disk, or -1
   int serverPages (const QString &fname) const;

   //! the stacks in the server's folder, by name, sorted
   QStringList serverStacks () const;

   //! run @p op, the user's, and then undo it before the server answers
   void doAndUndo (std::function<void ()> op);

   //! an empty client.conf, so that the user's servers are left alone
   std::unique_ptr<QTemporaryDir> _config;

   std::unique_ptr<QTemporaryDir> _repo;
   std::unique_ptr<ServerThread> _server;
   std::unique_ptr<Mainwindow> _me;
   Desktopwidget *_desktop = nullptr;

   /** where BlockWatch had got to as the test started: the main thread
       must not wait for the server anywhere in a test, not only in the
       stretches an AsyncCheck watches */
   int _mark = 0;

   //! true if the test waits on purpose, which it sets
   bool _waitsAllowed = false;
};

#endif // TEST_ASYNC_H
