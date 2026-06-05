/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2009 Simon Glass, chch-kiwi@users.sourceforge.net
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

#include <QCoreApplication>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QMessageBox>

#include "dirmodel.h"
#include "backend.h"
#include "localbackend.h"
#include "qmimedata.h"
#include "qurl.h"

#include "err.h"

#include "mainwindow.h"
#include "maxview.h"
#include "utils.h"

//#define TRACE_INDEX


Diritem::Diritem ()
   {
   _recent = false;
   _dir_cache = 0;
   _node = nullptr;
   }


Diritem::~Diritem ()
   {
   }


void Diritem::setBackend(Backend *backend)
   {
   _backend.reset(backend);
   }

#if 0 //p
void Diritem::setRecent(QModelIndex index)
{
   _recent = true;
   _index = index;
   _dir = "/_recent";
}
#endif

bool Diritem::setDir(QString &dir)
   {
   QDir qd (dir);

   // Try to get the canonical path, but if not, use the one supplied
   _dir = qd.canonicalPath ();
   if (_dir.isEmpty()) {
      if (dir.endsWith ("/"))
         dir.chop (1);
      _dir = dir;
      qDebug () << "Diritem invalid";
      return false;
      }

   return true;
   }


QString Diritem::dirCacheFilename() const
{
   QString user = utilUserName();

   if (!user.isEmpty())
      user.prepend(".");

   return _dir + "/.papertree" + user;
}

bool Diritem::readCache()
{
   _dir_cache = utilReadTree(dirCacheFilename(), "root");

   return _dir_cache != nullptr;
}

void Diritem::dropCache()
{
   if (_dir_cache) {
      TreeItem::freeTree(_dir_cache);
      _dir_cache = nullptr;
   }
}

TreeItem *Diritem::buildCache(Operation *op)
{
   _dir_cache = utilScanDir(_dir, op);
   if (!_dir_cache) {
      qInfo() << "Failed to scan directory";
      return nullptr;
   }

   if (!utilWriteTree(dirCacheFilename(), _dir_cache)) {
      qInfo() << "Failed to write cache";
      return nullptr;
   }

   return _dir_cache;
}

TreeItem *Diritem::ensureCache(Operation *op)
{
   if (_dir_cache || readCache())
      return _dir_cache;

   return buildCache(op);
}

bool Diritem::refreshCache(const QString dirPath, Operation *op)
{
   if (!_dir_cache && !readCache()) {
      if (!buildCache(op))
         return false;

      // No need to refresh as we just build it
      return true;
   }

    TreeItem *top;

    QString rel = dirPath.mid(_dir.size() + 1);

    Q_ASSERT(_dir_cache);
    top = _dir_cache->findItemW(rel);

    if (!top) {
        // Directory is not in the cache, e.g. it was created outside the
        // app since the cache was last built. Rebuild from scratch.
        dropCache();
        if (!buildCache(op))
            return false;
        return true;
    }

    TreeItem *updated = utilScanDir(dirPath, op);

    top->freeChildren();
    top->adopt(updated);
    delete updated;

    if (!utilWriteTree(dirCacheFilename(), _dir_cache)) {
        qInfo() << "Failed to write cache";
        return false;
    }
    return true;
}

bool Diritem::addFileToCache(const QString &dirPath,
                             const QString &filename)
{
   if (!_dir_cache && !readCache())
      return false;

   QString rel = dirPath.mid(_dir.size() + 1);

   Q_ASSERT(_dir_cache);
   TreeItem *top = _dir_cache->findItemW(rel);
   if (!top)
      return false;

   QVector<QVariant> columnData = {filename};
   TreeItem *child = new TreeItem(columnData, top);

   top->appendChild(child);
   if (!utilWriteTree(dirCacheFilename(), _dir_cache)) {
      qInfo() << "Failed to write cache";
      return false;
   }

   return true;
}

Dirmodel::Dirmodel (QObject * parent)
      : QAbstractItemModel (parent)
   {
   _invisibleRoot = new DirNode;
   _invisibleRoot->populated = true;  // children managed by addDir()
   }


DirNode *Dirmodel::nodeFromIndex(const QModelIndex &index) const
{
   if (!index.isValid())
      return _invisibleRoot;
   return static_cast<DirNode *>(index.internalPointer());
}


