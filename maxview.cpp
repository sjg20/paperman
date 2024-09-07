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
#include "desk.h"
#include "maxview.h"
#include "op.h"

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
/*
   printf ("   -p|--pdf        convert given file to .pdf\n");
   printf ("   -m|--max        convert given file to .max\n");
   printf ("   -j|--jpeg       convert given file to .jpg\n");
   printf ("   -v|--verbose    be verbose\n");
*/
   printf ("   -h|--help       display this usage information\n");
/*
   printf ("   -d|--debug <n>  set debug level (0-3)\n");
   printf ("   -f|--force      force overwriting of existing file\n");
   printf ("   -r|--relocate   move a processed file into a 'xxx.old' subdirectory\n");
   printf ("   -i|--info       display full info about a file\n");
   printf ("      --index <f>  build/update an index file f for the given directory\n");
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
/*
     {"jpg", 0, 0, 'j'},
     {"debug", 1, 0, 'd'},
     {"force", 0, 0, 'f'},
     {"info", 0, 0, 'i'},
     {"max", 0, 0, 'm'},
     {"pdf", 0, 0, 'p'},
     {"relocate", 0, 0, 'r'},
     {"sum", 0, 0, 's'},
     {"test", 0, 0, 't'},
     {"verbose", 0, 0, 'v'},
*/
     {0, 0, 0, 0}
   };
   int op_type = -1, c;
   QString index;

   struct rlimit limit;

   if (!getrlimit(RLIMIT_NOFILE, &limit) && limit.rlim_cur < 20000) {
      qDebug() << "limit" << limit.rlim_cur;
      limit.rlim_cur = 20000;
      limit.rlim_cur = 20000;

      if (setrlimit(RLIMIT_NOFILE, &limit))
         qDebug() << "Setting nofile limit failed";
   }

   while (c = getopt_long (argc, argv, "h",
                           long_options, NULL), c != -1)
      switch (c)
         {
	 case 's' :
	 case 't' :
	 case 'm' :
	 case 'p' :
	 case 'i' :
	 case 'j' :
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

   if (optind < argc)
      dir = argv [optind];

   //   printf ("type=%d/%c, debug=%d, verbose=%d, file=%s\n",
   //	   op_type, op_type, debug, verbose, dir ? dir : "<null>");

   if (!dir && op_type != 't')
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

// no longer used (debugging is set from maxdesk)
//   decpp_set_debug (debug);

   /* this is a hack to fix old broken files (the -z option). Shouldn't be
      needed now */
//    decpp_set_hack (hack);

   //qDebug () << "test" << "test2";

   switch (op_type)
      {
#if 0
      case 's' :
	 {
	 QString mydir = QString (dir);
	 Desk *maxdesk = new Desk (mydir, QString());

	 // decode everything in the given directory and checksum it
	 if (!maxdesk->checksum ())
	    printf ("checksum error\n");
	 break;
	 }

      case 't' :
	 {
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
	 break;
	 }

      case 'p' :
	 {
	 Desk maxdesk (QString(), QString());
	 QString fname = QString (dir);

	 // convert the file to PDF
	 maxdesk.convertPdf (fname);
	 break;
	 }

      case 'j' :
	 {
	 Desk maxdesk (QString(), QString());
	 QString fname = QString (dir);

	 // convert the file to JPEG
	 maxdesk.convertJpeg (fname);
	 break;
	 }

      case 'm' :
	 {
	 Desk maxdesk (QString(), QString());
	 QString fname = QString (dir);

      // convert the file to .max
	 e = maxdesk.convertMax (fname, QString(), verbose, force, reloc);
	 if (e)
	    printf ("error: %s\n", e->errstr);
	 break;
	 }

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


