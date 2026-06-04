/*
License: GPL-2
*/

#ifndef TOKENSTORE_H
#define TOKENSTORE_H

#include <QDateTime>
#include <QHash>
#include <QString>


/**
 * In-memory bearer-token store.  Server restart invalidates all
 * tokens; that's intentional for v1.  Tokens carry the user name they
 * were minted for and an expiry timestamp.
 */
class TokenStore
{
public:
    struct Info
    {
        QString user;
        QDateTime expiry;
    };

    TokenStore();

    /**
     * Mint a new token for the given user with the given TTL.  Returns
     * the opaque token string the client should pass in
     * `Authorization: Bearer <token>`.  Default TTL is 30 days.
     */
    QString mint(const QString &user, int ttl_days = 30);

    /**
     * Look up a token.  Returns the user name on success, or an empty
     * string if the token is unknown or expired (expired tokens are
     * also evicted as a side effect).
     */
    QString lookup(const QString &token);

    /** Forget a token (logout). */
    void revoke(const QString &token);

    /** Number of currently-live tokens (for tests). */
    int liveCount() const { return _tokens.size(); }

private:
    QHash<QString, Info> _tokens;
};

#endif // TOKENSTORE_H
