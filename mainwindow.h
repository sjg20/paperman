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

#include <functional>

#include <QElapsedTimer>
#include <QTimer>
#include "ui_mainwindow.h"
#include "op.h"

class QProgressBar;
class QPushButton;

class Activitydialog;
class Backgrounddialog;

class Desktopwidget;
class Mainwidget;

/**
 * @brief Main window
 * This contains all other windows and has the menu and status bars. It is the
 * main widget of the application.
 *
 * It contains a Mainwidget _main which can display either:
 *    - the desktop view, with folders, stacks and preview, or
 *    - the page view, with just a single stack
 */
class Mainwindow : public QMainWindow, public Ui::Mainwindow
{
    Q_OBJECT

public:
    Mainwindow(QWidget* parent = 0, const char* name = 0,
               Qt::WindowFlags fl = Qt::Window);
    ~Mainwindow();

    Desktopwidget * getDesktop();

    //! called with what went wrong, or with nothing if all is well
    using ServerAdded = std::function<void (const QString &error)>;

    /** Attach a server and remember it for next time, without waiting

       \param arg   the server: an address, the name of a machine to look
                    for one on, or the name of a server already listed
       \param done  called once the server's repositories are shown, or
                    with why they are not */
    void addServer(const QString &arg, ServerAdded done);

   // save window settings for next time (called when closing application)
   void saveSettings ();

   /** allow or forbid asking, on quitting, whether to stop reading the
       text of scanned pages. Tests forbid it, since nobody is there to
       answer

      \param allow  true to allow it, which is how things start */
   static void allowQuitQuestion (bool allow);

   /** the keys which Paperman's shortcuts use and which a browser also
       has a use for, so that a page can keep them from the browser

      \param seqs  the shortcuts
      \returns each key with modifiers, as a page sees it: the letters of
               the modifiers held (C for Ctrl or Command, A for Alt, S for
               Shift), a dash, then the browser's code for the key, such
               as 'C-KeyF', 'A-ArrowLeft' or '-F3'. Only letters, digits,
               function keys, arrows, Home and End are named. Keys with
               no Ctrl or Alt, other than the function keys, are left
               out, since they are needed for typing and moving about */
   static QStringList browserKeys (const QList<QKeySequence> &seqs);

   /** the keys of this window's shortcuts, and of the other keys it acts
       on, which a browser also has a use for, named as browserKeys()
       names them */
   QStringList keysToKeep ();

   // Select whether the 'search' menu-option is available
   void setSearchEnabled(bool enable);

   //! Add the given list of extra repositories, reporting any errors.
   //! If @p serverUrl is non-empty, also connect to that paperman-server
   //! and add every repository it serves as top-level items.
   void startup(const QStringList& dirs, const QString& serverUrl = QString());

   //! Shut down, saving settings
   void shutdown();

   /** \returns the folder offered for papers when there is nowhere to
       keep them: Paperman in the user's documents folder */
   static QString defaultRepository (void);

   //! \returns true if there are no repositories, nor any server
   bool needsRepository (void);

   /** Offer the user somewhere to keep papers if there is nowhere yet.
       This is asked once, however the user answers

       \param dir   folder to offer */
   void offerRepository (const QString &dir = defaultRepository ());

   // Run the GUI with the given arguments
   static void runGui(QApplication& app, QStringList args,
                      const QString& serverUrl = QString());

   /** Scan from the command line, without showing the GUI

      \param repo     repository to scan into (empty for the first one
                      configured)
      \param subdir   directory within the repository (empty for its top)
      \param device   scanner to use (empty for the last one used)
      \param pages    maximum number of sides to scan (0 for no limit)
      \param settings scanner options to set, as name=value
      \param autoColour  true to store pages without colour as grey or mono
      \param sideways    how the sheets are fed (Paperstack::t_sideways),
                         0 for upright
      \param blank       which blank pages to leave out
                         (Paperstack::t_blankPolicy), -1 for the setting
      \returns exit code: 0 on success */
   static int runScan(const QString& repo, const QString& subdir,
                      const QString& device, int pages,
                      const QStringList& settings = QStringList(),
                      bool autoColour = false, int sideways = 0,
                      int blank = -1);

public slots:
    virtual void on_actionPrint_triggered(bool);
    virtual void on_actionExit_triggered(bool);
    virtual void on_actionAbout_triggered(bool);
    virtual void on_actionSwap_triggered(bool);
    virtual void statusUpdate(QString str);
    virtual void on_actionScango_triggered(bool);
    virtual void on_actionPscan_triggered(bool);
    virtual void on_actionCamera_triggered(bool);
    virtual void on_actionReconnectScanner_triggered(bool);
    virtual void on_actionSelectall_triggered(bool);
    virtual void on_actionByPosition_triggered(bool);
    virtual void on_actionByDate_triggered(bool);
    virtual void on_actionByName_triggered(bool);
    virtual void on_actionResize_all_triggered(bool);
    virtual void on_actionRleft_triggered(bool);
    virtual void on_actionRright_triggered(bool);
    virtual void on_actionHflip_triggered(bool);
    virtual void on_actionVflip_triggered(bool);
    virtual void on_actionOptions_triggered(bool);
    virtual void on_actionOptionsm_triggered(bool);
    virtual void on_actionSearch_triggered(bool);
    virtual void on_actionGotoFolder_triggered(bool);

