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


bool Diritem::setDir(QString& dir, int row)
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

   QModelIndex ind = _qdmodel->index(_dir);
   _index = _model->createIndexFor(ind, this, row);
   qDebug() << "model" << _model << _index;
   _redir = ind;
   _parent = ind;
   bool valid = _index != QModelIndex ();
   //    printf ("addDir %s\n", _dir.latin1 ());
   //    _index = QPersistentModelIndex (_model->index (_dir));
   if (!valid)
      qDebug () << "Diritem invalid";
   return valid;
   }

QModelIndex Diritem::index(int row, int column, const QModelIndex &parent)
             const
{
   Q_ASSERT(parent.isValid());
      //return createIndex(row, column, (void *)this);
//      return _index;

   QModelIndex ind;
   ind = _qdmodel->index(row, column, _redir);
//   if (parent.internalPointer() == _index.internalPointer())
//      ind = _qdmodel->index(row, column, _redir);
//   else
//      ind = _qdmodel->index(row, column, parent);
   return _model->createIndexFor(ind, this);
//   return ind;
}

QVariant Diritem::data(const QModelIndex &index, int role) const
{
   if (index.internalPointer() == this) {
      if (role == QDirModel::FilePathRole)
         return _dir;

      QDir dir (_dir);

      return dir.dirName();
   }
   return _qdmodel->data(index, role);
}

QModelIndex Diritem::index(void) const
   {
   return _index;
   }

int Diritem::rowCount(const QModelIndex &parent) const
{
//   qDebug() << "rowCount" << parent;

   // If the parent is
//   if (parent.internalPointer() == _index.internalPointer())
//      return _qdmodel->rowCount(_redir);

   return _qdmodel->rowCount(parent);
}

int Diritem::columnCount(const QModelIndex &parent) const
{
//   qDebug() << "columnCount" << parent;

   // If the parent is
//   if (parent.internalPointer() == _index.internalPointer())
//      return _qdmodel->columnCount(_redir);

   return _qdmodel->columnCount(parent);
}

QModelIndex Diritem::parent(const QModelIndex &index) const
{
   qDebug() << "parent" << index;
   if (index.internalPointer() == _index.internalPointer())
      return QModelIndex();
   QModelIndex ind = _qdmodel->parent(index);

   if (ind == _parent)
      return _index;

   return ind;
}

QModelIndex Diritem::findPath(int row, QString path)
{
   return _qdmodel->index(path);
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

Dirmodel::Dirmodel (QObject *parent)
   {
   QStringList filters;

//   _root = index ("/");

   //   QDirModel (QStringList (), QDir::Dirs | QDir::NoDotAndDotDot,
   //                      QDir::IgnoreCase, parent)

   // Create the 'recent dirs' item at the top
//   Diritem *item = new Diritem (this);
//   item->setRgecent(createIndex(0, 0, item));
//   _item.append (item);
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

   bool ok = item->setDir(dir, _item.size());
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
// printf ("columnCount\n");
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
      Diritem *item = _map.value(ind).first;
//   int i = findIndex(ind);

   // If this is the item itself, return the data
//   if (i != -1) {
      Q_ASSERT(item);
      QModelIndex item_ind = _map.value(ind).second;
      return item->data(item_ind, role);
   }
//   else {
//      Diritem *item = findItem(ind);
//      Diritem *item = _map[ind];

//      if (item)
//         return item->data(ind, role);
//   }


//   Diritem *item = findItem(ind);

//   if (item)
//      return item->data(ind, role);
#if 0
   QModelIndex index = ind;
   QString name;
   bool recent = false;

   QModelIndex root = findRoot(ind);

   if (!index.isValid())
      {
//       printf ("   data invalid %p\n", index.internalPointer ());
      return QVariant();
      }

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
         if (role != QDirModel::FilePathRole)
            {
            QDir dir (name);

            name = dir.dirName ();
            }
         }
      recent = _item [i]->isRecent ();
      }

   switch (role)
      {
      case QDirModel::FilePathRole :
      case Qt::DisplayRole :
      case Qt::EditRole :
   case QDirModel::FileNameRole :
         if (!name.isEmpty ())
            return QVariant (name);

         return recent ? getRecent(role) : QDirModel::data (index, role);
      }
