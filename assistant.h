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
#include <QList>
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


/** Help with filing from an AI model: a name for a stack from what its
    pages say, and where a stack scanned as one pile divides into separate
    documents.

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

   //! a document found within a stack
   struct Doc
      {
      int first;       //!< its first page, from 0
      QString name;    //!< a name for it, empty if none was given

      bool operator== (const Doc &other) const
         { return first == other.first && name == other.name; }
      };

   //! the most text of a stack's pages which is sent to name it
   static constexpr int name_text_chars = 6000;

   /** how many of a stack's first pages are looked at to name it; how
       many of those which say something are sent; and the most text of
       each. A cover's title is often in type which OCR cannot read, and
       the page after it blank, so the title is found a few pages in */
   static constexpr int name_look_pages = 8;
   static constexpr int name_pages = 4;
   static constexpr int name_page_chars = 2000;

   //! how many names of other stacks are given, as the style to follow
   static constexpr int name_examples = 20;

   /** the text to name a stack from: what its first pages say, each under
       its page number, leaving out those which say nothing

      \param pages  what each of the stack's first pages says, from the
                    first; an empty string for one which says nothing
      \returns the text, empty if no page says anything */
   static QString nameText (const QStringList &pages);

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

   //! the most pages a stack may have to be divided into documents
   static constexpr int split_pages = 200;

   //! the most text sent to divide a stack, shared between its pages
   static constexpr int split_text_chars = 100000;

   //! the most and least of each page's text which is sent to divide a
   //! stack: the start of a page is what shows a new document
   static constexpr int split_page_chars = 1500;
   static constexpr int split_page_min_chars = 200;

   /** suggest how a stack divides into documents, as when a pile of
       post is scanned in one go

      \param pages     the text of each of its pages, empty for a page
                       with none
      \param examples  names of other stacks in its folder
      \param context   @p done is not called once this has gone
      \param done      called with the documents, in order, the first
                       starting at page 0; or with none and why */
   void suggestSplit (const QStringList &pages, const QStringList &examples,
                      QObject *context,
                      std::function<void (const QList<Doc> &docs,
                                          const QString &error)> done);

   //! the question which suggestSplit() asks
   static QString splitPrompt (const QStringList &pages,
                               const QStringList &examples);

   /** read the documents from the model's answer, making them usable
       whatever it gave: in order, each starting at a page of its own
       which the stack has, and the first at page 0

      \param answer     the answer, with pages numbered from 1
      \param pagecount  number of pages in the stack
      \returns the documents, empty if the answer holds none */
   static QList<Doc> splitFromAnswer (const QJsonObject &answer,
                                      int pagecount);

private:
   //! the names whose style a new name should follow, as a prompt's start
   static QString examplesPrompt (const QStringList &examples);

   std::unique_ptr<AssistantBackend> _backend;
   };
