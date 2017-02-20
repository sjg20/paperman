#ifndef QLISTWIDGETITEMITERATOR_H
#define QLISTWIDGETITEMITERATOR_H

class QListWidget;
class QListWidgetItem;

class QListWidgetItemIterator {
public:
    QListWidgetItemIterator(QListWidget *w);
    QListWidgetItemIterator &operator++(void);
    QListWidgetItem *current(void);
private:
    void next(void);

    int mUpto;
    QListWidget *mWidget;
    QListWidgetItem *mCur;
};

#endif // QLISTWIDGETITEMITERATOR_H
