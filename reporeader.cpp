/*
License: GPL-2
*/

#include "reporeader.h"

#include <algorithm>

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSqlError>
#include <QSqlQuery>
#include <QtConcurrent>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "err.h"
#include "file.h"
#include "filemax.h"
#include "utils.h"


RepoReader::RepoReader(const QString &repoPath, const QString &recordPath,
                       int workers, QObject *parent)
    : QObject(parent),
      _repoPath(QDir(repoPath).absolutePath()),
      _recordPath(recordPath),
      _workers(qMax(1, workers))
{
    static int count;

    _connection = QString("reporeader-%1").arg(++count);
    _pool.setMaxThreadCount(_workers);
    _rescanTimer.setSingleShot(true);
    connect(&_rescanTimer, &QTimer::timeout, this, [this]() {
        if (_running && isIdle())
            startScan();
    });
}


RepoReader::~RepoReader()
{
    stop();
    if (_db.isOpen())
        _db.close();
    _db = QSqlDatabase();
    QSqlDatabase::removeDatabase(_connection);
}


RepoReader::Stamp RepoReader::stampOf(const QString &path)
{
    QFileInfo fi(path);
    Stamp stamp;

    if (fi.exists()) {
        stamp.mtime = fi.lastModified().toMSecsSinceEpoch();
        stamp.size = fi.size();
    }
    return stamp;
}


/* Hidden folders, the trash among them, are not part of the repository's
   contents, and nor are hidden files, such as a stack being written */
static bool isHidden(const QString &relPath)
{
    const QStringList parts = relPath.split('/', Qt::SkipEmptyParts);

    for (const QString &part : parts)
        if (part.startsWith('.'))
            return true;
    return false;
}


QList<RepoReader::Found> RepoReader::scan(const QString &repoPath,
                                          const std::atomic<bool> *stop)
{
    QList<Found> found;
    QDirIterator it(repoPath, QStringList() << "*.max", QDir::Files,
                    QDirIterator::Subdirectories);
    const int skip = repoPath.size() + 1;

    while (it.hasNext() && !*stop) {
        QString path = it.next();
        QString rel = path.mid(skip);

        if (isHidden(rel))
            continue;

        QFileInfo fi = it.fileInfo();
        Found f;

        f.relPath = rel;
        f.stamp.mtime = fi.lastModified().toMSecsSinceEpoch();
        f.stamp.size = fi.size();
        found << f;
    }

    // the newest first: those are the ones most likely to be looked for
    std::stable_sort(found.begin(), found.end(),
                     [](const Found &a, const Found &b) {
                         return a.stamp.mtime > b.stamp.mtime;
                     });
    return found;
}


RepoReader::Result RepoReader::readStack(const QString &repoPath,
                                         const QString &relPath,
                                         Engine engine,
                                         const std::atomic<bool> *stop)
{
    Result result;
    QFileInfo fi(repoPath + "/" + relPath);

    result.relPath = relPath;
    result.stamp = stampOf(fi.filePath());

    File *file = File::createFile(fi.absolutePath() + "/", fi.fileName(),
                                  nullptr, File::Type_max);
    err_info *err = file ? file->load() : nullptr;

    if (!file || err) {
        result.error = err ? err->errstr : QString("cannot open the stack");
        delete file;
        return result;
    }

    for (int pagenum = 0; pagenum < file->pagecount(); pagenum++) {
        if (*stop) {
            result.cancelled = true;
            break;
        }

        // a page read already, or a stack which cannot keep what is read
        OcrPage have;
        if (file->getPageOcr(pagenum, have) || !have.isEmpty())
            continue;

        QImage image;
        QSize size, trueSize;
        int bpp;

        if (file->getImage(pagenum, false, image, size, trueSize, bpp, false)
            || image.isNull())
            continue;

        OcrPage page;
        QString error = engine(image, page);

        result.pagesRead++;
        if (!error.isEmpty()) {
            result.error = error;
            break;
        }
        if (!page.isEmpty())
            result.pages << qMakePair(pagenum, page);
    }
    delete file;
    return result;
}


