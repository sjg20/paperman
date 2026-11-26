/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2025 Simon Glass, chch-kiwi@users.sourceforge.net
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

#ifndef __searchindex_h
#define __searchindex_h

#include <functional>

#include <QString>
#include <QSqlDatabase>
#include <QList>

#include "docfacts.h"

struct err_info;

/**
 * Search result entry containing file, page, and matching text snippet
 */
struct SearchResult
   {
   QString filepath;      //!< Full path to the .max file
   QString filename;      //!< Just the filename
   int pagenum;           //!< Page number (0-based)
   QString snippet;       //!< Text snippet showing match context
   double rank;           //!< Search relevance rank
   };

/** A stack found by the facts kept about it */
struct FactsResult
   {
   QString filepath;      //!< full path to the stack
   Docfacts facts;        //!< the facts it has
   };

/** Which stacks to find by their facts
 *
 * Each field left empty matches any stack which has facts; those given
 * must all match
 */
struct FactsQuery
   {
   QString type;       //!< the kind of document, whatever its case
   QString from;       //!< part of who it is from
   QString currency;   //!< the currency of its amount

   /** the earliest and latest date of the document, each a year, a year
       and month, or a day ("2025", "2025-03"), so that a year for both
       finds everything dated within it. A stack with no date is left out
       when either is given */
   QString dateFrom, dateTo;

   /** the least and most its amount may be, as numbers. A stack with no
       amount is left out when either is given */
   QString minAmount, maxAmount;

   //! words which must each be somewhere in its type, sender, period or
   //! summary
   QString text;

   //! the directory the stack must be in or below
   QString underDir;
   };

/**
 * Full-text search index using SQLite FTS5
 *
 * This class manages a search index for OCR text extracted from .max files.
 * It uses SQLite's FTS5 (Full-Text Search) extension for fast text search.
 */
class SearchIndex
   {
public:
   SearchIndex();
   ~SearchIndex();

   /** Initialize the search index for a directory
    *
    * Creates or opens the .paperindex database in the specified directory
    *
    * \param dirPath  Directory containing .max files
    * \return         error, or NULL if successful
    */
   err_info *init(const QString &dirPath);

   /** Initialize a search index for a directory, kept elsewhere
    *
    * The index is kept at @p indexPath rather than in the directory.  It
    * must be on this computer's own disk: it is opened so that it can be
    * searched on one connection while another brings it up to date
    *
    * \param dirPath    Directory containing .max files
    * \param indexPath  Path to the index database
    * \return           error, or NULL if successful
    */
   err_info *init(const QString &dirPath, const QString &indexPath);

   /** Add or update OCR text for a file/page in the index
    *
    * \param filepath  Full path to the .max file
    * \param filename  Just the filename
    * \param pagenum   Page number (0-based)
    * \param text      OCR text content
    * \return          error, or NULL if successful
    */
   err_info *addPage(const QString &filepath, const QString &filename,
                     int pagenum, const QString &text);

   /** Remove all entries for a file from the index
    *
    * \param filepath  Full path to the .max file
    * \return          error, or NULL if successful
    */
   err_info *removeFile(const QString &filepath);

   /** Search the index for matching text
    *
    * \param query     Search query (can use FTS5 syntax)
    * \param results   Returns list of matching results
    * \param maxResults Maximum number of results to return (default 100)
    * \return          error, or NULL if successful
    */
   err_info *search(const QString &query, QList<SearchResult> &results,
                    int maxResults = 100, const QString &underDir = QString());

   /** Turn text typed by the user into a query for search()
    *
    * Each word must appear on the page, and the last may be the start of
    * a longer word, since it may still be being typed. Nothing typed is
    * taken as query syntax, so any text is a valid query
    *
    * \param text     what the user typed
    * \return         the query, empty if there are no words in @p text
    */
   static QString matchQuery(const QString &text);

   /** Get the facts the index has for a stack
    *
    * These are a copy of those in the stack's file, as of when it was
    * last indexed
    *
    * \param filepath  Full path to the stack
    * \param facts     Returns the facts, empty if it has none
    * \return          error, or NULL if successful
    */
   err_info *getFacts(const QString &filepath, Docfacts &facts);

   /** Find stacks by the facts kept about them
    *
    * Only stacks which have facts are found, the latest first, with those
    * which have no date last
    *
    * \param query       which stacks to find
    * \param results     returns the stacks found
    * \param maxResults  the most to return
    * \return            error, or NULL if successful
    */
   err_info *findFacts(const FactsQuery &query, QList<FactsResult> &results,
                       int maxResults = 100);

   /** Reports progress while bringing the index up to date
    *
    * \param done   how many stacks have been looked at
    * \param total  how many there are
    * \return false to stop
    */
   typedef std::function<bool (int done, int total)> Progress;

   /** Bring the index up to date with the stacks in a directory
    *
    * Each stack's pages are indexed from the text kept with them: what
    * OCR read, or for a PDF, its text. A stack is only read if it has
    * changed since it was last indexed, so this is quick once done, and
    * stacks which have gone are dropped from the index
    *
    * \param dirPath   directory to look through, with its subdirectories
    * \param progress  reports progress, or nullptr
    * \return          error, or NULL if successful
    */
   err_info *sync(const QString &dirPath, const Progress &progress = nullptr);

   /** Bring the index up to date with one stack
    *
    * The stack is read if it has changed since it was last indexed, and
    * dropped from the index if it has gone
    *
    * \param path  the stack's pathname
    * \return      error, or NULL if successful
    */
   err_info *syncStack(const QString &path);

   /** Check if index is open and ready
    *
    * \return true if index is initialized
    */
   bool isOpen() const { return _db.isOpen(); }

   /** Get the path to the index database file
    *
    * \return path to .paperindex file
    */
   QString indexPath() const { return _indexPath; }

   /** Close the index database */
   void close();

private:
   QSqlDatabase _db;       //!< SQLite database connection
   QString _connection;    //!< the name of the connection, unique to this
   QString _indexPath;     //!< Path to the .paperindex file
   QString _dirPath;       //!< Directory being indexed

   /** Create the FTS5 table if it doesn't exist */
   err_info *createTable();

   /** Keep a stack's facts, in place of any the index has for it */
   void putFacts(const QString &filepath, const Docfacts &facts);

   /** Index a stack's text and facts afresh, recording its time and size */
   void indexStack(const QString &path, qint64 mtime, qint64 size);
   };

#endif
