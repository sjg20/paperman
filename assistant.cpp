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
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>

#include "assistant.h"

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


QString Assistant::namePrompt (const QString &text,
                               const QStringList &examples)
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
   prompt += "Text read from the first pages of the document, which may "
             "have mistakes from OCR:\n\n";
   prompt += text.left (name_text_chars);
   return prompt;
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
      std::function<void (const QString &name, const QString &error)> done)
   {
   static const char *system =
      "You name scanned documents for a personal filing system. Given the "
      "text of a document, reply with a short, specific name for it: what "
      "it is, who it is from and the period or date it covers, as far as "
      "the text says. Follow the style of the other names in the folder, "
      "if any are given. Use only what the text says: do not guess a "
      "sender or a date which is not there. No file extension.";
   QJsonObject name_type, props, schema;

   name_type ["type"] = "string";
   props ["name"] = name_type;
   schema ["type"] = "object";
   schema ["properties"] = props;
   schema ["required"] = QJsonArray {"name"};
   schema ["additionalProperties"] = false;

   _backend->ask (system, namePrompt (text, examples), schema, context,
                  [done] (const QJsonObject &answer, const QString &error) {
      QString name = cleanName (answer.value ("name").toString ());

      if (error.isEmpty () && name.isEmpty ())
         done (name, tr ("The assistant did not suggest a name"));
      else
         done (name, error);
   });
   }
