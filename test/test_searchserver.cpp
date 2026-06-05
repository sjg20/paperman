#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <QScopeGuard>
#include <QStandardPaths>

#include "../remotebackend.h"
#include "../searchserver.h"
#include "../userstore.h"
#include "test.h"

#include "test_searchserver.h"

static QByteArray httpGetRaw(const QString &path, int port, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost("localhost", port);

    QCoreApplication::processEvents();

    if (!socket.waitForConnected(2000)) {
        qWarning() << "Failed to connect:" << socket.errorString();
        return QByteArray();
    }

    QString request = QString("GET %1 HTTP/1.1\r\n"
                             "Host: localhost\r\n"
                             "Connection: close\r\n"
                             "\r\n").arg(path);

    socket.write(request.toUtf8());
    socket.flush();

    QCoreApplication::processEvents();

    // Wait for first response bytes
    int totalWait = 0;
    while (socket.bytesAvailable() == 0 && totalWait < timeoutMs) {
        QCoreApplication::processEvents();

        if (!socket.waitForReadyRead(200)) {
            if (socket.state() != QAbstractSocket::ConnectedState)
                break;
        }
        totalWait += 200;
    }

    QByteArray response = socket.readAll();

    // Drain remaining data
    int drainTimeout = qMin(timeoutMs / 2, 5000);
    totalWait = 0;
    while (socket.state() == QAbstractSocket::ConnectedState &&
           totalWait < drainTimeout) {
        if (socket.waitForReadyRead(100))
            response += socket.readAll();
        totalWait += 100;
    }

    socket.close();
    return response;
}

TestSearchServer::Response TestSearchServer::get(const QString &path,
                                                  int timeoutMs)
{
    QByteArray raw = httpGetRaw(path, PORT, timeoutMs);

    Response resp;
    int sep = raw.indexOf("\r\n\r\n");
    if (sep >= 0) {
        resp.header = QString::fromUtf8(raw.left(sep));
        resp.body = raw.mid(sep + 4);
    }
    return resp;
}

/* Send a raw HTTP request string (already includes headers + body)
 * and split the response into header/body chunks. */
static TestSearchServer::Response sendRaw(const QByteArray &request,
                                          int port, int timeoutMs)
{
    TestSearchServer::Response resp;
    QTcpSocket socket;
    socket.connectToHost("localhost", port);
    QCoreApplication::processEvents();
    if (!socket.waitForConnected(2000))
        return resp;

    socket.write(request);
    socket.flush();
    QCoreApplication::processEvents();

    QByteArray raw;
    int totalWait = 0;
    while (socket.state() == QAbstractSocket::ConnectedState
           && totalWait < timeoutMs) {
        if (socket.waitForReadyRead(200))
            raw += socket.readAll();
        totalWait += 200;
    }
    raw += socket.readAll();
    socket.close();

    int sep = raw.indexOf("\r\n\r\n");
    if (sep >= 0) {
        resp.header = QString::fromUtf8(raw.left(sep));
        resp.body = raw.mid(sep + 4);
    }
    return resp;
}

TestSearchServer::Response
TestSearchServer::getWithBearer(const QString &path, const QString &token,
                                int timeoutMs)
{
    QByteArray req;
    req += "GET " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: localhost\r\n";
    req += "Authorization: Bearer " + token.toUtf8() + "\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";
    return sendRaw(req, PORT, timeoutMs);
}

TestSearchServer::Response
TestSearchServer::postJson(const QString &path, const QByteArray &body,
                           int timeoutMs)
{
    QByteArray req;
    req += "POST " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: localhost\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";
    req += body;
    return sendRaw(req, PORT, timeoutMs);
}

void TestSearchServer::createTestFiles(const QString& path)
{
    QDir dir(path);

    // Create some test files
    QFile file1(path + "/test-document.max");
    file1.open(QIODevice::WriteOnly);
    file1.write("test content");
    file1.close();

    QFile file2(path + "/invoice-2024.pdf");
    file2.open(QIODevice::WriteOnly);
    file2.write("invoice content");
    file2.close();

    QFile file3(path + "/photo.jpg");
    file3.open(QIODevice::WriteOnly);
    file3.write("photo data");
    file3.close();

    // Create subdirectory with files
    dir.mkpath(path + "/archive");
    QFile file4(path + "/archive/old-doc.max");
    file4.open(QIODevice::WriteOnly);
    file4.write("old content");
    file4.close();
}

