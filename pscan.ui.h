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
    res->insertItem ("200");
    res->insertItem ("300");
    res->insertItem ("400");
    res->insertItem ("Other");

    _scanner = 0;
    _scanDialog = 0;
    _preview = 0;

    setupBright ();
}



void Pscan::scannerChanged (QScanner *scanner)
{
    _scanner = scanner;

    bool disabled = _scanner == 0;

    adf->setDisabled(disabled);
    duplex->setDisabled(disabled);
    res->setDisabled(disabled);
    format->setDisabled(disabled);
    if (disabled)
        return;
    setCaption (_scanner->vendor () + " " + _scanner->model () + ": " + _scanner->name ());
    adf->setChecked(_scanner->useAdf ());
    duplex->setChecked(_scanner->duplex ());
    int dpix = _scanner->xResolutionDpi ();
    int dpiy = _scanner->yResolutionDpi ();
    if (dpix == dpiy
         && (dpix == 200 || dpix == 300 || dpix == 400))
         res->setCurrentItem (dpix / 100 - 2);
    else
         res->setCurrentItem(3);
    QScanner::format_t f = _scanner->format ();

    QButton *b;

    switch (f)
    {
        case QScanner::dither : b = dither; break;
        case QScanner::colour : b = colour; break;
        case QScanner::grey : b = grey; break;
        default :
        case QScanner::mono : b = mono; break;
    }

    format->setButton (format->id (b));
    setupBright ();
}


void Pscan::setMainwidget(Mainwidget *main)
{
    _main = main;
}


void Pscan::source_clicked()
{
   _main->selectScanner ();
}


void Pscan::settings_clicked()
{
   _main->scannerSettings ();
}


void Pscan::format_clicked( int )
{
   QButton *id = format->selected ();
   QScanDialog::format_t format = QScanDialog::mono;

   if (id == dither) format = QScanDialog::dither;
   if (id == mono) format = QScanDialog::mono;
   if (id == grey) format = QScanDialog::grey;
   if (id == colour) format = QScanDialog::colour;
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
    _scanDialog->setFormat (QScanDialog::mono, false);

//    exposure->setValue (128);
    _scanDialog->setExposure (128);

//    adf->setChecked(true);
   _scanDialog->setAdf (true);

//    duplex->setChecked(false);
//    res->setCurrentItem(1);
//    size->setCurrentItem (_a4_id);
   _scanDialog->setDuplex (false);
   _scanDialog->setDpi (300);
   _preview->setSize (_a4_id);
//   scannerChanged (_scanner);
}


void Pscan::scan_clicked()
{
   _main->scan ();
}


void Pscan::progress (const char *str)
{
    progressStr->setText (str);
    progressBar->setEnabled (false);
    progressBar->setTotalSteps(100);
    progressBar->setProgress (0);
}



void Pscan::progress (int percent)
{
    progressBar->setEnabled (true);
    progressBar->setProgress(percent);
}


QString Pscan::getStackName (void)
{
    return stackName->text ().stripWhiteSpace ();
}


QString Pscan::getPageName (void)
{
    return pageName->text ().stripWhiteSpace ();
}


void Pscan::size_activated( int id)
{
      _preview->setSize (id);
}


void Pscan::setPreviewWidget( PreviewWidget *widget )
{
   const char *name;
   int i = 0;

   size->clear ();
   _a4_id = widget->getPreDefA4 ();

   do
   {
      name = widget->getSizeName (i++);
      if (name)
         size->insertItem (name);
   } while (name);
   _preview = widget;
   if (_a4_id != -1)
   {
      _preview->setSize (_a4_id);
      size->setCurrentItem (_a4_id);
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
