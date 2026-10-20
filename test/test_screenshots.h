#ifndef TEST_SCREENSHOTS_H
#define TEST_SCREENSHOTS_H

#include <QObject>

#include "suite.h"

class Mainwindow;
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

   //! A folder of stacks, one selected, with its page and its text
   void testDesktop();

   //! The same, with the dark theme
   void testDesktopDark();

   //! A long stack open in the page view
   void testPageView();

   //! The search dialog, then the stacks it finds by their text
   void testSearch();

   //! Going to a folder by typing part of its name
   void testGoToFolder();

private:
   /** open the window on the repository, showing a folder

       \param me      window to use
       \param folder  folder within the repository to show
       \param stack   name of a stack to select, or empty for none */
   void open(Mainwindow *me, const QString &folder,
             const QString &stack = QString());

   //! wait for the stacks' images and the page to be drawn
   void settle();

   //! save a picture of a widget as \p name in the output directory
   void save(QWidget *widget, const QString &name);

   //! repository and output directory, empty if not given
   QString _repo, _out;
};

#endif // TEST_SCREENSHOTS_H
