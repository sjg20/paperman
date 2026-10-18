/*
License: GPL-2
*/

#ifndef LOCALBACKEND_H
#define LOCALBACKEND_H

#include "backend.h"
#include "cachedfile.h"

#include <QHash>
#include <QList>
#include <QStringList>


/**
 * In-process Backend that reads directly from the local filesystem.
 * The list of root paths is fixed for the lifetime of the instance;
 * the file cache it maintains can be rebuilt at runtime, from a walk of
 * the repository made on another thread (see scanRepo()).
 */
class LocalBackend : public Backend
{
public:
    explicit LocalBackend(const QStringList &rootPaths);

    // Backend
    QList<RepositoryInfo> listRepositories() override;
    DirectoryListing browseDirectory(const QString &repo,
                                     const QString &dir) override;
    FileFetch readFile(const QString &repo,
                       const QString &path) override;

    /** Build (or rebuild) the in-memory file cache for one repo from
     *  disk, by walking its folders.  Returns the number of files
     *  cached. */
    int loadCacheForRepo(const QString &rootPath);

    /** Walk the repository at @p rootPath for its stacks, as
     *  loadCacheForRepo() does, touching nothing else, so that it can be
     *  done on another thread */
    static QList<CachedFile> scanRepo(const QString &rootPath);

    /** Take @p files, from scanRepo(), as the file cache for @p rootPath */
    void setCache(const QString &rootPath, const QList<CachedFile> &files);

    /** Number of files cached for @p rootPath. */
    int cacheCount(const QString &rootPath) const;

    /** Total size in bytes of all cached files for @p rootPath. */
    qint64 cacheBytes(const QString &rootPath) const;

    /** Direct read-only access to the cached file list for
     *  @p rootPath.  Returned reference remains valid until the next
     *  loadCacheForRepo() call for the same path. */
    const QList<CachedFile> &fileCacheFor(const QString &rootPath) const;

    /** Translate a repository name (basename) to its on-disk root
     *  path.  Returns empty string for unknown names. */
    QString rootPathForName(const QString &repo) const;

    /** Bring the cached entry for the file @p relPath in @p rootPath up
     *  to date with the disk: add or update it if it is a stack there,
     *  or drop it if not.  Nothing is done for a repo not cached. */
    void updateCachedFile(const QString &rootPath, const QString &relPath);

private:
    static void scanDirectory(const QString &repoPath, const QString &dirPath,
                              QList<CachedFile> &fileList);
    int countFilesRecursive(const QString &repoPath,
                            const QString &dirPath) const;

    QStringList _rootPaths;
    QHash<QString, QList<CachedFile>> _fileCache;
    /** Returned by fileCacheFor() when the repo is unknown so callers
     *  don't need to handle a null reference. */
    QList<CachedFile> _emptyCache;
};

#endif // LOCALBACKEND_H
