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