bool RepoReader::openRecord(QString *error)
{
    if (_db.isOpen())
        return true;

    QDir().mkpath(QFileInfo(_recordPath).path());
    _db = QSqlDatabase::addDatabase("QSQLITE", _connection);
    _db.setDatabaseName(_recordPath);
    if (!_db.open()) {
        if (error)
            *error = _db.lastError().text();
        return false;
    }

    QSqlQuery query(_db);
    if (!query.exec("CREATE TABLE IF NOT EXISTS read ("
                    "  path TEXT PRIMARY KEY, mtime INTEGER, size INTEGER)")) {
        if (error)
            *error = query.lastError().text();
        return false;
    }

    // held here as well, since a pass looks up every stack in the repository
    _records.clear();
    query.exec("SELECT path, mtime, size FROM read");
    while (query.next()) {
        Stamp stamp;

        stamp.mtime = query.value(1).toLongLong();
        stamp.size = query.value(2).toLongLong();
        _records.insert(query.value(0).toString(), stamp);
    }
    return true;
}


bool RepoReader::isRecorded(const QString &relPath, const Stamp &stamp)
{
    auto it = _records.constFind(relPath);

    return it != _records.constEnd() && *it == stamp;
}


void RepoReader::record(const QString &relPath, const Stamp &stamp)
{
    QSqlQuery query(_db);

    query.prepare("INSERT OR REPLACE INTO read (path, mtime, size) "
                  "VALUES (?, ?, ?)");
    query.addBindValue(relPath);
    query.addBindValue(stamp.mtime);
    query.addBindValue(stamp.size);
    if (!query.exec())
        qWarning() << "RepoReader: cannot record" << relPath << ":"
                   << query.lastError().text();
    _records.insert(relPath, stamp);
}


bool RepoReader::start(QString *error)
{
    if (_running)
        return true;
    if (!openRecord(error))
        return false;

    if (!_engine) {
        _tesseract.setBackground(true);
        _engine = [this](QImage &image, OcrPage &page) {
            err_info *err = _tesseract.imageToPage(image, page);

            return err ? QString(err->errstr) : QString();
        };
    }
    _stop = false;
    _running = true;
    startScan();
    return true;
}


void RepoReader::stop()
{
    if (!_running)
        return;
    _stop = true;
    _running = false;
    _rescanTimer.stop();

    // the pages being read finish; their results are thrown away
    _pool.waitForDone();
    if (_scanWatcher)
        _scanWatcher->waitForFinished();
}


void RepoReader::startScan()
{
    if (_scanWatcher || _stop)
        return;

    _status.scanning = true;
    _scanWatcher = new QFutureWatcher<QList<Found>>(this);
    connect(_scanWatcher, &QFutureWatcher<QList<Found>>::finished,
            this, &RepoReader::scanDone);

    QString repo = _repoPath;
    const std::atomic<bool> *stop = &_stop;
    _scanWatcher->setFuture(QtConcurrent::run([repo, stop]() {
        return scan(repo, stop);
    }));
}


void RepoReader::scanDone()
{
    QList<Found> found = _scanWatcher->result();

    _scanWatcher->deleteLater();
    _scanWatcher = nullptr;
    _status.scanning = false;
    if (_stop)
        return;

    for (const Found &f : found)
        if (!isRecorded(f.relPath, f.stamp) && !_queued.contains(f.relPath)) {
            _queue << f.relPath;
            _queued.insert(f.relPath);
        }
    dispatch();
}


void RepoReader::stackChanged(const QString &relPath)
{
    QString rel = QDir::cleanPath(relPath);

    if (!_running || rel.isEmpty() || isHidden(rel)
        || !rel.endsWith(".max", Qt::CaseInsensitive))
        return;

    _queue.removeAll(rel);
    _queue.prepend(rel);
    _queued.insert(rel);
    dispatch();
}


