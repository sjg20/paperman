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

#include <QDebug>
#include <QHeaderView>
#include <QMessageBox>
#include <QProcess>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QStandardItemModel>
#include <QTableView>
#include <QTimer>

#include "folderlist.h"
#include "pscan.h"
#include <qvariant.h>
#include "qxmlconfig.h"
#include "sliderspin.h"
#include "qi/previewwidget.h"

Preset::Preset(QString name, QScanner::format_t format, int dpi, bool duplex)
    : _name(name), _format(format), _dpi(dpi), _duplex(duplex), _valid(true)
{
}

Preset::~Preset()
{
}

bool Preset::matches(Preset &other)
{
   return other._valid && _format == other._format && _dpi == other._dpi &&
         _duplex == other._duplex;
}

/*
 *  Constructs a Pscan as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
Pscan::Pscan(QWidget* parent, const char* name, bool modal, Qt::WindowFlags fl)
    : QDialog(parent, fl)
{
    setObjectName(name);
    setModal(modal);
    setupUi(this);
    connect(cancel, SIGNAL(clicked()), this, SLOT(cancel_clicked()));
    format->setId(mono, QScanner::mono);
    format->setId(grey, QScanner::grey);
    format->setId(dither, QScanner::dither);
    format->setId(colour, QScanner::colour);

    _folders = new Folderlist(folderName, this);

    init();
    readSettings();
}

/*
 *  Destroys the object and frees any allocated resources
 */
Pscan::~Pscan()
{
    // no need to delete child widgets, Qt does it all for us
}

void Pscan::reject()
{
   _folders->hide();
   QDialog::reject();
}

void Pscan::presetAdd(const Preset& pre)
{
    _presets.push_back(pre);
    preset->addItem(pre._name);
}

void Pscan::readSettings()
{
   QMap<QString, QScanner::format_t> formats;
   QSettings qs;

   restoreGeometry(qs.value("pscan/geometry").toByteArray());

   formats["mono"] = QScanner::mono;
   formats["dither"] = QScanner::dither;
   formats["grey"] = QScanner::grey;
   formats["colour"] = QScanner::colour;

   int size = qs.beginReadArray("preset");
   for (int i = 0; i < size; i++)
      {
      qs.setArrayIndex (i);
      QString name = qs.value("name").toString();
      QString format = qs.value("format").toString();
      uint dpi = qs.value("dpi").toInt();
      bool duplex = qs.value("duplex").toBool();
      QScanner::format_t fmt = QScanner::mono;

      if (format.contains(format))
         fmt = formats[format];

      Preset preset(name, fmt, dpi, duplex);
      presetAdd(preset);
      }
   qs.endArray();

   if (!size) {
       // Create some standard ones
       presetAdd(Preset("Monochrome 300dpi duplex", QScanner::mono, 300, true));
       presetAdd(Preset("Colour 200dpi duplex", QScanner::colour, 200, true));
   }
   preset->addItem ("Add preset...");
   preset->addItem ("Delete preset...");

   // Add a custom one which cannot be selected, but shows when the settings
   // don't match any item
   preset->addItem ("<custom>");
   presetSetEnabled(Preset::custom, false);
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void Pscan::languageChange()
{
    retranslateUi(this);
}

/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/

void Pscan::init()
{
    res->addItem("200");
    res->addItem ("300");
    res->addItem ("400");
    res->addItem ("Other");

    _scanner = 0;
    _scanDialog = 0;
    _preview = 0;

    _do_preset_check = false;

    setupBright ();

    // setup the shortcuts for finding a folder
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::focusFind);
    QObject::connect(new QShortcut(QKeySequence(Qt::Key_F4), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::focusFind);

    // setup the keyboard shortcuts Ctrl-1 to Ctrl-5 for presets
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_1), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::presetShortcut1);
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_2), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::presetShortcut2);
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_3), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::presetShortcut3);
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_4), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::presetShortcut4);
    QObject::connect(new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_5), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::presetShortcut5);

    QObject::connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_A), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::selectA4);
    QObject::connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_L), this,
                                   nullptr, nullptr, Qt::ApplicationShortcut),
                     &QShortcut::activated, this, &Pscan::toggleLetter);
}