#endif

   return QVariant();
}


void Dirmodel::traceIndex (const QModelIndex &index) const
   {
#if 0//p
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
#endif
   }


QString Dirmodel::filePath (const QModelIndex &index) const
   {
//    qDebug () << "Dirmodel::filePath";
//    int i = findIndex (index);
   QString path;
   Diritem *item = findItem(index);

//   if (item)
//      path = item->filePath(index);

//   QString path = QDirModel::filePath (index);
//    printf ("filePath() %d, %s\n", i, path.latin1 ());
//    traceIndex (index);
   return path;
   }


Qt::ItemFlags Dirmodel::flags(const QModelIndex &index) const
   {
// printf ("flags\n");
   if (!index.isValid())
      return Qt::NoItemFlags;

   return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
      | Qt::ItemIsDropEnabled;
   }


QVariant Dirmodel::headerData(int, Qt::Orientation orientation,
                                int role) const
   {
// printf ("headerData\n");
   if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
      return QVariant ("name");

   return QVariant();
   }


QModelIndex Dirmodel::index(int row, int column, const QModelIndex &parent)
             const
{
   QModelIndex ind;

   // If this is the top-level item, return an item
   if (parent == QModelIndex()) {
      if (row >= 0 && row < _item.size())
         ind = _item[row]->index();
   } else {
      Diritem *item = _map.value(parent).first;

      Q_ASSERT(item);
      QModelIndex item_ind = item->index(row, column, parent);
      ind = createIndex(row, column, item_ind.internalPointer());
   }

//   Diritem *item = findItem(parent);

//   if (item)
//      ind = item->index(row, column, parent);
//   else if (row >= 0 && row < _item.size())
//      ind = _item[row]->index();

#if 0
   // parent is root node - the children are our special nodes from _item
   if (parent == QModelIndex ()) {
      if (row >= 0 && row < _item.size ())
         ind = item->index(row, column, QModelIndex());
   } else {
      // parent is in one of the Diritem objects
      ind = item->index(row, column, parent);
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
#endif
//   QVariant v = data (ind, QDirModel::FileNameRole);
#ifdef TRACE_INDEX
   printf ("   index  '%s' row %d: %p %s\n", data (parent).toString ().latin1 (),
      row, ind.internalPointer (), v.toString ().latin1 ());
   traceIndex (ind);
#endif
   return ind;
   }

#if 0
bool Dirmodel::hasChildren (const QModelIndex &parent) const
{
   Diritem *item = findItem(parent);

   if (!item)
      return _item.size() > 0;
   return item->hasChildren(parent);
}
#endif

QModelIndex Dirmodel::findPath (int row, Diritem *item, QString path) const
{
   QModelIndex ind;

   ind = item->findPath(row, path);
#if 0
   QModelIndex ind = item->index ();
   ind = createIndex (row, 0, ind.internalPointer ());

//    printf ("findPath '%s'\n", path.latin1 ());

   if (path.isEmpty())
      return ind;

   // split the path
   QStringList dirs = path.split ('/');
   for (int i = 0; i < dirs.size (); i++)
      {
      bool found = false;

      int child_count = rowCount (ind);
//       printf ("   %d children\n", child_count);
      for (int j = 0; j < child_count; j++)
         {
         QModelIndex child = index (j, 0, ind);
         if (dirs [i] == fileName (child))
            {
            ind = child;
            found = true;
            }
//             printf ("   - found '%s' at %d\n", dirs [i].latin1 (), j);
         }
      if (!found)
          return QModelIndex ();
//      ind = QDirModel::index (path);
      }
//    printf ("ind = %p, path %s\n", ind.internalPointer (), filePath (ind).latin1 ());
#endif
   return ind;
}

QModelIndex Dirmodel::createIndexFor(QModelIndex item_ind, const Diritem *item,
                                     int row)
{
   if (row == -1)
      row = item_ind.row();
   QModelIndex ind = createIndex(row, item_ind.column(),
                                 item_ind.internalPointer());

   _map.insert(ind, QPair((Diritem *)item, item_ind));

   return ind;
}

QModelIndex Dirmodel::index (const QString &in_path, int) const
   {
// printf ("my index %s\n", path.latin1 ());
//    QModelIndex index = QDirModel::index (path, column);
//    _root = QPersistentModelIndex (index.parent ());
//    return _root;
//    printf ("index of path '%s'\n", path.latin1 ());

   // search all the items for the path which matches
   QString path = in_path;

   if (path.right(1) == "/")
       path.truncate(path.length() - 1);
   for (int i = 0; i < _item.size (); i++)
      {
      QString dir = _item [i]->dir ();

//       printf ("%s %s\n", path.left (dir.length ()).latin1 (), dir.latin1 ());
      if (path.startsWith (dir))
         {
         // we found a match, so now we need to search for the model index in this item
//          printf ("found item\n");
         return findPath (i, _item [i], path.mid(dir.length () + 1));
//         return _item [i]->index ();
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


QModelIndex Dirmodel::findRoot(const QModelIndex &index) const
   {
   QModelIndex ind = index;

   while (ind.parent () != QModelIndex ())
      ind = ind.parent ();
   return ind;
   }


Diritem * Dirmodel::findItem(QModelIndex index) const
{
   if (!index.isValid())
      return nullptr;

   int i = findIndex(index);
   if (i != -1) {
      Diritem *item = static_cast<Diritem *>((void *)index.internalPointer());
      return item;
   }

   while (index.isValid()) {
      int i = findIndex(index);

      if (i != -1)
         return _item[i];

      index = index.parent();
   };

   return nullptr;
}


QModelIndex Dirmodel::parent(const QModelIndex &index) const
{
   QModelIndex par;

   if (index.isValid()) {
      Diritem *item = _map.value(index).first;
      Q_ASSERT(item);
      QModelIndex item_ind = _map.value(index).second;

      par = item->parent(item_ind);
   }

//   if (!index.isValid())
//      return QModelIndex();

//   // If it is the top level of the Diritem, return QModelIndex() as the parent
//   int i = findIndex(index);

//   if (i != -1)
//      return QModelIndex();

//   Diritem *item = _map[index].first;
//   if (item)
//      return item->parent(index);
   // If its parent is a Diritem, return that
//   i = findIndex(index);

//      if (i != -1)
//         return _item[i];

//      index = index.parent();

//   Diritem *item = static_cast<Diritem *>((void *)index.model());

//   if (index.internalPointer() == item->index().internalPointer())
//      return QModelIndex();

//   return item->parent(index);
//      return _item[i]->index(i, 0, QModelIndex());

//   return index.model()->parent(index);
   return par;
   }

#if 0
//    printf ("[parent: ");
   if (index.isValid())
      {
      if (isRoot (index))
         // it is a top level node, then the parent is the root
         ind = QModelIndex ();
      else
         {
         ind = QDirModel::parent (index);
//          qDebug () << ind;
         int i = findIndex (ind);
         if (i != -1)
            {
            ind = _item [i]->index ();
            ind = createIndex (i, 0, ind.internalPointer ());
            }
         }
      }
//    printf ("%p %s = %p %s] ",
//          index.internalPointer (), data (index).toString ().latin1 (),
//          ind.internalPointer (), data (ind).toString ().latin1 ());
#endif


int Dirmodel::rowCount(const QModelIndex &parent) const
   {
   int count = 0;

   if (!parent.isValid()) {
      count = _item.size();
   } else {
      Diritem *item = _map.value(parent).first;
      Q_ASSERT(item);
      QModelIndex item_ind = _map.value(parent).second;

      return item->rowCount(item_ind);
   }

//   Diritem *item = findItem(parent);
//   if (item)
//      count = item->rowCount(parent);
//   else if (parent == QModelIndex())
//      count = _item.size();
#if 0
   if (parent == QModelIndex ()) //(!parent.parent ().isValid ())
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
//    QVariant v = data (parent, QDirModel::FileNameRole);
//    printf ("   row count %p %s, %d\n", parent.internalPointer (), v.toString ().latin1 (), count);
#endif
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
}

TreeItem *Dirmodel::ensureCache(const QModelIndex& root_ind, Operation *op)
{
   if (!isRoot(root_ind))
      return nullptr;

   int index = findIndex(root_ind);
   qDebug() << "index" << index;

   //Diritem *item = static_cast<Diritem *>(root_ind.internalPointer());
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

   qDebug() << rel;
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
