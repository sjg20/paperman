/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
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
*/

#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

/* A browser cannot run another program, and Qt's QProcess has nothing in
   it there. This stands in for it, as a program which can never be
   started, so that code which runs one builds as it is and takes the path
   it already has for a program which is not installed.

   Include this in place of <QProcess>. It does nothing on a platform which
   can run programs */
#if !QT_CONFIG(process)

class NoProcess : public QObject
   {
   Q_OBJECT

public:
   enum ProcessError { FailedToStart, Crashed, Timedout, ReadError,
                       WriteError, UnknownError };
   enum ProcessState { NotRunning, Starting, Running };
   enum ExitStatus { NormalExit, CrashExit };

   explicit NoProcess (QObject *parent = nullptr) : QObject (parent) {}

   //! say that it failed to start, once whoever asked is listening
   void start (const QString &, const QStringList & = QStringList ())
      {
      QTimer::singleShot (0, this, [this] {
         emit errorOccurred (FailedToStart);
      });
      }

   static bool startDetached (const QString &,
                              const QStringList & = QStringList ())
      { return false; }

   void setProcessEnvironment (const QProcessEnvironment &) {}
   void setProgram (const QString &) {}
   void setArguments (const QStringList &) {}
   void setProcessChannelMode (int) {}
   bool waitForStarted (int = 30000) { return false; }
   bool waitForFinished (int = 30000) { return false; }
   bool waitForReadyRead (int = 30000) { return false; }
   ProcessState state () const { return NotRunning; }
   ProcessError error () const { return FailedToStart; }
   QString errorString () const
      { return QStringLiteral ("Programs cannot be run here"); }
   qint64 processId () const { return 0; }
   int exitCode () const { return -1; }
   ExitStatus exitStatus () const { return CrashExit; }
   QByteArray readAll () { return QByteArray (); }
   QByteArray readLine () { return QByteArray (); }
   QByteArray readAllStandardOutput () { return QByteArray (); }
   QByteArray readAllStandardError () { return QByteArray (); }
   bool canReadLine () const { return false; }
   qint64 write (const QByteArray &) { return -1; }
   void closeWriteChannel () {}
   void kill () {}
   void terminate () {}

signals:
   void finished (int exitCode, NoProcess::ExitStatus exitStatus);
   void errorOccurred (NoProcess::ProcessError error);
   void readyReadStandardOutput ();
   void readyReadStandardError ();
   };

#define QProcess NoProcess

#endif
