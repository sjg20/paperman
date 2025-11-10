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
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QRegExp>
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
#include "searchindex.h"
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

   // Initialize search index
   SearchIndex searchIndex;
   err_info *err = searchIndex.init(dirPath);
   if (err)
      {
      fprintf(stderr, "Warning: Could not initialize search index: %s\n",
              err->errstr);
      fprintf(stderr, "Continuing without indexing...\n");
      }

   // Get OCR engine
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
         // File already has OCR text - index it without re-OCRing
         printf("  Indexing existing OCR text (%d chars)\n",
                existing_ocr.length());

         if (searchIndex.isOpen())
            {
            // Parse the existing OCR text to extract per-page content
            // Format: "text\n\n--- Page N ---\n\ntext..."
            QStringList parts = existing_ocr.split(QRegExp("\n\n--- Page \\d+ ---\n\n"));

            for (int page = 0; page < parts.size(); page++)
               {
               QString page_text = parts[page].trimmed();
               if (!page_text.isEmpty())
                  {
                  err = searchIndex.addPage(filePath, fileName, page, page_text);
                  if (err)
                     fprintf(stderr, "  WARNING: Failed to index page %d: %s\n",
                            page, err->errstr);
                  }
               }

            printf("  SUCCESS: Indexed %d pages from existing OCR\n", parts.size());
            processed++;
            }
         else
            {
            skipped++;
            }

         delete file;
         continue;
         }

      // No existing OCR text - process each page
      int page_count = file->pagecount();
      QString all_text;
      int pages_with_text = 0;

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
            // Add to combined text for annotation
            if (page > 0)
               all_text += "\n\n--- Page " + QString::number(page + 1) + " ---\n\n";
            all_text += page_text;

            // Add to search index (one entry per page)
            if (searchIndex.isOpen())
               {
               err = searchIndex.addPage(filePath, fileName, page, page_text);
               if (err)
                  fprintf(stderr, "  WARNING: Failed to index page %d: %s\n",
                         page, err->errstr);
               }

            pages_with_text++;
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
                   all_text.length(), pages_with_text);
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

   if (searchIndex.isOpen())
      {
      printf("\nSearch index created: %s\n", qPrintable(searchIndex.indexPath()));
      printf("Use --search <query> to search the indexed text\n");
      }

   return nullptr;
   }

static err_info *search_ocr_index(const QString &dirPath, const QString &query)
   {
   printf("Searching for: %s\n", qPrintable(query));

   // Determine directory to search in
   QString searchDir = dirPath;
   if (searchDir.isEmpty())
      {
      // If no directory specified, use current directory
      searchDir = QDir::currentPath();
      }

   // Ensure directory path ends with /
   if (!searchDir.endsWith('/'))
      searchDir += '/';

   QString indexPath = searchDir + ".paperindex";

   // Check if index exists
   if (!QFile::exists(indexPath))
      {
      fprintf(stderr, "Error: No search index found at %s\n", qPrintable(indexPath));
      fprintf(stderr, "Run --ocr on the directory first to create an index.\n");
      return err_make(ERRFN, ERR_cannot_open_file1, qPrintable(indexPath));
      }

   // Open search index
   SearchIndex searchIndex;
   err_info *err = searchIndex.init(searchDir);
   if (err)
      {
      fprintf(stderr, "Failed to open search index: %s\n", err->errstr);
      return err;
      }

   // Perform search
   QList<SearchResult> results;
   err = searchIndex.search(query, results, 100);
   if (err)
      {
      fprintf(stderr, "Search failed: %s\n", err->errstr);
      return err;
      }

   // Display results
   printf("\nFound %d results:\n\n", results.size());

   for (int i = 0; i < results.size(); i++)
      {
      const SearchResult &result = results[i];
      printf("[%d] %s (page %d)\n", i + 1, qPrintable(result.filename),
             result.pagenum + 1);
      printf("    Path: %s\n", qPrintable(result.filepath));
      printf("    %s\n\n", qPrintable(result.snippet));
      }

   if (results.isEmpty())
      {
      printf("No results found for '%s'\n", qPrintable(query));
      }

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
   printf ("   -q|--search     search indexed OCR text for a query\n");
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
     {"search", 1, 0, 'q'},
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
   QString searchQuery;

   struct rlimit limit;

   if (!getrlimit(RLIMIT_NOFILE, &limit) && limit.rlim_cur < 20000) {
      qDebug() << "limit" << limit.rlim_cur;
      limit.rlim_cur = 20000;
      limit.rlim_cur = 20000;

      if (setrlimit(RLIMIT_NOFILE, &limit))
         qDebug() << "Setting nofile limit failed";
   }

   while (c = getopt_long (argc, argv, "hj:m:o:p:q:s:t",
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

         case 'q' :
            searchQuery = QString(optarg);
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
       op_type != 'j' && op_type != 'o' && op_type != 'q')
      need_gui = true;

#ifdef Q_WS_X11
    bool useGUI = getenv( "DISPLAY" ) != 0 && op_type == -1;
#else
    bool useGUI = op_type == -1;
#endif
   if (op_type == 't')
      useGUI = true;

   // OCR batch mode and search don't need GUI
   if (op_type == 'o' || op_type == 'q')
      {
      useGUI = false;
      // Force offscreen platform for console mode
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

      case 'q' :
         {
         // Get directory from remaining arguments if provided
         QString searchDir;
         if (optind < argc)
            searchDir = QString(argv[optind]);

         err_info *err;

         // Search the OCR index
         err = search_ocr_index(searchDir, searchQuery);
         if (err)
            fprintf(stderr, "Search error: %s\n", err->errstr);
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


