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
//
// C++ Interface: userlist
//
// Description:
//
//
// Author: Simon Glass <sglass@bluewatersys.com>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include <QPersistentModelIndex>
#include <QList>
#include <QString>
#include <QStringList>

#include "err.h"

class Desktopmodel;


/** a user of the transfer system */

class User
   {
public:
   User (QString name, QString email);
   User ();
   bool fromLine (QString line);

   const QString &email () const { return _email; }
   const QString &name () const { return _name; }
   const QString &username () const { return _username; }

private:
   QString _domain;     //!< user's domain
   QString _area;       //!< user's area
   QString _name;       //!< full name of user, e.g. Fred Bloggs
   QString _username;   //!< username, e.g. fbloggs
   QString _email;      //!< email address, e.g. fbloggs@company.com
   };


/** a target server */

class Target
   {
public:
   Target (QString name, QString email);
   Target ();
   bool fromLine (QString line);

private:
   QString _domain;     //!< user's domain
   QString _area;       //!< user's area
   QString _host;       //!< host name
   };



/** a file that needs to be transferred */

class Tfile
   {
public:
   Tfile ();

   err_info *scanEnvelope (const QString &dir, const QString &fname);

   const QString &from (void) const { return _from; }
   const QStringList &to (void) const { return _to; }
   const QString toList (void) const;
   const QString &subject (void) const { return _subject; }
   const QString &notes (void) const { return _notes; }

private:
   QString _filename;
   QString _from;
   QStringList _to;
   QString _subject;
   QString _notes;
   };


class Transfer
   {
public:
   /** set up a transfer system for a given desk. We need to find the .transfer
       directory, and we do this by finding the desk and asking for its root
       directory.

      \param model   model containing desk
      \param parent_row row number of the desk we are connected to */
//    Transfer (Desktopmodel *model, int parent_row, QList<int> &rows);
   Transfer (Desktopmodel *contents, QPersistentModelIndex pparent,
         QList<QPersistentModelIndex> &pslist);

   /** set up a transfer system for the delivery agent */
   Transfer (QString root);

   ~Transfer ();

   /** create a new blank transfer file in the given filename */
   err_info *createTransferFile (QString fname);

   /** send the files according to the envelope provided

        \param env  envelope strings */
   err_info *send (QStringList &env);

   err_info *init (void);        //!< init the transfer syste,
   int userCount () { return _users.size (); }  //!< returns number of users
   User &user (int i) { return _users [i]; }    //!< returns info on a user
   const User *me ();

   /** scan all the users' outboxes for files to transfer */
   err_info *scanOutboxes (void);

   QList<Tfile> &outlist (void) { return _outlist; }
   int outlistSize (void) { return _outlist.size (); }

private:
   Desktopmodel *_contents;      //!< model containing the desks
   QPersistentModelIndex _parent;      //!< parent desk
   QList<QPersistentModelIndex> _slist;     //!< list of items to move
   QModelIndex parent;
   int _parent_row;           //!< desk number within model containing rot
   QList<int> _rows;          //!< rows to send from parent (each is a stack)
   QList<User> _users;        //!< list of users we know about
   QList<Target> _targets;    //!< list of targets we know about
   QString _dir;              //!< our transfer directory
   const User *_me;           //!< my user
   QString _username;         //!< my username (as reported by OS)
   QString _userdir;          //!< user's transfer directory
   QList<Tfile> _outlist;     //!< list of outgoing files
   QString _domain;           //!< paper domain (e.g. company name)
   QString _area;             //!< paper area (within domain)
   };