void Pscan::closing()
   {
   QMap<QScanner::format_t, QString> formats;
   QSettings qs;

   qs.setValue("pscan/geometry", saveGeometry());

   formats[QScanner::mono] = "mono";
   formats[QScanner::dither] = "dither";
   formats[QScanner::grey] = "grey";
   formats[QScanner::colour] = "colour";

   qs.setValue("pscan/geometry", saveGeometry());
   qs.beginWriteArray("preset");
   for (uint i = 0; i < _presets.size(); i++)
      {
      const Preset& preset = _presets[i];

      qs.setArrayIndex(i);
      qs.setValue("name", preset._name);
      qs.setValue("format", formats[preset._format]);
      qs.setValue("dpi", preset._dpi);
      qs.setValue("duplex", preset._duplex);
      }
   qs.endArray();
   }

void Pscan::scanStarting()
{
   _folders->scanStarting();
}

void Pscan::scannerChanged (QScanner *scanner)
{
    _scanner = scanner;

//     qDebug () << "Pscan::scannerChanged";
    bool disabled = _scanner == 0;

    adf->setDisabled(disabled);
    duplex->setDisabled(disabled);
    res->setDisabled(disabled);
    mono->setDisabled(disabled);
    grey->setDisabled(disabled);
    dither->setDisabled(disabled);
    colour->setDisabled(disabled);
    if (disabled)
        return;
    setWindowTitle (_scanner->vendor () + " " + _scanner->model () + ": " + _scanner->name ());

    int adf_type = _scanner->adfType();

    adf->setDisabled(adf_type == 0);
    adf->setChecked(_scanner->useAdf ());

    duplex->setChecked(_scanner->duplex ());
    int dpix = _scanner->xResolutionDpi ();
    int dpiy = _scanner->yResolutionDpi ();
    if ((dpix == dpiy || !dpiy)
         && (dpix == 200 || dpix == 300 || dpix == 400))
         res->setCurrentIndex (dpix / 100 - 2);
    else
         res->setCurrentIndex(3);
    QScanner::format_t f = _scanner->format ();

    QAbstractButton *b = format->button(f);

    if (b)
        b->setChecked(true);
    setupBright ();
    if (_do_preset_check)
       presetCheck();
    _folders->checkFolders();
}


void Pscan::setMainwidget(Mainwidget *main)
{
    _main = main;
   _folders->setMainwidget(main);
    presetSelect(0);
}


void Pscan::source_clicked()
{
   _main->selectScanner ();
}


void Pscan::settings_clicked()
{
   _main->scannerSettings ();
}


void Pscan::on_format_buttonClicked( int id)
{
   QScanner::format_t format = (QScanner::format_t)id;

   if (_scanDialog)
      _scanDialog->setFormat (format, xmlConfig->boolValue ("SCAN_USE_JPEG"));
}


void Pscan::pendingDone()
{
   // called when format changed (handle delay)
   setupBright();
}


void Pscan::adf_clicked()
{
   _scanDialog->setAdf (adf->isChecked ());
}


void Pscan::duplex_clicked()
{
   _scanDialog->setDuplex (duplex->isChecked ());
}


void Pscan::res_activated( int id)
{
    static int dpi_lookup [] = {200, 300, 400};

    if (id < 3)
      _scanDialog->setDpi (dpi_lookup [id]);
}


void Pscan::brightChanged(int bright)
{
    if (!_scanDialog)
       return;
    if (_scanner->format () == QScanner::mono)
       _scanDialog->setExposure (bright);
    else
       _scanDialog->setBrightness (bright);
//   _scanner->reloadOptions ();
}


void Pscan::contrastChanged(int contrast)
{
    if (!_scanDialog)
       return;
    if (_scanner->format () != QScanner::mono)
       _scanDialog->setContrast (contrast);
//   _scanner->reloadOptions ();
}


void Pscan::reset_clicked()
{
   if (_presets.size()) {
      presetSelect(0);
   } else if (_scanDialog) {
      _scanDialog->setFormat (QScanner::mono, false);
      _scanDialog->setExposure (128);
      _scanDialog->setAdf (true);
      _scanDialog->setDuplex (false);
      _scanDialog->setDpi (300);
   }
   _preview->setSize(_default_papersize_id);
   pageSize->setCurrentIndex(_default_papersize_id);
   presetCheck();
}

void Pscan::scan_clicked()
{
   QModelIndex ind;

   if (_folders->getSelected(ind, false))
      _main->scanInto(ind);
}


void Pscan::cancel_clicked()
{
   _main->stopScan (true);
}


void Pscan::on_stop_clicked()
{
   _main->stopScan (false);
}


