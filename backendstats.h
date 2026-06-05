/*
License: GPL-2
*/

#ifndef BACKENDSTATS_H
#define BACKENDSTATS_H

#include <QObject>
#include <QString>


/**
 * Lightweight aggregator for network activity across one or more
 * RemoteBackend instances.  Each request increments activeRequests
 * for the duration of the call and adds to bytesSent/bytesReceived
 * counters when the bytes leave/arrive.  The @c changed() signal
 * fires after every update so a status widget can rebuild its
 * display.
 *
 * The bytesSent/bytesReceived counters approximate body sizes —
 * HTTP request/response headers are not counted.  Good enough for
 * a user-facing "how much data have I pulled" indicator.
 */
class BackendStats : public QObject
{
    Q_OBJECT
public:
    explicit BackendStats(QObject *parent = nullptr) : QObject(parent) {}

    qint64 bytesSent() const     { return _sent; }
    qint64 bytesReceived() const { return _received; }
    int activeRequests() const   { return _active; }

    /** URL of the paperman-server the user pointed paperman at,
     *  shown next to the byte counters.  Empty when no remote
     *  server is configured. */
    QString url() const          { return _url; }
    void setUrl(const QString &url);

    /** Reset the byte counters and active count to zero.  Useful
     *  for tests and for an "Reset stats" UI action. */
    void reset();

    /** Hooks called by RemoteBackend at each stage of a request. */
    void requestStarted();
    void requestFinished();
    void recordSent(qint64 bytes);
    void recordReceived(qint64 bytes);

signals:
    void changed();

private:
    qint64 _sent = 0;
    qint64 _received = 0;
    int _active = 0;
    QString _url;
};

#endif // BACKENDSTATS_H