    /** Let the scan panel have Ctrl+F and F4 while it is shown

       It takes them from anywhere, to choose the folder to scan into, so
       Go to folder gives them up meanwhile, as neither would work if
       both had them

       \param shown   true if the scan panel is shown, false if hidden */
    void scanPanelShown(bool shown);

    /** Offer the camera only when there may be one

       The camera button is greyed out, and says why, when no camera is
       plugged in. This is checked as the window opens and each time it
       becomes the active one, which is cheap and opens no device */
    void updateCameraAction();
    virtual void on_actionDownloads_triggered(bool);
    virtual void on_actionAddRepository_triggered(bool);
    virtual void on_actionAddServer_triggered(bool);
    virtual void on_actionDocuments_triggered(bool);
    virtual void on_actionDirectory_triggered(bool);
    void on_actionFullScreen_triggered(bool);

    //! show the work going on in the background, which can be stopped there
    void on_actionBackground_triggered(bool);
    void on_actionActivity_triggered(bool);

    void welcome ();

    /** show a link to a newer version of paperman in the status bar

       \param version  its version, such as "1.5.0"
       \param url      the page to download it from */
    void newerVersion (const QString &version, const QUrl &url);

    void undoChanged (void);

    void on_actionDirFilter_triggered(bool state);

    /** save a snapshot of the window as shown on screen, for diagnosing
        display problems (see PAPERMAN_SNAP) */
    void snapWindow ();

protected slots:
    virtual void languageChange();
    void closeEvent(QCloseEvent *event);

    //! check for a camera as the window becomes the active one
    bool event(QEvent *event) override;

    /** Handle the operationProgress() signal from Operation */
    void updateProgress(enum Operation::state_t state, int percent,
                        QString name);

private:
    void init();

    /** Attach the server named on the command line, if any, and those
        listed in client.conf, without waiting for them */
    void connectServers(const QString &serverArg);

    //! attach the servers in client.conf, but @p except
    void connectConfigured(const QUrl &except);

    //! say that the server named on the command line cannot be used
    void reportServerError(const QString &server, const QString &error);

    //! a server has been attached: show its first folder if none is shown
    void serverAttached();

    //! true while there are servers still to attach
    bool _hasServers = false;

    QProgressBar *_progress;
    QLabel *_label;
    QLabel *_newer = nullptr;   //!< tells of a newer version, if any

    //! says how much text there is still to read in the background, and
    //! opens the window which shows it when pressed
    QPushButton *_reading = nullptr;

    //! the window showing the work going on in the background, once asked for
    Backgrounddialog *_background = nullptr;
    Activitydialog *_activity = nullptr;   //!< the window of what was done

    /** have the Activity window show the record for a folder: the one in
        its repository, or its server's */
    void showActivityFor (const QString &dir);

    //! true once the user has agreed to quit while pages are being read
    bool _quit_agreed = false;

    //! show how much text there is still to read, if any
    void readingChanged ();

    /** check that it is all right to quit: if pages are still being read
        in the background, the user is asked

       \returns true to quit */
    bool okToQuit ();

    /** how many operations are running: one shows the progress bar and
        the last one to finish takes it away again, since an operation
        started while another is still going would otherwise leave the
        one still running with nothing to report to */
    int _operations;

    //! true if we have already shown the welcome message
    bool _welcome_shown;
    Desktopwidget *_desktop;

    QTimer *_snapTimer;      //!< snapshot timer, or NULL (PAPERMAN_SNAP)
    QString _snapDir;        //!< where the snapshots go
    QElapsedTimer _snapClock; //!< time since the first snapshot
    int _snapCount;          //!< snapshots taken so far
};
