
#include "qlistwidgetitemiterator.h"

#include <QListWidget>

QListWidgetItemIterator::QListWidgetItemIterator(QListWidget *w)
{
    mWidget = w;
    mUpto = -1;
    next();
}

QListWidgetItemIterator &QListWidgetItemIterator::operator++(void)
{
    next();
    return *this;
}

QListWidgetItem *QListWidgetItemIterator::current(void)
{
    return mCur;
}

void QListWidgetItemIterator::next(void)
{
    mUpto++;
    if (mUpto >= 0 && mUpto < mWidget->count())
        mCur = mWidget->item(mUpto);
    else
        mCur = 0;
}

