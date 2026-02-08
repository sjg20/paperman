#ifndef SUITE_H
#define SUITE_H

#include <QObject>
#include <vector>

class QTemporaryDir;

// Info from here:
// https://alexhuszagh.github.io/2016/using-qttest-effectively

class Test : public QObject {
   Q_OBJECT
public:
   Test(QString name);
   ~Test();

   /**
    * @brief  Set up a test repo for use and return path
    * @param  add_files  true to add some files, false for just directories
    * @return Temporary-directory path
    */
   QString setupRepo(bool add_files = false);

   // Set up a test repo with extra files for move tests
   QString setupRepoWithExtra();

   // Get the path to a trash file given its filename
   QString trashFile(QString fname);

   // Get the path to the cache file for a top-level paper directory
   QString cacheFile(const QString &path);

   static const QString testSrc;

protected:
   /**
    * @brief Create a new, empty file
    * @param filePath  Path to create in (directory must exist)
    * @return true if OK, false on error
    */
   bool touch(QString filePath);

private:
   /**
    * @brief Remove all files and directories within a directory path
    * @param dirPath  Path to empty (this dir is not removed)
    */
   void emptyDirectory(const QString &dirPath);

public:
   QTemporaryDir *_tempDir;
   QString _name;
};

class Suite: public Test
{
public:
     Suite(QString name);

     static std::vector<Test*> & suite();
};
#endif // TESTSUITE_H
