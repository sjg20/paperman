/*
License: GPL-2
*/

#include "localbackend.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>


LocalBackend::LocalBackend(const QStringList &rootPaths)
   : _rootPaths(rootPaths)
{
}


QList<RepositoryInfo> LocalBackend::listRepositories()
{
   QList<RepositoryInfo> out;
   for (const QString &path : _rootPaths) {
      RepositoryInfo r;
      r.path   = path;
      r.exists = QDir(path).exists();
      r.name   = r.exists ? QFileInfo(path).fileName() : QString();
      out.append(r);
   }
   return out;
}


QString LocalBackend::rootPathForName(const QString &repo) const
{
   for (const QString &p : _rootPaths) {
      if (QFileInfo(p).fileName() == repo)
         return p;
   }
   return QString();
}


DirectoryListing LocalBackend::browseDirectory(const QString &repo,
                                               const QString &dir)
{
   DirectoryListing out;

   /* Reject absolute paths and any traversal attempt before touching
    * the filesystem. */
   if (dir.contains("..") || dir.startsWith("/")) {
      out.error = "invalid path";
      return out;
   }

   QString rootPath = repo.isEmpty()
                          ? (_rootPaths.isEmpty() ? QString()
                                                  : _rootPaths.first())
                          : rootPathForName(repo);
   if (rootPath.isEmpty()) {
      out.error    = "repository not found: " + repo;
      out.notFound = true;
      return out;
   }

   QString fullPath = rootPath;
   if (!dir.isEmpty())
      fullPath += "/" + dir;

   QDir d(fullPath);
   if (!d.exists()) {
      out.error    = "directory not found";
      out.notFound = true;
      return out;
   }

   /* Subdirectories from the live filesystem (the cache only tracks
    * files, not empty directory shells). */
   QFileInfoList subdirs =
       d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
   for (const QFileInfo &sub : subdirs) {
      if (sub.fileName().startsWith('.'))
         continue;
      DirectoryEntry e;
      e.name = sub.fileName();
      e.path = dir.isEmpty() ? sub.fileName()
                             : dir + "/" + sub.fileName();
      e.isDir = true;
      e.count = countFilesRecursive(rootPath, e.path);
      out.entries.append(e);
   }

   /* Files from the cache (faster than another filesystem scan). */
   const QList<CachedFile> &cached = _fileCache.value(rootPath);
   for (const CachedFile &file : cached) {
      QString fileDir = QFileInfo(file.path).path();
      bool inDir = (fileDir == "." && dir.isEmpty()) || fileDir == dir;
      if (!inDir)
         continue;

      DirectoryEntry e;
      e.name     = file.name;
      e.path     = file.path;
      e.size     = file.size;
      e.modified = file.modified.toString(Qt::ISODate);
      out.entries.append(e);
   }

   out.ok = true;
   return out;
}


int LocalBackend::loadCacheForRepo(const QString &rootPath)
{
   QList<CachedFile> &fileList = _fileCache[rootPath];
   fileList.clear();

   QString papertreeFile = rootPath + "/.papertree";
   if (QFile::exists(papertreeFile)
       && loadFromPapertree(rootPath, fileList)) {
      qDebug() << "LocalBackend: file cache loaded from papertree with"
               << fileList.size() << "files";
   } else {
      scanDirectory(rootPath, "", fileList);
      qDebug() << "LocalBackend: file cache built with"
               << fileList.size() << "files";
   }
   return fileList.size();
}


int LocalBackend::cacheCount(const QString &rootPath) const
{
   return _fileCache.value(rootPath).size();
}


qint64 LocalBackend::cacheBytes(const QString &rootPath) const
{
   qint64 total = 0;
   const QList<CachedFile> &files = _fileCache.value(rootPath);
   for (const CachedFile &f : files)
      total += f.size;
   return total;
}


const QList<CachedFile> &
LocalBackend::fileCacheFor(const QString &rootPath) const
{
   auto it = _fileCache.constFind(rootPath);
   return it == _fileCache.constEnd() ? _emptyCache : it.value();
}


int LocalBackend::countFilesRecursive(const QString &repoPath,
                                      const QString &dirPath) const
{
   const QList<CachedFile> &fileList = _fileCache.value(repoPath);

   int count = 0;
   QString prefix = dirPath.isEmpty() ? "" : dirPath + "/";
   for (const CachedFile &file : fileList) {
      if (dirPath.isEmpty() || file.path.startsWith(prefix))
         count++;
   }
   return count;
}


bool LocalBackend::loadFromPapertree(const QString &repoPath,
                                     QList<CachedFile> &fileList)
{
   QString papertreeFile = repoPath + "/.papertree";
   QFile file(papertreeFile);

   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "LocalBackend: failed to open papertree file:"
                 << papertreeFile;
      return false;
   }

   QTextStream in(&file);
   QStringList dirStack;

   while (!in.atEnd()) {
      QString line = in.readLine();

      int level = 0;
      for (int i = 0; i < line.length(); i++) {
         if (line[i] == ' ')
            level++;
         else
            break;
      }
      if (level < 1)
         continue;

      QString name = line.mid(level);

      while (dirStack.size() >= level)
         dirStack.removeLast();

      QString relativePath = dirStack.join("/");
      if (!relativePath.isEmpty())
         relativePath += "/";
      relativePath += name;

      QString fullPath = repoPath + "/" + relativePath;
      QFileInfo fileInfo(fullPath);
      if (!fileInfo.exists())
         continue;

      if (fileInfo.isDir()) {
         dirStack.append(name);
      } else if (fileInfo.isFile()) {
         QString ext = name.section('.', -1).toLower();
         if (ext == "max" || ext == "pdf" || ext == "jpg"
             || ext == "jpeg" || ext == "tiff" || ext == "tif") {
            CachedFile cachedFile;
            cachedFile.path     = relativePath;
            cachedFile.name     = name;
            cachedFile.size     = fileInfo.size();
            cachedFile.modified = fileInfo.lastModified();
            fileList.append(cachedFile);
         }
      }
   }

   file.close();
   return fileList.size() > 0;
}


void LocalBackend::scanDirectory(const QString &repoPath,
                                 const QString &dirPath,
                                 QList<CachedFile> &fileList)
{
   QString fullPath = repoPath;
   if (!dirPath.isEmpty())
      fullPath += "/" + dirPath;

   QStringList nameFilters;
   nameFilters << "*.max" << "*.pdf" << "*.jpg" << "*.jpeg"
               << "*.tiff" << "*.tif";

   QDirIterator it(fullPath, nameFilters,
                   QDir::Files | QDir::Readable,
                   QDirIterator::Subdirectories);

   while (it.hasNext()) {
      QString filePath = it.next();
      QFileInfo info(filePath);

      QString relativePath = filePath;
      if (relativePath.startsWith(repoPath + "/"))
         relativePath = relativePath.mid(repoPath.length() + 1);
      else if (relativePath.startsWith(repoPath))
         relativePath = relativePath.mid(repoPath.length());

      CachedFile cached;
      cached.path     = relativePath;
      cached.name     = info.fileName();
      cached.size     = info.size();
      cached.modified = info.lastModified();
      fileList.append(cached);
   }
}
