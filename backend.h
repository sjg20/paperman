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


/**
 * Abstract data source the server reads from.  The local deployment
 * uses LocalBackend (in-process file access); a future RemoteBackend
 * will forward calls to another paperman-server over HTTP, letting the
 * GUI client and the server share the same call sites.
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
};

#endif // BACKEND_H
