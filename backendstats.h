/*
License: GPL-2
*/

#ifndef BACKENDSTATS_H
#define BACKENDSTATS_H

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>


/**
 * Lightweight aggregator for network activity across one or more
 * RemoteBackend instances, so that the user can see the state of the
 * connection to the server: what is under way and for how long, how
 * much is still to come, how quickly the server answers, and whether
 * anything has failed.  The @c changed() signal fires after every
 * update so a status widget can rebuild its display.
 *
 * The bytesSent/bytesReceived counters approximate body sizes —
 * HTTP request/response headers are not counted.  Good enough for
 * a user-facing "how much data have I pulled" indicator.
 */
class BackendStats : public QObject
{
    Q_OBJECT

public:
    //! what a request is for
    enum Kind {
        Listing,    //!< a folder's contents, or the repositories
        Thumbnail,  //!< a small image of a stack or a page
        Page,       //!< a stack's contents, or a page of it
        Upload,     //!< a file sent to the server
        Change,     //!< an operation which changes a stack
        Other,      //!< anything else, such as a search
        KindCount
    };

    explicit BackendStats(QObject *parent = nullptr) : QObject(parent) {}

    qint64 bytesSent() const     { return _sent; }
    qint64 bytesReceived() const { return _received; }

    //! the number of requests under way
    int activeRequests() const   { return _requests.size(); }

    //! the number of requests of @p kind under way
    int activeRequests(Kind kind) const;

    /** the bytes still to come for the requests under way whose replies
     *  have said how long they are */
    qint64 bytesToCome() const;

    /** the number of requests under way whose replies have yet to say how
     *  long they are, so are not in bytesToCome() */
    int lengthsUnknown() const;

    //! how long the oldest request under way has waited, or 0 if none
    qint64 longestWaitMs() const;

    /** how long the server takes to start answering, on average over the
     *  last few requests, or -1 before any */
    qint64 responseMs() const;

    //! the requests which have failed, since the counters were reset
    int failures() const         { return _failures; }

    //! what went wrong with the last request to fail
    QString lastError() const    { return _lastError; }

    /** true if a request has failed since the server last answered one:
     *  the server is not answering, for now */
    bool failing() const         { return _failing; }

    //! the number of event streams which are connected
    int eventStreams() const;

    //! the number of event streams wanted but not connected
    int eventStreamsDown() const;

    /** URL of the paperman-server the user pointed paperman at,
     *  shown next to the byte counters.  Empty when no remote
     *  server is configured. */
    QString url() const          { return _url; }
    void setUrl(const QString &url);

    /** Reset the counters to zero, leaving what is under way.  Useful
     *  for tests and for an "Reset stats" UI action. */
    void reset();

    /** Hooks called by RemoteBackend at each stage of a request */

    /** Note that a request of @p kind has started

        @return an id for it, for the calls below */
    quint64 requestStarted(Kind kind);

    //! its reply has begun to come: the server has answered
    void requestAnswered(quint64 id);

    /** @p received bytes of its reply have come, out of @p total, or -1
        if the reply has not said */
    void requestProgress(quint64 id, qint64 received, qint64 total);

    //! it is done; @p error says why it failed, if it did
    void requestFinished(quint64 id, const QString &error = QString());

    void recordSent(qint64 bytes);
    void recordReceived(qint64 bytes);

    /** Note whether the event stream for @p repo is connected; one which
        is not is wanted, and being connected again */
    void setEventStream(const QString &repo, bool connected);

    //! what a request to @p path is for; @p post if it sends a body
    static Kind kindOf(const QString &path, bool post);

signals:
    void changed();

private:
    //! a request under way
    struct Request {
        Kind kind = Other;
        QElapsedTimer started;
        qint64 received = 0;   //!< bytes of its reply so far
        qint64 total = -1;     //!< bytes its reply has, once it says
        bool answered = false;
    };

    qint64 _sent = 0;
    qint64 _received = 0;
    quint64 _nextId = 1;
    QHash<quint64, Request> _requests;
    QList<qint64> _responses;     //!< the latest times to an answer
    int _failures = 0;
    QString _lastError;
    bool _failing = false;
    QHash<QString, bool> _streams; //!< each repo's event stream
    QString _url;
};

#endif // BACKENDSTATS_H
