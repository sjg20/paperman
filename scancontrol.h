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

#pragma once

#include <QString>

#include "qscanner.h"

class PreviewWidget;
class QScanDialog;

/** What the scan panel asks of the scanner: the settings it shows and
    sets, and the paper sizes on offer.

    The program's is QScanControl, over the scanner, its option dialog and
    its preview, which may each be missing. A test can give the panel its
    own, so as to try the panel with no scanner at all.

    A setting the scanner does not have reads as -1 or false, and setting
    it does nothing. Once a setting is changed, whoever looks after the
    panel tells it with Pscan::refresh(), as the main window does when the
    scanner says an option has changed */
class ScanControl
{
public:
   virtual ~ScanControl() {}

   //! true once a scanner is open, without which the rest are of no use
   virtual bool present() = 0;

   //! the vendor, model and name, for the panel's title
   virtual QString title() = 0;

   //! 0 if there is no document feeder
   virtual int adfType() = 0;
   virtual bool useAdf() = 0;
   virtual void setAdf(bool adf) = 0;

   virtual bool duplex() = 0;
   virtual void setDuplex(bool duplex) = 0;

   //! the resolution across and down the page, the latter 0 if just one
   virtual int xDpi() = 0;
   virtual int yDpi() = 0;
   virtual void setDpi(int dpi) = 0;

   virtual QScanner::format_t format() = 0;

   /** \param jpeg  true to have colour and grey pages sent compressed */
   virtual void setFormat(QScanner::format_t format, bool jpeg) = 0;

   // exposure is mono's threshold; brightness and contrast are the others'
   virtual int exposure() = 0;
   virtual int brightness() = 0;
   virtual int contrast() = 0;
   virtual bool exposureRange(int *minp, int *maxp) = 0;
   virtual bool brightnessRange(int *minp, int *maxp) = 0;
   virtual bool contrastRange(int *minp, int *maxp) = 0;
   virtual void setExposure(int value) = 0;
   virtual void setBrightness(int value) = 0;
   virtual void setContrast(int value) = 0;

   //! whether the scanner finds the length of each sheet itself
   virtual bool hasAutoSize() = 0;
   virtual bool autoSize() = 0;
   virtual void setAutoSize(bool on) = 0;

   //! true if its auto-size finds the width too, leaving none to choose
   virtual bool autoSizeTrimsWidth() = 0;

   //! whether the scanner straightens each sheet and cuts it out
   virtual bool hasDeskewCrop() = 0;
   virtual bool deskewCrop() = 0;
   virtual void setDeskewCrop(bool on) = 0;

   //! show all of the scanner's options
   virtual void showOptions() = 0;

   /* The paper sizes, numbered from 0. The list is rebuilt, and so
      renumbered, as the size changes, so look a size up just before use */

   //! \returns a size's name, or an empty string past the last
   virtual QString sizeName(int id) = 0;
   virtual void setSize(int id) = 0;

   //! these sizes' numbers, each -1 if it is not on offer
   virtual int sizeA4() = 0;
   virtual int sizeLetter() = 0;
   virtual int sizeLegal() = 0;
   virtual int sizeLong() = 0;
};

/** The panel's control of the scanner the program has open */
class QScanControl : public ScanControl
{
public:
   QScanControl();

   void setScanner(QScanner *scanner) { _scanner = scanner; }
   void setDialog(QScanDialog *dialog) { _dialog = dialog; }
   void setPreview(PreviewWidget *preview) { _preview = preview; }

   bool present() override;
   QString title() override;
   int adfType() override;
   bool useAdf() override;
   void setAdf(bool adf) override;
   bool duplex() override;
   void setDuplex(bool duplex) override;
   int xDpi() override;
   int yDpi() override;
   void setDpi(int dpi) override;
   QScanner::format_t format() override;
   void setFormat(QScanner::format_t format, bool jpeg) override;
   int exposure() override;
   int brightness() override;
   int contrast() override;
   bool exposureRange(int *minp, int *maxp) override;
   bool brightnessRange(int *minp, int *maxp) override;
   bool contrastRange(int *minp, int *maxp) override;
   void setExposure(int value) override;
   void setBrightness(int value) override;
   void setContrast(int value) override;
   bool hasAutoSize() override;
   bool autoSize() override;
   void setAutoSize(bool on) override;
   bool autoSizeTrimsWidth() override;
   bool hasDeskewCrop() override;
   bool deskewCrop() override;
   void setDeskewCrop(bool on) override;
   void showOptions() override;
   QString sizeName(int id) override;
   void setSize(int id) override;
   int sizeA4() override;
   int sizeLetter() override;
   int sizeLegal() override;
   int sizeLong() override;

private:
   QScanner *_scanner;
   QScanDialog *_dialog;
   PreviewWidget *_preview;
};
