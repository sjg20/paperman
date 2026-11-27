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
#include <QMap>
#include "wasm/noprocess.h"
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include "assistant.h"
#include "backgroundwork.h"

const char *Assistant::default_command = "claude";
const char *Assistant::default_model = "claude-opus-5-5";


ClaudeCommandBackend::ClaudeCommandBackend (const QString &command,
                                            const QString &model)
   {
   _command = command;
   _model = model;
   }


QStringList ClaudeCommandBackend::arguments (const QString &system,
                                             const QJsonObject &schema) const
   {
   QStringList args;

   /* print the answer and stop, with no tools, no settings of the user's
      and nothing kept afterwards: it reads the question and answers */
   args << "--print" << "--tools" << "" << "--setting-sources" << ""
        << "--no-session-persistence" << "--output-format" << "json"
        << "--system-prompt" << system << "--json-schema"
        << QString::fromUtf8 (QJsonDocument (schema)
                                 .toJson (QJsonDocument::Compact));
   if (!_model.isEmpty ())
      args << "--model" << _model;
   return args;
   }


QString ClaudeCommandBackend::parse (const QByteArray &output,
                                     QJsonObject &answer)
   {
   QJsonParseError err;
   QJsonObject reply = QJsonDocument::fromJson (output, &err).object ();

   answer = QJsonObject ();
   if (err.error != QJsonParseError::NoError)
      return QObject::tr ("The assistant gave an answer which cannot be "
                          "read: %1").arg (err.errorString ());
   if (reply.value ("is_error").toBool ())
      return QObject::tr ("The assistant could not answer: %1")
            .arg (reply.value ("result").toString ());
   if (!reply.value ("structured_output").isObject ())
      return QObject::tr ("The assistant gave no answer");
   answer = reply.value ("structured_output").toObject ();
   return QString ();
   }


void ClaudeCommandBackend::ask (const QString &system, const QString &prompt,
                                const QJsonObject &schema, QObject *context,
                                Done done)
   {
   QProcess *proc = new QProcess (context);
   QTimer *timer = new QTimer (proc);
   auto finished = std::make_shared<bool> (false);
   QString command = _command;

   // whichever comes first reports, and only once
   auto report = [proc, finished, done] (const QJsonObject &answer,
                                         const QString &error) {
      if (*finished)
         return;
      *finished = true;
      proc->deleteLater ();
      done (answer, error);
   };

   QObject::connect (proc, &QProcess::errorOccurred, proc,
                     [report, command] (QProcess::ProcessError error) {
      if (error == QProcess::FailedToStart)
         report (QJsonObject (),
                 QObject::tr ("Cannot run '%1': is it installed?")
                    .arg (command));
   });
   QObject::connect (proc,
         QOverload<int, QProcess::ExitStatus>::of (&QProcess::finished), proc,
         [proc, report] (int code, QProcess::ExitStatus status) {
      QJsonObject answer;
      QString error = parse (proc->readAllStandardOutput (), answer);

      /* it prints why it failed as JSON too, so prefer that to its exit
         code, which says only that it did */
      if (!error.isEmpty () && (status != QProcess::NormalExit || code))
         {
         QString said = QString::fromUtf8 (proc->readAllStandardError ())
               .trimmed ();

         if (!said.isEmpty ())
            error = QObject::tr ("The assistant failed: %1").arg (said);
         }
      report (answer, error);
   });
   timer->setSingleShot (true);
   QObject::connect (timer, &QTimer::timeout, proc, [proc, report] () {
      proc->kill ();
      report (QJsonObject (), QObject::tr ("The assistant took too long"));
   });

   /* the request is listed with the other work going on for as long as
      the command runs, where the user can give up on it */
   Backgroundjob *job = new Backgroundjob (QObject::tr ("Asking the "
                                                         "assistant"), proc);

   job->setStoppable (true);
   QObject::connect (job, &Backgroundjob::stopAsked, proc, [proc, report] () {
      proc->kill ();
      report (QJsonObject (), QObject::tr ("Stopped asking the assistant"));
   });

   proc->start (_command, arguments (system, schema));
   proc->write (prompt.toUtf8 ());
   proc->closeWriteChannel ();
   timer->start (timeout_ms);
   }


Assistant::Assistant (AssistantBackend *backend, QObject *parent)
      : QObject (parent)
   {
   _backend.reset (backend ? backend
                           : new ClaudeCommandBackend (command (), model ()));
   }


Assistant::~Assistant ()
   {
   }


bool Assistant::enabled ()
   {
   return QSettings ().value ("assistant/enabled", false).toBool ();
   }


void Assistant::setEnabled (bool enable)
   {
   QSettings ().setValue ("assistant/enabled", enable);
   }


QString Assistant::command ()
   {
   QString cmd = QSettings ().value ("assistant/command").toString ();

   return cmd.isEmpty () ? QString (default_command) : cmd;
   }


void Assistant::setCommand (const QString &command)
   {
   QSettings ().setValue ("assistant/command", command.trimmed ());
   }


