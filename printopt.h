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

#include <QFrame>
#include <QModelIndexList>
#include "ui_printopt.h"

class QPrinter;

class Printopt : public QFrame, public Ui::Printopt
   {
   Q_OBJECT
public:
   Printopt (QPrinter *printer, QModelIndexList &list, QWidget* parent = 0,
         const char* name = 0, Qt::WindowFlags fl = 0);

   ~Printopt();

   //! reload and reset dialogue
   void reset (void);
   
   //! set up the dialogue with the current settings
   void getxml (void);

   //! load the dialogue with the given settings
   void load ();

   //! save the dialogue's value to settings
   void save ();

   //! write the settings into the xml settings
   void putxml (void);

   //! calculates the number of pages in the print job
   int countPages (void);

protected:
   void paintEvent (QPaintEvent * event);

private:
    virtual void init();

private slots:
   /** called to check if the page count needs to be recalculated in
       response to a dialog button being pressed */
   void checkPagecount (bool checked);

   //! a colour selector button has been clicked
   void selectColour (void);

   //! event filter for opening the font dialogue when the font field is double clicked
   bool eventFilter (QObject *object, QEvent *event);

public:
   //! values stored for printing
   bool _printOdd, _printEven;  //! print odd, even sheets
   bool _sepSheet;             //! separate sheets for each stack
   bool _printNumbers, _printNames, _printSeq;
   bool _printImageInfo, _printTimestamp;   // annotations to print
   QColor _textColour;         //! text colour for annotations
   QColor _fillColour;         //! fill colour for annotations
   bool _shrinkFit;            //! shrink image to fit within margins
   bool _expandFit;            //! if smaller, expand image to fit within margins
   int _duplexMode;            //! printer duplex mode (off, short, long)
   QString _outputFilename;    //! printer output filename
   int _outputFormat;          //! printer output format (normal, PDF, PS)
   QString _printerName;       //! printer name
   QFont _font;                //! annotation font

private:
   // temporary editing values for colour
   QColor _textColourEdit;    //!< text colour in dialogue
   QColor _fillColourEdit;    //!< fill colour in dialogue
   QFont _fontEdit;           //!< font in dialogue
   QPrinter *_printer;        //!< the printer to which we are printing
   QModelIndexList &_list;    //!< list of model indexes to print
   };

