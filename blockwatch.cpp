/*
License: GPL-2
*/

#include "blockwatch.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMutex>
#include <QThread>


static QMutex watchMutex;
static bool watching;
static bool warning;
static QStringList noted;


void BlockWatch::watch (bool on, bool warn)
   {
   QMutexLocker locker (&watchMutex);

   watching = on;
   warning = on && warn;
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
   noted << what;
   if (warning)
      qWarning () << "main thread waits for the server:" << what;
   }


QStringList BlockWatch::take ()
   {
   QMutexLocker locker (&watchMutex);
   QStringList out = noted;

   noted.clear ();
   return out;
   }
