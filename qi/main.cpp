/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Don Jul 13 20:27:22 CEST 2000
    copyright            : (C) 2000 by Michael Herder
    email                : crapsite@gmx.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License version 2        *
 *   as published by the Free Software Foundation.                         *
 *                                                                         *
 ***************************************************************************/

#include "resource.h"
#include "qimageioext.h"
#include "qscannersetupdlg.h"
#include "quiteinsanenamespace.h"
#include "splashwidget.h"
#include "qxmlconfig.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

#ifdef KDEAPP
#include <kaboutdata.h>
#include <kapp.h>
#include <kcmdlineargs.h>
#include <kglobal.h>
#else
#include <qapplication.h>
#endif

#include <qstring.h>
#include <qdir.h>
#include <qlabel.h>
#include <qfile.h>
#include <qmessagebox.h>
#include <qnamespace.h>
#include <qobject.h>
#include <qprinter.h>
#include <qstring.h>
#include <qtranslator.h>
#include <qtextcodec.h>

#ifdef KDEAPP
static KCmdLineOptions options[] =
{
   {"local_dir ","Search translations in dirname",""},
   {"debug","Display debug output",0},
   {0,0,0}
};
#endif

void qisMessageHandler(QtMsgType type, const char *msg )
{
  //simply ignore all messages
}

int main(int argc, char *argv[])
{
  QString local_dir;
  local_dir = QString::null;
  QString inst_dir;
#ifdef INSTALL_DIR
  inst_dir = INSTALL_DIR;
#else
  inst_dir = "/usr/local";
#endif
  if(inst_dir.right(1) != "/") inst_dir += "/";

//install the message handler; if the user specified the --debug option, we
//reset it later
  qInstallMsgHandler(qisMessageHandler);

#ifdef KDEAPP
  KAboutData about("quiteinsane","QuiteInsane",VERSION,
     "A graphical SANE-Frontend",KAboutData::License_GPL,
     "(c) 2000-2003, Michael Herder",0,"http://quiteinsane.sourceforge.net");
  // Initialize command line args
  KCmdLineArgs::init(argc, argv, &about);
  KCmdLineArgs::addCmdLineOptions( options );
  KApplication::addCmdLineOptions();
  KApplication a(argc, argv);
#else
  QApplication a(argc, argv);
#endif
//Does .QuiteInsaneFolder exist? If it doesn't, create it.
  QString qs;
  qs = QDir::homeDirPath()+"/.QuiteInsaneFolder/";
  QDir qd(qs);
  if(!qd.exists())
  {//try to create it
    if(!qd.mkdir(qs))
    {
      QMessageBox::critical(0,QObject::tr("Abort"),
                            QObject::tr("Could not create the directory\n"
                            "%1/.QuiteInsaneFolder/.\n"
                            "QuiteInsane will not work without this directory.\n"
                            "Please ensure, that there's enough space on your\n"
                            "harddisk.").arg(QDir::homeDirPath()),
                            QObject::tr("Abort"));
       return 0;
    }
  }
  //lock file
  qs = QDir::homeDirPath()+"/.QuiteInsaneFolder/quiteinsane_lock";
  int fd;
  fd = open(qs.latin1(),O_WRONLY | O_CREAT | O_TRUNC,S_IRUSR  | S_IWUSR);
  if(fd == -1)
  {
    QMessageBox::critical(0,QObject::tr("Abort"),
       QObject::tr("Could not open lock file.\n"
                   "QuiteInsane will not work without this file.\n"
                   "Please ensure, that there's enough space on your\n"
                   "harddisk and that you have write permission for \n"
                   "the directory .QuiteInsaneFolder in your home\n"
                   "directory."),
                   QObject::tr("Abort"));
    return 0;
  }
  //We could open the file, now try to lock it
  struct flock lock_struct;
  lock_struct.l_type   = F_WRLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
  lock_struct.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
  lock_struct.l_start  = 0;        /* Offset from l_whence         */
  lock_struct.l_len    = 0;        /* length, 0 = to EOF           */
  lock_struct.l_pid    = getpid(); /* our PID*/
  int fd2;
  fd2 = fcntl(fd,F_SETLK,&lock_struct);
  if(fd2 == -1)
  {
    //file already locked
    QMessageBox::critical(0,QObject::tr("Abort"),
       QObject::tr("QuiteInsane is already running."),
                   QObject::tr("Abort"));
    close(fd);
    return 0;
  }

//tiff io
#ifdef HAVE_LIBTIFF
  initTiffIO();
#endif
  initPnmIO();
#ifdef KDEAPP
  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
  if(args->isSet("debug"))
  {
    qInstallMsgHandler(0);
  }
  local_dir = args->getOption("local_dir");
  if(local_dir.isEmpty())
  {
    local_dir = QString::null;
  }
#else

//Useful during development.
  if(argc >= 2)
  {
    bool must_exit = true;
    bool print_help = false;
    bool known_opt = false;
    for(int i=1;i<argc;i++)
    {
      qs = argv[i];
      if(qs == "--debug")
      {
        qInstallMsgHandler(0);
        known_opt = true;
        must_exit = false;
      }
      else if((qs == "--help") || (qs == "-h"))
      {
        print_help = true;
        must_exit = true;
        known_opt = true;
      }
      else if(qs == "--local_dir")
      {
        i +=1;
        if(i < argc)
        {
          local_dir = argv[i];
          known_opt = true;
          must_exit = false;
        }
      }
      else
      {
        print_help = true;
        known_opt = false;
        must_exit = true;
      }
    }
    if(!known_opt)
    {
      printf("Unknown or invalid option.\n");
    }
    if(print_help)
    {
      printf("QuiteInsane "VERSION"\n");
      printf("Usage: quiteinsane [options]\n");
      printf("--help, -h: Show this help message.\n");
      printf("--local_dir dirname: Search translations in dirname.\n");
      printf("--debug: Print debug messages.\n");
    }
    if(must_exit)
      return 0;
  }
#endif
//translators
  QTranslator* tor = new QTranslator(&a);
  QTranslator* tor2 = new QTranslator(&a);
  if(local_dir == QString::null)
    local_dir = inst_dir+"/share/quiteinsane/locale";
  if(tor->load(QString("quiteinsane_")+QString(QTextCodec::locale()).left(2),local_dir))
    a.installTranslator(tor);
  else
    delete tor;
  if(tor2->load(QString("qt_")+QString(QTextCodec::locale()).left(2),local_dir))
    a.installTranslator(tor2);
  else
    delete tor2;

  SplashWidget* sp = new SplashWidget(0);
  sp->show();
  qApp->processEvents();
  QScannerSetupDlg ssd(0);
  delete sp;
  a.setMainWidget(&ssd);
  ssd.show();
  int ret = a.exec();
  //unlock file
  lock_struct.l_type = F_UNLCK;
  fcntl(fd, F_SETLK, &lock_struct);
  close(fd);
  return ret;
}