void TestSearchServer::clearCaches()
{
    const char *dirs[] = {
        "/tmp/paperman-pages",
        "/tmp/paperman-converted",
        "/tmp/paperman-thumbnails",
    };
    for (const char *path : dirs) {
        QDir dir(path);
        if (dir.exists()) {
            foreach (const QString &f, dir.entryList(QDir::Files))
                dir.remove(f);
        }
    }
}

qint64 TestSearchServer::copyTestFile(const QString &fileName,
                                      const QString &destDir)
{
    QString src = testSrc + "/" + fileName;
    QString dst = destDir + "/" + fileName;
    if (!QFile::copy(src, dst)) {
        qWarning() << "Failed to copy" << src << "to" << dst;
        return -1;
    }
    return QFileInfo(dst).size();
}

void TestSearchServer::testServerStartStop()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    qDebug() << "Creating server on port" << PORT;
    SearchServer server(tmpDir.path(), PORT);

    qDebug() << "Starting server...";
    QVERIFY(server.start());
    qDebug() << "Server started";
    QCOMPARE(server.port(), PORT);
    QVERIFY(server.isRunning());

    qDebug() << "Stopping server...";
    server.stop();
    qDebug() << "Server stopped";
    QVERIFY(!server.isRunning());
    qDebug() << "testServerStartStop PASSED";
}

void TestSearchServer::testStatusEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    qDebug() << "Starting test status endpoint...";
    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    // Give server a moment to fully initialize
    QTest::qWait(100);

    qDebug() << "Making HTTP request to /status";
    auto resp = get("/status");
    qDebug() << "Response:" << resp.header;
    QVERIFY(resp.ok());
    QString body = QString::fromUtf8(resp.body);
    QVERIFY(body.contains("\"status\""));
    QVERIFY(body.contains("\"running\""));
    QVERIFY(body.contains("\"repository\""));
    QVERIFY(body.contains(tmpDir.path()));

    server.stop();
    qDebug() << "testStatusEndpoint PASSED";
}

void TestSearchServer::testV1StatusEndpoint()
{
   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());

   QString firstId;
   {
      SearchServer server(tmpDir.path(), PORT);
      QVERIFY(server.start());
      QTest::qWait(100);

      auto resp = get("/v1/status");
      QVERIFY(resp.ok());
      auto doc = QJsonDocument::fromJson(resp.body);
      QVERIFY(doc.isObject());
      auto obj = doc.object();
      QCOMPARE(obj["status"].toString(), QString("running"));
      QCOMPARE(obj["apiVersion"].toString(), QString(PAPERMAN_API_VERSION));
      QVERIFY(obj.contains("serverId"));
      QVERIFY(!obj["serverId"].toString().isEmpty());
      QVERIFY(obj["features"].isArray());

      firstId = obj["serverId"].toString();
      server.stop();
   }

   // A second server in the same config directory must reuse the same
   // serverId — clients key their local caches on it.
   {
      SearchServer server(tmpDir.path(), PORT);
      QVERIFY(server.start());
      QTest::qWait(100);

      auto resp = get("/v1/status");
      QVERIFY(resp.ok());
      auto doc = QJsonDocument::fromJson(resp.body);
      QCOMPARE(doc.object()["serverId"].toString(), firstId);
      server.stop();
   }
}

