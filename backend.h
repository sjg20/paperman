/*
License: GPL-2
*/

#ifndef BACKEND_H
#define BACKEND_H

#include <QList>
#include <QString>


/**
 * One entry in the repository list.  Mirrors the existing /repos JSON
 * shape: a repository may be configured but missing on disk, in which
 * case @c exists is false and @c name is empty.
 */
struct RepositoryInfo
{
    QString name;
    QString path;
    bool exists = false;
};


/** A single entry in a directory listing: either a file or a
 *  subdirectory.  Mirrors the JSON shape /browse returns. */
struct DirectoryEntry
{
    QString name;
    QString path;        //!< Repo-relative path
    qint64 size = 0;     //!< 0 for directories
    bool isDir = false;
    QString modified;    //!< ISO-8601 (files only)
    int count = 0;       //!< File count for directory entries (recursive)
};


struct DirectoryListing
{
    /** True if the call succeeded.  When false, @c error describes
     *  why and @c notFound distinguishes 404 from 400. */
    bool ok = false;
    bool notFound = false;
    QString error;
    QList<DirectoryEntry> entries;
};


/**
 * Abstract data source the server reads from.  The local deployment
 * uses LocalBackend (in-process file access); RemoteBackend forwards
 * calls to another paperman-server over HTTP, letting the GUI client
 * and the server share the same call sites.
 *
 * Only the surface the HTTP layer needs lives here.  Methods are added
 * as their HTTP handlers are migrated through the interface.
 */
class Backend
{
public:
    virtual ~Backend() = default;

    /** List configured repositories.  Order is the same as the
     *  paperman-server command line. */
    virtual QList<RepositoryInfo> listRepositories() = 0;

    /** Browse a directory in a repository.  @c repo is the basename
     *  returned by listRepositories(); @c dir is the path relative to
     *  the repo root (empty means the root).  Directories come first
     *  in the entries list. */
    virtual DirectoryListing browseDirectory(const QString &repo,
                                             const QString &dir) = 0;
};

#endif // BACKEND_H