void Dirmodel::populateNode(DirNode *node) const
{
   if (node->populated)
      return;

   /* Subdirectories come from the Backend.  Each Diritem owns a
    * Backend (set in addDir); top-level DirNodes carry a non-owning
    * pointer to it and we propagate that to every child as we
    * populate, so deeper nodes don't need a walk-up. */
   if (!node->backend) {
      node->populated = true;
      return;
   }

   /* The path passed to browseDirectory is relative to the repo
    * root.  Walk up to the root to compute it; the root is the
    * DirNode whose parent is the invisible root. */
   DirNode *root = node;
   while (root->parent && root->parent != _invisibleRoot)
      root = root->parent;
   QString relPath = (node == root)
                         ? QString()
                         : node->fullPath.mid(root->fullPath.length() + 1);

   DirectoryListing listing =
       node->backend->browseDirectory(root->repoName, relPath);

   /* Mark populated unconditionally so a failed call doesn't get
    * re-issued by every rowCount() probe.  refresh() clears both
    * populated and loadFailed, so users can retry by triggering a
    * refresh on the failed branch. */
   node->populated = true;

   if (!listing.ok) {
      node->loadFailed = true;
      node->loadError  = listing.error.isEmpty()
                             ? QStringLiteral("backend error")
                             : listing.error;
      emit const_cast<Dirmodel *>(this)->backendError(node->fullPath,
                                                      node->loadError);
      return;
   }

   node->loadFailed = false;
   node->loadError.clear();

   for (const DirectoryEntry &entry : listing.entries) {
      if (!entry.isDir)
         continue;
      DirNode *child = new DirNode;
      child->name      = entry.name;
      child->fullPath  = root->fullPath + "/" + entry.path;
      child->parent    = node;
      child->populated = false;
      child->row       = node->children.size();
      child->backend   = node->backend;
      child->repoName  = root->repoName;
      node->children.append(child);
   }
}


Dirmodel::~Dirmodel ()
   {
   while (!_item.empty ())
      delete _item.takeFirst ();
   delete _invisibleRoot;
   }

//     inline bool indexValid(const QModelIndex &index) const {
//          return (index.row() >= 0) && (index.column() >= 0) && (index.model() == q_func());
//     }


err_info *Dirmodel::checkOverlap (QString &dirname, QString &user_dirname)
   {
   foreach (const Diritem *item, _item)
      {
      const QString dir = item->dir ();

      if (dir.startsWith (dirname) ||
          dirname.startsWith (dir))
         return err_make (ERRFN, ERR_directories_and_overlap3,
                          qPrintable (dir), qPrintable(dirname),
                          qPrintable (user_dirname));
      }
   return NULL;
   }


void Dirmodel::addToRecent (QModelIndex &index __attribute__((unused)))
{
#if 0 //p
   if (!_recent.contains(index))
      {
      QModelIndex parent = _item[0]->index ();

      beginInsertRows(parent, _recent.size(), _recent.size());
      _recent.append(index);
      endInsertRows();
      }
#endif
}


int Dirmodel::count_files (QString path, int count, int max)
   {
   QDir dir (path);

   dir.setFilter((QDir::Filters)(QDir::Dirs | QDir::Files | QDir::NoSymLinks));

   const QFileInfoList list = dir.entryInfoList();
   if (!list.size ())
      return count;

   for (int i = 0; count < max && i < list.size (); i++)
      {
      QFileInfo fi = list.at (i);
      if (fi.isDir ())
         {
         if (fi.fileName () != "." && fi.fileName () != "..")
            count += count_files (path + "/" + fi.fileName (), count, max);
         }
      else
         count++;
      }
   return count;
   }


QString Dirmodel::countFiles(const QModelIndex &parent, int max)
   {
   int count = count_files (filePath (parent), 0, max);

   if (count == 0)
      return "no files";
   else if (count == 1)
      return "1 file";
   else if (count >= max)
      return QString ("more than %1 files").arg (count);
   return QString ("%1 files").arg (count);
   }


bool Dirmodel::rmdir(const QModelIndex &index)
{
   QString path = filePath(index);
   if (path.isEmpty())
      return false;

   QDir dir;
   return dir.rmdir(path);
}