void TestSearchServer::testV1AuthLogin()
{
   /* Redirect QStandardPaths so the UserStore writes into a per-test
    * temp tree.  setTestModeEnabled affects the whole process for the
    * duration of this test. */
   QStandardPaths::setTestModeEnabled(true);
   auto restoreStdPaths = qScopeGuard([] {
      QStandardPaths::setTestModeEnabled(false);
   });

   /* Wipe any stale users.json left from a previous run. */
   QString cfgFile = QStandardPaths::writableLocation(
                         QStandardPaths::GenericConfigLocation)
                     + "/paperman-server/users.json";
   QFile::remove(cfgFile);

   /* Pre-populate the user store before the server reads it. */
   {
      UserStore store;
      QVERIFY(store.addUser("alice", "s3cret"));
      QVERIFY(store.addUser("bob", "hunter2"));
      /* Bob is restricted to a repo that doesn't exist. */
      QVERIFY(store.setRepos("bob", {"nowhere"}));
   }

   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());
   createTestFiles(tmpDir.path());

   SearchServer server(tmpDir.path(), PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   /* /v1/status remains open even with auth enabled. */
   QVERIFY(get("/v1/status").ok());

   /* Without credentials, /repos returns 401. */
   auto unauth = get("/repos");
   QVERIFY(unauth.header.contains("401"));

   /* Bad password rejected. */
   auto bad = postJson("/v1/auth/login",
                       R"({"user":"alice","password":"wrong"})");
   QVERIFY(bad.header.contains("401"));

   /* Successful login mints a token. */
   auto ok = postJson("/v1/auth/login",
                      R"({"user":"alice","password":"s3cret"})");
   QVERIFY2(ok.ok(), ok.header.toUtf8().constData());
   auto okObj = QJsonDocument::fromJson(ok.body).object();
   QString token = okObj["token"].toString();
   QVERIFY(!token.isEmpty());
   QCOMPARE(okObj["user"].toString(), QString("alice"));

   /* Token grants access to /repos. */
   auto repos = getWithBearer("/repos", token);
   QVERIFY2(repos.ok(), repos.header.toUtf8().constData());

   /* Bob's allowlist blocks his own repo. */
   auto bobLogin = postJson("/v1/auth/login",
                            R"({"user":"bob","password":"hunter2"})");
   QVERIFY(bobLogin.ok());
   QString bobTok = QJsonDocument::fromJson(bobLogin.body)
                        .object()["token"].toString();
   QString repoName = QFileInfo(tmpDir.path()).fileName();
   auto bobSearch = getWithBearer(
       QString("/search?q=test&repo=%1").arg(repoName), bobTok);
   QVERIFY(bobSearch.header.contains("403"));

   /* Garbage tokens still 401. */
   auto garbage = getWithBearer("/repos", "not-a-real-token");
   QVERIFY(garbage.header.contains("401"));

   server.stop();
}

