/*

Paper filing system GUI based on scanner

Large chunks of code taken from quiteinsane
Used and tested a lot with the Fujitsu backend


SANE_DEBUG_FUJITSU=30


Fujitsu buttons

function    meaning     action
1           scan        perform a scan into current directory
2           sendto      perform a scan into user's sendto directory
3
4
5
6
7
8
9
C           copy        scan and print to default printer, save to 'photocopy' folder

*/


#include <unistd.h>
#include <getopt.h>
#include <sys/resource.h>

#include <QDebug>
#include <QSettings>
#include <QTranslator>

#include "qapplication.h"
#include "qmessagebox.h"
#include "qsettings.h"
#include "qxmlconfig.h"

#include "config.h"

#include "desktopmodel.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "md5.h"
#include "desk.h"
#include "maxview.h"
#include "op.h"
#include "test/test.h"

/*
extern "C" void rle_test (void);
extern "C" void decpp_set_debug (int d);
extern "C" void decpp_set_hack (int d);
extern "C" int test_main (void);
*/
#include "err.h"


bool err_complain (err_info *err)
   {
   if (err)
      QMessageBox::warning (0, "Maxview", err->errstr);
   return err != NULL;
   }


static void usage (void)
   {
   printf ("maxview - An electronic filing cabinet: scan, print, stack, arrange\n\n");
   printf ("(C) 2011 Simon Glass, chch-kiwi@users.sourceforge.net, v%s\n\n",
           CONFIG_version_str);
   printf ("Usage:  maxview <opts>  <dir/file>\n\n");
   printf ("   -p|--pdf        convert given file to .pdf\n");
   printf ("   -m|--max        convert given file to .max\n");
   printf ("   -j|--jpeg       convert given file to .jpg\n");
/*
   printf ("   -v|--verbose    be verbose\n");
*/
   printf ("   -h|--help       display this usage information\n");
   printf ("   -s|--sum        do an md5 checksum on a directory\n");
/*
   printf ("   -d|--debug <n>  set debug level (0-3)\n");
   printf ("   -f|--force      force overwriting of existing file\n");
   printf ("   -r|--relocate   move a processed file into a 'xxx.old' subdirectory\n");
   printf ("   -i|--info       display full info about a file\n");
   printf ("      --index <f>  build/update an index file f for the given directory\n");
*/
   printf ("   -t|--test       run unit tests\n");
/*
   printf ("\n");
   printf ("If none of -p, -m, -j are specified, maxview opens in desktop "
          "mode, displaying\nthumbails of the given directory\n");
*/
   }

static void run_gui(QApplication& app, int argc, char *argv[])
    {
    Desktopwidget *desktop;
    Mainwindow *me;

    me = new Mainwindow ();

    // get the desktop (this has the directory tree and page splitter view)
    desktop = me->_main->getDesktop ();

    QSettings qs;
    QList<err_info> err_list;
    err_info *err;

    int size = qs.beginReadArray ("repository");
    for (int i = 0; i < size; i++)
       {
       qs.setArrayIndex (i);
       err = desktop->addDir (qs.value ("path").toString (), true);
       if (err)
          err_list << *err;
       }
    qs.endArray ();

    /* Add dirs for any arguments */
    for (int c = argc - 1; c >= optind; c--)
       {
       err = desktop->addDir (argv [c]);
       if (err)
          err_list << *err;
       }

    me->show ();
    QModelIndex ind = QModelIndex ();
    desktop->selectDir (ind);

    if (err_list.size ())
       {
       QString msg;

       msg = app.tr ("%n error(s) on startup", "", err_list.size ());
       msg.append (":\n");
       foreach (const err_info &err, err_list)
          msg.append (QString ("%1\n").arg (err.errstr));
       QMessageBox::warning (0, "Maxview", msg);
       }
    app.exec ();

    // write back any configuration changes (scanner type, etc.)
    if (xmlConfig)
       xmlConfig->writeConfigFile ();
    me->_main->saveSettings ();
    delete me;
    }


