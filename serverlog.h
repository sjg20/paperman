#ifndef SERVERLOG_H
#define SERVERLOG_H

#include <QList>
#include <QString>

/**
 * Per-server request log.
 *
 * Records the sequence of high-level actions that the server performs
 * while handling requests (page extraction, format conversion, cache
 * hits, thumbnail generation, etc.).  This gives tests a way to verify
 * that the server took the expected code path for each request without
 * inspecting the HTTP response alone.
 *
 * Each SearchServer owns a ServerLog instance.  Server methods call log()
 * to append entries as they happen.  Tests walk the log with next()/end()
 * to verify the expected sequence.
 */
struct ServerLog {
    enum Action {
        PageCount,       // got page count
        PageExtract,     // extracted single page to PDF
        ConvertToPdf,    // ran paperman -p conversion
        ConvertCacheHit, // found cached converted PDF
        PageCacheHit,    // found cached extracted page
        ServeFile,       // served a file directly
        Thumbnail,       // generated a thumbnail
        ThumbnailCacheHit, // found cached thumbnail
    };

    /** A single log entry recorded by the server */
    struct Entry {
        Action action;   //!< what the server did
        QString path;    //!< file that was operated on
        int detail;      //!< action-specific value (e.g. page number)
        int elapsedMs;   //!< wall-clock time for the action
    };

    /** Append an entry to the log */
    void log(Action action, const QString &path,
             int detail = 0, int elapsedMs = 0);

    /** Clear all entries and reset the read position */
    void clear();

    /**
     * Check the entry at the read position and advance.
     * @param action  Expected action
     * @param detail  Expected detail, or -1 to skip the check
     * @return true if the entry matches
     */
    bool next(Action action, int detail = -1);

    /** Return true when the read position is past the last entry */
    bool end() const;

private:
    QList<Entry> _entries;
    int _readPos = 0;
};

#endif // SERVERLOG_H
