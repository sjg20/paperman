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
#include <QMap>
#include <QUndoCommand>
#include <QModelIndexList>
#include <QStringList>

#include "activitylog.h"
#include "docfacts.h"
#include "err.h"

#include <memory>


class Desktopmodel;
class Pagecover;



/** this is our version of an undo stack. It logs each command as it is
done, undone and redone, with --log, so that a log shows the steps which
led up to a problem */

class Desktopundostack : public QUndoStack
   {
public:
   Desktopundostack (QObject *parent = 0);
   ~Desktopundostack ();

   /** do a command and put it on the stack, as QUndoStack::push() does,
       saying so in the log

      \param cmd  the command, which the stack then owns */
   void push (QUndoCommand *cmd);

   /** start and end a run of commands which are undone as one, as
       QUndoStack does. In the record of activity they are a group, named
       by @p text */
   void beginMacro (const QString &text);
   void endMacro ();

   //! the model whose commands these are, which says which repository a
   //! folder is in, for the record of activity
   void setModel (Desktopmodel *model) { _model = model; }

   //! say who the commands pushed from now on are done by
   void setWho (Activity::e_who who) { _who = who; }
   Activity::e_who who () const { return _who; }

private:
   /** say which commands have been undone or redone, once the stack has
       moved to a new place

      \param index  the new place, the number of commands now done */
   void noteIndex (int index);

   //! what the record of activity has for a command on the stack
   struct Done
      {
      qint64 id = 0;    //!< its entry, or its group's; 0 if it has none
      QString root;     //!< the repository whose record that is in
      QString text;     //!< what the entry says
      };

   /** put what a command did in the record of activity

      \param acts  what it did, with full paths, as activity() gives
      \param text  the command's own text, for a group of several */
   void record (QList<Activity> acts, const QString &text);

   //! note what the record has for the command just done, at the top
   void noteDone ();

   //! record that a command was undone, or done again
   void recordAgain (int index, Activity::e_action action);

   int _index;          //!< the place the log last saw the stack at
   bool _pushing;       //!< a command is being pushed, which push() logs
   Desktopmodel *_model = nullptr;
   Activity::e_who _who = Activity::W_user;
   int _macro_depth = 0;      //!< how many macros are open
   QString _macro_text;       //!< the text of the outermost
   Done _current;             //!< what is recorded for the command under way
   QVector<Done> _done;       //!< for each command on the stack, by place
   };


/** says who the commands done while this exists are done by, in the record
    of activity; whoever it was before is put back when it goes */
class ActivityBy
   {
public:
   ActivityBy (Desktopundostack *stack, Activity::e_who who)
         : _stack (stack), _was (stack->who ())
      { _stack->setWho (who); }
   ~ActivityBy () { _stack->setWho (_was); }

private:
   Desktopundostack *_stack;
   Activity::e_who _was;
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

   /** Run @p op on the stacks in the desk for @p dir, after any queued
       before it if they are on a server, complaining of any error it
       ends with.  It must find what it acts on when it runs, by name,
       since what came before may have changed it */
   void queue (const QString &dir,
               std::function<void (Desktopmodel::OpDone done)> op);

   /** queue deleting the stacks named in @p names, in the desk for
       @p dir, as an undo which made them does */
   void deleteMade (const QString &dir, std::shared_ptr<QStringList> names);

   /** record that we are about to add items. We record the number of items
       currently in the desk so that we know later how many were added.
   
      \param parent     parent (desk) which will receive the new items */
   void aboutToAdd (QModelIndex parent);

   /** what this command did, for the record of activity: an entry for
       each stack it changed, with the full path of each. This is asked
       once the command has been done, so that the names of stacks it made
       are known. A command which changes no papers gives none */
   virtual QList<Activity> activity () const { return QList<Activity> (); }

protected:
   /** make an entry for activity()

      \param action  what was done
      \param dir     the directory the stack is in
      \param name    the stack's filename */
   static Activity entry (Activity::e_action action, const QString &dir,
                          const QString &name);

   //! the full path of a stack, from its directory and filename
   static QString fullPath (const QString &dir, const QString &name);

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
   QList<Activity> activity () const override;
   /** the names the stacks got in the trash, once moved, which for a
       local desk is at once */
   const QStringList &trashlist () const { return *_trashlist; }
private:
   QString _dir;           //!< desk directory
   //! the list of filenames to delete, updated as they come back
   std::shared_ptr<QStringList> _filenames;
   //! the resulting filenames in the trash, once moved
   std::shared_ptr<QStringList> _trashlist;
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
   /**
      \param nudge   true if this is one of a run of small moves made with
                     the keys, which are undone together */
   UCMove (Desktopmodel *model, QModelIndexList &list, QModelIndex &parent,
         QList<QPoint> &oldplist, QList<QPoint> &plist, bool nudge = false);
   void redo();
   void undo();

   //! nudges are told apart from other commands by this, to be joined up
   int id () const override { return _nudge ? nudge_id : -1; }

   /** join a nudge of the same stacks to the one before it, so that a run
       of them, as from holding a key down, is one step to undo */
   bool mergeWith (const QUndoCommand *other) override;

private:
   static constexpr int nudge_id = 1;

   bool _nudge;            //!< one of a run of small moves with the keys
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
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of filenames to move
   //! the list of duplicated filenames, once made
   std::shared_ptr<QStringList> _newnames;
   File::e_type _type;   // the type of duplication to perform
   int _odd_even;          //!< bit 0 means duplicate even pages, bit 1 means odd
   };


