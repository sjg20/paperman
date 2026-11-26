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
*/


#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

class File;
struct err_info;

/** what a document is: the facts about a stack which filing and finding
    go by

The name of a stack says a little of this, and the text of its pages holds
all of it somewhere, but neither can answer "what did the roofer charge
last year". These few facts can, once something has read the stack and
written them down.

They are kept in the stack's own properties (see File::getStackProps()),
each under a key starting "doc.", so that they go wherever the stack goes.
Anything else which keeps them, such as a search index, is a copy.

Each is plain text on one line. A fact which is not known is empty, and
is not kept */
class Docfacts
   {
public:
   //! what kind of document it is, in a word or two: "bill", "bank
   //! statement"
   QString type;

   //! who it is from
   QString from;

   //! the document's own date: "2026-03-14", or "2026-03" or "2026" where
   //! that is all it gives
   QString date;

   //! what it covers, as it would be said: "Feb 2026", "2025-26"
   QString period;

   //! the main amount of money in it, as a number: "184.20"
   QString amount;

   //! the currency of the amount, as three letters: "NZD"
   QString currency;

   //! what it is, in a line
   QString summary;

   //! the start of the key of each fact in a stack's properties
   static const char *const prefix;

   //! the most characters a fact may have, and the summary
   static constexpr int max_chars = 80;
   static constexpr int max_summary_chars = 200;

   //! true if nothing is known
   bool isEmpty () const;

   /** make each fact fit to keep: on one line, of no great length, with a
       date, amount or currency which is not in the form above dropped */
   void clean ();

   /** read the facts in a stack's properties

      \param props  the properties
      \returns the facts, cleaned; empty if the properties hold none */
   static Docfacts fromProps (const QMap<QString, QString> &props);

   /** put the facts in a stack's properties, in place of any there, and
       leaving every other property alone

      \param props  the properties to change */
   void toProps (QMap<QString, QString> &props) const;

   /** read the facts from JSON, as a model gives them and a server sends
       them: an object with a string for each fact which is known

      \param obj  the object
      \returns the facts, cleaned */
   static Docfacts fromJson (const QJsonObject &obj);

   //! the facts as JSON, with those which are not known left out
   QJsonObject toJson () const;

   /** the JSON schema for the object fromJson() reads, with a description
       of each fact, for asking a model for them */
   static QJsonObject schema ();

   /** read a stack's facts from its file

      \param f      the stack
      \param facts  returns the facts, empty if it has none
      \returns error, or NULL if ok; a file type which cannot keep them
               returns an error */
   static err_info *read (File *f, Docfacts &facts);

   /** keep a stack's facts in its file, in place of any it has, leaving
       its other properties alone

      \param f      the stack
      \param facts  the facts; empty to remove them
      \returns error, or NULL if ok */
   static err_info *write (File *f, const Docfacts &facts);

   bool operator== (const Docfacts &other) const;
   bool operator!= (const Docfacts &other) const
      { return !(*this == other); }

private:
   //! each fact with the name it is kept under, after the prefix
   QList<QPair<QString, QString *>> fields ();
   QList<QPair<QString, QString>> values () const;
   };
