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
// C++ Implementation: userlist
//
// Description:
//
//
// Author: Simon Glass <sglass@bluewatersys.com>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include <unistd.h>
#include <limits.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "desktopmodel.h"
#include "transfer.h"
#include "utils.h"


#ifndef DELIVER

Transfer::Transfer (Desktopmodel *contents, QPersistentModelIndex pparent,
         QList<QPersistentModelIndex> &pslist)
   {
   QString version;
   _contents = contents;
   _parent = pparent;
   _slist = pslist;
//    _parent_row = parent_row;
//    _rows = rows;
   _me = 0;
   _dir = _contents->deskRootDir (pparent) + ".transfer/";
   QDir dir (_dir);

   if (!dir.exists ())
      dir.mkdir (_dir);
   }

#endif


Transfer::Transfer (QString root)
   {
   _contents = 0;
   _me = 0;
   _dir = root + ".transfer/";
   }


err_info *Transfer::init (void)
   {
   // load the user list
   QString fname = _dir + "config";
   QFile file (fname);

   if (!file.exists (fname))
      createTransferFile (fname);

   // if there is a maxdesk.ini file, rename it to the official name
   QTextStream stream (&file);

   int linenum = 1;
   QString version = "1";
   if (!file.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (fname));

   // find my user
   CALL (util_getUsername (_username));

   while (!stream.atEnd())
      {
      QString line = stream.readLine().trimmed (); // line of text excluding '\n'

      if (line.isEmpty () || line [0] == '#')
         continue;
      int pos = line.indexOf (":");

      if (pos == -1)
         continue;
      QString tag = line.left (pos);
      QString value = line.mid (pos + 1).trimmed ();
      if (tag == "version")
         version = value;
      else if (tag == "user")
         {
         User user;

         if (!user.fromLine (value))
            return err_make (ERRFN, ERR_transfer_file_corrupt_on_line2,
                  qPrintable (fname), linenum);
         _users << user;
   //       qDebug () << _username << user.email ();
         if (_username == user.username ())
            _me = &_users.last ();
         }
      else if (tag == "domain")
         _domain = value;
      else if (tag == "area")
         _area = value;
      else if (tag == "target_area")
         {
         Target target;

         if (!target.fromLine (value))
            return err_make (ERRFN, ERR_transfer_file_corrupt_on_line2,
                  qPrintable (fname), linenum);
         _targets << target;
         }

      linenum++;
      }

   if (!_me)
      return err_make (ERRFN, ERR_failed_to_find_own_username2,
            qPrintable (_username), qPrintable (fname));
   _userdir = _dir + _me->username () + "/";
   QDir dir (_userdir);

   if (!dir.exists ())
      dir.mkdir (_userdir);

   // make sure we have inbox and outbox
   dir.mkdir (_userdir + "inbox");
   dir.mkdir (_userdir + "outbox");

   return NULL;
   }


const User *Transfer::me ()
   {
   return _me;
   }


Transfer::~Transfer ()
   {
   }


err_info *Transfer::createTransferFile (QString fname)
   {
   QFile file (fname);

   if (!file.open (QIODevice::WriteOnly))
      return err_make (ERRFN, ERR_cannot_open_file1, qPrintable (fname));

   QTextStream stream (&file);
   stream << "version 1" << '\n';
   stream << '\n';
   stream << "[users]" << '\n';
   stream << "# enter one line per user" << '\n';
   stream << "# name,username,email" << '\n';
   stream << "# for example:" << '\n';
   stream << "# Fred Bloggs, fredbloggs,fredbloggs@kiwi.co.nz" << '\n';
   stream << '\n';
   return NULL;
   }


