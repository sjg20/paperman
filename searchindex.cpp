/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2025 Simon Glass, chch-kiwi@users.sourceforge.net
*/

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

#include "searchindex.h"
#include "err.h"

SearchIndex::SearchIndex()
   {
   }

SearchIndex::~SearchIndex()
   {
   close();
   }

err_info *SearchIndex::init(const QString &dirPath)
   {
   _dirPath = dirPath;

   // Ensure directory path ends with /
   QString dir = dirPath;
   if (!dir.endsWith('/'))
      dir += '/';

   _indexPath = dir + ".paperindex";

   // Open/create SQLite database
   _db = QSqlDatabase::addDatabase("QSQLITE", "paperindex");
   _db.setDatabaseName(_indexPath);

   if (!_db.open())
      {
      return err_make(ERRFN, ERR_cannot_open_file1,
                     qPrintable(_indexPath));
      }

   // Create FTS5 table if it doesn't exist
   return createTable();
   }

err_info *SearchIndex::createTable()
   {
   QSqlQuery query(_db);

   // Create FTS5 virtual table for full-text search
   // Store: filepath, filename, page number, and text content
   QString sql =
      "CREATE VIRTUAL TABLE IF NOT EXISTS ocr_index USING fts5("
      "  filepath UNINDEXED, "
      "  filename, "
      "  pagenum UNINDEXED, "
      "  text, "
      "  tokenize='porter unicode61 remove_diacritics 1'"
      ")";

   if (!query.exec(sql))
      {
      QString error = query.lastError().text();
      return err_make(ERRFN, ERR_sql_error1, qPrintable(error));
      }

   return nullptr;
   }

err_info *SearchIndex::addPage(const QString &filepath, const QString &filename,
                                int pagenum, const QString &text)
   {
   if (!_db.isOpen())
      return err_make(ERRFN, ERR_index_not_open);

   QSqlQuery query(_db);

   // First, remove any existing entry for this file/page
   query.prepare("DELETE FROM ocr_index WHERE filepath = ? AND pagenum = ?");
   query.addBindValue(filepath);
   query.addBindValue(pagenum);

   if (!query.exec())
      {
      QString error = query.lastError().text();
      return err_make(ERRFN, ERR_sql_error1, qPrintable(error));
      }

   // Insert the new text
   query.prepare("INSERT INTO ocr_index (filepath, filename, pagenum, text) "
                 "VALUES (?, ?, ?, ?)");
   query.addBindValue(filepath);
   query.addBindValue(filename);
   query.addBindValue(pagenum);
   query.addBindValue(text);

   if (!query.exec())
      {
      QString error = query.lastError().text();
      return err_make(ERRFN, ERR_sql_error1, qPrintable(error));
      }

   return nullptr;
   }

err_info *SearchIndex::removeFile(const QString &filepath)
   {
   if (!_db.isOpen())
      return err_make(ERRFN, ERR_index_not_open);

   QSqlQuery query(_db);
   query.prepare("DELETE FROM ocr_index WHERE filepath = ?");
   query.addBindValue(filepath);

   if (!query.exec())
      {
      QString error = query.lastError().text();
      return err_make(ERRFN, ERR_sql_error1, qPrintable(error));
      }

   return nullptr;
   }

err_info *SearchIndex::search(const QString &searchQuery, QList<SearchResult> &results,
                               int maxResults)
   {
   if (!_db.isOpen())
      return err_make(ERRFN, ERR_index_not_open);

   results.clear();

   QSqlQuery query(_db);

   // Use FTS5 MATCH for full-text search with ranking
   // snippet() extracts context around matches
   QString sql =
      "SELECT filepath, filename, pagenum, "
      "       snippet(ocr_index, 3, '<b>', '</b>', '...', 20) as snippet, "
      "       rank "
      "FROM ocr_index "
      "WHERE text MATCH ? "
      "ORDER BY rank "
      "LIMIT ?";

   query.prepare(sql);
   query.addBindValue(searchQuery);
   query.addBindValue(maxResults);

   if (!query.exec())
      {
      QString error = query.lastError().text();
      return err_make(ERRFN, ERR_sql_error1, qPrintable(error));
      }

   while (query.next())
      {
      SearchResult result;
      result.filepath = query.value(0).toString();
      result.filename = query.value(1).toString();
      result.pagenum = query.value(2).toInt();
      result.snippet = query.value(3).toString();
      result.rank = query.value(4).toDouble();
      results.append(result);
      }

   return nullptr;
   }

void SearchIndex::close()
   {
   if (_db.isOpen())
      {
      _db.close();
      QSqlDatabase::removeDatabase("paperindex");
      }
   }
