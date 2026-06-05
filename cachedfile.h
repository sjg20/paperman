/*
License: GPL-2
*/

#ifndef CACHEDFILE_H
#define CACHEDFILE_H

#include <QDateTime>
#include <QString>


/**
 * One entry in the file cache LocalBackend maintains for each
 * repository.  The cache is built from a recursive scan (or loaded
 * from a `.papertree` index file) at startup and is the source of
 * truth for fast directory browsing and search.
 */
struct CachedFile
{
    QString path;        //!< Relative path from repository root
    QString name;        //!< Filename only
    qint64 size;         //!< File size in bytes
    QDateTime modified;  //!< Last modification time
};

#endif // CACHEDFILE_H
