/*
License: GPL-2
*/

#include "desklayout.h"

#include <QDir>
#include <QFile>
#include <QRect>
#include <QSaveFile>
#include <QSet>

/* how a desktop places stacks; see the POS_... values in desk.cpp */
enum {
    kLeftMargin = 25,       // left-most position for new stacks
    kTopMargin = 35,        // top-most position for new stacks
    kXmargin = 20,          // horizontal space between stacks
    kXstep = 140,           // distance between stacks across
    kYstep = 250,           // distance between stacks down
    kRightMargin = 950,     // a row ends here
    kCharWidth = 7,         // how wide a character of a name is, roughly
};


DeskLayout::DeskLayout(const QString &dir)
    : _dir(dir)
{
}


const char *DeskLayout::fileName()
{
    return ".paperdesk";
}


bool DeskLayout::read()
{
    QFile file(QDir(_dir).filePath(fileName()));

    _header.clear();
    _entries.clear();
    if (!file.open(QIODevice::ReadOnly))
        return false;
    parse(file.readAll());
    return true;
}


bool DeskLayout::write()
{
    // what a desktop would not show, as its file is not there
    QDir dir(_dir);
    const QStringList present = dir.entryList(QDir::Files);
    const QSet<QString> there(present.begin(), present.end());

    for (int i = _entries.size() - 1; i >= 0; i--)
        if (!there.contains(_entries[i].name))
            _entries.removeAt(i);

    QSaveFile out(dir.filePath(fileName()));

    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(toBytes());
    return out.commit();
}


void DeskLayout::parse(const QByteArray &bytes)
{
    const QStringList lines = QString::fromUtf8(bytes).split('\n');
    bool files = false;
    bool header = true;

    _header.clear();
    _entries.clear();
    for (const QString &raw : lines) {
        QString line = raw;

        if (line.endsWith('\r'))
            line.chop(1);
        if (!line.isEmpty() && line[0] == '[') {
            files = line == "[Files]";
            if (files)
                header = false;
            else if (header)
                _header += line + '\n';
            continue;
        }
        if (!files) {
            if (header)
                _header += line + '\n';
            continue;
        }

        // as Desk::readDesk() and File::decodeFile()
        int eq = line.indexOf('=');
        if (eq == -1)
            continue;
        QStringList args = line.mid(eq + 1).split(',');
        if (args.size() < 3)
            continue;

        Entry e;
        e.name = line.left(eq);
        e.pos = QPoint(args[0].toInt(), args[1].toInt());
        e.rest = QStringList(args.mid(2)).join(',');
        if (indexOf(e.name) == -1)
            _entries << e;
    }
}


QByteArray DeskLayout::toBytes() const
{
    // as Desk::writeDesk()
    QString out = _header;

    if (out.isEmpty())
        out = "[DesktopFile]\nFile=\n\n[Folder]\nVersion=0x00060000\n\n";
    out += "[Files]\n";
    for (const Entry &e : _entries)
        out += e.name + '=' + QString::number(e.pos.x()) + ','
               + QString::number(e.pos.y()) + ',' + e.rest + '\n';
    return out.toUtf8();
}


void DeskLayout::merge(const QByteArray &bytes)
{
    DeskLayout upload(_dir);

    upload.parse(bytes);
    for (const Entry &e : _entries)
        if (upload.indexOf(e.name) == -1)
            upload._entries << e;
    if (!upload._header.isEmpty())
        _header = upload._header;
    _entries = upload._entries;
}


int DeskLayout::indexOf(const QString &name) const
{
    for (int i = 0; i < _entries.size(); i++)
        if (_entries[i].name == name)
            return i;
    return -1;
}


bool DeskLayout::contains(const QString &name) const
{
    return indexOf(name) != -1;
}


bool DeskLayout::pos(const QString &name, QPoint *pos) const
{
    int i = indexOf(name);

    if (i == -1)
        return false;
    *pos = _entries[i].pos;
    return true;
}


void DeskLayout::setPos(const QString &name, const QPoint &pos)
{
    int i = indexOf(name);

    if (i == -1) {
        // no page is shown and the sizes are left to the desktop
        _entries << Entry{name, pos, "0"};
        return;
    }
    _entries[i].pos = pos;
}


/* as Desk::advanceOne() */
static void advanceOne(QPoint &pos)
{
    pos.rx() += kXstep;
    if (pos.x() >= kRightMargin)
        pos = QPoint(kLeftMargin, pos.y() + kYstep);
}


bool DeskLayout::clashes(const QPoint &pos) const
{
    QRect base(pos, QSize(kXstep, kYstep));

    for (const Entry &e : _entries)
        if (base.intersects(QRect(e.pos, base.size())))
            return true;
    return false;
}


QPoint DeskLayout::nextPos() const
{
    QPoint pos(kLeftMargin, kTopMargin);

    // level with the bottom row, past its right-most stack
    for (const Entry &e : _entries) {
        if (e.pos.y() < pos.y())
            continue;
        if (e.pos.y() > pos.y())
            pos = QPoint(kLeftMargin, e.pos.y());
        while (pos.y() == e.pos.y() && pos.x() < e.pos.x())
            advanceOne(pos);
    }

    // and clear of the others
    for (int i = 0; i < 1000 && clashes(pos); i++)
        advanceOne(pos);
    return pos;
}


void DeskLayout::place(const QString &name)
{
    placeAll(QStringList() << name);
}


void DeskLayout::placeAll(const QStringList &names)
{
    // as Desk::addFile() does for each, after Desk::readDesk()
    QPoint pos = nextPos();

    for (const QString &name : names) {
        if (contains(name))
            continue;

        int width = nameWidth(name);

        if (pos.x() + width >= kRightMargin)
            pos = QPoint(kLeftMargin, pos.y() + kYstep);
        setPos(name, pos);
        pos.rx() += qMax(width, int(kXstep)) + kXmargin;
        if (pos.x() >= kRightMargin)
            pos = QPoint(kLeftMargin, pos.y() + kYstep);
    }
}


void DeskLayout::rename(const QString &from, const QString &to)
{
    if (!contains(from) || from == to)
        return;
    remove(to);
    _entries[indexOf(from)].name = to;
}


void DeskLayout::remove(const QString &name)
{
    int i = indexOf(name);

    if (i != -1)
        _entries.removeAt(i);
}


QStringList DeskLayout::names() const
{
    QStringList out;

    for (const Entry &e : _entries)
        out << e.name;
    return out;
}


int DeskLayout::nameWidth(const QString &name)
{
    return name.size() * kCharWidth;
}
