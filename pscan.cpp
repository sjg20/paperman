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
#include <QProcess>
#include <QSettings>

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

void Pscan::presetAdd(const Preset& pre)
{
    _presets.push_back(pre);
    preset->addItem(pre._name);
}

void Pscan::readSettings()
{
   QSettings qs;

   restoreGeometry(qs.value("pscan/geometry").toByteArray());

   // Create some standard ones
   presetAdd(Preset("Monochrome 300dpi duplex", QScanner::mono, 300, true));
   presetAdd(Preset("Colour 200dpi duplex", QScanner::colour, 200, true));
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

    setupBright ();
}


void Pscan::closing()
   {
   QSettings settings;
   settings.setValue("pscan/geometry", saveGeometry());
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
}


void Pscan::setMainwidget(Mainwidget *main)
{
    _main = main;
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
    if (!_scanDialog)
       return;
//    format->setButton (0);
    _scanDialog->setFormat (QScanner::mono, false);

//    exposure->setValue (128);
    _scanDialog->setExposure (128);

//    adf->setChecked(true);
   _scanDialog->setAdf (true);

//    duplex->setChecked(false);
//    res->setCurrentItem(1);
//    size->setCurrentItem (_a4_id);
   _scanDialog->setDuplex (false);
   _scanDialog->setDpi (300);
   _preview->setSize (_default_papersize_id);
//   scannerChanged (_scanner);
}


void Pscan::scan_clicked()
{
   _main->scan ();
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
   _default_papersize_id = widget->getPreDefA4 ();

   QProcess paperconf;
   paperconf.start("paperconf", QStringList());
   if (paperconf.waitForStarted())
   {
      if (paperconf.waitForFinished())
      {
          QString size = QString(paperconf.readAll()).trimmed();

          // We should really look this up by name.
          if (size == "letter")
              _default_papersize_id = widget->getPreDefLetter ();
          else
              _default_papersize_id = widget->getPreDefA4 ();
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
}

void Pscan::on_preset_activated(int item)
{
   presetSelect(item);
}
