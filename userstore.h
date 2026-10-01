/*
License: GPL-2
*/

#ifndef USERSTORE_H
#define USERSTORE_H

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>


/**
 * User account store backed by a JSON file (default
 * ~/.config/paperman-server/users.json).
 *
 * Passwords are stored as PBKDF2-SHA256 hashes with a per-user random
 * salt and a fixed iteration count.  Each user has an optional repo
 * allowlist: if empty, the user can see all configured repositories;
 * otherwise only the listed repo names are visible.
 *
 * The store is intentionally small: no roles, no groups, no
 * password-complexity rules.  The caller is responsible for prompting
 * the user for a password and for rejecting empty inputs.
 */
class UserStore
{
public:
    /** Per-user record. */
    struct User
    {
        QString name;
        QString hash;          //!< pbkdf2-sha256$<iters>$<salt-b64>$<hash-b64>
        QStringList repos;     //!< Empty means "all repos"
        bool admin = false;    //!< Reserved for future use
    };

    /** Construct against the default users-file path. */
    UserStore();

    /** Construct against a specific users-file path (used by tests). */
    explicit UserStore(const QString &path);

    /** The on-disk file this store reads/writes. */
    QString filePath() const { return _path; }

    /** Reload from disk.  Returns true on success. */
    bool load();

    /** Persist the in-memory state to disk.  Returns true on success. */
    bool save();

    /** Read the file again if it has changed since it was last read,
     *  judged by its size and modification time, so that a running
     *  server follows useradd, passwd and userdel without a restart.
     *  A file which cannot be read leaves the users as they were.
     *  Returns true if the users were read again. */
    bool reloadIfChanged();

    /** Number of registered users. */
    int count() const { return _users.size(); }

    /** List user names. */
    QStringList userNames() const;

    /** True if `name` exists. */
    bool hasUser(const QString &name) const;

    /** Add a new user with the given password.  Fails if already exists. */
    bool addUser(const QString &name, const QString &password);

    /** Change a user's password.  Fails if user does not exist. */
    bool setPassword(const QString &name, const QString &password);

    /** Delete a user.  Fails if user does not exist. */
    bool delUser(const QString &name);

    /** Replace a user's repo allowlist.  Empty list means "all repos". */
    bool setRepos(const QString &name, const QStringList &repos);

    /** Returns true if the user exists and the password matches. */
    bool verify(const QString &name, const QString &password) const;

    /** True if the user can see the given repository.  Returns false for
     * unknown users.  For known users with an empty allowlist, returns
     * true.
     */
    bool repoAllowed(const QString &name, const QString &repo) const;

    /** Read access for tests and admin CLI. */
    const User *lookup(const QString &name) const;

    /** Hash a password with a fresh salt.  Public for testability. */
    static QString hashPassword(const QString &password);

    /** Verify a password against a stored hash.  Public for testability. */
    static bool verifyPassword(const QString &password,
                               const QString &storedHash);

private:
    /** Note how the file looks now, for reloadIfChanged() */
    void noteFile();

    QString _path;
    QHash<QString, User> _users;
    bool _fileExists = false;    //!< as last read
    qint64 _fileSize = -1;
    QDateTime _fileTime;
};

#endif // USERSTORE_H
