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

   //! every cell in the page list has room for its page, when the first
   //! page is far wider than the rest
   void testWideFirstPageLeavesRoom();

   //! sizing the cells sets up no page, which would clear its pixmap
   void testRelayoutLeavesPixmaps();

   //! a page drawn at one size and then shown at another, as when moving
   //! from the scan view, is scaled to the size it is shown at
   void testRenderedAtOtherSize();

   //! the size of a page's pixmap is had without asking for one
   void testKnownPixmapSize();

   //! Test that opening a stack shows the stack's current page
   void testOpenAtCurrentPage();

   //! Test the zoom in/out/original buttons update the zoom level
   void testZoomButtons();

   //! Test typing a zoom level into the zoom box
   void testZoomLevelEdit();

   //! Test the rotate and flip buttons transform the displayed page
   void testRotateButtons();

   //! Test the crop tool crops the page shown, with undo
   void testCropTool();

   //! Test dragging marks the part of the page to keep
   void testCropDrag();

   //! Test the scrollbars reach the page's edges once the view resizes
   void testScrollRangeResize();

   //! Test the crop tool can crop every page
   void testCropStack();

   //! Test leaving the crop tool changes nothing
   void testCropCancel();
   void testCropKeys();
   void testCropLeavesStack();

   //! Set cover marks the box drawn as the cover, or else the whole page
   void testSetCover();

   //! a stack has one cover, so setting it takes it off any other page
   void testCoverOnePerStack();

   //! setting and clearing the cover can be undone and redone
   void testCoverUndo();

   //! the page list marks the cover page, and keeps up with changes
   void testCoverBadge();

   //! the page list's menu clears the cover, on the cover page only
   void testClearCover();

   //! the crop bar opens on the cover page with the cover's box marked
   void testCoverBarStart();

   //! a stack which cannot keep a cover says so, rather than offering one
   void testCoverNotOffered();

   //! Test a rotated page's thumbnail is ready at once, not blanked
   void testRotateThumbnailReady();
   void testBackgroundRender();

   //! Test the rotate action updates the displayed page
   void testRotateActionDisplay();

   //! Test editing stack attributes and saving them
   void testEditAttributesSave();

   //! Test that revert discards unsaved attribute edits
   void testEditAttributesRevert();

   //! Test the scan preview keeps its bands when the page's shape changes
   void testScanPreviewReshape();

   //! Test the OCR pane shows the words read from each page
   void testPageOcrShown();

private:
   /** Open the 5-page test stack in the page view of a shown
       Mainwindow

      \param me     Mainwindow to use
      \param model  Returns the desktop model
      \param page   Returns the page widget showing the stack */
   void openTestStack(Mainwindow *me, Desktopmodel *&model,
                      Pagewidget *&page);

   //! whether the page list marks a page as the cover
   static bool coverShown(Pagewidget *page, int row);

   //! show a page of the stack in the page view
   static void showPage(Pagewidget *page, int pagenum);
};

#endif // TEST_PAGEWIDGET_H
