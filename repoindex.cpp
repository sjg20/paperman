/*
License: GPL-2
*/

#include "repoindex.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QSet>

#include "err.h"


RepoIndex::RepoIndex(const QString &repoPath, const QString &indexPath,
                     QObject *parent)
    : QObject(parent),
      _repoPath(QDir(repoPath).absolutePath()),
      _indexPath(indexPath)
{
    _rescanTimer.setSingleShot(true);
    connect(&_rescanTimer, &QTimer::timeout, this, &RepoIndex::syncAll);
}


RepoIndex::~RepoIndex()
{
    stop();
}


bool RepoIndex::start(QString *error)
{
    if (_running)
        return true;

    QDir().mkpath(QFileInfo(_indexPath).absolutePath());

    /* open the index here first, so that it is made, or made afresh if
       it is out of date, before the other thread opens it */
    err_info *err = _search.init(_repoPath, _indexPath);
    if (err) {
        if (error)
            *error = err->errstr;
        return false;
    }

    _stop = false;
    _worker = new QObject;
    _worker->moveToThread(&_thread);
    _thread.start(QThread::LowPriority);
    QMetaObject::invokeMethod(_worker, [this]() {
        _writer = new SearchIndex;
        err_info *err = _writer->init(_repoPath, _indexPath);

        if (err) {
            qWarning() << "RepoIndex: cannot open" << _indexPath << ":"
                       << err->errstr;
            delete _writer;
            _writer = nullptr;
        }
    }, Qt::QueuedConnection);
    _running = true;
    syncAll();
    return true;
}


void RepoIndex::stop()
{
    if (!_running)
        return;
    _running = false;
    _stop = true;
    _rescanTimer.stop();

    /* the index is closed on the thread which opened it, once it has
       stopped what it is doing */
    QMetaObject::invokeMethod(_worker, [this]() {
        delete _writer;
        _writer = nullptr;
    }, Qt::BlockingQueuedConnection);
    _thread.quit();
    _thread.wait();
    delete _worker;
    _worker = nullptr;
    _search.close();
}


void RepoIndex::syncAll()
{
    if (!_running || _syncing)
        return;
    _syncing = true;
    _done = 0;
    _total = 0;
    QMetaObject::invokeMethod(_worker, [this]() {
        if (_writer) {
            err_info *err = _writer->sync(_repoPath, [this](int done,
                                                            int total) {
                _done = done;
                _total = total;
                return !_stop;
            });
            if (err)
                qWarning() << "RepoIndex: cannot index" << _repoPath << ":"
                           << err->errstr;
        }
        bool finished = !_stop;

        _syncing = false;
        if (finished) {
            _ready = true;
            QMetaObject::invokeMethod(this, [this]() {
                if (_running && _rescanMs > 0)
                    _rescanTimer.start(_rescanMs);
                emit synced();
            }, Qt::QueuedConnection);
        }
    }, Qt::QueuedConnection);
}


void RepoIndex::stackChanged(const QString &relPath)
{
    QString suffix = QFileInfo(relPath).suffix().toLower();

    if (!_running || (suffix != "max" && suffix != "pdf"))
        return;

    // the index leaves out the trash and anything else hidden
    for (const QString &part : relPath.split('/'))
        if (part.isEmpty() || part.startsWith('.'))
            return;

    QString path = _repoPath + "/" + relPath;

    QMetaObject::invokeMethod(_worker, [this, path]() {
        if (_writer && !_stop) {
            err_info *err = _writer->syncStack(path);

            if (err)
                qWarning() << "RepoIndex: cannot index" << path << ":"
                           << err->errstr;
        }
    }, Qt::QueuedConnection);
}


bool RepoIndex::search(const QString &text, const QString &underDir,
                       int maxHits, QList<Hit> &hits, QString *error)
{
    hits.clear();

    QString query = SearchIndex::matchQuery(text);
    if (query.isEmpty())
        return true;

    QString dir = underDir.isEmpty() ? _repoPath
                                     : _repoPath + "/" + underDir;
    QList<SearchResult> results;

    /* each page found is a result, so ask for plenty to have enough
       stacks */
    err_info *err = _search.search(query, results, qMax(1000, maxHits * 10),
                                   dir);
    if (err) {
        if (error)
            *error = err->errstr;
        return false;
    }

    // each stack once, at its best page
    QSet<QString> seen;
    int prefix = _repoPath.size() + 1;

    for (const SearchResult &res : results) {
        if (hits.size() >= maxHits)
            break;
        if (seen.contains(res.filepath))
            continue;
        seen.insert(res.filepath);

        Hit hit;
        hit.path = res.filepath.mid(prefix);
        hit.page = res.pagenum;
        hit.snippet = res.snippet;
        hits << hit;
    }
    return true;
}


RepoIndex::Status RepoIndex::status() const
{
    Status st;

    st.syncing = _syncing;
    st.ready = _ready;
    st.done = _done;
    st.total = _total;
    return st;
}
