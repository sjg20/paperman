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

#include "err.h"

#include "mainwindow.h"
#include "utils.h"

//#define TRACE_INDEX


Diritem::Diritem(const QString& path, Dirmodel *model, bool recent)
{
   _model = model;
   _dir = path;
   _recent = recent;
   _dir_cache = 0;
   _qdmodel = new QDirModel(QStringList(), QDir::Dirs | QDir::NoDotAndDotDot,
                             QDir::IgnoreCase);
}


Diritem::~Diritem ()
   {
   }


//void Diritem::setRecent(QModelIndex index)
//{
//   _recent = true;
//   _index = index;
//   _dir = "/_recent";
//}


bool Diritem::setDir(QString& dir)
   {
//   QModelIndex ind;
   QDir qd (dir);

   // Try to get the canonical path, but if not, use the one supplied
   _dir = qd.canonicalPath ();
   if (_dir.isEmpty()) {
      if (dir.endsWith ("/"))
         dir.chop (1);
      _dir = dir;
      }

   _root = _qdmodel->index(_dir);
   if (!_root.isValid())
      return false;

   // _redir = ind;
   // _index = ind;

   return true;
   }

QModelIndex Diritem::index(int row, int column, const QModelIndex &parent)
             const
{
   // This should never be called for top-level items since
   // Dirmodel::createRootIndex() handles that
   Q_ASSERT(parent.isValid());

   return _qdmodel->index(row, column, parent);
}

QVariant Diritem::data(const QModelIndex &index, int role) const
{
   return _qdmodel->data(index, role);
}

int Diritem::rowCount(const QModelIndex &parent) const
{
   return _qdmodel->rowCount(parent);
}

int Diritem::columnCount(const QModelIndex &parent) const
{
   return _qdmodel->columnCount(parent);
}

QModelIndex Diritem::parent(const QModelIndex &index) const
{
   return _qdmodel->parent(index);
}

bool Diritem::hasChildren(const QModelIndex &parent) const
{
   return _qdmodel->hasChildren(parent);
}

void Diritem::refresh(const QModelIndex &parent)
{
   _qdmodel->refresh(parent);
}

QModelIndex Diritem::findPath(QString path)
{
   QModelIndex ind = _qdmodel->index(_dir + "/" + path);
   Q_ASSERT(ind.isValid());
   return ind;
}

QString Diritem::dirCacheFilename() const
{
   return _dir + "/.papertree";
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

Dirmodel::Dirmodel()
{
}

Dirmodel::~Dirmodel ()
{
   while (!_item.empty ())
      delete _item.takeFirst ();
}

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


void Dirmodel::addToRecent (QModelIndex &index)
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

QModelIndex Dirmodel::mkdir(const QModelIndex &par, const QString &name)
{
#if 0//p
//     printf ("row=%d, col=%d", parent.row (), parent.column ());
//     printf ("models %p - %p\n", this, parent.model ());
//     if (!indexValid(parent) || isReadOnly())
//         return QModelIndex();
    QModelIndex parent = par;

    QDir adir (filePath (parent));

    QString path = adir.absoluteFilePath (filePath (parent));
    // For the indexOf() method to work, the new directory has to be a direct child of
    // the parent directory.

    QDir newDir(name);
    QDir dir(path);
    if (newDir.isRelative())
        newDir = QDir(path + QLatin1Char('/') + name);
    QString childName = newDir.dirName(); // Get the singular name of the directory
    newDir.cdUp();

//     printf ("dirs '%s' - '%s', '%s'\n", newDir.absolutePath().latin1 (),
//          dir.absolutePath().latin1 (), name.latin1 ());
    if (newDir.absolutePath() != dir.absolutePath() || !dir.mkdir(name))
        return QModelIndex(); // nothing happened

    utilSetGroup(dir.filePath(name));
    qDebug() << "Dirmodel::mkdir" << parent << isRoot (parent) << parent.isValid ();
    if (isRoot (parent))
       parent = _item [parent.row ()]->index ();
    qDebug() << "Dirmodel::mkdir2" << parent << isRoot (parent) << parent.isValid ();
    parent = index (path);
    if (parent.isValid ())
       refresh (parent);

//     printf ("looking for '%s'\n", childName.latin1 ());
    QModelIndex i = index (path + QLatin1Char('/') + childName);

//     printf ("path = '%s'\n", path.latin1 ());
//     QStringList entryList = adir.entryList();  //(path)
//     for (int i = 0; i < entryList.size (); i++)
//        printf ("   - %s\n", entryList [i].latin1 ());
//     int r = entryList.indexOf(childName);
//     printf ("r=%d\n", r);
//     QModelIndex i = index(r, 0, parent); // return an invalid index

    return i;
#endif
    return QModelIndex ();
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
//    printf ("src='%s', leaf='%s', pos=%d\n", src.latin1 (), leaf.latin1 (), pos);

//    printf ("%s -> %s%s\n", src.latin1 (), dst.latin1 (), leaf.latin1 ());
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
//     if (!d->indexValid(parent) || isReadOnly())
//         return false;
#if 0//p
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

//       printf ("path = %s\n", path.latin1 ());
      emit droppedOnFolder (data, path);
    }

    if (success)
       {
       _parent = index(to);
       if (_parent != QModelIndex())
          refresh (_parent);
//     emit operationComplete (_parent);
       }
    return success;
#endif
   return false;
}


