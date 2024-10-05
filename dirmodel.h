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

class Dirmodel;
class Operation;
class TreeItem;

struct err_info;

/**
 * @brief An item in the list of top-level paper repositories
 *
 * This contains information about a paper repository, including the top-level
 * directory containing it. A list of these is contained in Dirmodel, which
 * handles all the repositories visible to paperman.
 */
class Diritem : public QAbstractItemModel
   {
public:
   Diritem(const QString& path, Dirmodel *model, bool recent=false);
   ~Diritem();

//   void setRecent(QModelIndex index);
   bool isRecent(void) { return _recent; }

//    QPersistentModelIndex index (void) const { return _index; }
   QModelIndex index(void) const;
   QString dir (void) const { return _dir; }
//   bool valid (void) { return _valid; }

   /**
    * @brief Sets the directory, returning true if ok
    * @param dir   directory to set, updated to canonical path
    * @return true if valid, false if directory is invalid
    */
   bool setDir(QString& dir, int row);

   //!< Read or create a cache
   TreeItem *ensureCache(Operation *op);

   //!< Build a cache
   TreeItem *buildCache(Operation *op);

   // Drop the cache and free memory
   void dropCache();

   // index is within _qdmodel
   // returns parent within _qdmodel
   QModelIndex index(int row, int column, const QModelIndex &parent) const
      override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const
      override;

   // Returns index within _qdmodel
   QModelIndex findPath(int row, QString path);

   int rowCount(const QModelIndex &parent) const override;
   int columnCount(const QModelIndex &parent) const override;

   // index is within _qdmodel
   // returns parent within _qdmodel
   QModelIndex parent(const QModelIndex &index) const override;

private:
   // Get the filename for the dir cache
   QString dirCacheFilename() const;

   // Read any available cache of the directory tree
   bool readCache();

private:
   QString _dir;      //!< the directory
   QDirModel *_qdmodel;
//   QDirModel *_model; //!< the directory model
//   QPersistentModelIndex _index;  //!< the index of this directory in the model
   bool _valid;      //!< true if the directory is valid
   bool _recent;     //!< true if this item displays a 'recent' list
   QModelIndex _index;  //!< index of this item
   QModelIndex _redir;  //!< index in of this item in QDirModel
//   QModelIndex _parent;  //!< parent (of the dir in QDirModel)
   TreeItem *_dir_cache;  //!< Cache of the directory tree, or 0
   Dirmodel *_model;
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

class Dirmodel : public QAbstractItemModel
   {
   Q_OBJECT

   friend class TestDirmodel;

public:
   Dirmodel (QObject * parent = 0);
   ~Dirmodel ();

   bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                     int row, int column, const QModelIndex &parent) override;
   /**
    * @brief add a new repository directory to the list
    * @param dir           directory add (updated to canonical path)
    * @param ignore_error  add the dir even if it doesn't exist
    * @return true on success, else false
    */
   bool addDir(QString& dir, bool ignore_error = false);

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

   QModelIndex createIndexFor(QModelIndex item_ind, const Diritem *item,
                              int row = -1);

   QModelIndex index(const QString & path, int column = 0) const;
   int rowCount(const QModelIndex &parent) const override;
   int columnCount(const QModelIndex &parent) const override;
   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const
      override;
   QModelIndex parent(const QModelIndex &index) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent)
      const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role)
      const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;

   /** returns the string for the given role for the 'recent' node */
   QString getRecent(int role) const;

   /** finds the index which corresponds to the 'root' for this index. This
       root index is in our _items list, and has a null parent

       \param index  index whose root is to be found
       \returns the index of the root, or QModelIndex() if not found */
   QModelIndex findRoot(const QModelIndex &index) const;

   Diritem * findItem(QModelIndex index) const;

   /** move a directory from to inside the given destination directory */
   struct err_info *moveDir (QString src, QString dst);

   Qt::DropActions supportedDropActions () const override;

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
   QModelIndex findPath(int i, Diritem *item, QString path) const;

   QString filePath (const QModelIndex &index) const;

   /** displays the filename of this index and all its parents up to the root */
   void traceIndex (const QModelIndex &index) const;

   QStringList mimeTypes() const override;

   //bool hasChildren(const QModelIndex &parent) const override;

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
    * @param op        Operation to update
    * @return cache pointer
    *
    * This reads a cache file in, if not already done. If there is no cache, one
    * is created and a cache file is written.
    */
   TreeItem *ensureCache(const QModelIndex& root_ind, Operation *op);

   /**
    * @brief  Find folders in the current repo which match a text string
    * @param  text    Text to match
    * @param  dirPath Full path of the directory to search
    * @param  root    Index of a top-level repository to search
    * @param  missing Suggestions for directories to create
    * @param  op      Operation to update
    * @return list of matching paths
    *
    * This looks for 4-digit years and 3-character months to try to guess
    * which folders to put at the top of the list
    */
   QStringList findFolders(const QString& text, const QString& dirPath,
                           const QModelIndex& root, QStringList& missing,
                           Operation *op);

   /**
    * @brief Find files matching a substring
    * @param text     Text to search for
    * @param dirPath  Full path to search, without trailing /
    * @param root     Index of the repository
    * @param op       Operation to updates
    * @return List of matches, as paths relative to the root directory
    */
   QStringList findFiles(const QString& text, const QString& dirPath,
                         const QModelIndex& root, Operation *op);

   //! Refresh the cache for a given repository
   void refreshCache(const QModelIndex& root_ind, Operation *op);

   void refresh(const QModelIndex &parent = QModelIndex());

