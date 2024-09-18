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

#include <QTableView>
#include "ui_presetadd.h"
#include "ui_pscan.h"

class QStandardItemModel;
class Folderlist;

class Preset
{
public:
    Preset(QString name, QScanner::format_t format, int dpi, bool duplex);
    ~Preset();

    /** returns true if the other preset matches this one, ignoring name */
    bool matches(Preset &other);

    // extra items in the presets combobox, after the real presets. The value
    // indicates how far it is past _presents.size()
    enum preset_item_t {
       add = 0,
       delete_it,
       custom,    // "<custom>" item
    };

    QString _name;
    QScanner::format_t _format;
    int _dpi;     // x & y dots-per-inch must be the same
    bool _duplex;
    bool _valid;  // preset is valid
};

class Presetadd : public QDialog, public Ui::Presetadd
{
    Q_OBJECT

public:
    Presetadd(QWidget* parent = 0, Qt::WindowFlags fl = Qt::WindowFlags());
    ~Presetadd();
};

class Pscan : public QDialog, public Ui::Pscan
{
    Q_OBJECT

public:
    Pscan(QWidget* parent = 0, const char* name = 0, bool modal = false, Qt::WindowFlags fl = Qt::WindowFlags());
    ~Pscan();

    virtual QString getStackName( void );
    virtual QString getPageName( void );
    virtual void setupBright();
    // called when the window is closing, to save settings
    void closing (void);
   void checkEnabled (bool scanning);

   // Tell pscan that a scan is starting
   void scanStarting();

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
    virtual void presetShortcut1();
    virtual void presetShortcut2();
    virtual void presetShortcut3();
    virtual void presetShortcut4();
    virtual void presetShortcut5();
    void on_preset_activated(int item);
    void on_config_clicked();
    void on_stop_clicked ();
    void on_folderName_textChanged(const QString &text);

    /**
     * @brief Select a directory from the folder list
     * @param target  Item in the folder list to select
     *
     * The selected directory is shown in the Dirview
     */
    void selectDir(const QModelIndex& target);

    // Select A4 paper-size
    void selectA4();

    // Toggle between US letter and legal
    void toggleLetter();

protected:
    void reject();

protected:
    PreviewWidget *_preview;
    QScanner *_scanner;
    Mainwidget *_main;

    // selected papersize element from PreviewWidget::predefs[], either
    // A4 or US letter, depending on what paperconf' says
    int _default_papersize_id;

    QScanDialog *_scanDialog;
    std::vector<Preset> _presets;

    // true to check the preset combbox to see an item matches current settings
    bool _do_preset_check;

    // List of folders found using the folderName search
    Folderlist *_folders;

    // Model containing the list of folders
    QStandardItemModel *_model;

    // string which we saw while in the middle of searching
    QString _next_search;

    virtual void languageChange();

    // papersize elements from PreviewWidget::predefs[]
    int _papersize_a4;
    int _papersize_letter;
    int _papersize_legal;

private:
    void init();

    /** read in the scanning-related settings from QSettings */
    void readSettings();

    /** add a new preset to the list of presets and the preset combo */
    void presetAdd(const Preset& preset);

    /** select a preset item, numbered from 0 */
    void presetSelect(int item);

    /** set whether a particular preset item is enabled or not */
    void presetSetEnabled(enum Preset::preset_item_t index, bool enabled);

    /** return the preset which matches the current settings, or -1 if none */
    int presetLocate();

    /** Fill in a Preset object with the current settings, returns true if OK */
    Preset presetCreate(QString name);

    /** check if the preset combobox needs updating */
    void presetCheck();

    /** allow the user to add a new preset */
    void presetAddUser();

    /** allow the user to delete a preset */
    void presetDeleteUser();

    /** Select a preset using a shortcut; item is numbered from 1 */
    void presetShortcut(uint item);

    /** Move the focus to the folder 'find' field */
    void focusFind();
};
