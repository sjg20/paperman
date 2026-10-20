#ifndef TEST_SCREENSHOTS_H
#define TEST_SCREENSHOTS_H

#include <QList>
#include <QMap>
#include <QObject>
#include <QRect>

#include "suite.h"

class Mainwindow;
class QAbstractButton;
class QWidget;

/** Takes the screenshots for the documentation and the website

    Each one opens the window on a repository, puts it in a state worth
    showing and saves a picture of it. They need the demo corpus (see
    doc/corpus.rst), read by OCR, so they are skipped unless
    PAPERMAN_SHOTS_REPO names it and PAPERMAN_SHOTS_DIR says where the
    pictures go. scripts/make_screenshots.sh sets all this up */
class TestScreenshots: public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   void init();
   void cleanup();

   //! A month's bills, one selected, with its page and its text
   void testDesktop();

   //! The icons of each toolbar, for the tables of what they do
   void testToolbarIcons();

   //! The same, with the dark theme
   void testDesktopDark();

   //! A long stack open in the page view
   void testPageView();

   //! The search dialog, then the stacks it finds by their text
   void testSearch();

   //! Going to a folder by typing part of its name
   void testGoToFolder();

   //! The scan panel, and the folders it suggests scanning into
   void testScanPanel();

   //! The menu of things to do with a stack, on a right click
   void testStackMenu();

   //! The menu of things to do with a folder, on a right click
   void testFolderMenu();

   //! The ways of duplicating a stack, from the stack menu
   void testDuplicateMenu();

   //! Moving a stack to another folder, chosen by part of its name
   void testMoveToFolder();

   //! The stacks of a folder, filtered by name
   void testFilter();

   //! A page marked to be removed, beside one kept
   void testRemovePage();

   //! Where a stack is dragged to, to move it to another folder
   void testDragToFolder();

   //! The ways of emailing and copying a stack, from the stack menu
   void testEmailMenu();

   //! Paperman's own options for printing, a tab of the print dialog
   void testPrintOptions();

   //! The desktop showing a repository on a server
   void testServer();

private:
   /** open the window on the repository, showing a folder

       \param me      window to use
       \param folder  folder within the repository to show
       \param stack   name of a stack to select, or empty for none */
   void open(Mainwindow *me, const QString &folder,
             const QString &stack = QString());

   /** right-click \p pos in \p widget and save a picture of the menu
       which comes up as \p name, or of its submenu \p sub if given */
   void saveMenu (QWidget *widget, QPoint pos, const QString &name,
                  const QString &sub = QString ());

   //! wait for the stacks' images and the page to be drawn
   void settle();

   //! save a picture of a widget as \p name in the output directory
   void save(QWidget *widget, const QString &name);

   /** save where each part of a window is, beside its picture, as
       \p name .json, for scripts/annotate_screenshot.py to point at

       \param window  window the picture is of
       \param name    name of the picture
       \param parts   each part's name, with where it is in the window */
   void saveParts (QWidget *window, const QString &name,
                   const QMap<QString, QRect> &parts);

   //! save the icon of each button as icons/<group>-<name>.png
   void saveIcons (const QString &group,
                   const QList<QAbstractButton *> &buttons);

   //! where \p widget is in \p window
   static QRect within (QWidget *window, QWidget *widget);

   //! the desktop's parts, for the annotated picture of it
   QMap<QString, QRect> desktopParts (Mainwindow *me);

   //! repository and output directory, empty if not given
   QString _repo, _out;
};

#endif // TEST_SCREENSHOTS_H
