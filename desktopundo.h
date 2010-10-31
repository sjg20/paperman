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

#include <QBitArray>
#include <QHash>
#include <QUndoCommand>
#include <QModelIndexList>
#include <QStringList>

#include "err.h"


class Desktopmodel;



/** this is our version of an undo stack. We use this subclass so we can add
new features to it later*/

class Desktopundostack : public QUndoStack
   {
public:
   Desktopundostack (QObject *parent = 0);
   ~Desktopundostack ();
   };


/** this is an undo command. All undo commands are subclasses of this. We
provide error handling and a way of remembering the sequence number of
every undo command */

class Desktopundocmd : public QUndoCommand
   {
public:
   Desktopundocmd (Desktopmodel *model);
   ~Desktopundocmd ();

   /** check if there has been an error, and complain to the user if so

      \param err     error to complain about (NULL if all ok) */
   void complain (err_info *err);

   /** record that we are about to add items. We record the number of items
       currently in the desk so that we know later how many were added.
   
      \param parent     parent (desk) which will receive the new items */
   void aboutToAdd (QModelIndex parent);

protected:
   /** sequence number of this undo command. This is allocated globally and
       is unique across all models which use undo/redo */
   int _seq;
   Desktopmodel *_model;   //!< the model in use
   };


/** delete a group of stacks. We record the filenames of the deleted items
since the model indexes may change. We move them into the trash (or another
directory if provided) and record the filenames obtained there also.
Finally we record the file information so we can restore page number
and position */

class UCTrashStacks : public Desktopundocmd
   {
public:
   UCTrashStacks (Desktopmodel *model,
         QModelIndexList &list, QModelIndex &parent, QString destdir, bool copy);
   void redo();
   void undo();
   const QStringList &trashlist () const { return _trashlist; }
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of filenames to delete
   QStringList _trashlist; //!< the resulting filenames in the trash
   QList<pagepos_info> _files; //!< file information for each file
   QString _destdir;       //!< destination directory ("" for trash)
   bool _copy;             //!< true to copy, else move
   };


/** move a group of stacks. We record the filenames of the deleted items
since the model indexes may change. We also record the original position
and the new position to support undo / redo */

class UCMove : public Desktopundocmd
   {
public:
   UCMove (Desktopmodel *model, QModelIndexList &list, QModelIndex &parent,
         QList<QPoint> &oldplist, QList<QPoint> &plist);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of filenames to move
   QList<QPoint> _oldpos;  //!< old positions
   QList<QPoint> _newpos;  //!< new positions
   };


/** duplicate a group of stacks. We record the original filenames and the
duplicate filenames. To redo we create the duplicates, while ensuring
that the duplicate filesnames are unit. To undo, we simply remove
the duplicates */

class UCDuplicate : public Desktopundocmd
   {
public:
   UCDuplicate (Desktopmodel *model, QModelIndexList &list, QModelIndex &parent,
      File::e_type type, int odd_even = 3);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of filenames to move
   QStringList _newnames;  //!< the list of duplicated filenames
   File::e_type _type;   // the type of duplication to perform
   int _odd_even;          //!< bit 0 means duplicate even pages, bit 1 means odd
   };


/** this unstacks a group of stacks completely, so that each stack becomes
a set of single page stacks.

To redo this, we record (for each stack) a list of filenames for each
of the unstacked pages. In other words, a single filename becomes a list
of filenames.

Note that one page always remains in the original stack - i.e. we do
not need to create a new stack for that page.

To undo, we restack all the filenames for each stack into the original
stack, deleting them afterwards */

class UCUnstackStacks : public Desktopundocmd
   {
public:
   UCUnstackStacks (Desktopmodel *model, QModelIndexList &list, QModelIndex &parent);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of filenames to unstack
   QList<QStringList> _newnames;  //!< the list of duplicated filenames for each stack
   };


/** this unstacks a single page from a stack, optionally removing it from
the original stack.

To redo this, we perform the stack.

To undo, we either insert the page back into the stack and delete its own
stack, or (if _remove is false) just delete its own stack. */

class UCUnstackPage : public Desktopundocmd
   {
public:
   UCUnstackPage (Desktopmodel *model, QModelIndex &ind,
      int pagenum, bool remove);
   void redo();
   void undo();
private:
   QString _dir;        //!< desk directory
   QString _filename;   //!< the filename of the original stack
   QString _newname;    //!< the filename of the unstacked page
   int _pagenum;        //!< page number to unstack
   bool _remove;        //!< true if the page was removed from the original stack
   };


