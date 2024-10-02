#ifndef TEST_DIRMODEL_H
#define TEST_DIRMODEL_H

#include <QObject>

#include "suite.h"

class TestDirmodel: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testBase();
};

#endif // TEST_DIRMODEL_H