/** add copies of some files to a desk, as stacks. We record the files'
paths and the names of the stacks made. To redo we copy the files in again;
to undo we remove the stacks. This mirrors UCDuplicate

The stacks made can be put into a stack which is there already. For that
we record its name and, as UCStackStacks does, the page they go in at and
what each of them was, so that undo can take them out of it again before
removing them */

class UCAddFiles : public Desktopundocmd
   {
public:
   UCAddFiles (Desktopmodel *model, const QStringList &paths,
               QModelIndex &parent, QPoint pos, const QModelIndex &dest,
               int pagenum = -1);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QStringList _paths;     //!< the files to add
   QPoint _pos;            //!< where the first goes on the desk
   QString _destname;      //!< the stack they go into; empty if none
   int _count;             //!< how many pages the files come to
   //! the names of the stacks, once made
   std::shared_ptr<QStringList> _newnames;
   //! page number in the stack they go in before, once stacked
   std::shared_ptr<int> _pagenum;
   //! what each stack made was, before it went into the other
   std::shared_ptr<QList<pagepos_info> > _pagepos;
   };


/** unfold a group of saddle-stitched booklet stacks into new stacks with the
pages split and put into reading order. We record the original filenames and
the new filenames. To redo we create the unfolded stacks; to undo we remove
them. This mirrors UCDuplicate */

class UCUnfoldBooklet : public Desktopundocmd
   {
public:
   UCUnfoldBooklet (Desktopmodel *model, QModelIndexList &list,
      QModelIndex &parent);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of booklet filenames to unfold
   //! the list of unfolded filenames, once made
   std::shared_ptr<QStringList> _newnames;
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
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QStringList _filenames; //!< the list of filenames to unstack
   //! the new stacks' names for each stack, once unstacked
   std::shared_ptr<QList<QStringList> > _newnames;
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
   QList<Activity> activity () const override;
private:
   QString _dir;        //!< desk directory
   QString _filename;   //!< the filename of the original stack
   //! the filename of the unstacked page, once unstacked
   std::shared_ptr<QString> _newname;
   //! page number to unstack
   std::shared_ptr<int> _pagenum;
   bool _remove;        //!< true if the page was removed from the original stack
   };


/** divide a stack into several, taking each run of pages out into a new
stack and leaving the pages before the first run where they are.

To redo this, we take each run of pages out, noting the name each new stack
is given.

To undo, we stack them back, in order, at the page the first run started at */

class UCSplitStack : public Desktopundocmd
   {
public:
   /** \param firsts  the first page of each new stack, from 0, in order
       \param names   the name to ask for each, without an extension
       \param facts   facts to keep with each, where it can keep them */
   UCSplitStack (Desktopmodel *model, QModelIndex &ind,
                 const QList<int> &firsts, const QStringList &names,
                 const QList<Docfacts> &facts = {});
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QString _filename;      //!< the filename of the stack divided
   int _first;             //!< the first page taken out
   QStringList _names;     //!< the name to ask for each new stack
   QList<Docfacts> _facts; //!< the facts to keep with each new stack
   QList<pagepos_info> _pagepos;   //!< how many pages each new stack has
   //! the filename of each new stack, once made
   std::shared_ptr<QStringList> _newnames;
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
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QString _filename;   //!< the filename of the original stack
   QString _newname;    //!< the filename of the stack where the unstacked pages end up
   QBitArray _pages;    //!< list of pages to delete, one bit for each (true = delete)
   //! information about deleted pages, once deleted
   std::shared_ptr<QByteArray> _del_info;
   //! number of pages deleted, once deleted
   std::shared_ptr<int> _count;
   };


/** put a stack's pages in a new order. Undo puts them back, with the
reverse order */

