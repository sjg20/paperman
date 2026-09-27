#ifndef TEST_PSCAN_H
#define TEST_PSCAN_H

#include <QObject>

#include "suite.h"

/** Tests for the scan panel with a fake scanner behind it (see
    fakecontrol.h), which run the same with or without a scanner */
class TestPscan: public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   void init();
   void cleanup();

   //! the list shows the preset the panel matches on a later start too
   void testLaterStartFollowsPanel();

   //! changing any setting a preset has moves the list off it, and back
   void testPresetFollowsPanel();

   //! choosing a preset sets the scanner, from the list or a shortcut
   void testPresetSelect();

   //! Add and Delete, and when Delete can be picked
   void testPresetEdit();

   //! Reset goes back to the default preset
   void testReset();

   //! with no scanner, nothing matches and nothing can be changed
   void testNoScanner();
};

#endif // TEST_PSCAN_H
