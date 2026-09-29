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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include "ocrpage.h"

/* The version of the stored form, which a later one would change when
   it is read differently */
static const int stored_version = 1;


bool OcrWord::operator== (const OcrWord &other) const
   {
   return box == other.box && text == other.text && conf == other.conf
          && line == other.line && para == other.para;
   }


bool OcrPage::operator== (const OcrPage &other) const
   {
   return size == other.size && words == other.words;
   }


QString OcrPage::text () const
   {
   QString out;
   const OcrWord *prev = nullptr;

   for (const OcrWord &word : words)
      {
      if (prev)
         out += word.para != prev->para ? "\n\n"
                : word.line != prev->line ? "\n" : " ";
      out += word.text;
      prev = &word;
      }
   return out;
   }


/* Each word is an array, rather than an object, since a page has
   hundreds of them: [left, top, width, height, conf, line, para, text] */
QByteArray OcrPage::toBytes () const
   {
   if (isEmpty ())
      return QByteArray ();

   QJsonArray list;

   for (const OcrWord &word : words)
      list.append (QJsonArray {word.box.left (), word.box.top (),
                               word.box.width (), word.box.height (),
                               word.conf, word.line, word.para, word.text});

   QJsonObject obj {{"v", stored_version}, {"w", size.width ()},
                    {"h", size.height ()}, {"words", list}};

   return QJsonDocument (obj).toJson (QJsonDocument::Compact);
   }


bool OcrPage::fromBytes (const QByteArray &data, OcrPage &page)
   {
   QJsonParseError err;
   QJsonDocument doc = QJsonDocument::fromJson (data, &err);

   page = OcrPage ();
   if (err.error != QJsonParseError::NoError || !doc.isObject ())
      return false;

   QJsonObject obj = doc.object ();

   if (obj.value ("v").toInt () != stored_version)
      return false;
   page.size = QSize (obj.value ("w").toInt (), obj.value ("h").toInt ());
   for (const QJsonValue &val : obj.value ("words").toArray ())
      {
      QJsonArray a = val.toArray ();

      if (a.size () < 8)
         return false;
      page.words.append ({QRect (a[0].toInt (), a[1].toInt (),
                                 a[2].toInt (), a[3].toInt ()),
                          a[7].toString (), a[4].toInt (), a[5].toInt (),
                          a[6].toInt ()});
      }
   return true;
   }


/* The columns are: level, page_num, block_num, par_num, line_num,
   word_num, left, top, width, height, conf, text. Level 5 is a word; its
   block, paragraph and line numbers say which line and paragraph it is
   in, counting afresh in each block and paragraph, so they are numbered
   through the page here */
bool OcrPage::fromTsv (const QByteArray &tsv, const QSize &size,
                       OcrPage &page)
   {
   QList<QByteArray> rows = tsv.split ('\n');
   QString cur_line, cur_para;
   int line = -1, para = -1;

   page = OcrPage ();
   page.size = size;
   if (rows.isEmpty () || !rows[0].startsWith ("level"))
      return false;
   for (int i = 1; i < rows.size (); i++)
      {
      QList<QByteArray> col = rows[i].split ('\t');

      if (col.size () < 12 || col[0] != "5")
         continue;

      QString text = QString::fromUtf8 (col[11]).trimmed ();

      if (text.isEmpty ())
         continue;

      QString para_key = col[2] + "." + col[3];
      QString line_key = para_key + "." + col[4];

      if (para_key != cur_para)
         {
         para++;
         cur_para = para_key;
         }
      if (line_key != cur_line)
         {
         line++;
         cur_line = line_key;
         }
      page.words.append ({QRect (col[6].toInt (), col[7].toInt (),
                                 col[8].toInt (), col[9].toInt ()),
                          text, qRound (col[10].toDouble ()), line, para});
      }
   return true;
   }
