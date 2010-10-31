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


#include <QColorDialog>
#include <QFontDialog>
#include <QPainter>
#include <QPrinter>


#include "desktopmodel.h"
#include "printopt.h"
#include "qxmlconfig.h"


#if QT_VERSION >= 0x040400
#define USE_DUPLEX
#endif


Printopt::Printopt (QPrinter *printer, QModelIndexList &list, QWidget* parent, const char* name, Qt::WindowFlags fl)
    : QFrame(parent, name, fl), _list (list)
   {
   _printer = printer;
   setupUi(this);

   init();
   connect (sepSheet, SIGNAL (toggled (bool)), this, SLOT (checkPagecount (bool)));
   connect (textColour, SIGNAL (clicked ()), this, SLOT (selectColour ()));
   connect (fillColour, SIGNAL (clicked ()), this, SLOT (selectColour ()));
   textFont->installEventFilter (this);
   }

/*
 *  Destroys the object and frees any allocated resources
 */
Printopt::~Printopt()
   {
   // no need to delete child widgets, Qt does it all for us
   }


void Printopt::init()
   {
   getxml ();
   load ();
   }


void Printopt::reset (void)
   {
   init ();
   }


void Printopt::getxml (void)
   {
   _printOdd = xmlConfig->intValue("PRINT_ODD", 1);
   _printEven = xmlConfig->intValue("PRINT_EVEN", 1);
   _sepSheet = xmlConfig->intValue("PRINT_SEPERATE", 1);

   _printNumbers = xmlConfig->intValue("PRINT_NUMBERS", 1);
   _printNames = xmlConfig->intValue("PRINT_NAMES", 0);
   _printSeq = xmlConfig->intValue("PRINT_SEQUENCE", 0);
   _printImageInfo = xmlConfig->intValue("PRINT_IMAGE_INFO", 0);
   _printTimestamp = xmlConfig->intValue("PRINT_IMAGE_TIMESTAMP", 0);

   _shrinkFit = xmlConfig->intValue("PRINT_SHRINK_FIT", 1);
   _expandFit = xmlConfig->intValue("PRINT_EXPAND_FIT", 1);

   int col = xmlConfig->intValue("PRINT_TEXT_COLOUR", 0);
   _textColour = QColor (col >> 16, (col >> 8) & 0xff, col & 0xff);
   col = xmlConfig->intValue("PRINT_FILL_COLOUR", 0);
   _fillColour = QColor (col >> 16, (col >> 8) & 0xff, col & 0xff);

   _font.fromString (xmlConfig->stringValue("PRINT_TEXT_FONT", ""));

   _duplexMode = xmlConfig->intValue("PRINT_DUPLEX_MODE", -1);
   _outputFilename = xmlConfig->stringValue("PRINT_OUTPUT_FILENAME", "");
   _outputFormat = xmlConfig->intValue("PRINT_OUTPUT_FORMAT", -1);
   QString printerName = xmlConfig->stringValue("PRINT_PRINTER_NAME", "");
   }


void Printopt::load ()
   {
   printOdd->setChecked (_printOdd);
   printEven->setChecked (_printEven);
   sepSheet->setChecked (_sepSheet);

   printNumbers->setChecked (_printNumbers);
   printNames->setChecked (_printNames);
   printSeq->setChecked (_printSeq);
   printImageInfo->setChecked (_printImageInfo);
   printTimestamp->setChecked (_printTimestamp);

   shrinkFit->setChecked (_shrinkFit);
   expandFit->setChecked (_expandFit);

   _textColourEdit = _textColour;
   _fillColourEdit = _fillColour;
   _fontEdit = _font;
   textFont->setFont (_fontEdit);

   _printer->setFullPage (!_shrinkFit);
   _printer->setCreator ("Maxview");
   if (_duplexMode != -1)
#ifdef USE_DUPLEX
      _printer->setDuplex ((QPrinter::DuplexMode)_duplexMode);
#else
      _printer->setDoubleSidedPrinting (_duplexMode != 0);
#endif
   if (!_outputFilename.isEmpty ())
      _printer->setOutputFileName (_outputFilename);
   if (_outputFormat != -1)
      _printer->setOutputFormat ((QPrinter::OutputFormat)_outputFormat);
   if (!_printerName.isEmpty ())
      _printer->setPrinterName (_printerName);
   }


