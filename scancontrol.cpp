/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
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

#include "scancontrol.h"
#include "qscandialog.h"
#include "previewwidget.h"

QScanControl::QScanControl()
   : _scanner(nullptr), _dialog(nullptr), _preview(nullptr)
{
}

bool QScanControl::present()
{
   return _scanner != nullptr;
}

/* the scanner is opened before its option dialog is made, which is what
   changes the settings */
bool QScanControl::settable()
{
   return _scanner && _dialog;
}

QString QScanControl::title()
{
   return _scanner ? _scanner->vendor() + " " + _scanner->model() + ": "
                     + _scanner->name() : QString();
}

int QScanControl::adfType()
{
   return _scanner ? _scanner->adfType() : 0;
}

bool QScanControl::useAdf()
{
   return _scanner && _scanner->useAdf();
}

void QScanControl::setAdf(bool adf)
{
   if (_dialog)
      _dialog->setAdf(adf);
}

bool QScanControl::duplex()
{
   return _scanner && _scanner->duplex();
}

void QScanControl::setDuplex(bool duplex)
{
   if (_dialog)
      _dialog->setDuplex(duplex);
}

int QScanControl::xDpi()
{
   return _scanner ? _scanner->xResolutionDpi() : 0;
}

int QScanControl::yDpi()
{
   return _scanner ? _scanner->yResolutionDpi() : 0;
}

void QScanControl::setDpi(int dpi)
{
   if (_dialog)
      _dialog->setDpi(dpi);
}

QScanner::format_t QScanControl::format()
{
   return _scanner ? _scanner->format() : QScanner::other;
}

void QScanControl::setFormat(QScanner::format_t format, bool jpeg)
{
   if (_dialog)
      _dialog->setFormat(format, jpeg);
}

int QScanControl::exposure()
{
   return _scanner ? _scanner->getExposure() : -1;
}

int QScanControl::brightness()
{
   return _scanner ? _scanner->getBrightness() : -1;
}

int QScanControl::contrast()
{
   return _scanner ? _scanner->getContrast() : -1;
}

bool QScanControl::exposureRange(int *minp, int *maxp)
{
   return _scanner && _scanner->getRangeExposure(minp, maxp);
}

bool QScanControl::brightnessRange(int *minp, int *maxp)
{
   return _scanner && _scanner->getRangeBrightness(minp, maxp);
}

bool QScanControl::contrastRange(int *minp, int *maxp)
{
   return _scanner && _scanner->getRangeContrast(minp, maxp);
}

void QScanControl::setExposure(int value)
{
   if (_dialog)
      _dialog->setExposure(value);
}

void QScanControl::setBrightness(int value)
{
   if (_dialog)
      _dialog->setBrightness(value);
}

void QScanControl::setContrast(int value)
{
   if (_dialog)
      _dialog->setContrast(value);
}

bool QScanControl::hasAutoSize()
{
   return _dialog && _dialog->hasAutoSize();
}

bool QScanControl::autoSize()
{
   return _dialog && _dialog->autoSize();
}

void QScanControl::setAutoSize(bool on)
{
   if (_dialog)
      _dialog->setAutoSize(on);
}

bool QScanControl::autoSizeTrimsWidth()
{
   return _dialog && _dialog->autoSizeTrimsWidth();
}

bool QScanControl::hasDeskewCrop()
{
   return _dialog && _dialog->hasDeskewCrop();
}

bool QScanControl::deskewCrop()
{
   return _dialog && _dialog->deskewCrop();
}

void QScanControl::setDeskewCrop(bool on)
{
   if (_dialog)
      _dialog->setDeskewCrop(on);
}

void QScanControl::showOptions()
{
   if (_dialog)
      _dialog->slotShowOptionsWidget();
}

QString QScanControl::sizeName(int id)
{
   return _preview && id >= 0 ? _preview->getSizeName(id) : QString();
}

void QScanControl::setSize(int id)
{
   if (_preview && id >= 0)
      _preview->setSize(id);
}

int QScanControl::sizeA4()
{
   return _preview ? _preview->getPreDefA4() : -1;
}

int QScanControl::sizeLetter()
{
   return _preview ? _preview->getPreDefLetter() : -1;
}

int QScanControl::sizeLegal()
{
   return _preview ? _preview->getPreDefLegal() : -1;
}

int QScanControl::sizeLong()
{
   return _preview ? _preview->getPreDefLong() : -1;
}
