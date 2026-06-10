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

   //! Test changing directory by clicking a folder in the tree
   void testDirTreeNavigation();

   //! Test emailing a stack puts it on the clipboard and opens Gmail
   void testEmailSingleStack();

   //! Test emailing several stacks packs them into a zip first
   void testEmailMultipleStacksZips();

   //! Test emailing a stack as PDF converts it first
   void testEmailAsPdfConverts();

   //! Test dragging a stack onto a folder in the tree moves it there
   void testDragDropToFolder();

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