void Dirmodel::refresh(const QModelIndex &parent)
{
   DirNode *node = nodeFromIndex(parent);
   if (!node)
      return;
   /* A previously-failed populate leaves populated=true with
    * loadFailed=true, so don't gate the refresh on populated alone. */
   if (!node->populated && !node->loadFailed)
      return;

   // beginRemoveRows/endRemoveRows alone is not enough: proxy models
   // (QSortFilterProxyModel) cache the post-remove state and won't
   // re-query the source when we lazily re-populate via rowCount(). Use
   // beginResetModel/endResetModel so the proxy invalidates and the view
   // re-asks for the up-to-date row list.
   beginResetModel();
   qDeleteAll(node->children);
   node->children.clear();
   node->populated  = false;
   node->loadFailed = false;
   node->loadError.clear();
   endResetModel();
}


/** Simon took this from QT as is doesn't work for some reason, and fixed it. Quite unable
to figure out what is wrong - seems to just always return an invalid index */

QModelIndex Dirmodel::mkdir(const QModelIndex &par, const QString &name,
                            Operation *op)
{
    Diritem *item = findItem(par);

    QString path = filePath(par);

    QDir newDir(name);
    QDir dir(path);
    if (newDir.isRelative())
        newDir = QDir(path + QLatin1Char('/') + name);
    QString childName = newDir.dirName(); // Get the singular name of the directory
    newDir.cdUp();

    if (newDir.absolutePath() != dir.absolutePath() || !dir.mkdir(name))
        return QModelIndex(); // nothing happened

    utilSetDirGroup(dir.filePath(name));
    item->refreshCache(path, op);

    // Invalidate this node's children so they get re-scanned
    DirNode *parentNode = nodeFromIndex(par);
    if (parentNode && parentNode->populated) {
       beginResetModel();
       qDeleteAll(parentNode->children);
       parentNode->children.clear();
       parentNode->populated = false;
       endResetModel();
    }

    QModelIndex i = index (path + QLatin1Char('/') + childName);

    return i;
}


err_info *Dirmodel::moveDir (QString src, QString dst)
{
   QString leaf;
   int pos;

   pos = src.lastIndexOf('/');
   if (pos == (int)src.length () - 1)
      // remove trailing /
      src.truncate (src.length () - 1);
   pos = src.lastIndexOf ('/');

   if (dst.right (1) != "/")
      dst.append ("/");

   leaf = src.mid (pos + 1);
   if (!leaf.length ())
      return err_make (ERRFN, ERR_directory_not_found1, src.toLatin1 ().constData());

   dst += leaf;

   QDir dir;

   if (!dir.rename (src, dst))
      return err_make (ERRFN, ERR_could_not_rename_dir1, dst.toLatin1 ().constData());
   return NULL;
}


