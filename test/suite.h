#ifndef SUITE_H
#define SUITE_H

#include <QObject>
#include <vector>

// Info from here:
// https://alexhuszagh.github.io/2016/using-qttest-effectively

class Test : public QObject {
   Q_OBJECT
public:
   Test(QString name);

public:
   QString _name;
};

class Suite: public Test
{
public:
     Suite(QString name);

     static std::vector<Test*> & suite();
};
#endif // TESTSUITE_H
