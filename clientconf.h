/*
License: GPL-2
*/

#ifndef CLIENTCONF_H
#define CLIENTCONF_H

#include <QList>
#include <QString>
#include <QUrl>

#include <functional>


/**
 * One server listed in the client configuration file.  @c name is a
 * friendly label the user can refer to on the command line and that the
 * GUI shows; it defaults to the URL's host when the config line gives
 * no explicit name.
 */
struct ServerEntry
{
    QString name;
    QUrl url;
};


/**
 * The client's directory of paperman-servers.  Both the GUI and the CLI
 * read the same file so a server configured once is reachable from
 * either.  The file lives at
 *
 *     <config>/paperman/client.conf
 *
 * (the same directory the per-server bearer tokens are cached in), where
 * <config> is QStandardPaths::GenericConfigLocation, normally
 * ~/.config.  The environment variable @c PAPERMAN_CONFIG overrides the
 * whole pathname, which keeps tests self-contained.
 *
 * Format: one server per line.  Blank lines and lines whose first
 * non-space character is '#' are ignored.  A line is either
 *
 *     https://host:port          # label defaults to the host
 *     Name = https://host:port   # explicit label before an '='
 *
 * Surrounding whitespace is trimmed from both the name and the URL.
 */
class ClientConfig
{
public:
    /** Pathname of the config file, honouring @c PAPERMAN_CONFIG.  The
     *  file need not exist. */
    static QString configPath();

    /** Read and parse the config file.  A missing file yields an empty
     *  list (not an error).  Lines that don't parse as a URL are
     *  skipped; if @p badLines is non-null each such line's text is
     *  appended to it so the caller can warn. */
    static QList<ServerEntry> load(QStringList *badLines = nullptr);

    /** True when two URLs name the same server, ignoring differences
     *  that don't change what is addressed (a trailing slash, the case
     *  of the host).  Used to dedupe config entries and to spot the
     *  server a command is already pointed at. */
    static bool sameServer(const QUrl &a, const QUrl &b);

    /** Look a server up by its friendly name (case-insensitive).
     *  Returns true and fills @p out on a match. */
    static bool findByName(const QString &name, ServerEntry *out);

    /** Append @p url (with optional @p name) to the config file,
     *  creating it and its directory if needed.  A server whose URL is
     *  already listed is left untouched and the call still succeeds;
     *  @p added, if non-null, says whether a line was really appended.
     *  Returns false only on a genuine write error (@p errorOut set). */
    static bool add(const QUrl &url, const QString &name = QString(),
                    QString *errorOut = nullptr, bool *added = nullptr);

    /** The URLs where a server named only by its host might be found,
     *  most likely first.  A bare host gives the default port 8080, the
     *  8081 used by the systemd example and then https on its usual
     *  port; "host:port" gives http and then https on that port.
     *  Returns an empty list when @p arg is a full URL (or is not a
     *  host at all), since that names a server outright. */
    static QList<QUrl> probeCandidates(const QString &arg);

    /** Ask each of @p candidates for its status, all at once, and
     *  return the first in the list that answers as a Paperman server.
     *  Returns an empty URL if none does within @p timeoutMs, with
     *  @p errorOut saying what was tried and what went wrong with each. */
    static QUrl probe(const QList<QUrl> &candidates,
                      QString *errorOut = nullptr, int timeoutMs = 3000);

    //! called with the server found, or with why none was
    using Found = std::function<void (const QUrl &url, const QString &error)>;

    /** Probe @p candidates as the other probe() does, but without
     *  waiting: @p done is called once one answers or the time is up,
     *  unless @p context, which must be given, has gone by then */
    static void probe(const QList<QUrl> &candidates, QObject *context,
                      Found done, int timeoutMs = 3000);

    /** Turn a server named by the user into a URL: the name of a
     *  configured server, a full URL, or a host (with an optional port)
     *  to probe.  Returns an empty URL with @p errorOut set if @p arg
     *  is none of these or no server answers on the host. */
    static QUrl resolve(const QString &arg, QString *errorOut = nullptr);

    /** Resolve @p arg as the other resolve() does, but without waiting
     *  for a probe: @p done is called with the URL, later, unless
     *  @p context has gone by then */
    static void resolve(const QString &arg, QObject *context, Found done);
};

#endif // CLIENTCONF_H