err_info *Transfer::send (QStringList &env)
   {
#ifdef DELIVER
   return err_make (ERRFN, ERR_not_supported_in_delivermv);
#else
   QModelIndexList list;
   QModelIndex parent = _parent;

   foreach (const QPersistentModelIndex ind, _slist)
      if (!ind.isValid ())
         return err_make (ERRFN, ERR_lost_access_to_file1,
            qPrintable (_contents->data (ind, Desktopmodel::Role_filename).toString ()));
      else
         list << ind;
   QString dir = _userdir + "outbox/";

   QStringList fnamelist;
   _contents->moveToDir (list, parent, dir, fnamelist, true);

//    qDebug () << env;

   // update each file to add the envelope
   foreach (const QString fname, fnamelist)
      {
      File::e_type type = File::typeFromName (fname);

      // create the envelope file
      QString envname = fname + ".env";

      QFile file (dir + envname);
      QTextStream stream (&file);
      QString line;

      if (!file.open (QIODevice::WriteOnly))
         return err_make (ERRFN, ERR_cannot_write_to_envelope_file2, qPrintable (envname),
                     qPrintable (QString ("error %1").arg ((int)file.error ())));

      // write header
      stream << "# Maxview envelope file" << '\n';
      stream << '\n';
      stream << "version:1" << '\n';
      stream << "filename:" << fname << '\n';
      for (int i = 0; i < File::Env_count; i++)
         stream << File::envToName ((File::e_env)i) << ":" << env [i].replace ("\n", "\\n") << '\n';
      file.close ();

      // we can only add info to max files at the moment; silently ignore the rest
      if (type != File::Type_max)
         continue;
      File *f = File::createFile (dir, fname, 0, type);
      CALL (f->load ());
      CALL (f->putEnvelope (env));
      CALL (f->flush ());
      delete f;
      }

   // make a note that there is outgoing material

   // later when the user says 'send now' it will be moved into the transfer area

   return NULL;
#endif
   }


err_info *Transfer::scanOutboxes (void)
   {
   foreach (const User &user, _users)
      {
      QString dirname = _dir + user.username () + "/outbox/";
      QDir dir (dirname);
      QStringList filters;

      filters << "*.env";
      foreach (const QString &fname, dir.entryList (filters, QDir::Files))
         {
         Tfile tfile;

         CALL (tfile.scanEnvelope (dirname, fname));
         _outlist << tfile;
         }
      }
   return NULL;
   }


User::User ()
   {
   }


bool User::fromLine (QString line)
   {
   QStringList sl = line.split (',');

   if (sl.size () < 5)
      return false;
   _domain = sl [0].trimmed ();
   _area = sl [1].trimmed ();
   _name = sl [2].trimmed ();
   _username = sl [3].trimmed ();
   _email = sl [4].trimmed ();
   return true;
   }


Target::Target ()
   {
   }


bool Target::fromLine (QString line)
   {
   QStringList sl = line.split (',');

   if (sl.size () < 3)
      return false;
   _domain = sl [0].trimmed ();
   _area = sl [1].trimmed ();
   _host = sl [2].trimmed ();
   return true;
   }


Tfile::Tfile ()
   {
   }


err_info *Tfile::scanEnvelope (const QString &dir, const QString &fname)
   {
   QString path = dir + fname;
   QFile file (path);
   QTextStream stream (&file);
   QString line;
   int version = 1;

   if (!file.open (QIODevice::ReadOnly))
      return err_make (ERRFN, ERR_cannot_read_from_envelope_file2, qPrintable (path),
                  qPrintable (QString ("error %1").arg ((int)file.error ())));
   while (!stream.atEnd())
      {
      QString line = stream.readLine().trimmed (); // line of text excluding '\n'
      User user;

      if (line.isEmpty () || line [0] == '#')
         continue;
      int pos = line.indexOf (":");

      if (pos == -1)
         continue;
      QString tag = line.left (pos);
      QString value = line.mid (pos + 1).trimmed ();

      if (tag == "version")
         {
         version = value.toInt ();
         UNUSED (version);
         continue;
         }
      else if (tag == "from")
         _from = value;
      else if (tag == "filename")
         _filename = value;
      else if (tag == "to")
         _to = value.split (",");
      else if (tag == "subject")
         _subject = value;
      else if (tag == "notes")
         _notes = value.replace ("\\n", "\n");
      }
   return NULL;
   }


const QString Tfile::toList (void) const
   {
   return _to.join ( ", ");
   }