Qt::DropActions Dirmodel::supportedDropActions () const
   {
   return Qt::MoveAction | Qt::CopyAction;
   }


bool Dirmodel::addDir(QString& dir, bool ignore_error)
   {
   Diritem *item = new Diritem(dir, this);

   bool ok = item->setDir(dir);
   if (!ok && !ignore_error) {
      delete item;
      return false;
   }

   beginInsertRows(QModelIndex (), _item.size(), _item.size());
   _item.append (item);
   endInsertRows();

   return true;
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
   if (ind.isValid()) {
      QModelIndex item_ind;
      Diritem *item = lookupItem(ind, item_ind);

      return item->data(item_ind, role);
   }

   return QVariant();
}

QString Dirmodel::filePath (const QModelIndex &index) const
   {
   return data(index, QDirModel::FilePathRole).toString();
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

Diritem *Dirmodel::lookupItem(QModelIndex ind, QModelIndex& item_ind) const
{
   Diritem *item = _map.value(ind).first;

   if (item) {
      Q_ASSERT(item);
      item_ind = _map.value(ind).second;
   } else {
      item = findItem(ind);
      Q_ASSERT(item);
      item_ind = item->rootIndex();
   }

   return item;
}

QModelIndex Dirmodel::index(int row, int column, const QModelIndex &parent)
             const
{
   QModelIndex ind;

   // If this is the top-level item, return an item
   if (parent == QModelIndex()) {
      if (row >= 0 && row < _item.size())
         ind = itemRootIndex(row);
   } else {
      QModelIndex dir_parent;
      Diritem *item = lookupItem(parent, dir_parent);

      QModelIndex dir_ind = item->index(row, column, dir_parent);
      Dirmodel *non_const = (Dirmodel *)this;
      ind = non_const->createIndexFor(dir_ind, item);
   }
\
   return ind;
   }

bool Dirmodel::hasChildren(const QModelIndex &parent) const
{
   if (!parent.isValid())
      return _item.size() > 0;

   QModelIndex dir_parent;
   Diritem *item = lookupItem(parent, dir_parent);

   return item->hasChildren(dir_parent);
}

QModelIndex Dirmodel::findPath(int row, Diritem *item, QString path) const
{
   QModelIndex dir_ind;

   if (path.isEmpty())
      return itemRootIndex(row);

   dir_ind = item->findPath(path);
   Dirmodel *non_const = (Dirmodel *)this;
   QModelIndex ind = non_const->createIndexFor(dir_ind, item);

   return ind;
}

QModelIndex Dirmodel::createRootIndex(QModelIndex item_ind, int row) const
{
   QModelIndex ind = createIndex(row, item_ind.column(),
                                 item_ind.internalPointer());

   return ind;
}

QModelIndex Dirmodel::createIndexFor(QModelIndex item_ind, const Diritem *item,
                                     int row)
{
   Q_ASSERT(item_ind.isValid());
   if (row == -1)
      row = item_ind.row();
   QModelIndex ind = createIndex(row, item_ind.column(),
                                 item_ind.internalPointer());

   _map.insert(ind, QPair((Diritem *)item, item_ind));

   return ind;
}

QModelIndex Dirmodel::index (const QString &in_path, int) const
   {
   QString path = in_path;

   if (path.right(1) == "/")
       path.truncate(path.length() - 1);
   for (int i = 0; i < _item.size (); i++)
      {
      QString dir = _item [i]->dir ();

      if (path.startsWith (dir))
         {
         QModelIndex ind = findPath (i, _item [i], path.mid(dir.length () + 1));

         return ind;
         }
      }
   return QModelIndex ();
   }


int Dirmodel::findIndex(const QModelIndex &index) const
   {
   for (int i = 0; i < _item.size (); i++)
      if (index.internalPointer () == itemRootIndex(i).internalPointer ())
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
   QModelIndex ind = index;

   while (ind.parent () != QModelIndex ())
      ind = ind.parent ();
   return ind;
   }


Diritem * Dirmodel::findItem(QModelIndex index) const
{
   for (int i = 0; i < _item.size (); i++)
      if (index.internalPointer () == itemRootIndex(i).internalPointer())
         return _item[i];

   return nullptr;
}


QModelIndex Dirmodel::parent(const QModelIndex &index) const
{
   QModelIndex par;

   if (index.isValid()) {
      Diritem *item = _map.value(index).first;
      if (!item)
         return QModelIndex();
      QModelIndex item_ind = _map.value(index).second;
      int i = _item.indexOf(item);
      Q_ASSERT(i != -1);

      if (item_ind.internalPointer() != itemRootIndex(i).internalPointer()) {
         QModelIndex item_par = item->parent(item_ind);
         if (item_par.isValid()) {

            // We never need to add par to the map, since index must be in the
            // map and therefore its parent is either in the map itself, or is
            // the top-level directory for this item
            par = createIndex(item_par.row(), item_par.column(),
                              item_par.internalPointer());
         }
      }
   }

   return par;
   }

int Dirmodel::rowCount(const QModelIndex &parent) const
   {
   int count;

   if (!parent.isValid()) {
      count = _item.size();
   } else {
      QModelIndex item_ind;
      Diritem *item = lookupItem(parent, item_ind);

      count = item->rowCount(item_ind);
   }

   return count;
 }

err_info *Dirmodel::rmdir (const QModelIndex &index)
   {
#if 0//p
   if (!QDirModel::rmdir (index))
      {
      QString path = filePath (index);

      int err = err_systemf ("rm -rf '%s'", path.toLatin1 ().constData());

      if (err)
         return err_make (ERRFN, ERR_could_not_remove_dir1,
                          path.toLatin1 ().constData());

      refresh (parent (index));
      }
#endif
   return 0;
   }

void Dirmodel::refresh(const QModelIndex &parent)
{
   QModelIndex dir_parent;
   Diritem *item = lookupItem(parent, dir_parent);

   item->refresh(dir_parent);
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
   //qDebug() << "index" << index;

   Diritem *item = _item[index];

   item->dropCache();;
}

void Dirmodel::buildCache(const QModelIndex& root_ind, Operation *op)
{
   Q_ASSERT(isRoot(root_ind));

   int index = findIndex(root_ind);
   //qDebug() << "index" << index;

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
   //qDebug() << "name" << name << ok;

   return ok;
}

void Dirmodel::addMatches(QStringList& matches, uint baseLen,
                          const QString &dirPath, const TreeItem *parent,
                          const QString &match)
{
   for (int i = 0; i < parent->childCount(); i++) {
      const TreeItem *child = parent->childConst(i);

      if (!child->childCount())
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

   //qDebug() << tree;

   QStringList matches;
   addMatches(matches, dirPath.size() + 1, dirPath + "/", tree, text);

   QDate date = QDate::currentDate();

   return utilDetectMatches(date, matches, missing);
}

const TreeItem *Dirmodel::findDir(const TreeItem *parent, QString path)
{
   QDir dir(path);

   // An empty string has a component of "." so handle that specially
   if (path.size()) {
      QStringList components = dir.path().split("/");
      for (const QString &component : components) {
         const TreeItem *child = parent->child(component);
         if (!child)
            return nullptr;
         parent = child;
      }
   }

   return parent;
}

void Dirmodel::addFileMatches(QStringList& matches, const uint baseLen,
                              const QString &dirPath, const TreeItem *parent,
                              const QString& text)
{
   for (int i = 0; i < parent->childCount(); i++) {
      const TreeItem *child = parent->childConst(i);
      const QString& fname = child->dirName();

      if (child->childCount()) {
         addFileMatches(matches, baseLen, dirPath + fname + "/", child, text);
      } else {

         if (fname.contains(text, Qt::CaseInsensitive))
            matches << dirPath.mid(baseLen) + fname;
      }
   }
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

   //qDebug() << rel;
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