int main (int argc, char *argv[])
   {
   //bool verbose = false, force = false, reloc = false, hack = false;
   //int debug = 0;
   char *dir = 0;
   bool need_gui = false;
//    err_info *e;
   static struct option long_options[] = {
//     {"index", 0, 0, '1'},
     {"help", 0, 0, 'h'},
     {"jpg", 1, 0, 'j'},
     /*
     {"debug", 1, 0, 'd'},
     {"force", 0, 0, 'f'},
     {"info", 0, 0, 'i'},
*/
     {"max", 1, 0, 'm'},
     {"pdf", 1, 0, 'p'},
/*
     {"relocate", 0, 0, 'r'},
*/
     {"sum", 1, 0, 's'},
     {"test", 0, 0, 't'},
     /*
     {"verbose", 0, 0, 'v'},
*/
     {0, 0, 0, 0}
   };
   int op_type = -1, c;
   QString index;
   QString fname;

   struct rlimit limit;

   if (!getrlimit(RLIMIT_NOFILE, &limit) && limit.rlim_cur < 20000) {
      qDebug() << "limit" << limit.rlim_cur;
      limit.rlim_cur = 20000;
      limit.rlim_cur = 20000;

      if (setrlimit(RLIMIT_NOFILE, &limit))
         qDebug() << "Setting nofile limit failed";
   }

   while (c = getopt_long (argc, argv, "hj:m:p:s:t",
                           long_options, NULL), c != -1)
      switch (c)
         {
         case 's' :
            dir = optarg;
            op_type = c;
            break;
         case 'j' :
         case 'm' :
         case 'p' :
            fname = optarg;
            op_type = c;
            break;

         case 't' :
         case 'i' :
            op_type = c;
            break;

         case '1' :
            op_type = c;
            index = QString (optarg);
            break;

         case 'h' :
         case '?' :
            usage ();
            return 1;

/*
         case 'd' : debug = atoi (optarg); break;
         case 'v' : verbose = true; break;
         case 'f' : force = true; break;
         case 'r' : reloc = true; break;
         case 'z' : hack = true; break;
*/
         }

   if (!dir && op_type != 't' && op_type != 'p' && op_type != 'm' &&
       op_type != 'j')
      need_gui = true;

#ifdef Q_WS_X11
    bool useGUI = getenv( "DISPLAY" ) != 0 && op_type == -1;
#else
    bool useGUI = op_type == -1;
#endif
   if (op_type == 't')
      useGUI = true;

   if (!need_gui && op_type == -1 && !useGUI)
      {
      printf ("** Warning: no display available for interactive mode\n");
      printf ("Please either run in Gnome/KDE/X or set the DISPLAY variable\n\n");
      usage ();
      return 1;
      }
   if (need_gui && !useGUI)
      {
      usage ();
      return 1;
      }
   QApplication app (argc, argv, useGUI);

   QTranslator translator;
   translator.load("maxview_en");
   app.installTranslator(&translator);

   QCoreApplication::setOrganizationName("maxview");
   //QCoreApplication::setOrganizationDomain("bluewatersys.com");
   QCoreApplication::setApplicationName("maxview");

   switch (op_type)
      {
      case 's' :
         {
         QString mydir = QString (dir);
         Desk *maxdesk = new Desk (mydir, QString());
         err_info *err;

         // decode everything in the given directory and checksum it
         err = maxdesk->checksum();
         if (err)
            printf("checksum error: %s", err->errstr);
         break;
         }

      case 't' :
         {
#if 0
         QString fname = QString (dir);
         Desk maxdesk (QString(), QString());

         dirmodel_tests ();
//      maxdesk.runTests ();
//      rle_test ();

      // decode everything in the given directory and checksum it
//	 if (e = maxdesk.test_decomp_comp (fname), e)
//      if (maxdesk.test_compare_with_tiff (fname))
//    if (maxdesk.test (fname))
//	   printf ("test error %s\n", e->errstr);
#endif
         // Drop the -t argument
         argv[argc--] = 0;
         int result = test_run(argc - 1, argv, &app);

         if (result)
            qInfo() << "Failed: " << result;
         break;
         }
      case 'j' :
      case 'm' :
      case 'p' :
         {
         QDir mydir(fname);
         mydir.cdUp();
         Desk desk(dir, QString(), false);
         File *file, *newfile;
         Operation op("Convert file", 0, 0);
         err_info *err;

         file = desk.createFile(dir, fname);
         err = file->load();
         if (!err) {
            QString leaf = QFileInfo(fname).completeBaseName();

            File::e_type type = op_type == 'p' ? File::Type_pdf :
                     op_type == 'j' ? File::Type_jpeg :
                     File::Type_max;
            err = file->duplicateToDesk(&desk, type, leaf, 3, op,
                                        newfile);
         }
         if (err)
            printf("error: %s", err->errstr);
         break;
         }
#if 0
      case 'i' :
         {
         Desk maxdesk (QString(), QString(), false, false, false);
         QString fname = QString (dir);

         // decode print info on the file
         e = maxdesk.dumpInfo (fname, debug);
         if (e)
            printf ("error: %s\n", e->errstr);
         break;
         }
#endif
      case -1 :
         run_gui(app, argc, argv);
#if 0
      case 1 :
         Desk maxdesk (QString(), QString());
         QString fname = QString (dir);

         // convert the file to PDF
         maxdesk.buildIndex (index, fname);
#endif
      }
   if (xmlConfig)
      delete xmlConfig;
   }


