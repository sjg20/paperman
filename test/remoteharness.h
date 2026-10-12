/*
License: GPL-2
*/

#ifndef REMOTEHARNESS_H
#define REMOTEHARNESS_H

#include <QAtomicInt>
#include <QElapsedTimer>
#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QUrl>

/** A Paperman server on a thread of its own, behind a proxy which can
    slow its replies or hold them back altogether

    A test needs this to see whether the desktop waits for a server.
    With the server on the test's own thread it answers from inside any
    event loop the desktop spins while waiting, and a hung server cannot
    be tried at all. */
class ServerThread : public QThread
{
public:
   /** Prepare a server for @p root, listening on @p port; begin() starts
       it */
   ServerThread (const QString &root, quint16 port);
   ~ServerThread () override;

   /** Start the server and its proxy, waiting until both listen

       @return true if they do */
   bool begin ();

   //! the URL clients use, which goes through the proxy
   QUrl url () const;

   /** Hold back each reply from the server by @p ms before passing it
       on; 0 passes replies straight on, and -1 holds them for ever, as a
       server which has hung would */
   void setDelay (int ms) { _delay.storeRelaxed (ms); }

protected:
   void run () override;

private:
   QString _root;
   quint16 _port;
   quint16 _proxyPort = 0;
   bool _ok = false;
   QAtomicInt _delay;
   QSemaphore _ready;
};


/** Measures how long the main thread goes without handling events

    A timer ticks every few milliseconds on the main thread; the longest
    gap between ticks is how long the window would have been frozen.
    Note that waiting in a nested event loop does not show here, since
    the loop handles the ticks: BlockWatch catches that. */
class StallMeter : public QObject
{
   Q_OBJECT
public:
   StallMeter ();

   //! start measuring afresh
   void start ();

   //! the longest gap between ticks since start(), in milliseconds
   qint64 longestGap () const;

private:
   void tick ();

   QTimer _timer;
   QElapsedTimer _since;
   qint64 _longest = 0;
};


/** Watches a stretch of a test for waits on the server: those the main
    thread made (through BlockWatch) and any time it went without
    handling events (through StallMeter) */
class AsyncCheck
{
public:
   /** the longest the main thread may go without handling events.  It
       draws a page itself once it has come, which takes a Windows runner
       several times as long as other machines, so allow for that there;
       BlockWatch still catches a wait for the server */
#ifdef Q_OS_WIN
   static constexpr qint64 kMaxStallMs = 1500;
#else
   static constexpr qint64 kMaxStallMs = 300;
#endif

   AsyncCheck ();
   ~AsyncCheck ();

   //! true if nothing waited for the server since construction
   bool ok ();

   //! what waited, for a failure message
   QString report () const;

private:
   StallMeter _meter;
   QStringList _waits;
   qint64 _gap = 0;
};

#endif // REMOTEHARNESS_H
