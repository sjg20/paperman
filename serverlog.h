#ifndef SERVERLOG_H
#define SERVERLOG_H

#include <QList>
#include <QString>

struct ServerLog {
    enum Action {
        PageCount,       // got page count
        PageExtract,     // extracted single page from PDF (pdftocairo)
        ConvertToPdf,    // ran paperman -p conversion
        ConvertPage,     // converted single page from non-PDF (paperman)
        ConvertCacheHit, // found cached converted PDF
        PageCacheHit,    // found cached extracted page
        ServeFile,       // served a file directly
    };

    struct Entry {
        Action action;
        QString path;    // file that was operated on
        int detail;      // e.g. page number or page count
        qint64 elapsedMs;
    };

    static void log(Action action, const QString &path,
                    int detail = 0, qint64 elapsedMs = 0);
    static QList<Entry> entries();
    static void clear();
};

#endif // SERVERLOG_H
