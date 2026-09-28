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

   //! an old preset gives the usual settings for those it lacks
   void testOldPresetGivesUsual();

   //! Add, Update, Rename and Delete, and which of them can be picked
   void testPresetEdit();

   //! the panel shows the scanner's exposure, brightness and contrast
   void testBrightShown();

   //! picking an item from the list shows a preset afterwards, not the item
   void testPresetMenu();

   //! Reset goes back to the default preset
   void testReset();

   //! with no scanner, nothing matches and nothing can be changed
   void testNoScanner();

   //! a preset chosen before there is a scanner is given to it once opened
   void testPresetBeforeScanner();
};

#endif // TEST_PSCAN_H
