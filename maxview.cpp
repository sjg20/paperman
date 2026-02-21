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
#include <QProcess>
#include <QRegExp>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
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

/**
 * Convert a PDF to .max using parallel child processes
 *
 * Each child converts a range of pages to a temporary .max file, then all
 * parts are merged via stackStack() which copies compressed chunks directly
 * with no decompression.
 *
 * @param file     Source PDF file (already loaded)
 * @param fname    Path to the PDF file
 * @param leaf     Leaf name for the output file
 * @param desk     Desk to create the output file in
 * @param jobs     Number of workers (0 = auto)
 * @return error, or NULL if ok
 */
static err_info *parallel_convert_to_max(File *file, const QString &fname,
                                         const QString &leaf, Desk *desk,
                                         int jobs)
   {
   int pc = file->pagecount();

   // Determine worker count: use CPU count but ensure each worker
   // has at least MIN_PAGES_PER_WORKER pages, to avoid process-spawn
   // overhead dominating on high-core machines
   const int MIN_PAGES_PER_WORKER = 10;
   int workers = QThread::idealThreadCount();
   if (workers < 1)
      workers = 2;
   if (jobs > 0)
      workers = jobs;
   if (workers > pc / MIN_PAGES_PER_WORKER)
      workers = pc / MIN_PAGES_PER_WORKER;
   if (workers < 1)
      workers = 1;

   // Find the paperman binary (argv[0] resolved through /proc/self/exe)
   QString self = QCoreApplication::applicationFilePath();

   QTemporaryDir tmpdir;
   if (!tmpdir.isValid())
      return err_make(ERRFN, ERR_cannot_open_file1, "temporary directory");

   // Split pages into N roughly-equal ranges (1-based for CLI)
   QVector<QPair<int, int>> ranges;
   int pages_per = pc / workers;
   int extra = pc % workers;
   int start = 1;

   for (int i = 0; i < workers; i++) {
      int count = pages_per + (i < extra ? 1 : 0);
      int end = start + count - 1;
      ranges.append(qMakePair(start, end));
      start = end + 1;
   }

   // Spawn child processes
   QVector<QProcess *> children;
   QStringList partPaths;

   for (int i = 0; i < workers; i++) {
      QString partPath = tmpdir.path() + QString("/part%1.max").arg(i);
      partPaths.append(partPath);

      QProcess *child = new QProcess();
      QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
      env.insert("QT_QPA_PLATFORM", "offscreen");
      child->setProcessEnvironment(env);

      QStringList args;
      args << "-m" << fname
           << "--page-range"
           << QString("%1:%2").arg(ranges[i].first).arg(ranges[i].second)
           << "--output" << partPath;
      child->start(self, args);
      children.append(child);
   }

   // Wait for all children
   bool all_ok = true;

   for (int i = 0; i < children.size(); i++) {
      children[i]->waitForFinished(-1);
      if (children[i]->exitCode() != 0 ||
          children[i]->exitStatus() != QProcess::NormalExit) {
         fprintf(stderr, "child %d failed (exit code %d)\n",
                 i, children[i]->exitCode());
         QByteArray err_out = children[i]->readAllStandardError();
         if (!err_out.isEmpty())
            fprintf(stderr, "  stderr: %s\n", err_out.constData());
         all_ok = false;
      }
   }

   // Clean up child processes
   qDeleteAll(children);

   if (!all_ok)
      return err_make(ERRFN, ERR_could_not_execute1, "parallel conversion");

   // Copy first part to final destination as the base
   QString ext = File::typeExt(File::Type_max);
   QString destDir = desk->dir();
   QString destName = leaf + ext;
   QString destPath = destDir + destName;
   if (!QFile::copy(partPaths[0], destPath))
      return err_make(ERRFN, ERR_cannot_open_file1,
                      qPrintable(destPath));

   // Open the destination and stack remaining parts onto it
   Filemax *dest = new Filemax(destDir, destName, desk);
   err_info *err = dest->load();
   if (err) {
      delete dest;
      return err;
   }

   for (int i = 1; i < partPaths.size(); i++) {
      QFileInfo fi(partPaths[i]);
      Filemax *part = new Filemax(fi.absolutePath() + '/', fi.fileName(),
                                  nullptr);
      err = part->load();
      if (err) {
         delete part;
         delete dest;
         return err;
      }

      dest->setPagenum(dest->pagecount());
      err = dest->stackItem(part);
      delete part;
      if (err) {
         delete dest;
         return err;
      }
   }

   desk->newFile(dest, nullptr, 1);
   return NULL;
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
   printf ("   -t|--test [class]  run unit tests (or a single suite; 'list' to list)\n");
   printf ("   --page-range S:E  convert only pages S to E (1-based)\n");
   printf ("   --output FILE   write output to FILE (used with --page-range)\n");
   printf ("   --jobs N        use N parallel workers (0 = auto)\n");
   printf ("   --rebuild-previews FILE|DIR  regenerate missing greyscale previews\n");
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
     {"page-range", 1, 0, 256},
     {"output", 1, 0, 257},
     {"jobs", 1, 0, 258},
     {"rebuild-previews", 1, 0, 259},
     {0, 0, 0, 0}
   };
   int op_type = -1, c;
   QString index;
   QString fname;
   QString searchQuery;
   int page_start = -1, page_end = -1;   // 1-based, -1 = all
   QString output_path;
   int jobs = 0;                          // 0 = auto

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

         case 256 :    // --page-range S:E
            {
            QString range = QString(optarg);
            QStringList parts = range.split(':');
            if (parts.size() == 2) {
               page_start = parts[0].toInt();
               page_end = parts[1].toInt();
            }
            break;
            }

         case 257 :    // --output
            output_path = QString(optarg);
            break;

         case 258 :    // --jobs
            jobs = atoi(optarg);
            break;

         case 259 :    // --rebuild-previews
            fname = optarg;
            op_type = c;
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
       op_type != 'j' && op_type != 'o' && op_type != 'q' &&
       op_type != 259)
      need_gui = true;

