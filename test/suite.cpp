#include <QTemporaryDir>

#include "suite.h"

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

QString Test::setupRepo()
{
   QDir dir(testSrc);

   QString dst = _tempDir->path();

   // Empty the temp directory
   QDir destDir(dst);
   QStringList files = destDir.entryList(QDir::Files);
   for (const QString &file : files) {
       QFile::remove(destDir.filePath(file));
   }

   foreach (QString fname, dir.entryList(QDir::Files)) {
       QFile::copy(testSrc + QDir::separator() + fname,
                   dst + QDir::separator() + fname);
   }

   return _tempDir->path();
}

QString Test::trashFile(QString fname)
{
   return _tempDir->path() + QDir::separator() + ".maxview-trash" +
         QDir::separator() + fname;
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
