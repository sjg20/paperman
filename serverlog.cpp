#include "serverlog.h"

void ServerLog::log(Action action, const QString &path,
                    int detail, int elapsedMs)
{
    Entry entry;
    entry.action = action;
    entry.path = path;
    entry.detail = detail;
    entry.elapsedMs = elapsedMs;
    _entries.append(entry);
}

void ServerLog::clear()
{
    _entries.clear();
    _readPos = 0;
}

bool ServerLog::next(Action action, int detail)
{
    if (_readPos >= _entries.size())
        return false;
    const Entry &e = _entries[_readPos++];
    if (e.action != action)
        return false;
    if (detail >= 0 && e.detail != detail)
        return false;
    return true;
}

bool ServerLog::end() const
{
    return _readPos >= _entries.size();
}
