/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2025 Simon Glass, chch-kiwi@users.sourceforge.net
*/

#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

#include "searchindex.h"
#include "err.h"
#include "file.h"
#include "ocrpage.h"

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

   // Open/create SQLite database, on a connection of our own
   static int count;

   _connection = QString("paperindex-%1").arg(++count);
   _db = QSqlDatabase::addDatabase("QSQLITE", _connection);
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

   // the stacks indexed, so that sync() can tell which have changed
   if (!query.exec("CREATE TABLE IF NOT EXISTS files ("
                   "  filepath TEXT PRIMARY KEY, "
                   "  mtime INTEGER, "
                   "  size INTEGER)"))
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

QString SearchIndex::matchQuery(const QString &text)
   {
   QStringList words;

   for (QString word : text.split(QRegularExpression("\\s+"),
                                  Qt::SkipEmptyParts))
      {
      // a word is a string to FTS5, where a double quote is doubled
      word.replace('"', "\"\"");
      words << '"' + word + '"';
      }
   if (!words.isEmpty())
      words.last() += '*';
   return words.join(' ');
   }


/* the text of a stack's pages: what OCR read from each, or else the
   text the file itself has, such as a PDF's */
static err_info *stackText(const QString &pathname, QStringList &pages)
   {
   QFileInfo fi(pathname);
   QString dir = fi.absolutePath() + "/";
   File *file = File::createFile(dir, fi.fileName(), nullptr,
                                 File::typeFromName(fi.fileName()));
   err_info *err = file->load();

   pages.clear();
   for (int i = 0; !err && i < file->pagecount(); i++)
      {
      OcrPage ocr;
      QString text;

      if (!file->getPageOcr(i, ocr) && !ocr.isEmpty())
         text = ocr.text();
      else
         file->getPageText(i, text);
      pages << text.trimmed();
      }
   delete file;
   return err;
   }


err_info *SearchIndex::sync(const QString &dirPath, const Progress &progress)
   {
   if (!_db.isOpen())
      return err_make(ERRFN, ERR_index_not_open);

   QString dir = QDir(dirPath).absolutePath() + "/";
   QHash<QString, QPair<qint64, qint64>> known;
   QSqlQuery query(_db);

   query.prepare("SELECT filepath, mtime, size FROM files "
                 "WHERE substr(filepath, 1, length(?)) = ?");
   query.addBindValue(dir);
   query.addBindValue(dir);
   if (!query.exec())
      return err_make(ERRFN, ERR_sql_error1,
                      qPrintable(query.lastError().text()));
   while (query.next())
      known.insert(query.value(0).toString(),
                   qMakePair(query.value(1).toLongLong(),
                             query.value(2).toLongLong()));

   QStringList paths;
   QDirIterator it(dir, QStringList() << "*.max" << "*.pdf", QDir::Files,
                   QDirIterator::Subdirectories);

   while (it.hasNext())
      paths << it.next();

   _db.transaction();
   int done = 0;
   for (const QString &path : paths)
      {
      QFileInfo fi(path);
      QPair<qint64, qint64> stamp(fi.lastModified().toMSecsSinceEpoch(),
                                  fi.size());
      auto found = known.find(path);

      if (found != known.end() && found.value() == stamp)
         {
         known.erase(found);
         }
      else
         {
         QStringList pages;

         known.remove(path);
         removeFile(path);
         if (!stackText(path, pages))
            for (int i = 0; i < pages.size(); i++)
               if (!pages[i].isEmpty())
                  addPage(path, fi.fileName(), i, pages[i]);

         // a stack which cannot be read is not looked at again until it changes
         query.prepare("INSERT OR REPLACE INTO files (filepath, mtime, size) "
                       "VALUES (?, ?, ?)");
         query.addBindValue(path);
         query.addBindValue(stamp.first);
         query.addBindValue(stamp.second);
         query.exec();
         }
      if (progress && !progress(done + 1, paths.size()))
         break;
      done++;
      }

   // drop the stacks which have gone
   if (done == paths.size())
      for (auto it = known.cbegin(); it != known.cend(); ++it)
         {
         removeFile(it.key());
         query.prepare("DELETE FROM files WHERE filepath = ?");
         query.addBindValue(it.key());
         query.exec();
         }
   _db.commit();
   return nullptr;
   }


err_info *SearchIndex::search(const QString &searchQuery, QList<SearchResult> &results,
                               int maxResults, const QString &underDir)
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
      "  AND substr(filepath, 1, length(?)) = ? "
      "ORDER BY rank "
      "LIMIT ?";
   // an empty string, not a null one, which would be NULL to SQLite
   QString dir = underDir.isEmpty() ? QString("")
                 : QDir(underDir).absolutePath() + "/";

   query.prepare(sql);
   query.addBindValue(searchQuery);
   query.addBindValue(dir);
   query.addBindValue(dir);
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
      _db = QSqlDatabase();
      QSqlDatabase::removeDatabase(_connection);
      }
   }