class UCReorderPages : public Desktopundocmd
   {
public:
   UCReorderPages (Desktopmodel *model, QModelIndex &ind,
                   const QVector<int> &order);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   //! apply @p order to the stack
   void reorder (const QVector<int> &order);

   QString _dir;           //!< desk directory
   QString _filename;      //!< the filename of the stack
   QVector<int> _order;    //!< the old number of each page, in its new place
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
provide an earlier possition (drag & drop has this problem)

A stack takes only pages of its own type. Where the destination is one
which takes others' pages (see Desktopmodel::canStackOnto()), a stack of
another type is converted to its type first: a copy of that type is made
and stacked in its place, and the stack itself goes to the trash. Undo
removes the copy, once it is unstacked, and brings the stack back */

class UCStackStacks : public Desktopundocmd
   {
public:
   UCStackStacks (Desktopmodel *model, QModelIndex &dest,
      QModelIndexList &list, int pagenum, QList<QPoint> *poslist);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   QString _dir;           //!< desk directory
   QString _destdir;           //!< dest desk directory
   QString _destname;   //!< the filename of the destination stack
   QString _srcdir;           //!< source desk directory
   QStringList _filenames; //!< the list of filenames to stack
   //! page number in dest to stack before, once stacked
   std::shared_ptr<int> _pagenum;
   QList<pagepos_info> _pagepos; //!< file information for each file
   //! the names of the stacks which went in: _filenames, or their copies
   std::shared_ptr<QStringList> _stacked;
   //! the stacks which were converted, the copy made of each, and the
   //! name each has in the trash
   std::shared_ptr<QStringList> _origs, _copies, _trashed;
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
   QList<Activity> activity () const override;
private:
   //! rename the stack @p from to @p to
   void rename (const QString &from, const QString &to);

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
   QList<Activity> activity () const override;
private:
   //! give the page the name @p name
   void rename (const QString &name);

   QString _dir;           //!< desk directory
   QString _fname;     //!< filename of stack to rename
   QString _newname;   //!< the new name
   QString _oldname;   //!< the old name
   int _pagenum;       //!< the current page number
   };


/** transform (rotate or mirror) a page of a stack */

/** keep facts about a stack's document in its file. We record the facts it
had and those it is given. To redo we write the new ones, and to undo the
old ones. The stack is found by its name, which a rename before or after
this one in the same step may have changed */

class UCStackFacts : public Desktopundocmd
   {
public:
   /**
      \param model    model to work with
      \param ind      model index of stack
      \param facts    the facts to keep about it */
   UCStackFacts (Desktopmodel *model, const QModelIndex &ind,
                 const Docfacts &facts);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   //! write @p facts to the stack
   void write (const Docfacts &facts);

   QString _dir;        //!< desk directory
   QString _filename;   //!< the stack
   Docfacts _old;       //!< the facts it had
   Docfacts _new;       //!< the facts it is given
   };


class UCTransformPage : public Desktopundocmd
   {
public:
   /** create an undo record for transforming a page, and execute it

      \param model    model to work with
      \param ind      model index of stack
      \param pagenum  page to transform
      \param op       transform to apply */
   UCTransformPage (Desktopmodel *model, const QModelIndex &ind, int pagenum,
         File::e_transform op);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   //! apply @p op to the page
   void transform (File::e_transform op);

   QString _dir;       //!< desk directory
   QString _fname;     //!< filename of stack
   int _pagenum;       //!< the page number to transform
   File::e_transform _op;   //!< the transform to apply
   };


/** crop a page of a stack, or every page. Undo puts back what was there */

class UCCropPage : public Desktopundocmd
   {
public:
   /** create an undo record for cropping, and execute it

      \param model    model to work with
      \param ind      model index of stack
      \param pagenum  page to crop, or -1 for every page
      \param area     as for Desktopmodel::cropPage() */
   UCCropPage (Desktopmodel *model, const QModelIndex &ind, int pagenum,
         const QRectF &area);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   QString _dir;       //!< desk directory
   QString _fname;     //!< filename of stack
   int _pagenum;       //!< the page number to crop
   QRectF _area;       //!< the part to keep
   //! what is needed to put the pages back, once cropped
   std::shared_ptr<QByteArray> _undo;
   };


/** make a page the stack's cover, or stop it being one. Undo puts back
the properties of each page changed */

class UCSetCover : public Desktopundocmd
   {
public:
   /** create an undo record for setting the cover, and execute it

      \param model    model to work with
      \param ind      model index of stack
      \param pagenum  the page, from 0
      \param cover    as for Desktopmodel::setCover() */
   UCSetCover (Desktopmodel *model, const QModelIndex &ind, int pagenum,
         const Pagecover &cover);
   void redo();
   void undo();
   QList<Activity> activity () const override;
private:
   QString _dir;       //!< desk directory
   QString _fname;     //!< filename of stack
   int _pagenum;       //!< the page to make the cover
   QString _cover;     //!< the cover's text, or empty to clear it
   //! the properties of each page changed, as they were before
   std::shared_ptr<QMap<int, QMap<QString, QString>>> _old;
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
   QList<Activity> activity () const override;
private:
   //! set the annotations to @p annots
   void update (const QHash<int, QString> &annots);

   QString _dir;           //!< desk directory
   QString _fname;               //!< filename of stack to adjust
   QHash<int, QString> _old;      //!< old details
   QHash<int, QString> _new;      //!< new details
   };


/** this adds a new repository to the list

Undoing this removes the repository */

class UCAddRepository : public Desktopundocmd
   {
public:
   UCAddRepository (Desktopmodel *model, QString dirPath);
   void redo();
   void undo();
private:
   QString _dirpath;   //!< the new directory
   };


/** this removes a new repository from the list

Undoing this adds the repository back */

class UCRemoveRepository : public Desktopundocmd
   {
public:
   UCRemoveRepository (Desktopmodel *model, QString dirPath);
   void redo();
   void undo();
private:
   QString _dirpath;   //!< the directory to remove
   };
