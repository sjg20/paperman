#ifndef TEST_QSCANNER_H
#define TEST_QSCANNER_H

#include <QObject>

#include "suite.h"

class TestQscanner: public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! Opening the simulated scanner succeeds and is queryable.
   void testOpenSimul();

   //! reconnect() leaves the scanner in an open, usable state.
   void testReconnectKeepsOpen();

   //! After reconnect(), setting DPI through QScanner sticks.
   //! (This is what reapplyCurrentPreset() ultimately relies on.)
   void testReapplyDpiAfterReconnect();
};

#endif // TEST_QSCANNER_H
