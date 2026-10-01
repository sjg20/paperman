#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>

#include "suite.h"
#include "utils.h"

const QString Test::testSrc = "test/files";

Test::Test(QString name) :
   QObject(), _name(name)
{
   _tempDir = new QTemporaryDir();
}

Test::~Test()
{
   delete _tempDir;
}

/* The files under @dir which this process still holds open.  Only Linux
   can say, through /proc; elsewhere the list is empty */
static QStringList openFilesUnder(const QString &dir)
{
   QStringList open;
   QDir fds("/proc/self/fd");
   const QString root = QFileInfo(dir).absoluteFilePath() + "/";

   if (!fds.exists())
      return open;
   const QStringList names = fds.entryList(QDir::NoDotAndDotDot
                                           | QDir::AllEntries | QDir::System);
   for (const QString &fd : names) {
      QString target = QFile::symLinkTarget(fds.filePath(fd));

      if (target.startsWith(root))
         open << target.mid(root.size());
   }
   return open;
}


static void removeContents(const QString &dirPath)
{
   QDir dir(dirPath);
   const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot
                                    | QDir::Files | QDir::Dirs | QDir::Hidden);

   for (const QFileInfo &entry : entries) {
      bool ok;

      if (entry.isDir()) {
         removeContents(entry.absoluteFilePath());
         ok = dir.rmdir(entry.fileName());
      } else {
         ok = dir.remove(entry.fileName());
      }
      if (!ok)
         qFatal("cannot remove %s left by an earlier test: is it still "
                "open?", qPrintable(entry.absoluteFilePath()));
   }
}


void Test::emptyDirectory(const QString &dirPath)
{
   if (!QDir(dirPath).exists())
      return;

   /* An earlier test left these open.  Linux removes an open file
      happily, but Windows refuses, so a file left open passes here and
      fails only on Windows CI, at the start of whichever test comes next.
      Catch it here instead, naming the files */
   QStringList open = openFilesUnder(dirPath);
   if (!open.isEmpty())
      qFatal("files left open by an earlier test: %s",
             qPrintable(open.join(", ")));

   removeContents(dirPath);
}


bool Test::touch(QString fname)
{
   QFile fil(fname);

   if (!fil.open(QIODevice::WriteOnly))
      return false;
   fil.close();

   return true;
}

QString Test::setupRepo(bool add_files)
{
   QDir dir(testSrc);

   QString dst = _tempDir->path();

   // Empty the temp directory
   QDir destDir(dst);

   emptyDirectory(dst);

   // Copy over the core test files
   for (auto &fname : {"testfile.max", "testpdf.pdf"})
       QFile::copy(testSrc + "/" + fname, dst + "/" + fname);

   // Create subdirectories which won't appear in the model
   Q_ASSERT(destDir.mkdir("wibble1"));
   Q_ASSERT(destDir.mkdir("wibble2"));
   Q_ASSERT(destDir.mkdir("wibble3"));

   // Create subdirectories
   Q_ASSERT(destDir.mkdir("main"));
   Q_ASSERT(destDir.mkdir("main/one"));
   Q_ASSERT(destDir.mkdir("main/one/a"));
   Q_ASSERT(destDir.mkdir("main/one/b"));
   Q_ASSERT(destDir.mkdir("main/two"));
   Q_ASSERT(destDir.mkdir("other"));
   Q_ASSERT(destDir.mkdir("other/three"));

   if (add_files) {
      Q_ASSERT(touch(dst + "/main/one/ofile"));
      Q_ASSERT(touch(dst + "/main/one/ofile2"));
   }

   /* paperman keeps a repository's directory canonical, so give the
      path in that form for tests to compare with; on macOS a temporary
      directory is under /var, which is a symlink to /private/var */
   return QDir(_tempDir->path()).canonicalPath();
}

QString Test::setupRepoWithExtra()
{
   // First set up the base repo
   QString path = setupRepo();

   // Then copy extra files from the extra subdirectory
   QString extraSrc = testSrc + "/extra";
   QDir extraDir(extraSrc);

   foreach (QString fname, extraDir.entryList(QDir::Files)) {
       QFile::copy(extraSrc + QDir::separator() + fname,
                   path + QDir::separator() + fname);
   }

   return path;
}

QString Test::trashFile(QString fname)
{
   return _tempDir->path() + QDir::separator() + ".maxview-trash" +
         QDir::separator() + fname;
}

QString Test::cacheFile(const QString &path)
{
   QString user = utilUserName();

   if (!user.isEmpty())
      user.prepend(".");

   return path + "/.papertree" + user;
}

Suite::Suite(QString name) :
   Test(name)
{
   suite().push_back(this);
}

std::vector<Test*>& Suite::suite()
{
    static std::vector<Test*> objects;
    return objects;
}
