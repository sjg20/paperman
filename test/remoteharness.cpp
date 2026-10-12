/*
License: GPL-2
*/

#include "remoteharness.h"

#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include <deque>
#include <memory>

#include "../blockwatch.h"
#include "../searchserver.h"


ServerThread::ServerThread (const QString &root, quint16 port)
   : _root (root), _port (port)
   {
   }


ServerThread::~ServerThread ()
   {
   quit ();
   wait ();
   }


bool ServerThread::begin ()
   {
   start ();
   _ready.acquire ();
   return _ok;
   }


QUrl ServerThread::url () const
   {
   /* not localhost, which Windows tries first as ::1, where the proxy
      does not listen */
   return QUrl (QString ("http://127.0.0.1:%1").arg (_proxyPort));
   }


void ServerThread::run ()
   {
   SearchServer server (_root, _port);
   QTcpServer proxy;

   server.setTextIndex (_textIndex);
   if (_readPages)
      server.setReadPages (1, [] (QImage &, OcrPage &) {
         return QString ();
      });
   _ok = server.start () && proxy.listen (QHostAddress::LocalHost);
   _proxyPort = proxy.serverPort ();

   /* Pass each connection on to the server: requests at once, replies
      after the delay, which also keeps them in order */
   QObject::connect (&proxy, &QTcpServer::newConnection, [&] () {
      while (QTcpSocket *client = proxy.nextPendingConnection ())
         {
         QTcpSocket *upstream = new QTcpSocket (client);
         QPointer<QTcpSocket> to (client);

         /* hold the request until the server's side is connected, since
            not every platform buffers a write made before then */
         auto request = std::make_shared<QByteArray> ();
         upstream->connectToHost (QHostAddress::LocalHost, _port);
         QObject::connect (client, &QTcpSocket::readyRead, upstream,
                           [client, upstream, request] () {
            *request += client->readAll ();
            if (upstream->state () == QAbstractSocket::ConnectedState)
               {
               upstream->write (*request);
               request->clear ();
               }
         });
         QObject::connect (upstream, &QTcpSocket::connected, upstream,
                           [upstream, request] () {
            upstream->write (*request);
            request->clear ();
         });

         /* pass the reply on after the delay, unless holding it for
            ever.  Timers of the same length need not fire in the order
            they were started, so each passes on the oldest thing waiting
            rather than its own: otherwise the close can overtake the
            last of the reply, as it does on Windows */
         struct Held
            {
            QByteArray data;
            bool close;
            };
         auto held = std::make_shared<std::deque<Held>> ();
         auto pass = [this, to, held] (const QByteArray &data, bool close) {
            int delay = _delay.loadRelaxed ();

            if (delay < 0)
               return;
            held->push_back ({data, close});
            QTimer::singleShot (delay, to, [to, held] () {
               if (!to || held->empty ())
                  return;
               Held next = held->front ();

               held->pop_front ();
               if (!next.data.isEmpty ())
                  to->write (next.data);
               if (next.close)
                  to->disconnectFromHost ();
            });
         };
         QObject::connect (upstream, &QTcpSocket::readyRead, client,
                           [upstream, pass] () {
            pass (upstream->readAll (), false);
         });

         /* the server closes once it has replied; pass on anything not
            yet read before closing too */
         QObject::connect (upstream, &QTcpSocket::disconnected, client,
                           [upstream, pass] () {
            pass (upstream->readAll (), true);
         });
         QObject::connect (client, &QTcpSocket::disconnected, client,
                           &QObject::deleteLater);
         }
   });

   _ready.release ();
   if (_ok)
      exec ();
   server.stop ();
   }


StallMeter::StallMeter ()
   {
   _timer.setTimerType (Qt::PreciseTimer);
   _timer.setInterval (5);
   connect (&_timer, &QTimer::timeout, this, &StallMeter::tick);
   }


void StallMeter::start ()
   {
   _longest = 0;
   _since.start ();
   _timer.start ();
   }


qint64 StallMeter::longestGap () const
   {
   // a stall still going on counts too
   return qMax (_longest, _since.elapsed ());
   }


void StallMeter::tick ()
   {
   _longest = qMax (_longest, _since.restart ());
   }


AsyncCheck::AsyncCheck ()
   {
   BlockWatch::watch (true);
   _mark = BlockWatch::mark ();
   _meter.start ();
   }


AsyncCheck::~AsyncCheck ()
   {
   BlockWatch::watch (false);
   }


bool AsyncCheck::ok ()
   {
   _waits = BlockWatch::since (_mark);
   _gap = qMax (_gap, _meter.longestGap ());
   return _waits.isEmpty () && (_allowStall || _gap <= kMaxStallMs);
   }


QString AsyncCheck::report () const
   {
   QStringList out;

   if (!_waits.isEmpty ())
      out << "waited for: " + _waits.join (", ");
   if (!_allowStall && _gap > kMaxStallMs)
      out << QString ("stalled for %1ms").arg (_gap);
   return out.join ("; ");
   }
