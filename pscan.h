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

#include "ui_pscan.h"


class Pscan : public QDialog, public Ui::Pscan
{
    Q_OBJECT

public:
    Pscan(QWidget* parent = 0, const char* name = 0, bool modal = false, Qt::WindowFlags fl = 0);
    ~Pscan();

    virtual QString getStackName( void );
    virtual QString getPageName( void );
    virtual void setupBright();
    // called when the window is closing, to save settings
    void closing (void);
   void checkEnabled (bool scanning);

public slots:
    virtual void scannerChanged( QScanner * scanner );
    virtual void setMainwidget( Mainwidget * main );
    virtual void source_clicked();
    virtual void settings_clicked();
    virtual void on_format_buttonClicked( int );
    virtual void pendingDone();
    virtual void adf_clicked();
    virtual void duplex_clicked();
    virtual void res_activated( int id );
    virtual void brightChanged( int bright );
    virtual void contrastChanged( int contrast );
    virtual void reset_clicked();
    virtual void scan_clicked();
    virtual void cancel_clicked();
    void warning (QString &warning);
    virtual void progress( const char * str );
    virtual void progress( int percent );
    virtual void size_activated( int id );
    virtual void setPreviewWidget( PreviewWidget * widget );
    virtual void setScanDialog( QScanDialog * dialog );
    virtual void progressSize( const char * str );
    virtual void info( const char * str );
    virtual void options_clicked();
    void on_config_clicked();
    void on_stop_clicked ();

protected:
    PreviewWidget *_preview;
    QScanner *_scanner;
    Mainwidget *_main;
    int _default_papersize_id;
    QScanDialog *_scanDialog;

protected slots:
    virtual void languageChange();

private:
    void init();

};

