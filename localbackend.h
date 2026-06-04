/*
License: GPL-2
*/

#ifndef LOCALBACKEND_H
#define LOCALBACKEND_H

#include "backend.h"

#include <QStringList>


/**
 * In-process Backend that reads directly from the local filesystem.
 * The list of root paths is fixed for the lifetime of the instance
 * (changing it requires restarting the server, same as today).
 */
class LocalBackend : public Backend
{
public:
    explicit LocalBackend(const QStringList &rootPaths);

    QList<RepositoryInfo> listRepositories() override;

private:
    QStringList _rootPaths;
};

#endif // LOCALBACKEND_H
