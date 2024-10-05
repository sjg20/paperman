#ifndef TEST_DIRMODEL_H
#define TEST_DIRMODEL_H

#include <QAbstractItemModel>
#include <QObject>

#include "suite.h"

class Dirmodel;

class TestDirmodel: public Suite
{
   Q_OBJECT
public:
    using Suite::Suite;

private slots:
   void testBase();
   void testProxy();

private:
   Dirmodel *setupModel();
   void checkModel(const QAbstractItemModel *model, const Dirmodel *dirmodel);
};

#endif // TEST_DIRMODEL_H
