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


QList<ServerThread::Exchange> ServerThread::takeExchanges ()
   {
   QMutexLocker locker (&_mutex);
   QList<Exchange> out = _exchanges;

   _exchanges.clear ();
   return out;
   }


/** Return the length of the HTTP message at the start of @a data, headers
    and body, or 0 if it is not all there yet; a reply with no length runs
    until the connection closes, so gives -1 */
static qint64 messageLength (const QByteArray &data, bool reply)
   {
   int end = data.indexOf ("\r\n\r\n");

   if (end < 0)
      return 0;

   QList<QByteArray> lines = data.left (end).toLower ().split ('\n');
   qint64 length = reply ? -1 : 0;

   // a reply which may not have a body has none
   int status = lines.first ().split (' ').value (1).toInt ();
   if (reply && (status < 200 || status == 204 || status == 304))
      length = 0;
   for (const QByteArray &line : lines)
      if (line.startsWith ("content-length:"))
         length = line.mid (15).trimmed ().toLongLong ();
   if (length < 0)
      return -1;
   return data.size () >= end + 4 + length ? end + 4 + length : 0;
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
      after the delay, which also keeps them in order.  The client sends a
      request only once it has the reply to the one before, so each reply
      belongs to the oldest request still waiting */
   QObject::connect (&proxy, &QTcpServer::newConnection, [&] () {
      while (QTcpSocket *client = proxy.nextPendingConnection ())
         {
         QTcpSocket *upstream = new QTcpSocket (client);
         QPointer<QTcpSocket> to (client);

         // what is on its way each way, not yet a whole request or reply
         struct Link
            {
            QByteArray requests;
            QByteArray replies;
            std::deque<std::shared_ptr<Exchange>> waiting;
            bool used = false;
            };
         auto link = std::make_shared<Link> ();

         auto finish = [this] (const Exchange &exchange) {
            QMutexLocker locker (&_mutex);

            _exchanges << exchange;
            _open.deref ();
         };

         /* hold the request until the server's side is connected, since
            not every platform buffers a write made before then */
         auto request = std::make_shared<QByteArray> ();
         upstream->connectToHost (QHostAddress::LocalHost, _port);
         QObject::connect (client, &QTcpSocket::readyRead, upstream,
                           [this, client, upstream, request, link] () {
            QByteArray data = client->readAll ();

            link->requests += data;
            while (qint64 length = messageLength (link->requests, false))
               {
               auto exchange = std::make_shared<Exchange> ();

               exchange->request = QString::fromUtf8 (link->requests.left (
                  link->requests.indexOf ("\r\n"))).section (' ', 0, 1);
               exchange->sent = length;
               exchange->connected = !link->used;
               link->used = true;
               link->waiting.push_back (exchange);
               link->requests.remove (0, length);
               _open.ref ();
               }
            *request += data;
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
         auto pass = [this, to, held, link, finish] (const QByteArray &data,
                                                      bool close) {
            int delay = _delay.loadRelaxed ();

            if (delay < 0)
               return;
            held->push_back ({data, close});
            QTimer::singleShot (delay, to, [to, held, link, finish] () {
               if (!to || held->empty ())
                  return;
               Held next = held->front ();

               held->pop_front ();
               if (!next.data.isEmpty ())
                  to->write (next.data);

               // each whole reply finishes the request it answers
               link->replies += next.data;
               while (!link->waiting.empty ())
                  {
                  Exchange &exchange = *link->waiting.front ();
                  qint64 length = messageLength (link->replies, true);

                  if (length < 0)
                     exchange.received = link->replies.size ();
                  if (length <= 0)
                     break;
                  exchange.received = length;
                  finish (exchange);
                  link->waiting.pop_front ();
                  link->replies.remove (0, length);
                  }
               if (next.close)
                  to->disconnectFromHost ();
            });
         };
         QObject::connect (upstream, &QTcpSocket::readyRead, client,
                           [upstream, pass] () {
            pass (upstream->readAll (), false);
         });

         /* the server closes the connection once it is done with it;
            pass on anything not yet read before closing too */
         QObject::connect (upstream, &QTcpSocket::disconnected, client,
                           [upstream, pass] () {
            pass (upstream->readAll (), true);
         });
         QObject::connect (client, &QTcpSocket::disconnected, client,
                           &QObject::deleteLater);

         /* once the client's side has gone, so are any requests it left
            unanswered, or answered up to the close */
         QObject::connect (client, &QObject::destroyed, [link, finish] () {
            for (const auto &exchange : link->waiting)
               finish (*exchange);
            link->waiting.clear ();
         });
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