bool Dirmodel::dropMimeData(const QMimeData *data, Qt::DropAction,
                             int /* row */, int /* column */, const QModelIndex &parent)
{
    bool success = true;
    QString to = filePath(parent) + QDir::separator();
    QModelIndex _parent = parent;
    err_info *err;

    QList<QUrl> urls = data->urls();
    QList<QUrl>::const_iterator it = urls.constBegin();

    if (urls.size ()) {
      bool ok;

      ok = QMessageBox::question(
         0,
         tr("Confirmation -- maxview"),
         tr("Do you want to move %n directory(s)?", "", urls.size ()),
         QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok;
      if (!ok)
         return false;

      for (; it != urls.constEnd(); ++it) {
         QString path = (*it).toLocalFile();
         err = moveDir (path, to);
         Mainwidget::singleton()->complain(err);
         if (!err) {
               _parent = index(to);
         } else {
               success = false;
         }
      }
    } else {
      QString path = filePath (parent);

      emit droppedOnFolder (data, path);
    }

    if (success)
       _parent = index(to);
    return success;
}


Qt::DropActions Dirmodel::supportedDropActions () const
   {
   return Qt::MoveAction | Qt::CopyAction;
   }


bool Dirmodel::addDir(QString& dir, bool ignore_error)
   {
   Diritem *item = new Diritem ();

   bool ok = item->setDir(dir);
   if (ok || ignore_error)
      {
      /* Each Diritem owns a LocalBackend bound to its single root
       * path; the repo name (the directory's basename) is the key
       * we use when calling browseDirectory.  A future addRemoteDir()
       * would hand the item a RemoteBackend instead. */
      item->setBackend(new LocalBackend(QStringList{item->dir()}));

      DirNode *node = new DirNode;
      node->name = QDir(item->dir()).dirName();
      node->fullPath = item->dir();
      node->parent = _invisibleRoot;
      node->row = _invisibleRoot->children.size();
      node->populated = false;
      node->backend = item->backend();
      node->repoName = node->name;

      item->setNode(node);

      beginInsertRows(QModelIndex (), _item.size(), _item.size());
      _invisibleRoot->children.append(node);
      _item.append (item);
      endInsertRows();
      }
   else
      delete item;
   return ok;
   }


bool Dirmodel::removeDirFromList (const QModelIndex &index)
   {
   if (!isRoot (index))
      return false;
   int itemnum = findIndex (index);

   beginRemoveRows(QModelIndex (), itemnum, itemnum);
   _invisibleRoot->children.removeAt(itemnum);
   _item.removeAt (itemnum);

   // Update row numbers for remaining items
   for (int i = itemnum; i < _invisibleRoot->children.size(); i++)
      _invisibleRoot->children[i]->row = i;

   endRemoveRows();
   return true;
   }


int Dirmodel::columnCount (const QModelIndex &parent) const
   {
    if (parent.column() > 0)
        return 0;
   return 1;
   }


QString Dirmodel::getRecent(int) const
   {
   return "Recent items";
   }


QVariant Dirmodel::data(const QModelIndex &index, int role) const
   {
   if (!index.isValid())
      return QVariant();

   if (index.column() != 0)
      return QVariant ();

   DirNode *node = nodeFromIndex(index);
   if (!node)
      return QVariant();

   // Handle recent items (currently disabled)
   if (isRoot(index)) {
      int i = findIndex(index);
      if (i >= 0 && _item[i]->isRecent())
         return getRecent(role);
      }

   switch (role)
      {
      case FilePathRole :
         return QVariant (node->fullPath);

      case Qt::DisplayRole :
      case Qt::EditRole :
      case FileNameRole :
         return QVariant (node->name);

      case Qt::ToolTipRole :
         if (node->loadFailed)
            return QVariant(QString("Could not list contents: %1")
                                .arg(node->loadError));
         return QVariant();
      }

   return QVariant();
   }


void Dirmodel::traceIndex (const QModelIndex &index) const
   {
   QString fname;
   bool first = true;

   printf ("trace: ");
   QModelIndex ind = index;
   do
      {
      if (isRoot (ind))
         fname = ind.row () == -1 ? "" : _item [ind.row ()]->dir ();
      else
         fname = fileName (ind);
      printf ("%s%p:%d:%s", first ? "" : ", ", ind.internalPointer (),
              ind.row(), fname.toLatin1 ().constData());
      ind = parent (ind);
      first = false;
      } while (ind != QModelIndex ());
   printf ("\n");
   }


QString Dirmodel::filePath (const QModelIndex &index) const
   {
   DirNode *node = nodeFromIndex(index);

   return node ? node->fullPath : QString();
   }


QString Dirmodel::fileName (const QModelIndex &index) const
   {
   DirNode *node = nodeFromIndex(index);

   return node ? node->name : QString();
   }


Qt::ItemFlags Dirmodel::flags(const QModelIndex &index) const
   {
   if (!index.isValid())
      return Qt::NoItemFlags;

   return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
      | Qt::ItemIsDropEnabled;
   }


QVariant Dirmodel::headerData(int, Qt::Orientation orientation,
                                int role) const
   {
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return QVariant ("name");

   return QVariant();
   }


QModelIndex Dirmodel::index(int row, int column, const QModelIndex &parent)
             const
   {
   DirNode *parentNode = nodeFromIndex(parent);
   populateNode(parentNode);

   // Handle recent items (currently disabled)
   if (parent.isValid() && isRoot(parent))
      {
      int i = findIndex(parent);
      if (i >= 0 && _item[i]->isRecent())
         {
         if (row >= 0 && row < _recent.size ())
            return _recent [row];
         return QModelIndex();
         }
      }

   if (row < 0 || row >= parentNode->children.size())
      return QModelIndex();

   DirNode *child = parentNode->children[row];
   QModelIndex ind = createIndex(row, column, child);

#ifdef TRACE_INDEX
   QVariant v = data (ind, FileNameRole);
   printf ("   index  '%s' row %d: %p %s\n", data (parent).toString ().latin1 (),
      row, ind.internalPointer (), v.toString ().latin1 ());
   traceIndex (ind);
#endif
   return ind;
   }


bool Dirmodel::hasChildren (const QModelIndex &parent) const
   {
   if (!parent.isValid())
      return _item.size () > 0;

   // All directory nodes potentially have children
   return true;
   }


QModelIndex Dirmodel::findPath (int, Diritem *item, QString path) const
   {
   DirNode *node = item->node();
   QModelIndex ind = createIndex(node->row, 0, node);

   if (path.isEmpty())
      return ind;

   // split the path
   QStringList dirs = path.split ('/');
   for (int i = 0; i < dirs.size (); i++)
      {
      bool found = false;

      int child_count = rowCount (ind);
      for (int j = 0; j < child_count; j++)
         {
         QModelIndex child = index (j, 0, ind);
         if (dirs [i] == fileName (child))
            {
            ind = child;
            found = true;
            break;
            }
         }
      if (!found)
         {
         // The filesystem may have changed; re-scan and try again
         DirNode *parentNode = nodeFromIndex(ind);
         if (parentNode && parentNode->populated)
            {
            qDeleteAll(parentNode->children);
            parentNode->children.clear();
            parentNode->populated = false;

            child_count = rowCount(ind);
            for (int j = 0; j < child_count; j++)
               {
               QModelIndex child = index(j, 0, ind);
               if (dirs[i] == fileName(child))
                  {
                  ind = child;
                  found = true;
                  break;
                  }
               }
            }
         if (!found)
            return QModelIndex ();
         }
      }

   return ind;
   }


QModelIndex Dirmodel::index (const QString &in_path, int) const
   {
   // search all the items for the path which matches
   QString path = in_path;

   if (path.right(1) == "/")
       path.truncate(path.length() - 1);
   for (int i = 0; i < _item.size (); i++)
      {
      QString dir = _item [i]->dir ();

      if (path.startsWith (dir))
         {
         // we found a match, so now we need to search for the model index in this item
         return findPath (i, _item [i], path.mid(dir.length () + 1));
         }
      }
   return QModelIndex ();
   }


int Dirmodel::findIndex(const QModelIndex &index) const
   {
   if (!index.isValid())
      return -1;
   DirNode *node = nodeFromIndex(index);
   if (node && node->parent == _invisibleRoot)
      {
      int r = node->row;
      if (r >= 0 && r < _item.size())
         return r;
      }
   return -1;
   }


int Dirmodel::isRoot (const QModelIndex &index) const
   {
   return findIndex (index) != -1;
   }


QModelIndex Dirmodel::findRoot(const QModelIndex &index) const
   {
   QModelIndex ind = (QModelIndex)index;

   while (ind.parent () != QModelIndex ())
      ind = ind.parent ();
   return ind;
   }

Diritem * Dirmodel::findItem(QModelIndex ind) const
{
    DirNode *node = nodeFromIndex(ind);
    while (node && node->parent != _invisibleRoot)
        node = node->parent;

    if (!node || node->parent != _invisibleRoot)
        return nullptr;

    int seq = node->row;
    Q_ASSERT(seq >= 0 && seq < _item.size());

    return _item[seq];
}


QModelIndex Dirmodel::parent(const QModelIndex &index) const
   {
   if (!index.isValid())
      return QModelIndex();

   DirNode *node = nodeFromIndex(index);
   if (!node || !node->parent || node->parent == _invisibleRoot)
      return QModelIndex();

   return createIndex(node->parent->row, 0, node->parent);
   }


int Dirmodel::rowCount(const QModelIndex &parent) const
   {
   if (parent.column() > 0)
      return 0;

   DirNode *node = nodeFromIndex(parent);

   // Handle recent items (currently disabled)
   if (parent.isValid() && isRoot(parent))
      {
      int item = findIndex(parent);
      if (item >= 0 && _item[item]->isRecent())
         return _recent.size();
      }

   populateNode(node);
   return node->children.size();
   }

TreeItem *Dirmodel::ensureCache(const QModelIndex& root_ind, Operation *op)
{
   if (!isRoot(root_ind))
      return nullptr;

   int index = findIndex(root_ind);

   Diritem *item = _item[index];

   return item->ensureCache(op);
}

void Dirmodel::dropCache(const QModelIndex& root_ind)
{
   Q_ASSERT(isRoot(root_ind));

   int index = findIndex(root_ind);

   Diritem *item = _item[index];

   item->dropCache();;
}

void Dirmodel::buildCache(const QModelIndex& root_ind, Operation *op)
{
   Q_ASSERT(isRoot(root_ind));

   int index = findIndex(root_ind);

   Diritem *item = _item[index];

   item->buildCache(op);
}

QStringList Dirmodel::mimeTypes() const
   {
   QStringList types;
   types << "text/uri-list";
   types << "application/vnd.text.list";
   return types;
   }

Dirproxy::Dirproxy(QObject *parent)
   : QSortFilterProxyModel(parent)
{
   _active = true;
}

Dirproxy::~Dirproxy()
{
}

void Dirproxy::setActive(bool active)
{
   _active = active;
   invalidateFilter();
}

bool Dirproxy::filterAcceptsRow(int source_row,
                                const QModelIndex &source_parent) const
{
   if (!_active)
      return true;

   Dirmodel *src = static_cast<Dirmodel *>(sourceModel());
   QModelIndex ind = src->index(source_row, 0, source_parent);
   QString name = ind.data().toString();

   int mpos, ypos;
   int year = utilDetectYear(name, ypos);
   int mon = utilDetectMonth(name, mpos);

   QDate date = QDate::currentDate();
   bool ok = true;

   if (year && year != date.year())
      ok = false;
   if (mon && mon != date.month())
      ok = false;

   return ok;
}

void Dirmodel::addMatches(QStringList& matches, uint baseLen,
                          const QString &dirPath, const TreeItem *parent,
                          const QString &match)
{
   for (int i = 0; i < parent->childCount(); i++) {
      const TreeItem *child = parent->childConst(i);

      if (!child->isDir())
         continue;
      QString leaf = child->dirName();
      QString fname = dirPath + leaf;
      if (match == QString() || fname.contains(match, Qt::CaseInsensitive)) {
         matches << fname.mid(baseLen);
         addMatches(matches, baseLen, fname + "/", child, QString());
      } else {
         addMatches(matches, baseLen, fname + "/", child, match);
      }
   }
}

QStringList Dirmodel::findFolders(const QString& text, const QString& dirPath,
                                  const QModelIndex& root, QStringList& missing,
                                  Operation *op)
{
   TreeItem *tree = ensureCache(root, op);

   QStringList matches;
   addMatches(matches, dirPath.size() + 1, dirPath + "/", tree, text);

   QDate date = QDate::currentDate();

   return utilDetectMatches(date, matches, missing);
}

const TreeItem *Dirmodel::findDir(const TreeItem *parent, QString path)
{
    return parent->findItem(path);
}

void Dirmodel::addFileMatches(QStringList& matches, const uint baseLen,
                              const QString &dirPath, const TreeItem *parent,
                              const QString& text)
{
   for (int i = 0; i < parent->childCount(); i++) {
      const TreeItem *child = parent->childConst(i);
      const QString& fname = child->dirName();

      if (child->isDir()) {
         addFileMatches(matches, baseLen, dirPath + fname + "/", child, text);
      } else {
         if (fname.contains(text, Qt::CaseInsensitive))
            matches << dirPath.mid(baseLen) + fname;
      }
   }
   if (!parent->childCount())
      matches << dirPath.mid(baseLen);
}

QStringList Dirmodel::findFiles(const QString& text, const QString& dirPath,
                                const QModelIndex& root, Operation *op)
{
   Q_ASSERT(isRoot(root));

   const TreeItem *tree = ensureCache(root, op);
   QString root_path = data(root, Dirmodel::FilePathRole).toString();
   const TreeItem *node;

   // Remove the root-path prefix
   QString rel = dirPath.mid(root_path.size() + 1);

   node = findDir(tree, rel);
   QStringList matches;
   if (node)
      addFileMatches(matches, dirPath.size() + 1, dirPath + "/", node, text);

   return matches;
}

void Dirmodel::refreshCache(const QModelIndex& root_ind, Operation *op)
{
   dropCache(root_ind);
   buildCache(root_ind, op);
}

void Dirmodel::refreshCacheFrom(const QModelIndex& parent, Operation *op)
{
   QString path = filePath(parent);

   Diritem *item = findItem(parent);

   Q_ASSERT(item);
   item->refreshCache(path, op);
}

void Dirmodel::addFileToCache(const QModelIndex &parent,
                              const QString &filename)
{
   QString path = filePath(parent);

   Diritem *item = findItem(parent);
   if (item)
      item->addFileToCache(path, filename);
}
