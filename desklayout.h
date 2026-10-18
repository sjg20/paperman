/*
License: GPL-2
*/

#ifndef DESKLAYOUT_H
#define DESKLAYOUT_H

#include <QByteArray>
#include <QList>
#include <QPoint>
#include <QString>
#include <QStringList>


/**
 * The layout of the stacks in a folder, as kept in its .paperdesk file:
 * where each stack is on the desk and what a desktop has noted about it.
 *
 * This lets the server keep a folder's layout up to date as stacks come
 * and go, without the desktop's classes.  It places a new stack after
 * the others the way a desktop does, though without the desktop's fonts
 * it can only estimate how wide a stack's name is.  What a desktop has
 * noted about a stack beyond its position is kept as it is.
 */
class DeskLayout
{
public:
    /** the layout of the folder @p dir, empty until read */
    explicit DeskLayout(const QString &dir);

    /** the name of the file holding a folder's layout */
    static const char *fileName();

    /** read the layout from the folder; false if it has none */
    bool read();

    /**
     * Write the layout to the folder, leaving out any stack which is no
     * longer there.  Returns false if it cannot be written.
     */
    bool write();

    /** take the layout from the contents of a .paperdesk file */
    void parse(const QByteArray &bytes);

    /** the contents of a .paperdesk file holding this layout */
    QByteArray toBytes() const;

    /**
     * Take the stacks in an uploaded .paperdesk file, @p bytes, keeping
     * those it does not mention, such as a stack which arrived after
     * whoever uploaded it read the layout
     */
    void merge(const QByteArray &bytes);

    bool contains(const QString &name) const;

    /** where the stack @p name is; false if the layout lacks it */
    bool pos(const QString &name, QPoint *pos) const;

    /** put the stack @p name at @p pos, adding it if needed */
    void setPos(const QString &name, const QPoint &pos);

    /** place the stack @p name after the others, if not there already */
    void place(const QString &name);

    /** place each of @p names after the others, in turn */
    void placeAll(const QStringList &names);

    /** keep the stack @p from where it is, now called @p to */
    void rename(const QString &from, const QString &to);

    void remove(const QString &name);

    /** the stacks, in the order the file lists them */
    QStringList names() const;

    /** how wide the name of a stack is likely to be on a desktop */
    static int nameWidth(const QString &name);

private:
    struct Entry
    {
        QString name;
        QPoint pos;
        QString rest;   //!< what follows the position: page, sizes
    };

    int indexOf(const QString &name) const;

    /** where a stack goes after the others, as Desk::advance() */
    QPoint nextPos() const;

    bool clashes(const QPoint &pos) const;

    QString _dir;
    QString _header;    //!< what comes before the list of stacks
    QList<Entry> _entries;
};

#endif