#ifdef Q_WS_X11
    bool useGUI = getenv( "DISPLAY" ) != 0 && op_type == -1;
#else
    bool useGUI = op_type == -1;
#endif
   if (op_type == 't')
      useGUI = true;

   // OCR batch mode, search, and rebuild-previews don't need GUI
   if (op_type == 'o' || op_type == 'q' || op_type == 259)
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
         // Build a clean argv for QTest (drop -t and optional suite name)
#ifdef ENABLE_TEST
         {
         const char *filter = (optind < argc) ? argv[optind] : nullptr;
         char *qt_argv[] = { argv[0], nullptr };
         int qt_argc = 1;
         int result = test_run(qt_argc, qt_argv, &app, filter);

         if (result)
            qInfo() << "Failed: " << result;
         }
#else
         qInfo() << "Use this to build with tests: qmake CONFIG+=test";
#endif
         break;
         }
      case 'j' :
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
            File::e_type type = op_type == 'p' ? File::Type_pdf :
                     File::Type_jpeg;

            if (!output_path.isEmpty()) {
               QFileInfo outInfo(output_path);
               QString outDir = outInfo.absolutePath() + '/';
               QString outFname = outInfo.fileName();
               newfile = File::createFile(outDir, outFname, nullptr,
                                          type);
               if (!newfile) {
                  err = File::not_impl();
               } else {
                  err = newfile->create();
                  if (!err)
                     err = file->copyTo(newfile, 3, op, false,
                                     page_start - 1, page_end - 1);
                  delete newfile;
               }
            } else {
               QString leaf = QFileInfo(fname).completeBaseName();
               err = file->duplicateToDesk(&desk, type, leaf, 3, op,
                                           newfile);
            }
         }
         if (err)
            printf("error: %s", err->errstr);
         break;
         }

      case 'm' :
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

            if (!output_path.isEmpty()) {
               // Child-mode: write to specified output file
               QFileInfo outInfo(output_path);
               QString outDir = outInfo.absolutePath() + '/';
               QString outFname = outInfo.fileName();
               newfile = File::createFile(outDir, outFname, nullptr,
                                          File::Type_max);
               if (!newfile) {
                  err = File::not_impl();
               } else {
                  err = newfile->create();
                  if (!err)
                     err = file->copyTo(newfile, 3, op, false,
                                        page_start - 1, page_end - 1);
                  delete newfile;
               }
            } else {
               int pc = file->pagecount();
               const int PARALLEL_THRESHOLD = 8;
               if (pc >= PARALLEL_THRESHOLD && page_start < 0)
                  err = parallel_convert_to_max(file, fname, leaf,
                                                &desk, jobs);
               else
                  err = file->duplicateToDesk(&desk, File::Type_max,
                           leaf, 3, op, newfile);
            }
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
      case 259 :
         {
         QFileInfo info(fname);
         err_info *err = NULL;

         if (info.isFile())
            {
            QString fileDir = info.absolutePath();
            if (!fileDir.endsWith('/'))
               fileDir += '/';
            QString fileName = info.fileName();

            Filemax *max = new Filemax(fileDir, fileName, nullptr);
            err = max->load();
            if (!err)
               err = max->rebuildPreviews();
            if (err)
               fprintf(stderr, "Error processing %s: %s\n",
                       qPrintable(fileName), err->errstr);
            delete max;
            }
         else if (info.isDir())
            {
            QDirIterator it(fname, QStringList() << "*.max",
                            QDir::Files,
                            QDirIterator::Subdirectories);
            while (it.hasNext())
               {
               QString filePath = it.next();
               QFileInfo fi(filePath);
               QString fileDir = fi.absolutePath();
               if (!fileDir.endsWith('/'))
                  fileDir += '/';
               QString fileName = fi.fileName();

               printf("Processing: %s\n", qPrintable(filePath));
               Filemax *max = new Filemax(fileDir, fileName, nullptr);
               err = max->load();
               if (!err)
                  err = max->rebuildPreviews();
               if (err)
                  fprintf(stderr, "Error: %s\n", err->errstr);
               delete max;
               }
            }
         else
            {
            fprintf(stderr, "Not a file or directory: %s\n",
                    qPrintable(fname));
            }
         break;
         }

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


