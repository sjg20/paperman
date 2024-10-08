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

   // Set up a test repo for use and return path
   QString setupRepo();

   // Get the path to a trash file given its filename
   QString trashFile(QString fname);

   static const QString testSrc;

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