void Pscan::warning (QString &warning)
{
    progressStr->setText (warning);
}


void Pscan::progress (const char *str)
{
    progressStr->setText (str);
    progressBar->setEnabled (false);
    progressBar->setMaximum(100);
    progressBar->setValue(0);
}



void Pscan::progress (int percent)
{
    progressBar->setEnabled (true);
    progressBar->setValue(percent);
}


QString Pscan::getStackName (void)
{
    return stackName->text ().trimmed ();
}


QString Pscan::getPageName (void)
{
    return pageName->text ().trimmed ();
}


void Pscan::size_activated( int id)
{
      _preview->setSize (id);
}


void Pscan::setPreviewWidget( PreviewWidget *widget )
{
   QString name;
   int i = 0;

   pageSize->clear ();
   _papersize_a4 = widget->getPreDefA4();
   _papersize_letter = widget->getPreDefLetter();
   _papersize_legal = widget->getPreDefLegal();
   _default_papersize_id = _papersize_a4;

   QProcess paperconf;
   paperconf.start("paperconf", QStringList());
   if (paperconf.waitForStarted())
   {
      if (paperconf.waitForFinished())
      {
          QString size = QString(paperconf.readAll()).trimmed();

          // We should really look this up by name.
          if (size == "letter")
              _default_papersize_id = _papersize_letter;
          else
              _default_papersize_id = _papersize_a4;
      }
   }

   do
   {
      name = widget->getSizeName (i++);
      if (!name.isEmpty())
         pageSize->addItem (name);
   } while (!name.isEmpty());
   _preview = widget;
   if (_default_papersize_id != -1)
   {
//      _preview->setSize (_a4_id);
      pageSize->setCurrentIndex (_default_papersize_id);
   }
}


void Pscan::setScanDialog( QScanDialog *dialog )
{
   _scanDialog = dialog;
}


void Pscan::progressSize( const char *str )
{
    sizeStr->setText (str);
}


void Pscan::setupBright()
{
    int exp, min, max;

    if (_scanner && _scanner->format () == QScanner::mono)
    {
        bright->setTitle ("Exposure");
        contrast->setEnabled (false);
        exp = _scanner->getExposure ();
        if (exp != -1)
           bright->setValue (exp);
        if (_scanner->getRangeExposure (&min, &max))
           bright->setRange (min, max);
    }
    else
    {
        bright->setTitle ("Brightness");
        contrast->setTitle ("Contrast");
        contrast->setEnabled (true);

        if (_scanner)
        {
            exp = _scanner->getBrightness ();
            if (exp != -1)
               bright->setValue (exp);
            exp = _scanner->getContrast ();
            if (exp != -1)
               contrast->setValue (exp);
            if (_scanner->getRangeBrightness (&min, &max))
               bright->setRange (min, max);
            if (_scanner->getRangeContrast (&min, &max))
               contrast->setRange (min, max);
        }
    }
}




void Pscan::info( const char * str )
{
    infoStr->setText (str);
}



void Pscan::options_clicked()
{
   _main->options ();
}


void Pscan::on_config_clicked()
{
   _scanDialog->slotShowOptionsWidget ();
}


void Pscan::checkEnabled (bool scanning)
   {
   scan->setEnabled (!scanning);
   cancel->setEnabled (scanning);
   stop->setEnabled (scanning);
   source->setEnabled (!scanning);
   reset->setEnabled (!scanning);
   }

void Pscan::presetSelect(int item)
{
   Preset pre = _presets[item];

   _do_preset_check = false;

   duplex->setChecked(pre._duplex);
   if (_scanDialog)
      _scanDialog->setDuplex (pre._duplex);

   if (pre._dpi == 200 || pre._dpi == 300 || pre._dpi == 400)
      res->setCurrentIndex (pre._dpi / 100 - 2);
   else
      res->setCurrentIndex(3);
   if (_scanDialog)
      _scanDialog->setDpi (pre._dpi);

   QAbstractButton *b = format->button(pre._format);
   if (b)
      b->setChecked(true);
   if (_scanDialog)
      _scanDialog->setFormat (pre._format,
                              xmlConfig->boolValue ("SCAN_USE_JPEG"));

   setupBright ();
   _do_preset_check = true;
}

void Pscan::on_preset_activated( int item )
{
   if (item < (int)_presets.size())
      presetSelect(item);
   else if (item == (int)_presets.size() + Preset::add)
      presetAddUser();
   else if (item == (int)_presets.size() + Preset::delete_it)
      presetDeleteUser();
   presetSetEnabled(Preset::delete_it, presetLocate() >= 0);
}

