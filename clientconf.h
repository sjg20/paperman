/*
License: GPL-2
*/

#ifndef CLIENTCONF_H
#define CLIENTCONF_H

#include <QList>
#include <QString>
#include <QUrl>


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
};

#endif // CLIENTCONF_H
