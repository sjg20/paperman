#include "suite.h"

Test::Test(QString name) :
   QObject(), _name(name)
{
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
