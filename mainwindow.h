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




#include "ui_mainwindow.h"

class QProgressBar;

class Desktopwidget;
class Mainwidget;


class Mainwindow : public QMainWindow, public Ui::Mainwindow
{
    Q_OBJECT

public:
    Mainwindow(QWidget* parent = 0, const char* name = 0,
               Qt::WindowFlags fl = Qt::Window);
    ~Mainwindow();

    Desktopwidget * getDesktop();

   // save window settings for next time (called when closing application)
   void saveSettings ();

public slots:
    virtual void on_actionPrint_triggered(bool);
    virtual void on_actionExit_triggered(bool);
    virtual void on_actionAbout_triggered(bool);
    virtual void on_actionSwap_triggered(bool);
    virtual void statusUpdate(QString str);
    virtual void on_actionScango_triggered(bool);
    virtual void on_actionPscan_triggered(bool);
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
    virtual void on_actionFind_triggered(bool);
    void on_actionFullScreen_triggered(bool);

    void welcome ();

    void undoChanged (void);

protected slots:
    virtual void languageChange();
    void closeEvent(QCloseEvent *event);

    /** advise of the current progress level for the status bar
         0   means just starting
         100 means finished */
    void setProgress (int percent, QString name);

private:
    void init();

    QProgressBar *_progress;
    QLabel *_label;

    //! true if we have already shown the welcome message
    bool _welcome_shown;
    Desktopwidget *_desktop;
};
