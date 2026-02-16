#include "serverlog.h"

static QList<ServerLog::Entry> _logEntries;
static int _readPos;

void ServerLog::log(Action action, const QString &path,
                    int detail, qint64 elapsedMs)
{
    Entry entry;
    entry.action = action;
    entry.path = path;
    entry.detail = detail;
    entry.elapsedMs = elapsedMs;
    _logEntries.append(entry);
}

QList<ServerLog::Entry> ServerLog::entries()
{
    return _logEntries;
}

void ServerLog::clear()
{
    _logEntries.clear();
    _readPos = 0;
}

bool ServerLog::next(Action action, int detail)
{
    if (_readPos >= _logEntries.size())
        return false;
    const Entry &e = _logEntries[_readPos++];
    if (e.action != action)
        return false;
    if (detail >= 0 && e.detail != detail)
        return false;
    return true;
}

bool ServerLog::end()
{
    return _readPos >= _logEntries.size();
}
