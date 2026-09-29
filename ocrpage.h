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

#include <QByteArray>
#include <QList>
#include <QRect>
#include <QSize>
#include <QString>

/** One word read from a page, with where it is on the page */
struct OcrWord
   {
   QRect box;       //!< where the word is, in the page image's pixels
   QString text;    //!< the word, without spaces
   int conf;        //!< how sure the engine is of it, 0 to 100
   int line;        //!< which line of the page it is on, from 0
   int para;        //!< which paragraph of the page it is in, from 0

   bool operator== (const OcrWord &other) const;
   };

/** What was read from one page: its words, in reading order, with where
    each is, from which the page's text is made and a PDF's text layer
    placed. It is kept with the page, in the page's own file, so that it
    goes wherever the page goes */
class OcrPage
   {
public:
   QSize size;              //!< the page image it was read from, in pixels
   QList<OcrWord> words;    //!< in reading order

   bool isEmpty () const { return words.isEmpty (); }

   /** the page's text: the words of a line separated by spaces, lines by
       a newline and paragraphs by a blank line */
   QString text () const;

   /** the form kept in a file: compact UTF-8 JSON

      \returns the bytes, empty for a page with no words */
   QByteArray toBytes () const;

   /** read what toBytes() wrote

      \param data   the bytes
      \param page   returns the page
      \returns true if they are a page, false if not */
   static bool fromBytes (const QByteArray &data, OcrPage &page);

   /** read what tesseract writes with its 'tsv' output: a row for each
       page, block, paragraph, line and word, with the words' boxes

      \param tsv    tesseract's output
      \param size   the size of the image it read
      \param page   returns the page
      \returns true if it could be read, false if not */
   static bool fromTsv (const QByteArray &tsv, const QSize &size,
                        OcrPage &page);

   bool operator== (const OcrPage &other) const;
   };