private:
   /** counts the number of files in 'path', adds it to count and returns it.
       Stops if count > max

      \param path    path to check
      \param count   initial file count
      \param max     maximum count (to stop at)
      \return number of files found */
   int count_files (QString path, int count, int max);

   /**
    * @brief Add folder-name matches to a list
    * @param matches  List to update
    * @param baseLen  String length of the base directory path
    * @param dirPath  Full directory patch to search
    * @param parent   Cache node for dirPath
    * @param match    Search string to use
    *
    * Any matches found are added to matches - this function is recursive
    */
   void addMatches(QStringList& matches, const uint baseLen,
                   const QString &dirPath, const TreeItem *parent,
                   const QString &match);

   /**
    * @brief Add filename matches to a list
    * @param matches  List to update
    * @param baseLen  String length of the base directory path
    * @param dirPath  Full directory patch to search
    * @param parent   Cache node for dirPath
    * @param match    Search string to use
    *
    * Any matches found are added to matches as paths relative to dirPath
    * This function is recursive
    */
   void addFileMatches(QStringList& matches, uint baseLen,
                       const QString &dirPath, const TreeItem *parent,
                       const QString& text);

   /**
    * @brief Find the a directory path in a tree
    * @param parent  Root of tree
    * @param path   Path to search for
    * @return Node representing the path
    *
    * Given a path like "a/b/c" and a tree, this finds the assocated node for
    * the path. In this case it would expect a child "a" of parent, then a
    * grandchild "b", then a great grandchild "c", returning that node
    */
   const TreeItem *findDir(const TreeItem *parent, QString path);

   // Drop the cache for a repository
   void dropCache(const QModelIndex& root_ind);

   // Build a new cache
   void buildCache(const QModelIndex& root_ind, Operation *op);

signals:
   void droppedOnFolder (const QMimeData *data, QString &path);

private:
   QList<Diritem *> _item;   //!< a list of items to display
//   QModelIndex _root;   //!< the model index of the root node
   QModelIndexList _recent;   //!< list of recent directories

   /**
    * @brief Maps a Dirmodel index to its associated Diritem and QDirModel index
    */
   QMap<QModelIndex, QPair<Diritem *, QModelIndex>> _map;
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


