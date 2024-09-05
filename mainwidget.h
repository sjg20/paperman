/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
 .
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 .
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

X-Comment: On Debian GNU/Linux systems, the complete text of the GNU General
 Public License can be found in the /usr/share/common-licenses/GPL file.
*/
#ifndef __mainwidget_h
#define __mainwidget_h

class QTimer;
class QPrinter;
class QPrintDialog;
class QScanDialog;
class QScanner;

class Desktopmodel;
class Desktopview;
class Desktopwidget;
class Desk;
class Filepage;
class Options;
class PPage;
class Pagewidget;
class Paperscan;
class Paperstack;
class PreviewWidget;
class Printopt;
class Pscan;

typedef struct file_info file_info;
struct err_info;
struct print_info;

#include <QModelIndex>
#include <QStackedWidget>

#include "desk.h"
#include "qwidget.h"
#include "options.h"


extern "C"
{
#include <sane/sane.h>
}

/**
 * @brief Main widget
 * This can display either:
 *    - the desktop view _desktop, with folders, stacks and preview, or
 *    - the page view _page, with just a single stack
 *
 * The swapDesktop() method switches between the two, trigger by Mainwindow,
 * the parent widget, which has an actionSwap icon
 */
class Mainwidget : public QStackedWidget
   {
   Q_OBJECT
public:
   Mainwidget (QWidget *parent, const char *name = 0);
   ~Mainwidget();
   Desktopwidget *getDesktop (void) { return _desktop; }
   Pagewidget *getPage (void) { return _page; }
//p    Desktopviewer *getViewer (void) { return _viewer; }

   /** set the current operation name */
//    void progress (const char *fmt, ...);

   /** sets the displayed size of the scanned file so far */
   void progressSize (int size);

   /** set the current information text */
   void info (const QString &str);

   enum t_arrangeBy
      {
	byPosition, byName, byDate
      };

//    bool selectedFile (Desk * &maxdesk, file_info * &file);

   struct err_info *operation (Desk::operation_t type, int ival);

   // called when the main window is closing, to save window settings
   void closing (void);

   /**
    * @brief  Find folders in the current repo which match a text string
    * @param  text    Text to match
    * @param  dirPath Returns the path to the root directory
    * @param  missing Suggestions for directories to create
    * @return list of matching paths
    *
    * This looks for 4-digit years and 3-character months to try to guess
    * which folders to put at the top of the list
    */
   QStringList findFolders(const QString &text, QString& dirPath,
                           QStringList& missing);

   /* Start a new scan using dir_ind as the destination dir in Dirmodel */
   void scanInto(QModelIndex dir_ind);

   /**
    * @brief Start a new scan
    * @param dir_path   Full path of the directory to scan into
    */
   void scanInto(const QString& dir_path);

   /**
    * @brief Start a new scan into a new directory
    * @param dir_path   Full path of the directory to create and scan into
    */
   void scanIntoNewDir(const QString& dir_path);

signals:
   void newContents (QString);

   //* new configuration (may affect UI settings)
   void newConfig ();

public slots:
   /** bring up a print dialogue and allow the user to print stacks / pages */
   void print (void);

   /** options have changed (e.g. some have gone) so reload them */
   void slotReloadOptions(void);

   /** option value has changed */
   void slotSetOption(int num);

   void showPage (const QModelIndex &index, bool delay_smoothing = true);

   /** move left a page in the current stack */
   void pageLeft (void);

   /** move right a page in the current stack */
   void pageRight (void);

   /** move left a stack in the current stack */
   void stackLeft (void);

   /** move right a stack in the current stack */
   void stackRight (void);

   /** switch between desktop and page view */
   void showDesktop ();

   /** swap between desktop and page view */
   void swapDesktop ();

   /** scan a page or group of pages into the current directory */
   void scan (void);

   /** stop the scan in progress

      \param cancel  true to cancel the scan now (else just stops
                     after current page) */
   void stopScan (bool cancel);

   /** scan a page or group of pages under control of the pscan dialogue */
   void pscan (void);

   /** update buttons on the pscan dialogue */
   void updatePscan (void);

   /** open the options dialogue */
   void options (void);

   /** allow the user to select a scanner to use */
   void selectScanner (void);

   /** allow the user to change the scanner settings */
   void scannerSettings (void);

   /** ensures there is a current scanner - returns false on failure, true if ok */
   bool ensureScanner (void);

   /** select all items in current directory */
   void selectAll (void);

   /** arrange items by given type (t_arrangeBy...) */
   void arrangeBy (t_arrangeBy type);

   /** resize all the items in this directory */
   void resizeAll ();

   void rotate (int degrees);

   void flip (bool horiz);

   /** set whether a smooth (but slow) scaling algorithm is used */
   void setSmoothing (bool smooth);

   /** returns true if smoothing should be used */
   bool isSmoothing (void);

   /** update the match string and perform a new search */
   void matchUpdate (QString match, bool subdirs);

   /** exit the current search and display the current window */
   void resetSearch (void);

   /** save scanner device settings */
   void saveSettings (void);

   /** check the buttons on the scanner */
   void checkButtons (void);

private slots:
   void scanDialogClosed (void);

   //! called when pending changes to the ScanDialog are complete (these happen while scanning)
   void slotPendingDone (void);

   void savePrintSettings (void);

   void openPrintopt (void);

   /** handle a new stack being started. We add it to the model with the
       given name (or a default one if this is blank. From now on we expect
       to get pages until the stack is complete.

      \param stackName  suggested stack name*/
   void slotStackNew (const QString &stack_name);

   /** handle the completion of scanning

      \param status  Sane status - will be reported if there is an error
      \param msg     a final message to display
      \param err     an error to display (normally NULL) */

   void slotScanComplete (SANE_Status status, const QString &msg, const err_info *err);

   /** handle a new page signal. We add the page to maxdesk and update the
       display with the information given.

      \param mp      the new page in compressed form ready for maxdesk
      \param coverageStr the page coverage
      \param infoStr information about the page for the user (e.g. coverage) */
   void slotStackNewPage (const Filepage *mp, const QString &coverageStr,
      const QString &infoStr);

   /** handle a signal that the stack is now confirmed. We tell the model
       to confirm the stack */
   void slotStackConfirm (void);

   /** handle a signal that the stack is cancelled and should be deleted. We
       pass this message on to the model */
   void slotStackCancel (void);

   void progress (const QString &str);

   /** set the target total for the progress bar

      \param total   total number of bytes expected from scan
      \param page    the page being started */
   void slotStackPageStarting (int total, const PPage *page);

   /** set the current progress value
      \param page    the page in progress */
   void slotStackPageProgress (const PPage *page);

   void slotWarning (QString &str);

private:
   /** print a page if it is in range

      \param seq     The page number of this page within the whole print job
                     (0 = first)
      \param ind     Model index of stack to print from
      \param pnum    Page within stack (0 = first)
      \param numpages Number of pages in stack */
   err_info *printPage (int seq, QModelIndex &ind, int pnum, int numpages);

   //! setup the scan dialog and our _preview pointer
   void setupScanDialog (void);

   /** update any fields required in the scan dialog, based on config changes */
   void updateScanDialog (void);

   /** add a new image to the current stack */
   void newImage (char *buf, int size);

   /** draw some text with a background

       The text is drawn either on the left, right or centre of the very
       bottom of the printable area. This position is the first line,
       where linenum = 0. The second line of text (linenum=1) is immediately
       above this line.

       The background and text colours are set by _opt->fillColour and
       _opt->textColour

       This function supports printing the text in two parts, with the first
       being in shown in bold font.

      \param flags   alignment flags: Qt::AlignLeft, Qt::AlignRight, Qt::AlignHCenter
      \param text    text to draw
      \param linenum line number to put text on (normally 0 for the first line)
      \param do_bold text consists of two lines separated by \n. The first should be drawn
                     in bold followed by ": ", then the second immediately after it on the
                     same line */
   void drawTextOnWhite (int flags, QString text, int linenun = 0, bool do_bold = false);

   /** ensure that we have a current stack, creating a new one if necessary

      \param stack_name    name of stack to create
      \param page_name     name of page to create
      \param parameters    current sane parameters */
   err_info *ensureStack (QString &stack_name, QString &page_name,
         SANE_Parameters &parameters);

   /** connect up the scanner signals */
   void connectScanner (void);

   /**
    * @brief Search for directories matching a string
    * @param matches    List of matches to update
    * @param baseLen    Number of characters to omit at the start of each match
    * @param dirPath    Directory to scan
    * @param match      String to match
    * @param op         Operation, to show progress
    *
    * Adds dirPath if it matches the match string, then scans directories within
    * dirPath recursively.
    */
   void addMatches(QStringList& matches, uint baseLen, const QString &dirPath,
                   const QString &match, Operation *op);

private:
   Desktopwidget *_desktop;
   Pagewidget *_page;
   Desktopmodel *_contents;   //!< the model for the desktop items
   Desktopview *_view;

   /** the current scanner being used, 0 if none */
   QScanner *_scanner;

   /** the current scan dialogue */
   QScanDialog *_scanDialog;

   /** the current pscan dialogue */
   Pscan *_pscan;

   /** the current options dialogue */
   Options *_options;

   PreviewWidget *_preview;

   /** number of bytes we expect to receive for the page being scanned */
   int _progressTotal;

   bool _smooth;            //!< sets whether smooth scaling is used or not
   QTimer *_buttonTimer;    //!< timer to control checking scanner buttons
   bool _watchButtons;      //!< true to watch buttons

   //! information about a print job in progress
   Printopt *_opt;         //!< our print options dialogue
   QModelIndexList *_list; //!< list of stacks to print
   QPrinter *_printer;     //!< our printer
   QPrintDialog *_dialog;  //!< the printer dialog (to which we add Printopt)
   bool _saved;            //!< true if settings were saved
   int _from_page;         //!< from page to print
   int _to_page;           //!< to page to print
   bool _new_page;         //!< true if we need to call newPage() before starting
   QRect _body;            //!< bounding rectangle for images
   QRect _printable;       //!< printable area of the page
   QPainter *_painter;     //!< our painter
   int _pagecount;         //!< total number of pages being printed
   bool _reverse;          //!< printing pages in reverse order

   //! information about a scanning job in progress
   bool _scanning;         //!< true if scanning
   bool _scan_cancelling;  //!< true if cancelling the scan
//   Paperstack *_stack;     //!< paper stack to scan into
   Paperscan *_scan;       //!< the current scan in progress
   };


#endif