void TestSearchServer::testReposFilteredByUser()
{
   QStandardPaths::setTestModeEnabled(true);
   auto restoreStdPaths = qScopeGuard([] {
      QStandardPaths::setTestModeEnabled(false);
   });
   QString cfgFile = QStandardPaths::writableLocation(
                         QStandardPaths::GenericConfigLocation)
                     + "/paperman-server/users.json";
   QFile::remove(cfgFile);

   /* Two on-disk repos.  alice gets the first only; carol gets both. */
   QTemporaryDir repoA, repoB;
   QVERIFY(repoA.isValid() && repoB.isValid());
   QString nameA = QFileInfo(repoA.path()).fileName();
   QString nameB = QFileInfo(repoB.path()).fileName();

   {
      UserStore store;
      QVERIFY(store.addUser("alice", "pw"));
      QVERIFY(store.setRepos("alice", {nameA}));
      QVERIFY(store.addUser("carol", "pw"));
      /* carol has empty allowlist → all repos. */
   }

   SearchServer server({repoA.path(), repoB.path()}, PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   auto login = [&](const QString &user) {
      auto resp = postJson(
          "/v1/auth/login",
          QString(R"({"user":"%1","password":"pw"})").arg(user).toUtf8());
      Q_ASSERT(resp.ok());
      return QJsonDocument::fromJson(resp.body).object()["token"].toString();
   };

   /* alice sees only repoA. */
   auto aliceRepos = getWithBearer("/repos", login("alice"));
   QVERIFY(aliceRepos.ok());
   auto aObj = QJsonDocument::fromJson(aliceRepos.body).object();
   QCOMPARE(aObj["count"].toInt(), 1);
   QCOMPARE(aObj["repositories"].toArray()[0].toObject()["name"].toString(),
            nameA);

   /* carol sees both. */
   auto carolRepos = getWithBearer("/repos", login("carol"));
   QVERIFY(carolRepos.ok());
   auto cObj = QJsonDocument::fromJson(carolRepos.body).object();
   QCOMPARE(cObj["count"].toInt(), 2);

   server.stop();
}

void TestSearchServer::testRemoteBackendEndToEnd()
{
   /* Full client→server→client round-trip: spin up a real
    * SearchServer on a temp repo, then drive it through the same
    * RemoteBackend code path the GUI will use.  The test sits on both
    * sides of the socket to confirm the wire format and the
    * production client class agree. */

   QStandardPaths::setTestModeEnabled(true);
   auto restoreStdPaths = qScopeGuard([] {
      QStandardPaths::setTestModeEnabled(false);
   });
   QString cfgFile = QStandardPaths::writableLocation(
                         QStandardPaths::GenericConfigLocation)
                     + "/paperman-server/users.json";
   QFile::remove(cfgFile);

   /* Server-side setup: one user, one repo with a known set of files. */
   {
      UserStore store;
      QVERIFY(store.addUser("dave", "passw0rd"));
   }

   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());
   createTestFiles(tmpDir.path());

   SearchServer server(tmpDir.path(), PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   /* Client side: RemoteBackend pointed at the running server. */
   RemoteBackend client(QUrl(QString("http://localhost:%1").arg(PORT)));

   /* Wrong password is rejected without setting a token. */
   QVERIFY(!client.login("dave", "wrong"));
   QVERIFY(!client.isAuthenticated());

   /* Correct credentials grant access. */
   QVERIFY2(client.login("dave", "passw0rd"),
            client.lastError().toUtf8().constData());
   QVERIFY(client.isAuthenticated());

   /* listRepositories shows the configured repo. */
   QList<RepositoryInfo> repos = client.listRepositories();
   QCOMPARE(repos.size(), 1);
   QCOMPARE(repos[0].name, QFileInfo(tmpDir.path()).fileName());
   QVERIFY(repos[0].exists);

   /* browseDirectory on the repo root returns the seeded files +
    * subdirectory.  createTestFiles() lays down:
    *   test-document.max, invoice-2024.pdf, photo.jpg, archive/    */
   DirectoryListing root = client.browseDirectory(repos[0].name, "");
   QStringList names;
   for (const DirectoryEntry &e : root.entries)
      names << (e.isDir ? e.name + "/" : e.name);

   QVERIFY2(names.contains("archive/"), qPrintable(names.join(',')));
   QVERIFY2(names.contains("test-document.max"), qPrintable(names.join(',')));
   QVERIFY2(names.contains("invoice-2024.pdf"), qPrintable(names.join(',')));
   QVERIFY2(names.contains("photo.jpg"), qPrintable(names.join(',')));

   /* Subdirectories come before files. */
   int firstFile = -1, lastDir = -1;
   for (int i = 0; i < root.entries.size(); i++) {
      if (root.entries[i].isDir) lastDir = i;
      else if (firstFile < 0)    firstFile = i;
   }
   QVERIFY(lastDir < firstFile);

   /* Files carry a non-zero size from the cache. */
   for (const DirectoryEntry &e : root.entries) {
      if (!e.isDir)
         QVERIFY2(e.size > 0, qPrintable(e.name));
   }

   /* readFile round-trips file bytes through the wire.  Verify against
    * the source-of-truth on disk. */
   FileFetch f = client.readFile(repos[0].name, "invoice-2024.pdf");
   QVERIFY2(f.ok, f.error.toUtf8().constData());
   QCOMPARE(f.contentType, QString("application/pdf"));

   QFile src(tmpDir.path() + "/invoice-2024.pdf");
   QVERIFY(src.open(QIODevice::ReadOnly));
   QCOMPARE(f.bytes, src.readAll());

   /* Unknown file is reported as not-found, not crash. */
   FileFetch missing = client.readFile(repos[0].name, "no-such-file.pdf");
   QVERIFY(!missing.ok);

   /* Traversal attempt is rejected client-side... actually here the
    * server rejects it; either way the call must fail. */
   FileFetch evil = client.readFile(repos[0].name, "../etc/passwd");
   QVERIFY(!evil.ok);

   server.stop();
}

void TestSearchServer::testSearchEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    createTestFiles(tmpDir.path());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    // Search for "invoice"
    auto resp = get("/search?q=invoice");
    QVERIFY(resp.ok());
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("invoice-2024.pdf"));
    QVERIFY(resp.body.contains("\"count\":1"));

    // Search for "test"
    resp = get("/search?q=test");
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("test-document.max"));

    // Search for something that doesn't exist
    resp = get("/search?q=nonexistent");
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("\"count\":0"));

    server.stop();
}

