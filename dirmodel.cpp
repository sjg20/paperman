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

#include <QDate>
#include <QDebug>
#include <QMessageBox>

#include "dirmodel.h"
#include <QDirModel>
#include "qmimedata.h"
#include "qurl.h"

#include "err.h"

#include "mainwindow.h"
#include "maxview.h"
#include "utils.h"

//#define TRACE_INDEX


Diritem::Diritem (QDirModel *model)
   {
   _model = model;
   _recent = false;
   _dir_cache = 0;
   }


Diritem::~Diritem ()
   {
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
   QModelIndex index;
   QDir qd (dir);

   // Try to get the canonical path, but if not, use the one supplied
   _dir = qd.canonicalPath ();
   if (_dir.isEmpty()) {
      if (dir.endsWith ("/"))
         dir.chop (1);
      _dir = dir;
      }

   index = _model->index (_dir);
   bool valid = index != QModelIndex ();
   if (!valid)
      qDebug () << "Diritem invalid";
   return valid;
   }


QModelIndex Diritem::index (void) const
   {
   QModelIndex ind;

   if (_recent)
      ind = _index;
   else
      ind = _model->index (_dir);

   return ind;
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

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
Dirmodel::Dirmodel (QObject * parent)
      : QDirModel (QStringList (), QDir::Dirs | QDir::NoDotAndDotDot,
                   QDir::IgnoreCase, parent)
   {
QT_WARNING_POP
   QStringList filters;

   _root = index ("/");

   // Create the 'recent dirs' item at the top
//   Diritem *item = new Diritem (this);
//   item->setRgecent(createIndex(0, 0, item));
//   _item.append (item);
   _map = new QMap<QModelIndex, QPair<Diritem *, QModelIndex>>;
   }


Dirmodel::~Dirmodel ()
   {
   while (!_item.empty ())
      delete _item.takeFirst ();
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


/** Simon took this from QT as is doesn't work for some reason, and fixed it. Quite unable
to figure out what is wrong - seems to just always return an invalid index */

QModelIndex Dirmodel::mkdir(const QModelIndex &par, const QString &name,
                            Operation *op)
{
    QModelIndex parent = par;
    Diritem *item = findItem(par);

    QDir adir (filePath (parent));

    QString path = adir.absoluteFilePath (filePath (parent));

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

    // qDebug() << "Dirmodel::mkdir" << parent << isRoot (parent) << parent.isValid ();
    if (isRoot (parent))
       parent = _item [parent.row ()]->index ();
    // qDebug() << "Dirmodel::mkdir2" << parent << isRoot (parent) << parent.isValid ();
    parent = index (path);
    if (parent.isValid ())
       refresh (parent);

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
               QModelIndex idx=index(QFileInfo(path).path());
               if(idx.isValid()) {
                  refresh (idx);
                  //the previous call to refresh may invalidate the _parent. so recreate a new QModelIndex
                  _parent = index(to);
               }
         } else {
               success = false;
         }
      }
    } else {
      QString path = filePath (parent);

      emit droppedOnFolder (data, path);
    }

    if (success)
       {
       _parent = index(to);
       if (_parent != QModelIndex())
          refresh (_parent);
       }
    return success;
}


Qt::DropActions Dirmodel::supportedDropActions () const
   {
   return Qt::MoveAction | Qt::CopyAction;
   }