QString Assistant::model ()
   {
   QString name = QSettings ().value ("assistant/model").toString ();

   return name.isEmpty () ? QString (default_model) : name;
   }


void Assistant::setModel (const QString &model)
   {
   QSettings ().setValue ("assistant/model", model.trimmed ());
   }


QString Assistant::examplesPrompt (const QStringList &examples)
   {
   QString prompt;

   if (!examples.isEmpty ())
      {
      prompt += "Names of other documents in the same folder, whose style "
                "the new name should follow:\n";
      for (const QString &name : examples.mid (0, name_examples))
         prompt += "- " + name + "\n";
      prompt += "\n";
      }
   return prompt;
   }


QString Assistant::namePrompt (const QString &text,
                               const QStringList &examples)
   {
   QString prompt = examplesPrompt (examples);

   prompt += "Text read from the first pages of the document, which may "
             "have mistakes from OCR. Large or decorative type, as on a "
             "cover, is often missing from it, so a later page may give a "
             "title more fully than the first:\n\n";
   prompt += text.left (name_text_chars);
   return prompt;
   }


QString Assistant::nameText (const QStringList &pages)
   {
   QString text;
   int used = 0;

   for (int i = 0; i < pages.size () && used < name_pages; i++)
      {
      QString page = pages [i].trimmed ();

      if (page.isEmpty ())
         continue;
      text += QString ("=== Page %1 ===\n%2\n\n").arg (i + 1)
                 .arg (page.left (name_page_chars));
      used++;
      }
   return text;
   }


QString Assistant::cleanName (const QString &name)
   {
   QString clean = name.simplified ();

   // nothing which a filename cannot hold on some system
   clean.remove (QRegularExpression ("[\\\\/:*?\"<>|]"));
   clean.remove (QRegularExpression ("\\.(max|pdf|jpe?g)$",
         QRegularExpression::CaseInsensitiveOption));
   while (clean.startsWith ('.'))
      clean.remove (0, 1);
   return clean.left (80).trimmed ();
   }


void Assistant::suggestName (const QString &text,
      const QStringList &examples, QObject *context,
      std::function<void (const QString &name, const Docfacts &facts,
                          const QString &error)> done)
   {
   static const char *system =
      "You name scanned documents for a personal filing system. Given the "
      "text of a document, reply with a short, specific name for it: what "
      "it is, who it is from and the period or date it covers, as far as "
      "the text says. A book or a report is named by its title, in full, "
      "main title first: take it from whichever page gives it most fully. "
      "Follow the style of the other names in the folder, "
      "if any are given. Use only what the text says: do not guess a "
      "sender or a date which is not there. No file extension. "
      "Give too the facts which the text states about the document, to be "
      "kept with it: leave out any which the text does not give.";
   QJsonObject name_type, props, schema;

   name_type ["type"] = "string";
   props ["name"] = name_type;
   props ["facts"] = Docfacts::schema ();
   schema ["type"] = "object";
   schema ["properties"] = props;
   schema ["required"] = QJsonArray {"name"};
   schema ["additionalProperties"] = false;

   _backend->ask (system, namePrompt (text, examples), schema, context,
                  [done] (const QJsonObject &answer, const QString &error) {
      QString name = cleanName (answer.value ("name").toString ());
      Docfacts facts = Docfacts::fromJson (answer.value ("facts").toObject ());

      if (error.isEmpty () && name.isEmpty ())
         done (name, Docfacts (),
               tr ("The assistant did not suggest a name"));
      else
         done (name, error.isEmpty () ? facts : Docfacts (), error);
   });
   }


QString Assistant::folderPrompt (const QString &text, const QString &name,
                                 const QStringList &folders,
                                 const QString &today)
   {
   QString prompt = "Folders the document can be filed in:\n";

   for (const QString &folder : folders)
      prompt += "- " + folder + "\n";
   prompt += QString ("\nToday is %1.\n").arg (today);
   if (!name.isEmpty ())
      prompt += QString ("The document is called: %1\n").arg (name);
   prompt += "\nText read from the first pages of the document, which may "
             "have mistakes from OCR:\n\n";
   prompt += text.left (name_text_chars);
   return prompt;
   }


void Assistant::suggestFolder (const QString &text, const QString &name,
      const QStringList &folders, const QString &today, QObject *context,
      std::function<void (const QString &folder, const QString &error)> done)
   {
   static const char *system =
      "You file scanned documents for a personal filing system. Given the "
      "text of a document and the folders it can go in, reply with the one "
      "folder it belongs in, written exactly as it is listed. Go by what "
      "the document is and who it is from, and, where folders are named "
      "for a year or a month, by the date of the document or the period "
      "it covers, not by today's date. If no folder listed fits, reply "
      "with an empty string: do not make one up, and do not pick one "
      "merely because something must be picked.";
   QJsonObject folder_type, props, schema;

   if (folders.isEmpty ())
      {
      done (QString (), tr ("There are no folders to file it in"));
      return;
      }
   if (folders.size () > folder_max)
      {
      done (QString (), tr ("There are too many folders to choose between "
                            "(%1; the most is %2)").arg (folders.size ())
                           .arg (folder_max));
      return;
      }

   folder_type ["type"] = "string";
   props ["folder"] = folder_type;
   schema ["type"] = "object";
   schema ["properties"] = props;
   schema ["required"] = QJsonArray {"folder"};
   schema ["additionalProperties"] = false;

   _backend->ask (system, folderPrompt (text, name, folders, today), schema,
                  context,
                  [done, folders] (const QJsonObject &answer,
                                   const QString &error) {
      QString folder = answer.value ("folder").toString ().trimmed ();

      // a slash at either end is neither here nor there
      while (folder.startsWith ('/'))
         folder.remove (0, 1);
      while (folder.endsWith ('/'))
         folder.chop (1);
      if (!error.isEmpty ())
         done (QString (), error);

      // one which is not on the list is no answer, however likely
      else if (!folder.isEmpty () && !folders.contains (folder))
         done (QString (), tr ("The assistant named a folder which is not "
                               "there: %1").arg (folder));
      else
         done (folder, QString ());
   });
   }


