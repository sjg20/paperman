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


#include <QDebug>
#include <QMessageBox>

#include "dirmodel.h"
#include <QDirModel>
#include "qmimedata.h"
#include "qurl.h"

#include "err.h"

#include "maxview.h"


//#define TRACE_INDEX


Diritem::Diritem (QDirModel *model)
   {
   _model = model;
   _recent = false;
   }


Diritem::~Diritem ()
   {
   }


void Diritem::setRecent(QModelIndex index)
{
   _recent = true;
   _index = index;
   _dir = "/_recent";
}


bool Diritem::setDir(QString &dir)
   {
   QModelIndex index;
   QDir qd (dir);

   // Try to get the canonical path, but if not, use the one supplied
   _dir = qd.canonicalPath ();
   if (_dir.isEmpty ())
      {
      if (dir.endsWith ("/"))
         dir.chop (1);
      _dir = dir;
      }

   index = _model->index (_dir);
   bool valid = index != QModelIndex ();
   //    printf ("addDir %s\n", _dir.latin1 ());
   //    _index = QPersistentModelIndex (_model->index (_dir));
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


Dirmodel::Dirmodel (QObject * parent)
      : QDirModel (QStringList (), QDir::Dirs | QDir::NoDotAndDotDot,
                   QDir::IgnoreCase, parent)
   {
   QStringList filters;

   _root = index ("/");

   // Create the 'recent dirs' item at the top
   Diritem *item = new Diritem (this);
   item->setRecent(createIndex(0, 0, item));
   _item.append (item);
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
   if (!_recent.contains(index))
      {
      QModelIndex parent = _item[0]->index ();

      beginInsertRows(parent, _recent.size(), _recent.size());
      _recent.append(index);
      endInsertRows();
      }
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

    qDebug() << "Dirmodel::mkdir" << parent << isRoot (parent) << parent.isValid ();
    if (isRoot (parent))
       parent = _item [parent.row ()]->index ();
    qDebug() << "Dirmodel::mkdir2" << parent << isRoot (parent) << parent.isValid ();
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
//     return QModelIndex ();
}


err_info *Dirmodel::moveDir (QString src, QString dst)
{
   QString leaf;
   int pos;

   pos = src.findRev ('/');
   if (pos == (int)src.length () - 1)
      // remove trailing /
      src.truncate (src.length () - 1);
   pos = src.findRev ('/');

   if (dst.right (1) != "/")
      dst.append ("/");

   leaf = src.mid (pos + 1);
//    printf ("src='%s', leaf='%s', pos=%d\n", src.latin1 (), leaf.latin1 (), pos);

//    printf ("%s -> %s%s\n", src.latin1 (), dst.latin1 (), leaf.latin1 ());
   if (!leaf.length ())
      return err_make (ERRFN, ERR_directory_not_found1, src.latin1 ());

   dst += leaf;

   QDir dir;

   if (!dir.rename (src, dst))
      return err_make (ERRFN, ERR_could_not_rename_dir1, dst.latin1 ());
   return NULL;
}


bool Dirmodel::dropMimeData(const QMimeData *data, Qt::DropAction,
                             int /* row */, int /* column */, const QModelIndex &parent)
{
//     if (!d->indexValid(parent) || isReadOnly())
//         return false;

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
         err_complain (err);
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
}


Qt::DropActions Dirmodel::supportedDropActions () const
   {
   return Qt::MoveAction | Qt::CopyAction;
   }


bool Dirmodel::addDir (QString &dir, bool ignore_error)
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
   QModelIndex index = ind;
   QString name;
   bool recent = false;

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
      printf ("%s%p:%d:%s", first ? "" : ", ", ind.internalPointer (), ind.row(), fname.latin1 ());
      ind = parent (ind);
      first = false;
      } while (ind != QModelIndex ());
   printf ("\n");
   }


QString Dirmodel::filePath (const QModelIndex &index) const
   {
//    qDebug () << "Dirmodel::filePath";
//    int i = findIndex (index);

   QString path = QDirModel::filePath (index);
//    printf ("filePath() %d, %s\n", i, path.latin1 ());
//    traceIndex (index);
   return path;
   }


Qt::ItemFlags Dirmodel::flags(const QModelIndex &index) const
   {
// printf ("flags\n");
   if (!index.isValid())
      return 0;

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

//    printf ("findPath '%s'\n", path.latin1 ());

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
   QModelIndex ind = (QModelIndex)index;

   while (ind.parent () != QModelIndex ())
      ind = ind.parent ();
   return ind;
   }


QModelIndex Dirmodel::parent(const QModelIndex &index) const
   {
   QModelIndex ind;

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
   return ind;
   }


int Dirmodel::rowCount(const QModelIndex &parent) const
   {
   int count;

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
   return count;
 }

err_info *Dirmodel::rmdir (const QModelIndex &index)
   {
   if (!QDirModel::rmdir (index))
      {
      QString path = filePath (index);

      int err = err_systemf ("rm -rf '%s'", path.latin1 ());

      if (err)
         return err_make (ERRFN, ERR_could_not_remove_dir1, path.latin1());

      refresh (parent (index));
      }

   return 0;
   }

void dirmodel_tests (void)
   {
   QString dir = "desktopl";
   QString dir2 = "test";
   Dirmodel model;
   QModelIndex parent, ind;

   model.addDir (dir);
   model.addDir (dir2);
   parent = QModelIndex ();
   printf ("%s\n", model.data (parent, Qt::DisplayRole).toString ().latin1 ());
   int rows = model.rowCount (parent);
   for (int i = 0; i < rows; i++)
      {
      ind = model.index (i, 0, parent);
      printf ("   %s %p\n", model.data (ind, Qt::DisplayRole).toString ().latin1 (),
         ind.internalPointer ());
      if (model.parent (ind) != QModelIndex ())
         {
         printf ("   - root parent bad\n");
         return;
         }
      if (ind.row () != i)
         qDebug () << "row is" << ind.row () << "should be" << i;
      int rows2 = model.rowCount (ind);
      for (int j = 0; j < rows2; j++)
         {
         QModelIndex ind2 = model.index (j, 0, ind);
         printf ("       %s %p\n", model.data (ind2, Qt::DisplayRole).toString ().latin1 (),
            ind2.internalPointer ());
         if (model.parent (ind2) != ind)
            {
            qDebug () << "   - parent bad, is" << model.parent (ind2) << "should be" << ind;
            return;
            }
         if (ind2.row () != j)
            qDebug () << "row is" << ind2.row () << "should be" << j;
         }
      }
   }


QStringList Dirmodel::mimeTypes() const
   {
   QStringList types;
   types << "text/uri-list";
   types << "application/vnd.text.list";
   return types;
   }