void TestSearchServer::testListEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    createTestFiles(tmpDir.path());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    // List files in root
    auto resp = get("/list");
    QVERIFY(resp.ok());
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("test-document.max"));
    QVERIFY(resp.body.contains("invoice-2024.pdf"));
    QVERIFY(resp.body.contains("photo.jpg"));
    QVERIFY(resp.body.contains("\"count\":3"));

    // List files in subdirectory
    resp = get("/list?path=archive");
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("old-doc.max"));
    QVERIFY(resp.body.contains("\"count\":1"));

    server.stop();
}

void TestSearchServer::testInvalidEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    auto resp = get("/invalid");
    QVERIFY(resp.header.contains("404") || resp.header.contains("Not Found"));

    server.stop();
}

void TestSearchServer::testMissingSearchParameter()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    // Search without 'q' parameter should return error
    auto resp = get("/search");
    QVERIFY(resp.header.contains("400") || resp.body.contains("\"success\":false")
            || resp.body.contains("error"));

    server.stop();
}

void TestSearchServer::testReposEndpoint()
{
    QTemporaryDir tmpDir1;
    QTemporaryDir tmpDir2;
    QVERIFY(tmpDir1.isValid());
    QVERIFY(tmpDir2.isValid());

    // Test with multiple repositories
    QStringList repos;
    repos << tmpDir1.path() << tmpDir2.path();
    SearchServer server(repos, PORT);
    QVERIFY(server.start());

    // Request repository list
    auto resp = get("/repos");
    QVERIFY(resp.ok());
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("\"count\":2"));
    QVERIFY(resp.body.contains(tmpDir1.path().toUtf8()));
    QVERIFY(resp.body.contains(tmpDir2.path().toUtf8()));
    QVERIFY(resp.body.contains("\"repositories\""));
    QVERIFY(resp.body.contains("\"exists\":true"));

    server.stop();

    // Test with single repository (backward compatibility)
    SearchServer server2(tmpDir1.path(), PORT);
    QVERIFY(server2.start());

    resp = get("/repos");
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("\"count\":1"));
    QVERIFY(resp.body.contains(tmpDir1.path().toUtf8()));

    server2.stop();
}

void TestSearchServer::testSearchWithRepo()
{
    QTemporaryDir tmpDir1;
    QTemporaryDir tmpDir2;
    QVERIFY(tmpDir1.isValid());
    QVERIFY(tmpDir2.isValid());

    // Create different test files in each repository
    QFile file1(tmpDir1.path() + "/repo1-file.max");
    file1.open(QIODevice::WriteOnly);
    file1.write("repo1 content");
    file1.close();

    QFile file2(tmpDir2.path() + "/repo2-file.max");
    file2.open(QIODevice::WriteOnly);
    file2.write("repo2 content");
    file2.close();

    // Setup server with multiple repositories
    QStringList repos;
    repos << tmpDir1.path() << tmpDir2.path();
    SearchServer server(repos, PORT);
    QVERIFY(server.start());

    // Get repository names from paths
    QString repo1Name = QFileInfo(tmpDir1.path()).fileName();
    QString repo2Name = QFileInfo(tmpDir2.path()).fileName();

    // Search without repo parameter (should search in default/first repo)
    auto resp = get("/search?q=repo1");
    QVERIFY(resp.ok());
    QVERIFY(resp.body.contains("repo1-file.max"));
    QVERIFY(!resp.body.contains("repo2-file.max"));

    // Search in specific repo (repo2)
    resp = get(QString("/search?q=repo2&repo=%1").arg(repo2Name));
    QVERIFY(resp.body.contains("repo2-file.max"));
    QVERIFY(!resp.body.contains("repo1-file.max"));

    // Search in specific repo (repo1)
    resp = get(QString("/search?q=repo1&repo=%1").arg(repo1Name));
    QVERIFY(resp.body.contains("repo1-file.max"));

    // Search in non-existent repo
    resp = get("/search?q=test&repo=nonexistent");
    QVERIFY(resp.header.contains("404") || resp.body.contains("Repository not found"));

    server.stop();
}

