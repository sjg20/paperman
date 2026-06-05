/*
License: GPL-2
   Project:    Paperman
   File:       paperman-client.cpp

   Thin command-line wrapper around RemoteBackend.  Lets you exercise
   a paperman-server from the shell — useful for poking at a remote
   deployment while the GUI still talks to the local filesystem.
*/

#include "remotebackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

#include <cstdio>
#include <iostream>
#include <termios.h>
#include <unistd.h>


static void usage(const char *prog)
{
   std::cerr <<
      "Usage: " << prog << " [--server URL] <command> [args]\n"
      "\n"
      "Commands:\n"
      "  login <user>               Prompt for password, save token\n"
      "  logout                     Forget the saved token\n"
      "  status                     Show server status\n"
      "  repos                      List repositories\n"
      "  ls <repo> [path]           List files in a repo directory\n"
      "  cat <repo> <path> [-o OUT] Fetch a file (stdout, or to OUT)\n"
      "\n"
      "Default server is $PAPERMAN_SERVER, else http://localhost:8080.\n"
      "Tokens are stored per-server-id in "
      "~/.config/paperman/<serverId>.token (0600).\n";
}


static QString readHidden(const QString &prompt)
{
   std::cout << prompt.toStdString() << std::flush;

   termios oldt;
   bool toggled = false;
   if (isatty(fileno(stdin)) && tcgetattr(fileno(stdin), &oldt) == 0) {
      termios newt = oldt;
      newt.c_lflag &= ~(tcflag_t)ECHO;
      if (tcsetattr(fileno(stdin), TCSANOW, &newt) == 0)
         toggled = true;
   }

   char buf[256];
   QString line;
   if (fgets(buf, sizeof(buf), stdin)) {
      line = QString::fromLocal8Bit(buf);
      if (line.endsWith('\n'))
         line.chop(1);
   }

   if (toggled) {
      tcsetattr(fileno(stdin), TCSANOW, &oldt);
      std::cout << "\n";
   }
   return line;
}


static QString tokenDir()
{
   QString d = QStandardPaths::writableLocation(
                   QStandardPaths::GenericConfigLocation)
               + "/paperman";
   QDir().mkpath(d);
   return d;
}


static QString tokenPathFor(const QString &serverId)
{
   return tokenDir() + "/" + serverId + ".token";
}


static QString loadToken(const QString &serverId)
{
   QFile f(tokenPathFor(serverId));
   if (!f.open(QIODevice::ReadOnly))
      return QString();
   return QString::fromUtf8(f.readAll()).trimmed();
}


static bool saveToken(const QString &serverId, const QString &token)
{
   QString p = tokenPathFor(serverId);
   QFile f(p);
   if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      return false;
   f.write(token.toUtf8());
   f.write("\n");
   f.close();
   QFile::setPermissions(p, QFile::ReadOwner | QFile::WriteOwner);
   return true;
}


/* Construct a backend pre-loaded with any token we already have for
 * this serverId.  An empty token is fine — endpoints that don't need
 * auth still work. */
static RemoteBackend *makeBackend(const QUrl &url, bool loadCachedToken)
{
   auto *b = new RemoteBackend(url);
   if (loadCachedToken) {
      QString sid = b->serverId();
      if (!sid.isEmpty()) {
         QString tok = loadToken(sid);
         if (!tok.isEmpty())
            b->setBearerToken(tok);
      }
   }
   return b;
}


static int cmdStatus(RemoteBackend *b)
{
   QString sid = b->serverId();
   if (sid.isEmpty()) {
      std::cerr << "Failed: " << b->lastError().toStdString() << "\n";
      return 1;
   }
   std::cout << "serverId: " << sid.toStdString() << "\n";
   return 0;
}


static int cmdRepos(RemoteBackend *b)
{
   QList<RepositoryInfo> repos = b->listRepositories();
   if (repos.isEmpty() && !b->lastError().isEmpty()) {
      std::cerr << "Failed: " << b->lastError().toStdString() << "\n";
      return 1;
   }
   for (const RepositoryInfo &r : repos) {
      std::cout << r.name.toStdString() << "\t"
                << (r.exists ? "ok" : "missing") << "\t"
                << r.path.toStdString() << "\n";
   }
   return 0;
}


static int cmdCat(RemoteBackend *b, const QString &repo, const QString &path,
                  const QString &outPath)
{
   FileFetch f = b->readFile(repo, path);
   if (!f.ok) {
      std::cerr << "Failed: " << f.error.toStdString() << "\n";
      return 1;
   }
   if (outPath.isEmpty()) {
      std::cout.write(f.bytes.constData(), f.bytes.size());
   } else {
      QFile out(outPath);
      if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
         std::cerr << "Cannot write " << outPath.toStdString()
                   << ": " << out.errorString().toStdString() << "\n";
         return 1;
      }
      out.write(f.bytes);
      std::cerr << "Wrote " << f.bytes.size() << " bytes ("
                << f.contentType.toStdString() << ") to "
                << outPath.toStdString() << "\n";
   }
   return 0;
}


