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

   //! Setting DPI via QScanDialog widgets reaches the scanner.
   void testScanDialogSetDpi();

   //! After QScanner::reconnect(), a freshly rebuilt QScanDialog can push
   //! settings to the new SANE handle (the mainwidget rebuild-on-reconnect
   //! contract).
   void testScanDialogRebuildAfterReconnect();

   //! Demonstrates the bug: a STALE QScanDialog (i.e. one not rebuilt after
   //! reconnect) may fail to push values to the new SANE handle. This is
   //! why mainwidget rebuilds the dialog on reconnect.
   void testStaleScanDialogAfterReconnect();

   //! reconnect() snapshots and restores scanner state so the user's
   //! settings (DPI, duplex, format, etc) survive the teardown.
   void testReconnectPreservesSettings();

   //! The pscan dialog controls (resolution, ADF, duplex, reset) drive
   //! the scanner settings.
   void testPscanControls();
};

#endif // TEST_QSCANNER_H