void TestSearchServer::testFileEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Create a test file with known content
    QString testContent = "This is test file content for the file endpoint test.";
    QFile testFile(tmpDir.path() + "/test-file.pdf");
    testFile.open(QIODevice::WriteOnly);
    testFile.write(testContent.toUtf8());
    testFile.close();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    // Test retrieving existing file
    auto resp = get("/file?path=test-file.pdf");
    QVERIFY(resp.ok());
    QVERIFY(resp.header.contains("Content-Type: application/pdf"));
    QVERIFY(resp.body.contains(testContent.toUtf8()));

    // Test missing path parameter
    resp = get("/file");
    QVERIFY(resp.header.contains("400") || resp.body.contains("Missing 'path' parameter"));

    // Test non-existent file
    resp = get("/file?path=nonexistent.pdf");
    QVERIFY(resp.header.contains("404") || resp.body.contains("File not found"));

    // Test directory traversal prevention
    resp = get("/file?path=../etc/passwd");
    QVERIFY(resp.header.contains("400") || resp.body.contains("Invalid file path"));

    // Test absolute path prevention
    resp = get("/file?path=/etc/passwd");
    QVERIFY(resp.header.contains("400") || resp.body.contains("Invalid file path"));

    server.stop();
}

void TestSearchServer::testFilePageCount()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QVERIFY(copyTestFile("testpdf.pdf", tmpDir.path()) > 0);

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    // Request page count
    auto resp = get("/file?path=testpdf.pdf&pages=true");
    qDebug() << "Page count response:" << resp.header;
    QVERIFY(resp.ok());
    QVERIFY(resp.body.contains("\"success\":true"));
    QVERIFY(resp.body.contains("\"pages\":5"));

    server.stop();
}

void TestSearchServer::testFilePageExtract()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    qint64 fullSize = copyTestFile("testpdf.pdf", tmpDir.path());
    QVERIFY(fullSize > 0);

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    // Request single page
    auto resp = get("/file?path=testpdf.pdf&page=1");
    qDebug() << "Page extract header:" << resp.header;
    QVERIFY(resp.ok());
    QVERIFY(resp.header.contains("application/pdf"));

    // Body should be smaller than the full file
    qint64 bodySize = resp.body.size();
    qDebug() << "Full file:" << fullSize << "bytes, page 1:" << bodySize << "bytes";
    QVERIFY2(bodySize < fullSize,
             qPrintable(QString("Page (%1) should be smaller than full file (%2)")
                       .arg(bodySize).arg(fullSize)));

    server.stop();
}

void TestSearchServer::verifyPageFetch(ServerLog &slog,
                                       const QString &fileName, int page,
                                       ServerLog::Action expectedAction,
                                       qint64 *bodySize, int timeoutMs)
{
    auto resp = get(QString("/file?path=%1&page=%2").arg(fileName).arg(page),
                    timeoutMs);
    QVERIFY2(resp.ok(),
             qPrintable(QString("Page %1 fetch failed: %2")
                       .arg(page).arg(resp.header)));
    QVERIFY(resp.header.contains("application/pdf"));
    QVERIFY2(resp.body.startsWith("%PDF"),
             qPrintable(QString("Page %1 should be a valid PDF").arg(page)));

    if (bodySize)
        *bodySize = resp.body.size();

    QVERIFY(slog.next(expectedAction, page));
}

void TestSearchServer::testLargePdfProgressive()
{
    // Test progressive loading with a 100-page PDF
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString fileName = "100pp.pdf";
    qint64 fullFileSize = copyTestFile(fileName, tmpDir.path());
    QVERIFY(fullFileSize > 0);

    clearCaches();

    SearchServer server(tmpDir.path(), PORT, nullptr, true);
    QVERIFY(server.start());
    QTest::qWait(100);
    ServerLog &slog = server._log;

    // 1. Page count — should return 100 and log PageCount
    auto resp = get(QString("/file?path=%1&pages=true").arg(fileName));
    QVERIFY(resp.ok());
    QVERIFY(resp.body.contains("\"pages\":100"));
    QVERIFY(slog.next(ServerLog::PageCount, 100));

    // 2. Extract page 1
    qint64 page1Size;
    verifyPageFetch(slog, fileName, 1, ServerLog::PageExtract, &page1Size);
    QVERIFY2(page1Size < fullFileSize / 5,
             qPrintable(QString("Page 1 (%1 bytes) should be < 1/5 of full "
                                "file (%2)")
                       .arg(page1Size).arg(fullFileSize)));

    // 3. Request page 1 again — should hit cache
    verifyPageFetch(slog, fileName, 1, ServerLog::PageCacheHit);

    // 4. Extract page 50
    verifyPageFetch(slog, fileName, 50, ServerLog::PageExtract);

    QVERIFY(slog.end());
    server.stop();
}