static int cmdLs(RemoteBackend *b, const QString &repo, const QString &path)
{
   DirectoryListing l = b->browseDirectory(repo, path);
   if (l.entries.isEmpty() && !b->lastError().isEmpty()) {
      std::cerr << "Failed: " << b->lastError().toStdString() << "\n";
      return 1;
   }
   for (const DirectoryEntry &e : l.entries) {
      if (e.isDir) {
         std::cout << "d\t-\t" << e.name.toStdString() << "/\n";
      } else {
         std::cout << "-\t" << e.size << "\t"
                   << e.name.toStdString() << "\n";
      }
   }
   return 0;
}


static int cmdLogin(const QUrl &url, const QString &user)
{
   RemoteBackend b(url);
   QString sid = b.serverId();
   if (sid.isEmpty()) {
      std::cerr << "Could not reach server at "
                << url.toString().toStdString() << ": "
                << b.lastError().toStdString() << "\n";
      return 1;
   }
   QString pw = readHidden("Password: ");
   if (pw.isEmpty()) {
      std::cerr << "Aborted: empty password\n";
      return 1;
   }
   if (!b.login(user, pw)) {
      std::cerr << "Login failed: " << b.lastError().toStdString() << "\n";
      return 1;
   }
   if (!saveToken(sid, b.bearerToken())) {
      std::cerr << "Warning: could not write token file\n";
      return 1;
   }
   std::cout << "Logged in as " << user.toStdString()
             << "; token saved to "
             << tokenPathFor(sid).toStdString() << "\n";
   return 0;
}


static int cmdLogout(const QUrl &url)
{
   RemoteBackend b(url);
   QString sid = b.serverId();
   if (sid.isEmpty()) {
      std::cerr << "Could not reach server: "
                << b.lastError().toStdString() << "\n";
      return 1;
   }
   QString p = tokenPathFor(sid);
   if (QFile::exists(p) && !QFile::remove(p)) {
      std::cerr << "Failed to remove " << p.toStdString() << "\n";
      return 1;
   }
   std::cout << "Logged out (removed " << p.toStdString() << ")\n";
   return 0;
}


int main(int argc, char *argv[])
{
   QCoreApplication app(argc, argv);
   QStringList args = app.arguments();

   QString serverUrl = QString::fromLocal8Bit(qgetenv("PAPERMAN_SERVER"));
   if (serverUrl.isEmpty())
      serverUrl = "http://localhost:8080";

   /* Argument parsing: optional --server then a subcommand. */
   QStringList rest;
   for (int i = 1; i < args.size(); i++) {
      if ((args[i] == "--server" || args[i] == "-s")
          && i + 1 < args.size()) {
         serverUrl = args[++i];
      } else if (args[i] == "-h" || args[i] == "--help") {
         usage(argv[0]);
         return 0;
      } else {
         rest << args[i];
      }
   }
   if (rest.isEmpty()) {
      usage(argv[0]);
      return 1;
   }

   QUrl url(serverUrl);
   if (!url.isValid() || url.scheme().isEmpty()) {
      std::cerr << "Bad server URL: " << serverUrl.toStdString() << "\n";
      return 1;
   }

   QString cmd = rest[0];
   if (cmd == "login") {
      if (rest.size() < 2) { usage(argv[0]); return 1; }
      return cmdLogin(url, rest[1]);
   }
   if (cmd == "logout") {
      return cmdLogout(url);
   }
   if (cmd == "status") {
      RemoteBackend *b = makeBackend(url, /*loadCachedToken=*/false);
      int r = cmdStatus(b);
      delete b;
      return r;
   }
   if (cmd == "repos") {
      RemoteBackend *b = makeBackend(url, /*loadCachedToken=*/true);
      int r = cmdRepos(b);
      delete b;
      return r;
   }
   if (cmd == "ls") {
      if (rest.size() < 2) { usage(argv[0]); return 1; }
      QString repo = rest[1];
      QString path = rest.size() > 2 ? rest[2] : QString();
      RemoteBackend *b = makeBackend(url, /*loadCachedToken=*/true);
      int r = cmdLs(b, repo, path);
      delete b;
      return r;
   }
   if (cmd == "cat") {
      if (rest.size() < 3) { usage(argv[0]); return 1; }
      QString repo = rest[1];
      QString path = rest[2];
      QString outPath;
      for (int i = 3; i < rest.size(); i++) {
         if ((rest[i] == "-o" || rest[i] == "--out")
             && i + 1 < rest.size()) {
            outPath = rest[++i];
         }
      }
      RemoteBackend *b = makeBackend(url, /*loadCachedToken=*/true);
      int r = cmdCat(b, repo, path, outPath);
      delete b;
      return r;
   }

   std::cerr << "Unknown command: " << cmd.toStdString() << "\n";
   usage(argv[0]);
   return 1;
}
