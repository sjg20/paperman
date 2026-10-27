#ifndef TEST_UPDATECHECK_H
#define TEST_UPDATECHECK_H

#include <QObject>

#include "suite.h"

/** Tests for the check for a newer version, against a small web server
    standing in for the website */
class TestUpdateCheck : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! each test starts with nothing checked or found
   void init();

   //! The request carries the version, system, processor and package only
   void testRequest();

   //! A newer version is told of, with the page to get it from
   void testNewer();

   //! The same or an older version is not told of
   void testNotNewer();

   //! A second start within the day does not ask again, but still tells
   void testOncePerDay();

   //! Nothing is asked when checking is turned off
   void testDisabled();

   //! A reply which is not a version and a web page is ignored
   void testBadReply();

   //! A snap asks, but its store updates it, so nothing is told
   void testSnap();
};

#endif // TEST_UPDATECHECK_H
