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

#include <QString>
#include <QSqlDatabase>
#include <QList>

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
                    int maxResults = 100);

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
   QString _indexPath;     //!< Path to the .paperindex file
   QString _dirPath;       //!< Directory being indexed

   /** Create the FTS5 table if it doesn't exist */
   err_info *createTable();
   };

#endif
