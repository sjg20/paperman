/*
License: GPL-2
*/

#ifndef TOKENSTORE_H
#define TOKENSTORE_H

#include <QDateTime>
#include <QHash>
#include <QString>


/**
 * Bearer-token store.  Tokens carry the user name they were minted for,
 * an expiry and a stamp, which the server uses to tie a token to the
 * user's password as it was when the token was issued.
 *
 * Given a path, the store keeps its tokens in that file, so that logins
 * survive a restart of the server.  Only a SHA-256 hash of each token is
 * written, never the token itself, so reading the file does not give a
 * way in.  Without a path the tokens live in memory only.
 */
class TokenStore
{
public:
    struct Info
    {
        QString user;
        QDateTime expiry;
        QString stamp;
    };

    /** A store held in memory only */
    TokenStore();

    /** A store kept in @p path, which is read now and written whenever
     *  a token is minted or dropped */
    explicit TokenStore(const QString &path);

    /** Where the server keeps its tokens: beside users.json */
    static QString defaultPath();

    /** The file the tokens are kept in, or empty if in memory only */
    QString filePath() const { return _path; }

    /**
     * Mint a new token for the given user with the given TTL.  Returns
     * the opaque token string the client should pass in
     * `Authorization: Bearer <token>`.  Default TTL is 30 days.
     * @p stamp is kept with the token and handed back by lookup().
     */
    QString mint(const QString &user, int ttl_days = 30,
                 const QString &stamp = QString());

    /**
     * Look up a token.  Returns the user name on success, or an empty
     * string if the token is unknown or expired (expired tokens are
     * also evicted as a side effect).  @p stamp, if given, returns the
     * stamp the token was minted with.
     */
    QString lookup(const QString &token, QString *stamp = nullptr);

    /** Forget a token (logout). */
    void revoke(const QString &token);

    /** Number of currently-live tokens (for tests). */
    int liveCount() const { return _tokens.size(); }

private:
    /** The key a token is kept under: a hash of it, so the file never
     *  holds a usable token */
    static QString keyFor(const QString &token);

    void load();
    void save();

    QString _path;
    QHash<QString, Info> _tokens;   //!< by keyFor() of the token
};

#endif // TOKENSTORE_H