void Assistant::suggestSplit (const QStringList &pages,
      const QStringList &examples, QObject *context,
      std::function<void (const QList<Doc> &docs, const QString &error)> done)
   {
   static const char *system =
      "You sort scanned paper for a personal filing system. A pile of "
      "separate documents, such as a day's post, is scanned in one go, and "
      "you are given the text of each page in order. Work out where each "
      "document starts. Reply with every document in order: the number of "
      "its first page, and a short, specific name for it: what it is, who "
      "it is from and the period or date it covers, as far as the text "
      "says. The first document starts at page 1. A page which carries on "
      "from the one before (the same sender and subject, a page number "
      "which follows, a sentence which runs on, the back of a sheet) "
      "belongs with it, as does a page with no text. Start a new document "
      "only where the text shows one: when unsure, keep pages together. "
      "Follow the style of the other names in the folder, if any are "
      "given. Use only what the text says. No file extension. Give too "
      "the facts which the text states about each document, to be kept "
      "with it: leave out any which the text does not give.";
   QJsonObject page_type, name_type, props, doc, docs_type, top, schema;
   int pagecount = pages.size ();

   if (pagecount > split_pages)
      {
      done (QList<Doc> (), tr ("This stack has too many pages for the "
                               "assistant to divide (the most is %1)")
               .arg (split_pages));
      return;
      }

   page_type ["type"] = "integer";
   name_type ["type"] = "string";
   props ["first_page"] = page_type;
   props ["name"] = name_type;
   props ["facts"] = Docfacts::schema ();
   doc ["type"] = "object";
   doc ["properties"] = props;
   doc ["required"] = QJsonArray {"first_page", "name"};
   doc ["additionalProperties"] = false;
   docs_type ["type"] = "array";
   docs_type ["items"] = doc;
   top ["documents"] = docs_type;
   schema ["type"] = "object";
   schema ["properties"] = top;
   schema ["required"] = QJsonArray {"documents"};
   schema ["additionalProperties"] = false;

   _backend->ask (system, splitPrompt (pages, examples), schema, context,
                  [done, pagecount] (const QJsonObject &answer,
                                     const QString &error) {
      QList<Doc> docs;

      if (error.isEmpty ())
         docs = splitFromAnswer (answer, pagecount);
      if (error.isEmpty () && docs.isEmpty ())
         done (docs, tr ("The assistant did not say how to divide the "
                         "stack"));
      else
         done (docs, error);
   });
   }


QString Assistant::splitPrompt (const QStringList &pages,
                                const QStringList &examples)
   {
   QString prompt = examplesPrompt (examples);

   // each page gets an equal share of what may be sent
   int chars = qBound ((int)split_page_min_chars,
                       split_text_chars / qMax ((int)pages.size (), 1),
                       (int)split_page_chars);

   prompt += QString ("The stack has %1 pages. This is the start of the "
                      "text read from each, which may have mistakes from "
                      "OCR:\n").arg (pages.size ());
   for (int i = 0; i < pages.size (); i++)
      {
      QString text = pages [i].trimmed ().left (chars);

      prompt += QString ("\n=== Page %1 ===\n").arg (i + 1);
      prompt += text.isEmpty () ? QString ("(no text)") : text;
      prompt += "\n";
      }
   return prompt;
   }


QList<Assistant::Doc> Assistant::splitFromAnswer (const QJsonObject &answer,
                                                  int pagecount)
   {
   QMap<int, Doc> starts;

   // in order, with one document at most starting at any page
   for (const QJsonValue &val : answer.value ("documents").toArray ())
      {
      QJsonObject obj = val.toObject ();
      int first = obj.value ("first_page").toInt () - 1;

      if (first >= 0 && first < pagecount && !starts.contains (first))
         starts [first] = Doc {first,
               cleanName (obj.value ("name").toString ()),
               Docfacts::fromJson (obj.value ("facts").toObject ())};
      }

   QList<Doc> docs;

   // the pages before the first one given belong to it
   for (Doc doc : starts)
      {
      if (docs.isEmpty ())
         doc.first = 0;
      docs << doc;
      }
   return docs;
   }