void Pscan::presetSetEnabled(enum Preset::preset_item_t index, bool enabled)
{
   QStandardItemModel *model =
         qobject_cast<QStandardItemModel *>(preset->model());
   Q_ASSERT(model != nullptr);

   QStandardItem *item = model->item(_presets.size() + index);
   item->setFlags(enabled ? item->flags() | Qt::ItemIsEnabled :
                            item->flags() & ~Qt::ItemIsEnabled);
}

Preset Pscan::presetCreate(QString name)
{
   int dpix = _scanner->xResolutionDpi ();
   int dpiy = _scanner->yResolutionDpi ();

   Preset preset(name, _scanner->format(),  dpix, _scanner->duplex());

   preset._valid = !dpiy || dpix == dpiy;

   return preset;
}

int Pscan::presetLocate()
{
   if (!_scanner)
      return -1;

   Preset to_find = presetCreate("");

   for (uint i = 0; i < _presets.size(); i++) {
      if (_presets[i].matches(to_find))
         return i;
   }

   return -1;
}

void Pscan::presetCheck()
{
   int item = presetLocate();

   preset->setCurrentIndex(item >= 0 ? item : _presets.size() + Preset::custom);
   presetSetEnabled(Preset::delete_it, item >= 0);
}

void Pscan::presetAddUser()
{
   if (!_scanner)
      return;

   Presetadd add;

   uint next_id = _presets.size();
   QString msg;
   if (next_id < 5)
      msg = QString("Press Ctrl-%1 to activate your new preset").arg(next_id + 1);
   else
      msg = "Shortcuts Ctrl-1 to Ctrl-5 are in use already";
   add.msg->setText(msg);
   if (!add.exec())
      return;

   if (add.name->text().isEmpty()) {
      QMessageBox msg;
      msg.setText("The name cannot be empty");
      msg.exec();
      return;
   }

   int existing = presetLocate();

   if (existing != -1) {
      QMessageBox msg;
      msg.setText(QString("An existing preset has the same settings (%1)")
                  .arg(_presets[existing]._name));
      msg.exec();
      presetCheck();
      return;
   }

   Preset to_create = presetCreate(add.name->text());

   preset->insertItem(_presets.size(), to_create._name);
   preset->setCurrentIndex(_presets.size());

   _presets.push_back(to_create);
}

void Pscan::presetDeleteUser()
{
   int item;

   // We know that the current settings must match the item the user wants to
   // delete, unless the current item is <custom>, in which case the option is
   // disabled and we should not get here
   item = presetLocate();
   Q_ASSERT(item >= 0 && item < (int)_presets.size());

   QMessageBox msg;
   msg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
   msg.setText(QString("Delete preset %1?").arg(_presets[item]._name));
   if (msg.exec() != QMessageBox::Ok) {
      presetCheck();
      return;
   }

   _presets.erase(_presets.begin() + item);
   preset->removeItem(item);
   presetCheck();
}

void Pscan::presetShortcut(uint item)
{
   // subtract one, since the presets are numbered from 1
   item--;
   if (item >= _presets.size())
      return;

   presetSelect(item);
   preset->setCurrentIndex(item);
}

void Pscan::presetShortcut1()
{
   presetShortcut(1);
}

void Pscan::presetShortcut2()
{
   presetShortcut(2);
}

void Pscan::presetShortcut3()
{
   presetShortcut(3);
}

void Pscan::presetShortcut4()
{
   presetShortcut(4);
}

void Pscan::presetShortcut5()
{
   presetShortcut(5);
}

void Pscan::selectA4()
{
   pageSize->setCurrentIndex(_papersize_a4);
   _preview->setSize(_papersize_a4);
}

void Pscan::toggleLetter()
{
   int id;

   if (pageSize->currentIndex() ==_papersize_letter)
      id = _papersize_legal;
   else
      id = _papersize_letter;
   pageSize->setCurrentIndex(id);
   _preview->setSize(id);
}

Presetadd::Presetadd(QWidget* parent, Qt::WindowFlags fl)
   : QDialog(parent, fl)
{
   setupUi (this);
}

Presetadd::~Presetadd()
{
}

void Pscan::focusFind()
{
   activateWindow();
   folderName->setFocus(Qt::ShortcutFocusReason);
   folderName->setSelection(0, folderName->text().size());
}
