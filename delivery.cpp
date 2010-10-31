

#include <iostream>

#include "config.h"
#include "delivery.h"
#include "transfer.h"



Deliveryroot::Deliveryroot (QString dir)
   {
   _dir = dir;
   if (!_dir.endsWith ("/"))
      _dir += "/";
   _trans = new Transfer (_dir);
   }


err_info *Deliveryroot::scan (void)
   {
   CALL (_trans->init ());
   CALL (_trans->scanOutboxes ());
   return NULL;
   }


void Deliveryroot::showQueue (void)
   {
   printf ("Queue: %s\n", qPrintable (_dir));
   foreach (const Tfile &tf, _trans->outlist ())
      {
      printf ("   From: %s\n", qPrintable (tf.from ()));
      printf ("   To: %s\n", qPrintable (tf.toList ()));
      printf ("   Subject: %s\n", qPrintable (tf.subject ()));
      printf ("   Notes: %s\n", qPrintable (tf.notes ()));
      printf ("\n");
      }
   }


int Deliveryroot::queueSize (void)
   {
   return _trans->outlistSize ();
   }


Delivery::Delivery ()
   {
   }


void Delivery::server (void)
   {
   _server = new Maxserver ();
   std::cout << "delivermv - maxview wide area delivery agent\n";
   printf ("(C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net, v%s\n", CONFIG_version_str);
   std::cout << std::endl;
   _server->serve ();
   }
   

void Delivery::addRoot (QString dir)
   {
   Deliveryroot root (dir);

   _roots << root;
   }


err_info *Delivery::scan (void)
   {
   for (int i = 0; i < _roots.size (); i++)
      CALL (_roots [i].scan ());
   return NULL;
   }


void Delivery::showQueues ()
   {
   int count = 0;

   for (int i = 0; i < _roots.size (); i++)
      {
      _roots [i].showQueue ();
      count += _roots [i].queueSize ();
      }
   printf ("%d active queue(s), %d total messages\n", _roots.size (), count);
   }


Maxthread::Maxthread (int fd, QObject *parent)
   : QThread (parent), _fd (fd)
   {
   _hdr.len = 0;
   qDebug () << "new connection";
   }


/** returns a character representing 'byte' in our 6-bit character set [A-Za-z0-9./] */

static int charset (int byte)
   {
   if (byte < 26)
      return byte + 'A';
   byte -= 26;
   if (byte < 26)
      return byte + 'a';
   byte -= 26;
   if (byte < 10)
      return byte + '0';
   return byte ? '/' : '.';
   }
   
   
void Maxthread::sendmsg (int type, void *msg, int size)
   {
   msghdr_info hdr;
   
   hdr.type = type;
   hdr.len = sizeof (hdr) + size;
   _sock->write ((const char *)&hdr, sizeof (hdr));
   _sock->write ((const char *)msg, size);
   }
   
   
void Maxthread::run (void)
   {
   _sock = new QTcpSocket ();

   if (!_sock->setSocketDescriptor (_fd))
      {
      emit error (err_make (ERRFN, ERR_socket_error1, _sock->error()));
      return;
      }
   connect (_sock, SIGNAL (readyRead()), this, SLOT (readFromSocket ()));
   
   msg_hello_info hello;
   
   hello.version = CONFIG_version;
   hello.salt [0] = charset ((getpid () + rand ()) & 63);
   hello.salt [1] = charset ((getpid () + rand ()) & 63);
   sendmsg (MSGT_hello, &hello, sizeof (hello));
   while (_sock->waitForReadyRead ())
      ;
   }


void Maxthread::processMsg (void)
   {
   const msg_info *msg = (const msg_info *)_buff.constData ();
   
   qDebug () << "msg" << msg->hdr.type;
   }
   
   
void Maxthread::readFromSocket ()
   {
   unsigned toread = _sock->bytesAvailable ();
   int nread;
   
   if (!_hdr.len)
      {
      if (toread >= sizeof (_hdr))
         {
         nread = _sock->read ((char *)&_hdr, sizeof (_hdr));
         if (nread < (int)sizeof (_hdr))
            emit error (err_make (ERRFN, ERR_failed_to_read_bytes1, sizeof (_hdr)));
         _buff.clear ();
         _buff.reserve (_hdr.len);
         _buff.append ((const char *)&_hdr, sizeof (_hdr));
         }
      }
   else
      {
      if (_buff.size () + toread > _hdr.len)
         toread = _hdr.len - _buff.size ();
      
      QByteArray ba;
      ba.resize (toread);
      nread = _sock->read (ba.data (), toread);
      if (nread < (int)sizeof (_hdr))
         emit error (err_make (ERRFN, ERR_failed_to_read_bytes1, sizeof (_hdr)));
         
      _buff.append (ba);
      if (_buff.size () == (int)_hdr.len)
         processMsg ();
      }
   }
 
 
Maxserver::Maxserver (QObject *parent)
      : QTcpServer (parent)
   {
   }


void Maxserver::incomingConnection (int fd)
   {
   Maxthread *thread = new Maxthread (fd, this);

   connect (thread, SIGNAL (finished()), thread, SLOT (deleteLater()));
   thread->start();
   }


void Maxserver::serve (void)
   {
   std::cout << "Server started on port " << CONFIG_port << std::endl;
   
   listen (QHostAddress::Any, CONFIG_port);
   
   // wait forever
   while (1)
      waitForNewConnection (-1);
   }

