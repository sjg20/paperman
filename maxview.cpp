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
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QSettings>
#include <QTranslator>

#include "qapplication.h"
#include "qmessagebox.h"
#include "qsettings.h"

#include "config.h"

#include "desktopmodel.h"
#include "desktopwidget.h"
#include "dirmodel.h"
#include "mainwidget.h"
#include "mainwindow.h"
#include "md5.h"
#include "desk.h"
#include "filemax.h"
#include "maxview.h"
#include "ocr.h"
#include "op.h"
#include "test/test.h"

/*
extern "C" void rle_test (void);
extern "C" void decpp_set_debug (int d);
extern "C" void decpp_set_hack (int d);
extern "C" int test_main (void);
*/
#include "err.h"

static err_info *batch_ocr_directory(const QString &dirPath)
   {
   printf("Scanning directory: %s\n", qPrintable(dirPath));

   // Get OCR engine
   err_info *err = nullptr;
   Ocr *ocr = Ocr::getOcr(err);
   if (err || !ocr)
      {
      fprintf(stderr, "Failed to initialize OCR engine: %s\n",
              err ? err->errstr : "unknown error");
      return err;
      }

   int total_files = 0;
   int processed = 0;
   int skipped = 0;
   int errors = 0;

   // Recursively find all .max files
   QDirIterator it(dirPath, QStringList() << "*.max", QDir::Files,
                   QDirIterator::Subdirectories);

   while (it.hasNext())
      {
      QString filePath = it.next();
      total_files++;
      }

   printf("Found %d .max files to process\n\n", total_files);

   // Process each file
   QDirIterator it2(dirPath, QStringList() << "*.max", QDir::Files,
                    QDirIterator::Subdirectories);

   while (it2.hasNext())
      {
      QString filePath = it2.next();
      QFileInfo fileInfo(filePath);
      QString fileDir = fileInfo.absolutePath();
      if (!fileDir.endsWith('/'))
         fileDir += '/';
      QString fileName = fileInfo.fileName();

      printf("[%d/%d] Processing: %s\n", processed + 1, total_files,
             qPrintable(fileName));

      // Open the .max file
      Filemax *file = new Filemax(fileDir, fileName, nullptr);
      err = file->load();

      if (err)
         {
         fprintf(stderr, "  ERROR: Failed to load file: %s\n", err->errstr);
         errors++;
         delete file;
         continue;
         }

      // Check if file already has OCR text
      QString existing_ocr;
      err = file->getAnnot(File::Annot_ocr, existing_ocr);
      if (!err && !existing_ocr.isEmpty())
         {
         printf("  Skipped: Already has OCR text (%d chars)\n",
                existing_ocr.length());
         skipped++;
         delete file;
         continue;
         }

      // Process each page
      int page_count = file->pagecount();
      QString all_text;

      for (int page = 0; page < page_count; page++)
         {
         // Get the page image
         QImage qimage;
         QSize size, trueSize;
         int bpp;

         err = file->getImage(page, false, qimage, size, trueSize, bpp, false);

         if (err || qimage.isNull())
            {
            if (err)
               fprintf(stderr, "  WARNING: Failed to get image for page %d: %s\n",
                      page, err->errstr);
            continue;
            }

         // Run OCR on the page
         QString page_text;
         err = ocr->imageToText(qimage, page_text);

         if (!err && !page_text.isEmpty())
            {
            if (page > 0)
               all_text += "\n\n--- Page " + QString::number(page + 1) + " ---\n\n";
            all_text += page_text;
            }
         }

      // Save OCR text if we got any
      if (!all_text.isEmpty())
         {
         QHash<int, QString> updates;
         updates[File::Annot_ocr] = all_text;
         err = file->putAnnot(updates);

         if (!err)
            {
            file->flush();
            printf("  SUCCESS: Extracted %d characters from %d pages\n",
                   all_text.length(), page_count);
            processed++;
            }
         else
            {
            fprintf(stderr, "  ERROR: Failed to save OCR text: %s\n",
                   err->errstr);
            errors++;
            }
         }
      else
         {
         printf("  Skipped: No text extracted\n");
         skipped++;
         }

      delete file;
      }

   printf("\n=== OCR Batch Processing Complete ===\n");
   printf("Total files:     %d\n", total_files);
   printf("Processed:       %d\n", processed);
   printf("Skipped:         %d\n", skipped);
   printf("Errors:          %d\n", errors);

   return nullptr;
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
   printf ("   -o|--ocr        run OCR on all .max files in directory (recursive)\n");
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
     {"ocr", 1, 0, 'o'},
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

   while (c = getopt_long (argc, argv, "hj:m:o:p:s:t",
                           long_options, NULL), c != -1)
      switch (c)
         {
         case 'o' :
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
       op_type != 'j' && op_type != 'o')
      need_gui = true;

#ifdef Q_WS_X11
    bool useGUI = getenv( "DISPLAY" ) != 0 && op_type == -1;
#else
    bool useGUI = op_type == -1;
#endif
   if (op_type == 't')
      useGUI = true;

   // OCR batch mode doesn't need GUI
   if (op_type == 'o')
      {
      useGUI = false;
      // Force offscreen platform for batch OCR mode
      setenv("QT_QPA_PLATFORM", "offscreen", 1);
      }

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
      case 'o' :
         {
         QString mydir = QString (dir);
         err_info *err;

         // Run OCR on all .max files in the directory
         err = batch_ocr_directory(mydir);
         if (err)
            fprintf(stderr, "OCR batch error: %s\n", err->errstr);
         break;
         }

      case 's' :
         {
         QString mydir = QString (dir);
         Desk *maxdesk = new Desk(mydir, QString());
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
#ifdef ENABLE_TEST
         argv[argc--] = 0;
         int result = test_run(argc, argv, &app);

         if (result)
            qInfo() << "Failed: " << result;
#else
         qInfo() << "Use this to build with tests: qmake CONFIG+=test";
#endif
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
         QStringList args;
         for (int i = 1; i < argc; i++)
            args << argv[i];
         Mainwindow::runGui(app, args);
#if 0
      case 1 :
         Desk maxdesk (QString(), QString());
         QString fname = QString (dir);

         // convert the file to PDF
         maxdesk.buildIndex (index, fname);
#endif
      }
   }


