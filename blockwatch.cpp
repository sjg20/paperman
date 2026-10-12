/*
License: GPL-2
*/

#include "blockwatch.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMutex>
#include <QThread>


static QMutex watchMutex;
static int watching;       // how many have started watching
static bool warning;
static bool keeping;       // something has asked for a mark
static QStringList noted;


void BlockWatch::watch (bool on, bool warn)
   {
   QMutexLocker locker (&watchMutex);

   if (on)
      {
      watching++;
      warning |= warn;
      return;
      }
   if (watching && --watching)
      return;
   warning = false;
   keeping = false;
   noted.clear ();
   }


void BlockWatch::note (const QString &what)
   {
   QCoreApplication *app = QCoreApplication::instance ();

   if (!app || QThread::currentThread () != app->thread ())
      return;

   QMutexLocker locker (&watchMutex);

   if (!watching)
      return;
   if (keeping)
      noted << what;
   if (warning)
      qWarning () << "main thread waits for the server:" << what;
   }


int BlockWatch::mark ()
   {
   QMutexLocker locker (&watchMutex);

   keeping = true;
   return noted.size ();
   }


QStringList BlockWatch::since (int mark)
   {
   QMutexLocker locker (&watchMutex);

   return noted.mid (mark);
   }
