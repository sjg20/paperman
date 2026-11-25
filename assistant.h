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

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

/** Something which answers a question put to an AI model.

    The question is text, and the answer is JSON in a shape which the
    caller gives as a JSON schema, so that it can be used without picking
    words out of prose. Which model answers, and how it is reached, is up
    to the backend: the rest of the assistant does not know */
class AssistantBackend
   {
public:
   //! called with the answer, or with an empty object and why it failed
   typedef std::function<void (const QJsonObject &answer,
                               const QString &error)> Done;

   virtual ~AssistantBackend () {}

   /** ask a question

      \param system   what the model is to do, which is the same for every
                      question of a kind
      \param prompt   the question itself
      \param schema   JSON schema which the answer must match
      \param context  @p done is not called once this has gone
      \param done     called when the answer comes, or cannot */
   virtual void ask (const QString &system, const QString &prompt,
                     const QJsonObject &schema, QObject *context,
                     Done done) = 0;
   };


/** Asks the Claude Code command, run as a program for each question, with
    the question on its input.

    It runs with no tools, so that it can only read what it is given and
    answer, and uses whatever account the command is logged in with, so
    Paperman holds no key. The model itself is not on this computer: the
    text of the question goes to it */
class ClaudeCommandBackend : public AssistantBackend
   {
public:
   ClaudeCommandBackend (const QString &command, const QString &model);

   void ask (const QString &system, const QString &prompt,
             const QJsonObject &schema, QObject *context,
             Done done) override;

   //! the arguments the command is run with
   QStringList arguments (const QString &system,
                          const QJsonObject &schema) const;

   /** read the command's output

      \param output  what the command printed
      \param answer  returns the answer
      \returns empty if ok, else why there is no answer */
   static QString parse (const QByteArray &output, QJsonObject &answer);

   //! how long to wait for an answer, in milliseconds
   static constexpr int timeout_ms = 120 * 1000;

private:
   QString _command;
   QString _model;
   };


/** Help with filing from an AI model: for now, a name for a stack from
    what its pages say.

    It is off unless turned on in the options, since it sends the text of
    the user's papers to the model. What it suggests is only ever offered:
    the user accepts or changes it, and can undo it */
class Assistant : public QObject
   {
   Q_OBJECT

public:
   /** \param backend  what answers; nullptr for the one in the settings.
                       The assistant owns it */
   explicit Assistant (AssistantBackend *backend = nullptr,
                       QObject *parent = nullptr);
   ~Assistant ();

   //! whether the assistant may be used, from the settings; off unless
   //! turned on
   static bool enabled ();
   static void setEnabled (bool enable);

   //! the command to run, from the settings
   static QString command ();
   static void setCommand (const QString &command);

   //! the model to ask for, from the settings
   static QString model ();
   static void setModel (const QString &model);

   static const char *default_command;
   static const char *default_model;

   //! the most text of a stack's pages which is sent to name it
   static constexpr int name_text_chars = 6000;

   //! how many pages of a stack are looked at to name it
   static constexpr int name_pages = 2;

   //! how many names of other stacks are given, as the style to follow
   static constexpr int name_examples = 20;

   /** suggest a name for a stack

      \param text      the text of its first pages
      \param examples  names of other stacks in its folder
      \param context   @p done is not called once this has gone
      \param done      called with the name, or with an empty name and why
                       there is none */
   void suggestName (const QString &text, const QStringList &examples,
                     QObject *context,
                     std::function<void (const QString &name,
                                         const QString &error)> done);

   //! the question which suggestName() asks
   static QString namePrompt (const QString &text,
                              const QStringList &examples);

   /** make what the model gave usable as a stack's name: one line, with
       nothing a filename cannot hold, no extension and no great length

      \returns the name, empty if nothing is left of it */
   static QString cleanName (const QString &name);

private:
   std::unique_ptr<AssistantBackend> _backend;
   };
