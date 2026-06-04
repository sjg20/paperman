/*
License: GPL-2
*/

#include "localbackend.h"

#include <QDir>
#include <QFileInfo>


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