void RepoReader::dispatch()
{
    while (!_stop && _inFlight.size() < _workers && !_queue.isEmpty()) {
        QString rel = _queue.takeFirst();

        _queued.remove(rel);
        if (_inFlight.contains(rel))
            continue;   // it is read again if it changed meanwhile

        Stamp stamp = stampOf(_repoPath + "/" + rel);
        if (stamp.size < 0 || isRecorded(rel, stamp))
            continue;   // gone, or read since it was queued

        auto *watcher = new QFutureWatcher<Result>(this);
        connect(watcher, &QFutureWatcher<Result>::finished, this,
                [this, watcher]() { readDone(watcher); });

        QString repo = _repoPath;
        Engine engine = _engine;
        const std::atomic<bool> *stop = &_stop;
        _inFlight.insert(rel, true);
        watcher->setFuture(QtConcurrent::run(&_pool,
            [repo, rel, engine, stop]() {
                return readStack(repo, rel, engine, stop);
            }));
    }

    _status.pending = _queue.size() + _inFlight.size();
    if (_running && isIdle()) {
        if (_rescanMs > 0)
            _rescanTimer.start(_rescanMs);
        emit idle();
    }
}


void RepoReader::readDone(QFutureWatcher<Result> *watcher)
{
    Result result = watcher->result();

    watcher->deleteLater();
    _inFlight.remove(result.relPath);
    if (_stop || result.cancelled)
        return;

    QString path = _repoPath + "/" + result.relPath;
    Stamp now = stampOf(path);

    _status.stacks++;
    _status.pages += result.pagesRead;

    if (now.size < 0) {
        ;   // the stack has gone
    } else if (now != result.stamp) {
        // it changed while being read: read it again as it is now
        if (!_queued.contains(result.relPath)) {
            _queue << result.relPath;
            _queued.insert(result.relPath);
        }
    } else if (!result.pages.isEmpty()) {
        bool changed = false;
        QString error;

        if (writeWords(result, &changed, &error)) {
            record(result.relPath, stampOf(path));
            _status.written++;
            emit stackRead(result.relPath, result.pages.size());
        } else if (changed) {
            if (!_queued.contains(result.relPath)) {
                _queue << result.relPath;
                _queued.insert(result.relPath);
            }
        } else {
            // looked at again on the next pass
            qWarning() << "RepoReader: cannot keep what was read from"
                       << path << ":" << error;
        }
    } else {
        /* nothing on it to read, or it cannot be read: leave it until it
           changes */
        if (!result.error.isEmpty())
            qInfo() << "RepoReader: cannot read" << path << ":"
                    << result.error;
        record(result.relPath, now);
    }
    dispatch();
}


bool RepoReader::writeWords(const Result &result, bool *changed,
                            QString *error)
{
    QFileInfo fi(_repoPath + "/" + result.relPath);
    QString dir = fi.absolutePath() + "/";
    QString path = fi.absoluteFilePath();
    QString tmpName = "." + fi.fileName() + ".reading";
    QString tmp = dir + tmpName;

    *changed = false;
    QFile::remove(tmp);
    if (!QFile::copy(path, tmp)) {
        *error = "cannot copy the stack";
        return false;
    }

#ifndef _WIN32
    /* the copy is the stack from now on, so it keeps the stack's
       permissions and, where the server may set it, its group */
    struct stat st;
    if (!::stat(QFile::encodeName(path).constData(), &st)) {
        QByteArray name = QFile::encodeName(tmp);

        ::chmod(name.constData(), st.st_mode & 07777);

        /* this fails for a group the server's user is not in, leaving
           the copy in the user's own group */
        (void)!::chown(name.constData(), (uid_t)-1, st.st_gid);
    }
#else
    QFile::setPermissions(tmp, QFile::permissions(path));
#endif

    err_info *err = nullptr;
    {
        Filemax max(dir, tmpName, nullptr);

        err = max.load();
        for (int i = 0; !err && i < result.pages.size(); i++)
            if (result.pages[i].first < max.pagecount())
                err = max.putPageOcr(result.pages[i].first,
                                     result.pages[i].second);
        if (!err)
            err = max.flush();
    }
    if (err) {
        *error = err->errstr;
        QFile::remove(tmp);
        return false;
    }

    // the last moment to notice a change made while the copy was written
    if (stampOf(path) != result.stamp) {
        QFile::remove(tmp);
        *changed = true;
        return false;
    }
    if (!utilReplaceFile(tmp, path, error)) {
        QFile::remove(tmp);
        return false;
    }
    return true;
}


RepoReader::Status RepoReader::status() const
{
    return _status;
}


bool RepoReader::isIdle() const
{
    return !_scanWatcher && _queue.isEmpty() && _inFlight.isEmpty();
}
