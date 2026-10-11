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
   void cleanup ();

   //! the checks catch a wait for the server, a stall and a slow reply
   void testHarness ();

   //! attaching a server
   void testConnect ();

   //! opening a folder on the server
   void testOpenFolder ();

   //! showing a stack's pages
   void testPreviewStack ();

   //! turning a stack's page in the folder, before it is fetched
   void testTurnStackPage ();

   //! renaming a stack
   void testRename ();

   //! rotating a page
   void testRotate ();

   //! moving a stack to the trash
   void testTrash ();

private:
   //! attach the server, waiting until it is there
   void connectServer ();

   //! open the folder holding the test stack, waiting until it is shown
   void openFolder ();

   //! the test stack, as an index in the desktop model
   QModelIndex stackIndex ();

   //! the path of a file in the server's folder
   QString serverPath (const QString &fname) const;

   std::unique_ptr<QTemporaryDir> _repo;
   std::unique_ptr<ServerThread> _server;
   std::unique_ptr<Mainwindow> _me;
   Desktopwidget *_desktop = nullptr;
};

#endif // TEST_ASYNC_H
