#include "serverlog.h"

static QList<ServerLog::Entry> _logEntries;

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
}
