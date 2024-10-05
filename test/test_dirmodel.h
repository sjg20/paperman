#ifndef TEST_DIRMODEL_H
#define TEST_DIRMODEL_H

class QAbstractItemModel;
class QAbstractProxyModel;

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
   void testModel();

private:
   Dirmodel *setupModel();
   void checkModel(const QAbstractItemModel *model, const Dirmodel *dirmodel,
                   const QAbstractProxyModel *proxy);
};

#endif // TEST_DIRMODEL_H