void Printopt::save ()
   {
   _printOdd = printOdd->isChecked ();
   _printEven = printEven->isChecked ();
   _sepSheet = sepSheet->isChecked ();

   _printNumbers = printNumbers->isChecked ();
   _printNames = printNames->isChecked ();
   _printSeq = printSeq->isChecked ();
   _printImageInfo = printImageInfo->isChecked ();
   _printTimestamp = printTimestamp->isChecked ();

   _shrinkFit = shrinkFit->isChecked ();
   _expandFit = expandFit->isChecked ();

   _textColour = _textColourEdit;
   _fillColour = _fillColourEdit;
   _font = _fontEdit;

   if (_printer)
      {
#ifdef USE_DUPLEX
      _duplexMode = _printer->duplex ();
#else
      _duplexMode = _printer->doubleSidedPrinting () ? 2 : 0;
#endif
      _outputFilename = _printer->outputFileName ();
      _outputFormat = _printer->outputFormat ();
      _printerName = _printer->printerName ();
      _printer->setFullPage (!_shrinkFit);
      }
   }



void Printopt::putxml ()
   {
   xmlConfig->setIntValue("PRINT_ODD", _printOdd);
   xmlConfig->setIntValue("PRINT_EVEN", _printEven);
   xmlConfig->setIntValue("PRINT_SEPERATE", _sepSheet);

   xmlConfig->setIntValue("PRINT_NUMBERS", _printNumbers);
   xmlConfig->setIntValue("PRINT_NAMES", _printNames);
   xmlConfig->setIntValue("PRINT_SEQUENCE", _printSeq);
   xmlConfig->setIntValue("PRINT_IMAGE_INFO", _printImageInfo);
   xmlConfig->setIntValue("PRINT_IMAGE_TIMESTAMP", _printTimestamp);

   xmlConfig->setIntValue("PRINT_SHRINK_FIT", _shrinkFit);
   xmlConfig->setIntValue("PRINT_EXPAND_FIT", _expandFit);

   QRgb rgb = _textColour.rgb ();
   xmlConfig->setIntValue("PRINT_TEXT_COLOUR",
      (qRed (rgb) << 16) | (qGreen (rgb) << 8) | qBlue (rgb));
   rgb = _fillColour.rgb ();
   xmlConfig->setIntValue("PRINT_FILL_COLOUR",
      (qRed (rgb) << 16) | (qGreen (rgb) << 8) | qBlue (rgb));
   xmlConfig->setStringValue("PRINT_TEXT_FONT", _font.toString ());

   xmlConfig->setIntValue("PRINT_DUPLEX_MODE", _duplexMode);
   xmlConfig->setStringValue("PRINT_OUTPUT_FILENAME", _outputFilename);
   xmlConfig->setIntValue("PRINT_OUTPUT_FORMAT", _outputFormat);
   xmlConfig->setStringValue("PRINT_PRINTER_NAME", _printerName);
   }



int Printopt::countPages (void)
   {
   Desktopmodel *contents = (Desktopmodel *)_list [0].model ();

   return contents->countPages (_list, sepSheet->isChecked ());
//       && _printer->doubleSidedPrinting ());
   }


void Printopt::checkPagecount (bool checked)
   {
   int count;

   // we try to change the page count, but this seems to be ignored
   _printer->setFromTo (1, count);
   }


void Printopt::paintEvent (QPaintEvent * event)
   {
   QPainter p (this);

   p.fillRect (textColour->geometry (), _textColourEdit);
   p.fillRect (fillColour->geometry (), _fillColourEdit);
   QFrame::paintEvent (event);
   }


void Printopt::selectColour (void)
   {
   // if only Norwegians could spell :-)
   QColor &colour = sender () == textColour ? _textColourEdit : _fillColourEdit;
   QColor col;

   col = QColorDialog::getColor (colour, this);
   if (colour.isValid ())
      colour = col;
   }


bool Printopt::eventFilter (QObject *object, QEvent *event)
   {
   if (object == textFont && event->type() == QEvent::MouseButtonDblClick)
      {
      bool ok;

      QFont font = QFontDialog::getFont (&ok, _fontEdit, this);
      if (ok)
         {
         _fontEdit = font;
         textFont->setFont (_fontEdit);
         }
      return true;
      }
   return false;
   }
