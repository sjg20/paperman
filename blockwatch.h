/*
License: GPL-2
*/

#ifndef BLOCKWATCH_H
#define BLOCKWATCH_H

#include <QString>
#include <QStringList>

/** Notes each time the application's main thread waits for the network

    The desktop must not wait for a server on its main thread: the wait
    either freezes the window or, through a nested event loop, lets the
    user act again before the first action is done.  Code which still
    waits calls note() as it does so.  Tests watch for these waits to
    show that a flow makes none, and the desktop logs them so that any
    left over are seen in use. */
namespace BlockWatch
{
   /** Start or stop noting waits.  Watching nests: noting stops, and the
       waits noted are forgotten, once each start has been stopped

       @param on    true to start, false to stop
       @param warn  true to log each wait as well */
   void watch (bool on, bool warn = false);

   /** Note that the calling thread is about to wait for @p what, such
       as a request's path; ignored unless watching, and unless called
       on the main thread */
   void note (const QString &what);

   /** Return where the waits noted have got to, to pass to since().
       Waits are only kept once something has asked for a mark */
   int mark ();

   //! Return the waits noted since @p mark, from mark()
   QStringList since (int mark);
}

#endif // BLOCKWATCH_H