/** this deletes a set of pages from a stack, removing then from
the original stack. We do this by just marking them as deleted, so that undo
is easy.

To redo this, we perform the delete.

To undo, we unmark the pages in the stack so that they reappear
 */

class UCDeletePages : public Desktopundocmd
   {
public:
   UCDeletePages (Desktopmodel *model, QModelIndex &ind, QBitArray &pages);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QString _filename;   //!< the filename of the original stack
   QString _newname;    //!< the filename of the stack where the unstacked pages end up
   QBitArray _pages;    //!< list of pages to delete, one bit for each (true = delete)
   QByteArray _del_info;   //!< information about deleted pages
   int _count;          //!< number of pages to delete
   };


/** stack one or more stacks into a destination stack, inserting them at
a given page number. This records the destination filename and the filenames
of all the stacked items, so that we can unstack them. It also records the
destination page number, which gives us the starting page number for
unstacking during an undo. Finally, we record the file information for each
stacked item, which gives us the number of pages in each. That allows us to
know how many pages to put into each unstacked stack during an undo.

It is also possible to pass in the list of positions for each item. This is
used when the current positions have been updated already and we want to
provide an earlier possition (drag & drop has this problem) */

class UCStackStacks : public Desktopundocmd
   {
public:
   UCStackStacks (Desktopmodel *model, QModelIndex &dest,
      QModelIndexList &list, int pagenum, QList<QPoint> *poslist);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QString _destdir;           //!< dest desk directory
   QString _destname;   //!< the filename of the destination stack
   QString _srcdir;           //!< source desk directory
   QStringList _filenames; //!< the list of filenames to stack
   int _pagenum;        //!< page number in dest to stack before
   QList<pagepos_info> _pagepos; //!< file information for each file
   };


/** this changes directory. This is a bit of an odd thing to undo, but
our Desktopmodel does not cope with multiple directories. Maybe this needs
to change but for the moment that is the way it is.

Changing directories resets the Desktopmodel since there is now a whole
new set of items to remember. The items in the previous model are forgotten
and a maxdesk.ini file is written out.

The effect of having this undo command is that you can move something to
another directory, change there, move it around and stack it, and then
undo the whole sequence back to the original directory if you like.

Redoing this changes the path and root to the new directory.

Undoing this changes it back */

class UCChangeDir : public Desktopundocmd
   {
public:
   UCChangeDir (Desktopmodel *model, QString dirPath, QString rootPath,
         QString oldPath, QString oldRoot);
   void redo();
   void undo();
private:
   QString _newpath;   //!< the new directory
   QString _oldpath;   //!< the old directory
   QString _newroot;   //!< the root path for the new directory
   QString _oldroot;   //!< the root path for the old directory
   };


/** rename a stack. To redo this we change the stack name in the model
and rename on the filesystem. To undo we change the name back in both
places */

class UCRenameStack : public Desktopundocmd
   {
public:
   /** create an undo record for renaming a stack, and execute it

      \param model    model to work with
      \param ind      model index of stack
      \param newname  new filename (including .max extension) */
   UCRenameStack (Desktopmodel *model, const QModelIndex &ind, QString newname);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QString _newname;   //!< the new name
   QString _oldname;   //!< the old name
   };


/** rename a page. To redo this we rename in the maxdesk layer. To undo we
change the name back */

class UCRenamePage : public Desktopundocmd
   {
public:
   /** create an undo record for renaming a stack, and execute it

      \param model    model to work with
      \param ind      model index of stack
      \param newname  new filename (including .max extension) */
   UCRenamePage (Desktopmodel *model, const QModelIndex &ind, QString newname);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QString _fname;     //!< filename of stack to rename
   QString _newname;   //!< the new name
   QString _oldname;   //!< the old name
   int _pagenum;       //!< the current page number
   };


/** update stack annotations. To redo this we write the updatein the
maxdesk layer. To undo we write the old details */

class UCUpdateAnnot : public Desktopundocmd
   {
public:
   /** create an undo record for renaming a stack, and execute it

      \param model    model to work with
      \param ind      model index of stack
      \param updates  annotation strings to update, indexed by MAXA_... */
   UCUpdateAnnot (Desktopmodel *model, const QModelIndex &ind, QHash<int, QString> &updates);
   void redo();
   void undo();
private:
   QString _dir;           //!< desk directory
   QString _fname;               //!< filename of stack to adjust
   QHash<int, QString> _old;      //!< old details
   QHash<int, QString> _new;      //!< new details
   };