void TestSearchServer::testLargeMaxProgressive()
{
    // Test progressive loading with a 100-page MAX file
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString fileName = "100pp_from_pdf.max";
    qint64 fullFileSize = copyTestFile(fileName, tmpDir.path());
    QVERIFY(fullFileSize > 0);

    clearCaches();

    SearchServer server(tmpDir.path(), PORT, nullptr, true);
    QVERIFY(server.start());
    QTest::qWait(100);
    ServerLog &slog = server._log;

    // 1. Fetch a thumbnail for page 1
    auto resp = get(
        QString("/thumbnail?path=%1&page=1&size=small").arg(fileName),
        30000);
    QVERIFY2(resp.ok(),
             qPrintable("Thumbnail fetch failed: " + resp.header));
    QVERIFY(resp.header.contains("image/jpeg"));
    QVERIFY2(resp.body.size() > 0, "Thumbnail should not be empty");
    QVERIFY2(resp.body.startsWith("\xff\xd8"),
             "Thumbnail should be a valid JPEG");
    QVERIFY(slog.next(ServerLog::Thumbnail, 1));

    // 2. Page count — File class loads directly, no ConvertToPdf needed
    resp = get(
        QString("/file?path=%1&pages=true").arg(fileName), 30000);
    QVERIFY(resp.ok());
    QVERIFY2(resp.body.contains("\"pages\":100"),
             qPrintable("Expected 100 pages, got: " +
                        QString::fromUtf8(resp.body)));
    QVERIFY(slog.next(ServerLog::PageCount, 100));

    // 3. Extract page 10 — File class converts to PDF in-process
    qint64 pageSize;
    verifyPageFetch(slog, fileName, 10, ServerLog::PageExtract, &pageSize, 30000);
    QVERIFY2(pageSize < fullFileSize / 5,
             qPrintable(QString("Page 10 (%1 bytes) should be < 1/5 of full "
                                "file (%2)")
                       .arg(pageSize).arg(fullFileSize)));

    // 4. Request page 10 again — should hit cache
    verifyPageFetch(slog, fileName, 10, ServerLog::PageCacheHit);

    // 5. Extract page 50
    verifyPageFetch(slog, fileName, 50, ServerLog::PageExtract, nullptr, 30000);

    QVERIFY(slog.end());
    server.stop();
}

void TestSearchServer::testMaxPageJpegCompression()
{
    /*
     * Verify that per-page PDFs use JPEG (DCTDecode) for greyscale
     * pages.  Uses greyscale_gradient.jpg which is a true 8-bit
     * greyscale image that goes through convertPageWithFile().
     */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QString fileName = "greyscale_gradient.jpg";
    qint64 fullFileSize = copyTestFile(fileName, tmpDir.path());
    QVERIFY(fullFileSize > 0);

    clearCaches();

    SearchServer server(tmpDir.path(), PORT, nullptr, true);
    QVERIFY(server.start());
    QTest::qWait(100);
    ServerLog &slog = server._log;

    // Extract the single page — should be JPEG-compressed
    qint64 pageSize;
    verifyPageFetch(slog, fileName, 1, ServerLog::PageExtract,
                    &pageSize, 30000);

    /*
     * The source is a 2400x3300 greyscale JPEG (262 KB).  With
     * FlateDecode the uncompressed 8-bit raster would be ~7.9 MB
     * in the PDF.  JPEG q80 should keep it well under 800 KB.
     */
    QVERIFY2(pageSize < 800 * 1024,
             qPrintable(QString("Greyscale page should be JPEG-compressed "
                                "(got %1 bytes, expected < 800 KB)")
                       .arg(pageSize)));

    /*
     * The PDF stream should contain the DCTDecode filter name,
     * confirming JPEG encoding rather than FlateDecode.
     */
    auto resp = get(QString("/file?path=%1&page=1").arg(fileName),
                    30000);
    QVERIFY(resp.ok());
    QVERIFY2(resp.body.contains("DCTDecode"),
             "Page PDF should contain DCTDecode filter for greyscale pages");

    QVERIFY(slog.next(ServerLog::PageCacheHit, 1));
    QVERIFY(slog.end());
    server.stop();
}
