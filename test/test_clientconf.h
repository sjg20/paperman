#ifndef TEST_CLIENTCONF_H
#define TEST_CLIENTCONF_H

#include <QObject>

#include "suite.h"

/** Tests for ClientConfig, the client.conf directory of servers shared
    by the GUI and the command-line client */
class TestClientConf : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! A missing file is an empty directory, not an error
   void testMissingFile();

   //! Named and bare URLs parse, with comments and blanks ignored
   void testParsing();

   //! Lines that are not URLs are reported rather than dropped
   void testBadLines();

   //! Servers are looked up by name, ignoring case
   void testFindByName();

   //! Adding appends, and skips a server that is already listed
   void testAdd();

   //! Trailing slashes and host case do not make a distinct server
   void testSameServer();
};

#endif // TEST_CLIENTCONF_H
