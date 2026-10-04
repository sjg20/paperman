/*
License: GPL-2
*/

#ifndef REPOINDEX_H
#define REPOINDEX_H

#include <atomic>

#include <QList>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>

#include "searchindex.h"


/**
 * Keeps an index of the text on the pages of every stack in a repository,
 * for the server to search.
 *
 * The index is kept on this computer's own disk, not in the repository,
 * which may be on a network filesystem.  It is brought up to date on a
 * thread of its own: the whole repository when started and every so
 * often after that, and a stack at a time as stacks change.  Searches
 * are made on the thread which owns this object, on a connection of
 * their own, and see what the other thread has done so far, so the index
 * can be searched while it is first being built, which for a large
 * repository takes some hours.
 */
class RepoIndex : public QObject
{
    Q_OBJECT

public:
    /** a stack which has the words searched for */
    struct Hit
    {
        QString path;       //!< repo-relative path of the stack
        int page = 0;       //!< the page which matches best
        QString snippet;    //!< the words around the match on that page
    };

    struct Status
    {
        bool syncing = false;   //!< looking through the repository
        bool ready = false;     //!< has looked through it all at least once
        int done = 0;           //!< stacks looked at in this look through
        int total = 0;          //!< stacks to look at
    };

    /**
     * @param repoPath   the repository's top directory
     * @param indexPath  the index database, on this computer's own disk
     */
    RepoIndex(const QString &repoPath, const QString &indexPath,
              QObject *parent = nullptr);
    ~RepoIndex() override;

    /** How long to wait after looking through the repository before
     *  looking again; 0 never looks again on its own */
    void setRescanInterval(int ms) { _rescanMs = ms; }

    /** Open the index and start bringing it up to date */
    bool start(QString *error = nullptr);

    /** Stop: what is being done is abandoned, though what is done is
     *  kept */
    void stop();

    /** A stack (repo-relative path) has changed, arrived or gone */
    void stackChanged(const QString &relPath);

    /**
     * Search for stacks with text on a page, best first
     *
     * @param text      words, as typed by the user (see
     *                  SearchIndex::matchQuery())
     * @param underDir  repo-relative directory to search in, or empty for
     *                  the whole repository
     * @param maxHits   most stacks to return
     * @param hits      returns the stacks found
     * @param error     returns an error message
     * @return true if ok
     */
    bool search(const QString &text, const QString &underDir, int maxHits,
                QList<Hit> &hits, QString *error = nullptr);

    Status status() const;

signals:
    /** a look through the whole repository has finished */
    void synced();

private:
    void syncAll();

    QString _repoPath;
    QString _indexPath;
    SearchIndex _search;            //!< for searching, on this thread
    QThread _thread;
    QObject *_worker = nullptr;     //!< lives on _thread, for its work
    SearchIndex *_writer = nullptr; //!< used only on _thread
    std::atomic<bool> _stop{false};
    std::atomic<bool> _syncing{false};
    std::atomic<bool> _ready{false};
    std::atomic<int> _done{0};
    std::atomic<int> _total{0};
    bool _running = false;
    int _rescanMs = 15 * 60 * 1000;
    QTimer _rescanTimer;
};

#endif // REPOINDEX_H
