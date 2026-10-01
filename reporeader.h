/*
License: GPL-2
*/

#ifndef REPOREADER_H
#define REPOREADER_H

#include <atomic>
#include <functional>

#include <QHash>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QThreadPool>
#include <QTimer>

#include "ocrpage.h"
#include "ocrtess.h"

template <typename T> class QFutureWatcher;


/**
 * Reads the pages of every stack in a repository in the background, as
 * the server's OCR, and keeps the words with each page.
 *
 * A pass looks through the repository (but not the trash or any hidden
 * folder) for max stacks which have changed since they were last read,
 * newest first, and reads each one's unread pages on one of a few worker
 * threads.  The words are written on this object's thread, which in the
 * server is the one that makes every other change to the stacks: the
 * stack is copied, the words put into the copy, and the copy put in
 * place of the stack in one step, so a stack is never left half-written.
 * A stack which changed while it was being read is left as it is and
 * read again.
 *
 * Which stacks have been read is kept in a small database (not in the
 * repository, which may be on a network filesystem), so that a stack
 * with no words in it, or one which cannot be read, is not read again
 * until it changes, and a restart carries on where it left off.
 */
class RepoReader : public QObject
{
    Q_OBJECT

public:
    /** reads an image, on a worker thread; returns an error message, or
     *  an empty string if ok */
    typedef std::function<QString (QImage &image, OcrPage &page)> Engine;

    struct Status
    {
        bool scanning = false;  //!< looking for stacks to read
        int pending = 0;        //!< stacks waiting or being read
        int stacks = 0;         //!< stacks read since starting
        int pages = 0;          //!< pages read since starting
        int written = 0;        //!< stacks given words since starting
    };

    /**
     * @param repoPath    the repository's top directory
     * @param recordPath  the database recording which stacks are read
     * @param workers     how many stacks to read at once
     */
    RepoReader(const QString &repoPath, const QString &recordPath,
               int workers, QObject *parent = nullptr);
    ~RepoReader() override;

    /** Use this engine rather than tesseract, e.g. for testing */
    void setEngine(const Engine &engine) { _engine = engine; }

    /** How long to wait after reading everything before looking through
     *  the repository again; 0 never looks again on its own */
    void setRescanInterval(int ms) { _rescanMs = ms; }

    /** Start reading: look through the repository and read what has
     *  changed */
    bool start(QString *error = nullptr);

    /** Stop: the pages being read are finished, but nothing more is read
     *  and nothing more written */
    void stop();

    /** A stack (repo-relative path) has changed or arrived: read it
     *  before the others */
    void stackChanged(const QString &relPath);

    Status status() const;

    /** true when there is nothing to read and nothing being read */
    bool isIdle() const;

signals:
    /** what was read from @p pages pages of a stack has been put in it */
    void stackRead(const QString &relPath, int pages);

    /** there is nothing more to read, for now */
    void idle();

private:
    /** a stack's size and time of change, which tell when it changes */
    struct Stamp
    {
        qint64 mtime = 0;
        qint64 size = -1;

        bool operator==(const Stamp &o) const
            { return mtime == o.mtime && size == o.size; }
        bool operator!=(const Stamp &o) const { return !(*this == o); }
    };

    struct Found
    {
        QString relPath;
        Stamp stamp;
    };

    struct Result
    {
        QString relPath;
        Stamp stamp;                       //!< as it was when read
        QList<QPair<int, OcrPage>> pages;  //!< what was read, by page
        int pagesRead = 0;
        QString error;
        bool cancelled = false;
    };

    static Stamp stampOf(const QString &path);
    static QList<Found> scan(const QString &repoPath,
                             const std::atomic<bool> *stop);
    static Result readStack(const QString &repoPath, const QString &relPath,
                            Engine engine, const std::atomic<bool> *stop);

    bool openRecord(QString *error);
    bool isRecorded(const QString &relPath, const Stamp &stamp);
    void record(const QString &relPath, const Stamp &stamp);

    void startScan();
    void scanDone();
    void dispatch();
    void readDone(QFutureWatcher<Result> *watcher);

    /** put the words into a copy of the stack and the copy in its place;
     *  returns false with @p changed set if the stack changed first */
    bool writeWords(const Result &result, bool *changed, QString *error);

    QString _repoPath;
    QString _recordPath;
    QString _connection;
    QSqlDatabase _db;
    int _workers;
    int _rescanMs = 15 * 60 * 1000;
    Engine _engine;
    Ocrtess _tesseract;
    QThreadPool _pool;
    std::atomic<bool> _stop{false};
    bool _running = false;
    QFutureWatcher<QList<Found>> *_scanWatcher = nullptr;
    QTimer _rescanTimer;
    QHash<QString, Stamp> _records;      //!< stacks read, as recorded
    QList<QString> _queue;               //!< stacks to read, in order
    QSet<QString> _queued;               //!< the same, to look up
    QHash<QString, bool> _inFlight;      //!< stacks being read
    Status _status;
};

#endif // REPOREADER_H
