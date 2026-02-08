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

void Test::emptyDirectory(const QString &dirPath)
{
   // Gemini
    QDir dir(dirPath);

    if (!dir.exists())
        return;

    // Get the list of files and directories within the directory
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot |
                                    QDir::Files | QDir::Dirs | QDir::Hidden);

    // Iterate through the entries and remove them
    foreach (const QFileInfo &entry, entries) {
        if (entry.isDir()) {
            emptyDirectory(entry.absoluteFilePath());
            Q_ASSERT(dir.rmdir(entry.fileName()));
        } else {
            Q_ASSERT(dir.remove(entry.fileName()));
        }
    }
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

   // Copy over the test files
   foreach (QString fname, dir.entryList(QDir::Files))
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

   return _tempDir->path();
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
