#ifndef TEST_DESKTOPUI_H
#define TEST_DESKTOPUI_H

#include <QObject>

#include "suite.h"

class Desktopmodel;
class Desktopview;
class Mainwindow;
class QModelIndex;

/** Tests which drive the desktop through real UI events (mouse clicks,
    key presses and QAction triggers) rather than calling the underlying
    operations directly. These check that what the user does with the
    mouse and keyboard produces the behaviour they see on screen */
class TestDesktopUi: public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! Test that clicking a stack selects it and clicking space deselects
   void testClickSelectsStack();

   //! Test that ctrl-click adds a second stack to the selection
   void testCtrlClickMultiSelect();

   //! Test that double-clicking a stack opens it in the page view
   void testDoubleClickOpensStack();

   //! Test the swap-view action returns from page view to the desktop
   void testSwapViewAction();

   //! Test moving between stacks with the toolbar prev/next buttons
   void testToolbarStackNavigation();

   //! Test flipping pages with the toolbar page prev/next buttons
   void testToolbarPageNavigation();

   //! Test stack navigation wraps at either end
   void testStackNavigationWraps();

   //! Test the rotate and flip actions transform the current page
   void testRotateActions();

   //! Test rotating one page in the preview pane keeps the selection
   void testRotatePageKeepsSelection();

   //! Test rotating in the page view does not move the preview pane's page
   void testRotatePageViewKeepsPreviewPage();

   //! Test a rotated page's preview thumbnail is ready at once
   void testRotatePreviewThumbnailReady();

   //! Test rotating a stack which has not been viewed yet
   void testRotateUnloadedStack();

   //! Test the Edit->Select-all menu action selects every stack
   void testSelectAllAction();

   //! Test combining stacks with the stack action, then menu undo/redo
   void testStackAndMenuUndo();

   //! Test typing in the filter box filters stacks, and cancel restores
   void testFilterStacks();

   //! Test searching folders for a stack and locating its folder
   void testSearchAndLocate();

   //! Test that the Escape key leaves search mode
   void testSearchEscapeReturns();

   //! Test that repeated searches show the right results each time
   void testRepeatedSearch();

   //! Test changing directory by clicking a folder in the tree
   void testDirTreeNavigation();

   //! Test emailing a stack puts it on the clipboard and opens Gmail
   void testEmailSingleStack();

   //! Test emailing several stacks packs them into a zip first
   void testEmailMultipleStacksZips();

   //! Test emailing a stack as PDF converts it first
   void testEmailAsPdfConverts();

   //! Test copying a stack puts a PDF on the clipboard without a browser
   void testCopyAsPdf();

   //! Test dragging a stack onto a folder in the tree moves it there
   void testDragDropToFolder();

   //! Repositioning a stack on the desk moves it, persists and undoes
   void testMoveStackOnDesk();

   //! A multi-selected group dropped on a folder moves together and
   //! one undo brings the whole group back
   void testDragDropGroupToFolder();

   //! The same group drop against a remote repository moves the
   //! files on the server, with undo
   void testRemoteGroupMoveViaUi();

   //! Test importing files from a directory and moving one in
   void testImportFlow();

   //! Test scanning into a new stack with the simulated scanner
   void testScanIntoStack();

   //! Test adding and removing a repository, with undo and redo
   void testRepositoryAddRemoveUndo();

   //! Test renaming a stack by typing into the item editor
   void testRenameStackViaEditor();

   //! Test the year/month directory filter hides other years
   void testDirFilterHidesOtherYears();

private:
   /** Set up a repo in a shown Mainwindow, returning the model and the
       index of the repo root

      \param me        Mainwindow to use
      \param model     Returns the desktop model
      \param repo_ind  Returns the source-model index of the repo root */
   void setupShown(Mainwindow *me, Desktopmodel *&model,
                   QModelIndex &repo_ind);

   //! Get the proxy index of the desktop item in the given row
   QModelIndex itemIndex(Desktopview *view, int row);

   //! Click the centre of the given desktop item
   void clickItem(Desktopview *view, int row,
                  Qt::KeyboardModifiers modifiers = Qt::NoModifier);
};

#endif // TEST_DESKTOPUI_H
