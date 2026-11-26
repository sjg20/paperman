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

#include <QJsonValue>
#include <QRegularExpression>

#include "docfacts.h"
#include "err.h"
#include "file.h"

const char *const Docfacts::prefix = "doc.";


QList<QPair<QString, QString *>> Docfacts::fields ()
   {
   return {{"type", &type}, {"from", &from}, {"date", &date},
           {"period", &period}, {"amount", &amount},
           {"currency", &currency}, {"summary", &summary}};
   }


QList<QPair<QString, QString>> Docfacts::values () const
   {
   QList<QPair<QString, QString>> list;

   for (const auto &field : const_cast<Docfacts *> (this)->fields ())
      list << qMakePair (field.first, *field.second);
   return list;
   }


bool Docfacts::isEmpty () const
   {
   for (const auto &value : values ())
      if (!value.second.isEmpty ())
         return false;
   return true;
   }


bool Docfacts::operator== (const Docfacts &other) const
   {
   return values () == other.values ();
   }


void Docfacts::clean ()
   {
   static const QRegularExpression date_form (
      "^\\d{4}(-(0[1-9]|1[0-2])(-(0[1-9]|[12]\\d|3[01]))?)?$");
   static const QRegularExpression amount_form ("^-?\\d+(\\.\\d+)?$");
   static const QRegularExpression currency_form ("^[A-Z]{3}$");

   for (const auto &field : fields ())
      {
      int max = field.second == &summary ? max_summary_chars : max_chars;

      // on one line, which is what a property can hold
      *field.second = field.second->simplified ().left (max).trimmed ();
      }

   // a number is written with no separators for thousands
   amount.remove (',');
   currency = currency.toUpper ();
   if (!date_form.match (date).hasMatch ())
      date.clear ();
   if (!amount_form.match (amount).hasMatch ())
      amount.clear ();
   if (!currency_form.match (currency).hasMatch ())
      currency.clear ();
   }


Docfacts Docfacts::fromProps (const QMap<QString, QString> &props)
   {
   Docfacts facts;

   for (const auto &field : facts.fields ())
      *field.second = props.value (prefix + field.first);
   facts.clean ();
   return facts;
   }


void Docfacts::toProps (QMap<QString, QString> &props) const
   {
   Docfacts facts = *this;

   facts.clean ();
   for (const auto &value : facts.values ())
      {
      if (value.second.isEmpty ())
         props.remove (prefix + value.first);
      else
         props [prefix + value.first] = value.second;
      }
   }


Docfacts Docfacts::fromJson (const QJsonObject &obj)
   {
   Docfacts facts;

   for (const auto &field : facts.fields ())
      {
      QJsonValue val = obj.value (field.first);

      // a model may give an amount as a number
      if (val.isDouble ())
         *field.second = QString::number (val.toDouble (), 'f', 2);
      else
         *field.second = val.toString ();
      }
   facts.clean ();
   return facts;
   }


QJsonObject Docfacts::toJson () const
   {
   QJsonObject obj;

   for (const auto &value : values ())
      if (!value.second.isEmpty ())
         obj [value.first] = value.second;
   return obj;
   }


QJsonObject Docfacts::schema ()
   {
   static const char *const describe [][2] = {
      {"type", "The kind of document, in a word or two, lower case: "
               "bill, bank statement, letter, receipt, contract"},
      {"from", "Who the document is from: the organisation or person"},
      {"date", "The document's own date, as YYYY-MM-DD, or YYYY-MM or "
               "YYYY if that is all it gives"},
      {"period", "The period it covers, if any, as it would be said: "
                 "Feb 2026, 2025-26"},
      {"amount", "The main amount of money in it, such as the total due, "
                 "as a plain number with no symbol: 184.20"},
      {"currency", "The currency of the amount, as its three-letter "
                   "code, only if the text shows which it is"},
      {"summary", "What the document is, in one short line"},
   };
   QJsonObject props, schema;

   for (const auto &item : describe)
      {
      QJsonObject field;

      field ["type"] = "string";
      field ["description"] = item [1];
      props [item [0]] = field;
      }
   schema ["type"] = "object";
   schema ["properties"] = props;
   schema ["additionalProperties"] = false;
   return schema;
   }


err_info *Docfacts::read (File *f, Docfacts &facts)
   {
   QMap<QString, QString> props;

   facts = Docfacts ();
   CALL (f->getStackProps (props));
   facts = fromProps (props);
   return NULL;
   }


err_info *Docfacts::write (File *f, const Docfacts &facts)
   {
   QMap<QString, QString> props;

   CALL (f->getStackProps (props));
   facts.toProps (props);
   return f->putStackProps (props);
   }
