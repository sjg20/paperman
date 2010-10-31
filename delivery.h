
#include <stdint.h>

#include <QList>
#include <QString>
#include <QThread>
#include <QTcpServer>
#include <QTcpSocket>



class Transfer;
struct err_info;


enum msg_t
   {
   MSGT_hello,       //!< hello banner
   MSGT_login,       //!< login to domain, area
   MSGT_sendfilehdr, //!< send file header info
   MSGT_
   };

typedef struct msghdr_info
   {
   uint32_t type;        //!< message type (msg_t)
   uint32_t len;         //!< total message length including header
   } msghdr_info;


typedef struct msg_hello_info
   {
   int32_t version;     //!< server version number
   char salt [16];      //!< salt to use to encrypt password
   } msg_hello_info;


typedef struct msg_login_info
   {
   char domain [32];    //!< domain to log into
   char area [32];      //!< area to log into
   char encrypted [16]; //!< password encrypted with given salt
   } msg_login_info;

   //int32_t frag_pos;    //!< fragment position
   //int32_t frag_size;   //!< fragment size


typedef struct msg_info
   {
   msghdr_info hdr;
   union
      {
      msg_login_info login;
//      msg_sendfilehdr_info sendfilehdr;
      char data [0];
      };
   } msg_info;


class Maxthread : public QThread
   {
   Q_OBJECT

public:
   Maxthread (int fd, QObject *parent);

   void run ();

protected:
   /** send a message. A header is prepended.
   
      \param type    message type (msg_t)
      \param msg     pointer to message contents
      \param size    size of message contents */
   void sendmsg (int type, void *msg, int size);

   void processMsg (void);

public slots:
   void readFromSocket (void);

signals:
   void error (err_info *err);

private:
   int _fd;             //!< incoming file descriptor
   QTcpSocket *_sock;   //!< socket
   msghdr_info _hdr;    //!< header we have read (_hdr.len == 0 if none)
   QByteArray _buff;    //!< byte array buffer
   };


/**

The server listens for incoming connections. The protocol is very simple:


*/

class Maxserver : public QTcpServer
   {
   Q_OBJECT

public:
   Maxserver (QObject *parent = 0);

   /** start the server */
   void serve (void);

protected:
   void incomingConnection (int fd);

private:
   };


class Deliveryroot
   {
public:
   Deliveryroot (QString dir);

   err_info *scan (void);

   void showQueue (void);

   int queueSize (void);

private:
   QString _dir;
   Transfer *_trans;
   };


class Delivery
   {
public:
   Delivery ();

   void addRoot (QString root);

   err_info *scan (void);

   void showQueues (void);

   int rootCount (void) { return _roots.size (); }

   void server (void);

private:
   QList<Deliveryroot> _roots;      //!< a list of delivery roots that we are aware of
   Maxserver *_server;              //!< point to server
   };

