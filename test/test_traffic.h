/*
License: GPL-2
*/

#ifndef TEST_TRAFFIC_H
#define TEST_TRAFFIC_H

#include <QModelIndex>
#include <QObject>
#include <QTemporaryDir>

#include <memory>

#include "remoteharness.h"
#include "suite.h"

class Desktopwidget;
class Mainwindow;

/** Measures what goes over the wire between the desktop and a server

    Each test drives the desktop through one thing a user does, against
    a server behind the test harness's proxy, which records every
    request with the bytes it sent and received.  The test prints them,
    grouped by endpoint, and checks the total against a budget, so that
    a change which sends more shows up, and one which sends less can
    tighten it. */
class TestTraffic : public Suite
{
   Q_OBJECT
public:
   // defined where the members' types are complete
   explicit TestTraffic (QString name);
   ~TestTraffic () override;

private slots:
   void initTestCase ();
   void cleanupTestCase ();
   void init ();
   void cleanup ();

   //! attaching a server
   void testConnect ();

   //! opening a folder of stacks, with their thumbnails
   void testOpenFolder ();

   //! opening the folder again, once its stacks have been shown
   void testReopenFolder ();

   //! showing a stack, as a click on it does
   void testClickStack ();

   //! showing a stack and turning through its pages
   void testShowStack ();

   //! renaming a stack
   void testRename ();

   /** opening the folder and showing a stack in a new session, which has
       them in its cache from the one before */
   void testNewSession ();

   //! opening the folder again once a stack in it has changed
   void testChangedStack ();

   //! listing a large folder
   void testListLargeFolder ();

   //! closing the window once the folder's layout is on the server
   void testCloseUnchanged ();

private:
   //! attach the server, waiting until it is there
   void connectServer ();

   //! open the folder of stacks, waiting until every thumbnail is shown
   void openFolder ();

   //! a stack in the folder shown, by name, as a source index
   QModelIndex stackIndex (const QString &name);

   /** click a stack, then if @p turn, show each of its five pages in
       turn */
   void showStack (const QString &name, bool turn = true);

   //! the server's repository in the folder tree, as a path
   QString repoPath () const;

   //! what went over the wire for something the user did
   struct Totals
      {
      QStringList lines;      //!< each request's first line
      int requests = 0;
      qint64 sent = 0;
      qint64 received = 0;
      };

   /** wait until the desktop has stopped asking the server for things,
       then print what it asked for since the last call, as @p what, and
       return the totals */
   Totals traffic (const QString &what);

   /** check that @p totals keeps within a budget of @p requests and
       @p received bytes, which allows for thumbnails and the like being
       a little bigger on another platform */
   void withinBudget (const Totals &totals, int requests, qint64 received);

   //! an empty client.conf, so that the user's servers are left alone
   std::unique_ptr<QTemporaryDir> _config;

   std::unique_ptr<QTemporaryDir> _repo;
   std::unique_ptr<ServerThread> _server;
   std::unique_ptr<Mainwindow> _me;
   Desktopwidget *_desktop = nullptr;
};

#endif // TEST_TRAFFIC_H
