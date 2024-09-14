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


#include <QDirModel>
#include <QSortFilterProxyModel>

class TreeItem;

struct err_info;

/**
 * @brief An item in the list of top-level paper repositories
 *
 * This contains information about a paper repository, including the top-level
 * directory containing it. A list of these is contained in Dirmodel, which
 * handles all the repositories visible to paperman.
 */
class Diritem
   {
public:
   Diritem (QDirModel *model);
   ~Diritem ();

   void setRecent(QModelIndex index);
   bool isRecent(void) { return _recent; }

//    QPersistentModelIndex index (void) const { return _index; }
   QModelIndex index (void) const;
   QString dir (void) const { return _dir; }
//   bool valid (void) { return _valid; }

   /** Sets the directory, returning true if ok.

    \param dir    directory to set
    \returns true if valid, false if directory is invalid */
   bool setDir(QString &dir);

   //!< Read or create a cache
   TreeItem *ensureCache();

private:
   // Get the filename for the dir cache
   QString dirCacheFilename() const;

   // Read any available cache of the directory tree
   bool readCache();

private:
   QString _dir;      //!< the directory
   QDirModel *_model; //!< the directory model
//   QPersistentModelIndex _index;  //!< the index of this directory in the model
   bool _valid;      //!< true if the directory is valid
   bool _recent;     //!< true if this item displays a 'recent' list
   QModelIndex _index;  //!< index of this item, if _recent
   TreeItem *_dir_cache;  //!< Cache of the directory tree, or 0
   };


/** this model is like a QDirModel, but adds the facility to create some
top level 'mounts'. At the top level, only these mounts are present, and
each points to a directory somewhere in the tree. Therefore once in the
tree somewhere it is only possible to rise up to the top level mount for
that position

For example we might have top level directories like this:

/pub/paper
   finance
   marketing
   projects
/pub/starmpaper


So the root parent will indicate that there are two rows, and asking for
either of these will return one of our special indexes (corresponding to
a Diritem). Going below (for example) /pub/paper you will see whatever
is in that directory. In this case that is the three items finance,
marketing and projects. Asking for the parent of one of these three will
return our special Diritem parent for /pub/paper, not the normal
QDirModel node. Asking for the parent again (of /pub/paper) will return
QModelIndex()

*/

class Dirmodel : public QDirModel
   {
   Q_OBJECT
public:
   Dirmodel (QObject * parent = 0);
   ~Dirmodel ();

   bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                             int /* row */, int /* column */, const QModelIndex &parent);

   /** add a new repository directory to the list

      \param ignore_error  add the dir even if it doesn't exist
      \returns true on success, else false */
   bool addDir (QString &dir, bool ignore_error = false);

   /** Remove a repository directory from the list

     \param index    model index of directory to remove */
   bool removeDirFromList (const QModelIndex &index);

   /** make a new directory

      \param parent  parent directory to create it in
      \param name    name of directory to create */
   QModelIndex mkdir(const QModelIndex &parent, const QString &name);

   /** count the number of files in a directory, upto the given maximum. Then
       return a string like '45 files', or 'no files' */
   QString countFiles(const QModelIndex &parent, int max);

   QModelIndex index (const QString & path, int column = 0) const;
   int rowCount (const QModelIndex &parent) const;
   int columnCount (const QModelIndex &parent) const;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
   QModelIndex parent(const QModelIndex &index) const;
   QModelIndex index(int row, int column, const QModelIndex &parent)
               const;
   QVariant headerData(int section, Qt::Orientation orientation,
                                 int role) const;
   Qt::ItemFlags flags(const QModelIndex &index) const;

   /** returns the string for the given role for the 'recent' node */
   QString getRecent(int role) const;

   /** finds the index which corresponds to the 'root' for this index. This
       root index is in our _items list, and has a null parent

       \param index  index whose root is to be found
       \returns the index of the root, or QModelIndex() if not found */
   QModelIndex findRoot(const QModelIndex &index) const;

   /** move a directory from to inside the given destination directory */
   struct err_info *moveDir (QString src, QString dst);

   Qt::DropActions supportedDropActions () const;

   /** finds the given index in our list of indices, if present, and returns the sequence
      number of it

      \returns  sequence number if found, else -1 */
   int findIndex (const QModelIndex &index) const;

   /** checks if the index given is one of our special root indices

     These are the top level repositories which appear in the dir tree.

      \returns true if this index is a root index */
   int isRoot (const QModelIndex &index) const;

   /** given a directory path within an item, this finds the model index for that
      path

      \param item   item to check within
      \param path   path to find (relative to the item's root path)

      \returns  the model index if found, or empty index if not */
   QModelIndex findPath (int i, Diritem *item, QString path) const;

   QString filePath (const QModelIndex &index) const;

   /** displays the filename of this index and all its parents up to the root */
   void traceIndex (const QModelIndex &index) const;

   QStringList mimeTypes() const;

   bool hasChildren (const QModelIndex &parent) const;

   /** add a new index to the recent list

      \param index    index to add */
   void addToRecent (QModelIndex &index);

   /** check that a dirname does not overlap any existing top-level dirnames.
     This means that it must not contain or be contained by any of them

     \param dirname  Dir to check
     \param user_dirname   Dir name as supplied by user (not canonical)
     \returns NULL if ok, else error */
   err_info *checkOverlap (QString &dirname, QString &user_dirname);

   err_info *rmdir (const QModelIndex &index);

   /**
    * @brief Ensure that a cache is available
    * @param root_ind  Indirect of the top-level item the cache is for
    * @return cache pointer
    *
    * This reads a cache file in, if not already done. If there is no cache, one
    * is created and a cache file is written.
    */
   TreeItem *ensureCache(const QModelIndex& root_ind);

private:
   /** counts the number of files in 'path', adds it to count and returns it.
       Stops if count > max

      \param path    path to check
      \param count   initial file count
      \param max     maximum count (to stop at)
      \return number of files found */
   int count_files (QString path, int count, int max);

signals:
   void droppedOnFolder (const QMimeData *data, QString &path);

private:
   QList<Diritem *> _item;   //!< a list of items to display
   QModelIndex _root;   //!< the model index of the root node
   QModelIndexList _recent;   //!< list of recent directories
   };


class Dirproxy : public QSortFilterProxyModel
{
public:
   Dirproxy(QObject *parent = nullptr);
   ~Dirproxy();
   void setActive(bool active);

protected:
   virtual bool filterAcceptsRow(int source_row,
                                 const QModelIndex &source_parent) const;

   // true if the proxy is filtering, false if it is just a pass-through
   bool _active;
};

void dirmodel_tests (void);