bool Dirmodel::addDir(QString& dir, bool ignore_error)
   {
   Diritem *item = new Diritem (this);

   bool ok = item->setDir(dir);
   if (ok || ignore_error)
      {
      beginInsertRows(QModelIndex (), _item.size(), _item.size());
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
   _item.removeAt (itemnum);
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


QVariant Dirmodel::data(const QModelIndex &ind, int role) const
   {
   QModelIndex index = ind;
   QString name;
   bool recent = false;

   if (!index.isValid())
      return QVariant();

   if (index.column() != 0)
      return QVariant ();

   int i = findIndex (index);
   if (isRoot (index))
      {
      index = _item [i]->index ();
      if (i && index == QModelIndex ())
         {
         /*
          * special case where an invalid / non-existent dir was added. It
          * might be a mount point that has gone away, so keep it around
          * to avoid annoying the user
          */
         name = _item [i]->dir ();
         if (role != FilePathRole)
            {
            QDir dir (name);

            name = dir.dirName ();
            }
         }
      recent = _item [i]->isRecent ();
      }

   switch (role)
      {
      case FilePathRole :
      case Qt::DisplayRole :
      case Qt::EditRole :
      case FileNameRole :
         if (!name.isEmpty ())
            return QVariant (name);

         return recent ? getRecent(role) : QDirModel::data (index, role);
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
   QString path = QDirModel::filePath (index);

   return path;
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

#if 0
Diritem *Dirmodel::lookupItem(QModelIndex ind, QModelIndex& item_ind) const
{
   Diritem *item = _map->value(ind).first;

   if (item) {
      Q_ASSERT(item);
      item_ind = _map->value(ind).second;
   } else {
      item = findItem(ind);
      Q_ASSERT(item);
      item_ind = item->rootIndex();
   }

   return item;
}
#endif

QModelIndex Dirmodel::index(int row, int column, const QModelIndex &parent)
             const
   {
   QModelIndex ind;

   // parent is root node - the children are our special nodes from _item
   if (parent == QModelIndex ())
      {
      if (row >= 0 && row < _item.size ())
         {
         ind = _item [row]->index ();
         ind = createIndex (row, column, ind.internalPointer ());
         }
      }
   // parent is a special node - we just do a redirect
   else if (isRoot (parent))
      {
      int i;

      i = findIndex (parent);
      if (_item [i]->isRecent ())
         {
         if (row >= 0 && row < _recent.size ())
            ind = _recent [row];
         }
      else
         ind = QDirModel::index (row, column, _item [i]->index ());
      }
   else
      ind = QDirModel::index (row, column, parent);
   QVariant v = data (ind, QDirModel::FileNameRole);
#ifdef TRACE_INDEX
   printf ("   index  '%s' row %d: %p %s\n", data (parent).toString ().latin1 (),
      row, ind.internalPointer (), v.toString ().latin1 ());
   traceIndex (ind);
#endif
   return ind;
   }


bool Dirmodel::hasChildren (const QModelIndex &parent) const
   {
   if (isRoot (parent))
      return _item.size () > 0;
   return QDirModel::hasChildren (parent);
   }


QModelIndex Dirmodel::findPath (int row, Diritem *item, QString path) const
   {
   QModelIndex ind = item->index ();
   ind = createIndex (row, 0, ind.internalPointer ());

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
            }
         }
      if (!found)
          return QModelIndex ();
      }

   return ind;
   }

QModelIndex Dirmodel::createRootIndex(QModelIndex item_ind, int row) const
{
   QModelIndex ind = createIndex(row, item_ind.column(),
                                 item_ind.internalPointer());

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
//    return QDirModel::index (path, column);
   }


int Dirmodel::findIndex(const QModelIndex &index) const
   {
   for (int i = 0; i < _item.size (); i++)
      if (index.internalPointer () == _item [i]->index ().internalPointer ())
         return i;
   return -1;
   }


int Dirmodel::isRoot (const QModelIndex &index) const
   {
   return findIndex (index) != -1;
   }

QModelIndex Dirmodel::itemRootIndex(int row) const
{
   return createRootIndex(_item[row]->rootIndex(), row);
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
    while (!isRoot(ind))
        ind = ind.parent();

    int seq = findIndex(ind);
    Q_ASSERT(seq!= -1);

    return _item[seq];
}

#if 0
Diritem * Dirmodel::findItem(QModelIndex index) const
{
   for (int i = 0; i < _item.size (); i++)
      if (index.internalPointer () == itemRootIndex(i).internalPointer())
         return _item[i];

   return nullptr;
}
#endif

QModelIndex Dirmodel::parent(const QModelIndex &index) const
   {
   QModelIndex ind;

   if (index.isValid())
      {
      if (isRoot (index))
         // it is a top level node, then the parent is the root
         ind = QModelIndex ();
      else
         {
         ind = QDirModel::parent (index);
         int i = findIndex (ind);
         if (i != -1)
            {
            ind = _item [i]->index ();
            ind = createIndex (i, 0, ind.internalPointer ());
            }
         }
      }

   return ind;
   }


int Dirmodel::rowCount(const QModelIndex &parent) const
   {
   int count;

   if (parent == QModelIndex ())
      count = _item.size ();
   else if (isRoot (parent))
      {
      int item = findIndex (parent);

      if (_item [item]->isRecent())
         count = _recent.size();
      else
         count = QDirModel::rowCount (_item [item]->index ());
      }
   else
      count = QDirModel::rowCount (parent);

   return count;
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
   QString root_path = data(root, QDirModel::FilePathRole).toString();
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
   QDir dir;
   QString path = dir.absoluteFilePath(filePath(parent));

   Diritem *item = findItem(parent);

   Q_ASSERT(item);
   item->refreshCache(path, op);
}
