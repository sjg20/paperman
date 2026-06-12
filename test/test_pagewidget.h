#ifndef TEST_PAGEWIDGET_H
#define TEST_PAGEWIDGET_H

#include <QObject>

#include "suite.h"

class Desktopmodel;
class Mainwindow;
class Pagewidget;
class QModelIndex;

/** Tests for the page view: opening a stack, choosing pages from the
    thumbnail list, zooming and rotating the display. These drive the
    page-view tool buttons with real clicks and check the state the
    user sees (e.g. the zoom-level box) */
class TestPagewidget: public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! Test that clicking a page thumbnail displays that page
   void testThumbnailClickShowsPage();

   //! Test that opening a stack shows the stack's current page
   void testOpenAtCurrentPage();

   //! Test the zoom in/out/original buttons update the zoom level
   void testZoomButtons();

   //! Test typing a zoom level into the zoom box
   void testZoomLevelEdit();

   //! Test the rotate buttons transform the displayed page
   void testRotateButtons();

   //! Test the rotate action updates the displayed page
   void testRotateActionDisplay();

   //! Test editing stack attributes and saving them
   void testEditAttributesSave();

   //! Test that revert discards unsaved attribute edits
   void testEditAttributesRevert();

private:
   /** Open the 5-page test stack in the page view of a shown
       Mainwindow

      \param me     Mainwindow to use
      \param model  Returns the desktop model
      \param page   Returns the page widget showing the stack */
   void openTestStack(Mainwindow *me, Desktopmodel *&model,
                      Pagewidget *&page);
};

#endif // TEST_PAGEWIDGET_H
