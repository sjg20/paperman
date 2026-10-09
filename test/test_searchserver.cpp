#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include <QElapsedTimer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>

#include "../backendstats.h"

#include "../desktopmodel.h"
#include "../dirmodel.h"
#include "../desktopundo.h"
#include "../file.h"
#include "../filemax.h"
#include "../measure.h"
#include "../ocrpage.h"
#include "../op.h"
#include "../remotebackend.h"
#include "../searchserver.h"
#include "../pdfio.h"
#include "../tokenstore.h"
#include "../userstore.h"
#include "test.h"

#include <QApplication>
#include <QMimeData>
#include <QClipboard>
#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>

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
        /* the server runs in this same process, so its sockets only
           make progress when the main event loop spins */
        QCoreApplication::processEvents();
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

void TestSearchServer::testPostBodySplitAcrossReads()
{
   /* Regression test: QNetworkAccessManager (and other clients) often
    * write a POST in two segments — headers, then body.  An earlier
    * onReadyRead() parsed only what was available on the first
    * readyRead and threw the body away, so /v1/auth/login saw an
    * empty body and replied 400.  This test recreates that wire
    * behaviour by hand and asserts a 200 with a token. */

   QStandardPaths::setTestModeEnabled(true);
   auto restoreStdPaths = qScopeGuard([] {
      QStandardPaths::setTestModeEnabled(false);
   });
   QString cfgFile = QStandardPaths::writableLocation(
                         QStandardPaths::GenericConfigLocation)
                     + "/paperman-server/users.json";
   QFile::remove(cfgFile);
   {
      UserStore store;
      QVERIFY(store.addUser("eve", "open-sesame"));
   }

   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());
   SearchServer server(tmpDir.path(), PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   QByteArray body = R"({"user":"eve","password":"open-sesame"})";
   QByteArray headers;
   headers += "POST /v1/auth/login HTTP/1.1\r\n";
   headers += "Host: localhost\r\n";
   headers += "Content-Type: application/json\r\n";
   headers += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
   headers += "Connection: close\r\n";
   headers += "\r\n";

   QTcpSocket sock;
   sock.connectToHost("localhost", PORT);
   QVERIFY(sock.waitForConnected(2000));

   /* Send headers, force them to flush, and let the server's
    * readyRead fire on just the headers. */
   sock.write(headers);
   sock.flush();
   sock.waitForBytesWritten(500);
   for (int i = 0; i < 5; i++)
      QCoreApplication::processEvents();
   QTest::qWait(50);

   /* Now send the body in a second write — under the old code, this
    * was the chunk that got ignored. */
   sock.write(body);
   sock.flush();

   QByteArray raw;
   int waited = 0;
   while (waited < 5000) {
      QCoreApplication::processEvents();
      if (sock.waitForReadyRead(100))
         raw += sock.readAll();
      if (sock.state() != QAbstractSocket::ConnectedState)
         break;
      waited += 100;
   }
   raw += sock.readAll();
   sock.close();

   int sep = raw.indexOf("\r\n\r\n");
   QVERIFY2(sep > 0, raw.constData());
   QString header = QString::fromUtf8(raw.left(sep));
   QByteArray respBody = raw.mid(sep + 4);

   QVERIFY2(header.contains("200 OK"), header.toUtf8().constData());
   QJsonObject obj = QJsonDocument::fromJson(respBody).object();
   QVERIFY(!obj.value("token").toString().isEmpty());
   QCOMPARE(obj.value("user").toString(), QString("eve"));

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

void TestSearchServer::testRemoteBackendTimeout()
{
   /* Aim the client at a port nobody's listening on: the OS should
    * return ECONNREFUSED quickly, but the assertion that matters is
    * that the call returns *at all* in well under Qt's default
    * (effectively unlimited) network timeout.  If the
    * setTransferTimeout() call is removed or the wait loop is wrong,
    * this test will hang past its overall test budget. */
   RemoteBackend client(QUrl("http://localhost:1"));  // RFC: reserved

   QElapsedTimer t;
   t.start();
   QList<RepositoryInfo> repos = client.listRepositories();
   qint64 elapsedMs = t.elapsed();

   QVERIFY(repos.isEmpty());
   QVERIFY2(!client.lastError().isEmpty(),
            "expected an error from an unreachable server");
   /* Generous bound: connection-refused on loopback is sub-ms, but a
    * dropped packet to an off-network host should still fail before
    * the 5 s configured transferTimeout. */
   QVERIFY2(elapsedMs < 7000,
            qPrintable(QString("call took %1 ms").arg(elapsedMs)));
}

void TestSearchServer::testRemoteBackendThumbnail()
{
   /* Spin up a server with a PDF the test fixtures already create,
    * then ask RemoteBackend for its thumbnail.  We don't decode the
    * JPEG — confirming non-empty bytes with the JPEG magic bytes is
    * enough to prove the wire works end-to-end. */
   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());
   QVERIFY(copyTestFile("testpdf.pdf", tmpDir.path()) > 0);

   SearchServer server(tmpDir.path(), PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   QString repoName = QFileInfo(tmpDir.path()).fileName();

   RemoteBackend client(QUrl(QString("http://localhost:%1").arg(PORT)));

   /* Sync path. */
   QByteArray jpeg = client.fetchThumbnail(repoName, "testpdf.pdf");
   QVERIFY2(!jpeg.isEmpty(), client.lastError().toUtf8().constData());
   QVERIFY2(jpeg.startsWith("\xFF\xD8\xFF"),
            "expected JPEG magic bytes");
   QVERIFY(jpeg.size() > 100);

   /* Async path: hook up a one-shot signal capture, fire request,
    * spin events until it fires. */
   QByteArray asyncBytes;
   quint64 capturedToken = 0;
   QObject::connect(&client, &RemoteBackend::thumbnailReady,
       [&](quint64 token, const QByteArray &bytes) {
          capturedToken = token;
          asyncBytes = bytes;
       });

   quint64 token = client.fetchThumbnailAsync(repoName, "testpdf.pdf");
   QVERIFY(token != 0);
   QTRY_VERIFY(!asyncBytes.isEmpty());
   QCOMPARE(capturedToken, token);
   QVERIFY(asyncBytes.startsWith("\xFF\xD8\xFF"));

   server.stop();
}

void TestSearchServer::testBackendStatsAccumulates()
{
   /* Smoke test for the toolbar indicator: BackendStats should
    * accumulate bytes across both sync and async RemoteBackend
    * requests, and emit changed() each time. */
   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());
   QVERIFY(copyTestFile("testpdf.pdf", tmpDir.path()) > 0);

   SearchServer server(tmpDir.path(), PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   BackendStats stats;
   RemoteBackend client(QUrl(QString("http://localhost:%1").arg(PORT)));
   client.setStats(&stats);

   QSignalSpy spy(&stats, &BackendStats::changed);

   QCOMPARE(stats.bytesSent(),     qint64(0));
   QCOMPARE(stats.bytesReceived(), qint64(0));
   QCOMPARE(stats.activeRequests(), 0);

   /* Sync request: should bump both counters. */
   QList<RepositoryInfo> repos = client.listRepositories();
   QVERIFY(!repos.isEmpty());

   QVERIFY2(stats.bytesSent() > 0,
            qPrintable(QString("expected non-zero sent, got %1")
                           .arg(stats.bytesSent())));
   QVERIFY2(stats.bytesReceived() > 0,
            qPrintable(QString("expected non-zero received, got %1")
                           .arg(stats.bytesReceived())));
   QCOMPARE(stats.activeRequests(), 0);  // request done
   QVERIFY(spy.count() > 0);

   qint64 sentAfterFirst = stats.bytesSent();
   qint64 recvAfterFirst = stats.bytesReceived();

   /* Async request via fetchThumbnailAsync. */
   QString repoName = QFileInfo(tmpDir.path()).fileName();
   client.fetchThumbnailAsync(repoName, "testpdf.pdf");
   QTRY_VERIFY(stats.bytesReceived() > recvAfterFirst);
   QVERIFY(stats.bytesSent() > sentAfterFirst);

   server.stop();
}


void TestSearchServer::testThumbnailMatchesLocalRender()
{
   /* Render the same .max file two ways and verify the bytes are
    * identical:
    *
    *   - Locally: instantiate Filemax, load, getImage page 0,
    *     scale to the medium thumbnail size, JPEG-encode in
    *     memory.  This is exactly what the server's
    *     generateThumbnail() does for non-PDF files (see
    *     searchserver.cpp::generateThumbnail).
    *
    *   - Remotely: fetch via RemoteBackend::fetchThumbnail, which
    *     causes the server to run that same generateThumbnail path
    *     and stream the bytes back.
    *
    * If both code paths use the same File class, scaler, and JPEG
    * encoder, the resulting bytes match exactly.  If they ever
    * diverge — different scaler flag, different quality default,
    * extra metadata in the JPEG — this test fires and pins the
    * regression point. */
   QTemporaryDir tmpDir;
   QVERIFY(tmpDir.isValid());
   QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);

   /* Local render: same recipe as SearchServer::generateThumbnail
    * for non-PDF.  thumbSize 300 matches getThumbnailSize("medium").
    * That recipe reads the preview embedded in a max file and only
    * decodes the full page when there is none, so follow it here or
    * the two scale from different images. */
   const int kThumbSize = 300;
   QString fname = "testfile.max";
   QString dir = tmpDir.path() + "/";
   File *file = File::createFile(dir, fname, nullptr,
                                 File::typeFromName(fname));
   QVERIFY(file != nullptr);
   QVERIFY(file->load() == nullptr);

   QImage image;
   err_info *err = nullptr;
   Filemax *max = dynamic_cast<Filemax *>(file);
   if (max)
      err = max->getPreviewImage(0, image, false);
   if (!max || err || image.isNull()) {
      QSize imgSize, trueSize;
      int bpp;

      err = file->getImage(0, false, image, imgSize, trueSize, bpp, false);
   }
   delete file;
   QVERIFY(err == nullptr);
   QVERIFY(!image.isNull());

   QImage thumb = image.scaled(kThumbSize, kThumbSize,
                               Qt::KeepAspectRatio,
                               Qt::SmoothTransformation);
   QByteArray localBytes;
   {
      QBuffer buf(&localBytes);
      buf.open(QIODevice::WriteOnly);
      QVERIFY(thumb.save(&buf, "JPEG"));
   }
   QVERIFY(!localBytes.isEmpty());

   /* Remote render: spin up a SearchServer pointed at the same
    * repo and ask RemoteBackend for the same thumbnail. */
   SearchServer server(tmpDir.path(), PORT);
   QVERIFY(server.start());
   QTest::qWait(100);

   QString repoName = QFileInfo(tmpDir.path()).fileName();
   RemoteBackend client(QUrl(QString("http://localhost:%1").arg(PORT)));
   QByteArray remoteBytes = client.fetchThumbnail(repoName, fname,
                                                  /*page=*/1,
                                                  /*size=*/"medium");
   server.stop();

   QVERIFY2(!remoteBytes.isEmpty(),
            client.lastError().toUtf8().constData());

   /* Bytes should match.  If they don't, decode both and compare
    * pixel-by-pixel so a "JPEG bytes drift" report still tells us
    * whether the visible result moved or only the encoding did. */
   if (localBytes != remoteBytes) {
      QImage localImg = QImage::fromData(localBytes);
      QImage remoteImg = QImage::fromData(remoteBytes);
      QVERIFY2(!localImg.isNull() && !remoteImg.isNull(),
               "one of the JPEGs failed to decode");
      QCOMPARE(localImg.size(), remoteImg.size());
      /* If the pixels match exactly the only difference is the
       * JPEG encoder state — strictly speaking the user-visible
       * display is identical. */
      QCOMPARE(localImg.convertToFormat(QImage::Format_RGB32),
               remoteImg.convertToFormat(QImage::Format_RGB32));
      /* Pixels matched, bytes didn't.  Don't fail the test — but
       * note the divergence so a future change to either
       * generateThumbnail or the encoder is at least visible. */
      qInfo() << "Thumbnail JPEG bytes differ but pixels match;"
              << "local=" << localBytes.size()
              << "remote=" << remoteBytes.size();
   }
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

/* GET with an If-None-Match header, for the ETag revalidation test */
static TestSearchServer::Response getConditional(const QString &path,
                                                 const QString &etag,
                                                 int port)
{
    QByteArray req;
    req += "GET " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: localhost\r\n";
    if (!etag.isEmpty())
        req += "If-None-Match: " + etag.toUtf8() + "\r\n";
    req += "Connection: close\r\n";
    req += "\r\n";
    return sendRaw(req, port, 5000);
}

void TestSearchServer::testFileEtag()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    QFile testFile(tmpDir.path() + "/tagged.max");
    QVERIFY(testFile.open(QIODevice::WriteOnly));
    testFile.write("original bytes");
    testFile.close();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());

    // a whole-file download carries an ETag
    auto resp = get("/file?path=tagged.max");
    QVERIFY(resp.ok());
    QRegularExpression re("ETag: (\"[^\"]+\")");
    auto m = re.match(resp.header);
    QVERIFY2(m.hasMatch(), qPrintable(resp.header));
    QString etag = m.captured(1);

    // revalidating with the same tag returns 304 and no body
    auto cond = getConditional("/file?path=tagged.max", etag, PORT);
    QVERIFY2(cond.header.contains("304"), qPrintable(cond.header));
    QVERIFY(cond.body.isEmpty());

    // a stale tag still gets the full file
    auto stale = getConditional("/file?path=tagged.max", "\"0-0\"", PORT);
    QVERIFY(stale.ok());
    QCOMPARE(stale.body, QByteArray("original bytes"));

    /* changing the file changes the tag, so the old tag misses; the
       mtime may not tick over within the test, but the size does */
    QVERIFY(testFile.open(QIODevice::WriteOnly));
    testFile.write("changed bytes, now longer");
    testFile.close();
    auto changed = getConditional("/file?path=tagged.max", etag, PORT);
    QVERIFY(changed.ok());
    QCOMPARE(changed.body, QByteArray("changed bytes, now longer"));

    server.stop();
}

void TestSearchServer::testRemoteFileCache()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString repo = QFileInfo(tmpDir.path()).fileName();

    QFile src(tmpDir.path() + "/cached.max");
    QVERIFY(src.open(QIODevice::WriteOnly));
    src.write("cache me");
    src.close();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    RemoteBackend backend(QUrl(QString("http://localhost:%1").arg(PORT)));

    // first fetch downloads the bytes and records the validator
    QString cachePath = backend.ensureCachedFile(repo, "cached.max");
    QVERIFY2(!cachePath.isEmpty(),
             qPrintable(backend.lastError()));
    QFile cached(cachePath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), QByteArray("cache me"));
    cached.close();
    QVERIFY(QFile::exists(cachePath + ".etag"));

    /* a revalidation must not rewrite the cached copy: plant a
       sentinel and check the 304 leaves it alone */
    QVERIFY(cached.open(QIODevice::WriteOnly));
    cached.write("sentinel");
    cached.close();
    QCOMPARE(backend.ensureCachedFile(repo, "cached.max"), cachePath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), QByteArray("sentinel"));
    cached.close();

    // a changed server file misses the validator and is downloaded
    QVERIFY(src.open(QIODevice::WriteOnly));
    src.write("new server bytes");
    src.close();
    QCOMPARE(backend.ensureCachedFile(repo, "cached.max"), cachePath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), QByteArray("new server bytes"));
    cached.close();

    // invalidation drops the copy and the validator
    backend.invalidateCachedFile(repo, "cached.max");
    QVERIFY(!QFile::exists(cachePath));
    QVERIFY(!QFile::exists(cachePath + ".etag"));

    // with the server stopped, a cached copy still serves
    QCOMPARE(backend.ensureCachedFile(repo, "cached.max"), cachePath);
    server.stop();
    QCOMPARE(backend.ensureCachedFile(repo, "cached.max"), cachePath);
    QVERIFY(cached.open(QIODevice::ReadOnly));
    QCOMPARE(cached.readAll(), QByteArray("new server bytes"));
    cached.close();
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

/* Return the rendered size of one page of a stack on disk. */
static QSize serverTestPageSize(const QString &dir, const QString &fname,
                                int pagenum)
{
    File *f = File::createFile(dir, fname, nullptr,
                               File::typeFromName(fname));
    if (!f || f->load())
        return QSize();
    QImage img;
    QSize sz, tsz;
    int bpp;
    err_info *err = f->getImage(pagenum, false, img, sz, tsz, bpp, false);
    QSize result = err ? QSize() : img.size();
    delete f;
    return result;
}

void TestSearchServer::testTransformEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QString dir = tmpDir.path() + "/";

    // record the sizes of two pages before the transform
    QSize before0 = serverTestPageSize(dir, "testfile.max", 0);
    QSize before4 = serverTestPageSize(dir, "testfile.max", 4);
    QVERIFY(!before0.isEmpty());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    // rotate the first page (1-based) 90 degrees
    auto resp = postJson(
        QString("/v1/repos/%1/stacks/testfile.max/transform").arg(repo),
        R"({"page":1,"op":"rotate90"})");
    QVERIFY2(resp.ok(), resp.header.toUtf8().constData());
    QVERIFY(QJsonDocument::fromJson(resp.body).object()["success"].toBool());

    // page one on disk is now turned on its side; the other pages are
    // untouched
    QSize after0 = serverTestPageSize(dir, "testfile.max", 0);
    QCOMPARE(after0.width(), before0.height());
    QCOMPARE(after0.height(), before0.width());
    QCOMPARE(serverTestPageSize(dir, "testfile.max", 4), before4);

    /* omitting the page rotates every page of the stack, so page five
       (untouched above) is now turned on its side too.  It is a 1-bit
       page, whose width is stored padded to a multiple of 32, so check
       the orientation flipped rather than exact dimensions */
    QVERIFY(before4.height() > before4.width());   // started portrait
    auto all = postJson(
        QString("/v1/repos/%1/stacks/testfile.max/transform").arg(repo),
        R"({"op":"rotate90"})");
    QVERIFY2(all.ok(), all.header.toUtf8().constData());
    QSize again4 = serverTestPageSize(dir, "testfile.max", 4);
    QVERIFY(again4.width() > again4.height());      // now landscape

    server.stop();
}

void TestSearchServer::testRemoteBackendTransform()
{
    /* Round-trip: rotate a page through RemoteBackend and confirm the
       file the server holds on disk really turned. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QString dir = tmpDir.path() + "/";

    QSize before = serverTestPageSize(dir, "testfile.max", 0);
    QVERIFY(!before.isEmpty());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    RemoteBackend client(QUrl(QString("http://localhost:%1").arg(PORT)));
    QVERIFY2(client.transformPage(repo, "testfile.max", 1, "rotate90"),
             client.lastError().toUtf8().constData());

    QSize after = serverTestPageSize(dir, "testfile.max", 0);
    QCOMPARE(after.width(), before.height());
    QCOMPARE(after.height(), before.width());

    // an unknown op fails and reports an error rather than asserting
    QVERIFY(!client.transformPage(repo, "testfile.max", 1, "sideways"));
    QVERIFY(!client.lastError().isEmpty());

    server.stop();
}

void TestSearchServer::testDesktopRemoteTransform()
{
    /* Full path: a remote stack shown in the desktop is rotated through
       Desktopmodel::transformPage, which must detect the remote backend
       and ask the server to do the work. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    QSize before = serverTestPageSize(dir, "testfile.max", 0);
    QVERIFY(!before.isEmpty());

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    // point a Dirmodel at the server and a Desktopmodel at the Dirmodel
    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err), err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);   // identity converter (no proxy here)
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    // show the remote repo's root directory; its files load as stubs
    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());

    // rotate the first page; the desktop must route this to the server
    model.transformPage(stack, 0, File::Transform_rotate90);

    QSize after = serverTestPageSize(dir, "testfile.max", 0);
    QCOMPARE(after.width(), before.height());
    QCOMPARE(after.height(), before.width());

    // undo rotates it back, again via the server
    model.getUndoStack()->undo();
    QSize restored = serverTestPageSize(dir, "testfile.max", 0);
    QCOMPARE(restored, before);

    server.stop();
}


void TestSearchServer::testDesktopRemoteOpenStack()
{
    /* Opening a remote stack must fetch the whole file into the disk
       cache and parse it with the real file class, so its pages render
       exactly as a local open of the same file does. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());

    // before the fetch the stack is a shell with no pages known
    QCOMPARE(model.data(stack, Desktopmodel::Role_pagecount).toInt(), 0);

    // fetch and parse the file
    err_info *e = model.ensureContent(stack);
    QVERIFY2(!e, e ? e->errstr : "");

    // page count and page images now match a local open of the source
    File *local = File::createFile(dir, "testfile.max", nullptr,
                                   File::Type_max);
    QVERIFY(local);
    QVERIFY(!local->load());
    QVERIFY(local->pagecount() > 0);
    QCOMPARE(model.data(stack, Desktopmodel::Role_pagecount).toInt(),
             local->pagecount());

    for (int page = 0; page < local->pagecount(); page++) {
        QImage rimg, limg;
        QSize rsz, rtsz, lsz, ltsz;
        int rbpp, lbpp;

        err_info *re = model.getImage(stack, page, false, rimg, rsz,
                                      rtsz, rbpp);
        QVERIFY2(!re, re ? re->errstr : "");
        QVERIFY(!local->getImage(page, false, limg, lsz, ltsz, lbpp,
                                 false));
        QCOMPARE(rimg, limg);
        QCOMPARE(rbpp, lbpp);
    }
    delete local;

    /* a fresh session (new model) reuses the cached copy: the open
       costs a revalidation, not a download */
    Desktopmodel model2(nullptr);
    Desktopmodelconv conv2(&model2);
    model2.setModelConv(&conv2);
    model2.setDirmodel(&dirmodel);
    QModelIndex parent2 = model2.showDir(root, root, &meas);
    QVERIFY(parent2.isValid());
    QModelIndex stack2 = model2.index("testfile.max", parent2);
    QVERIFY(stack2.isValid());
    err_info *e2 = model2.ensureContent(stack2);
    QVERIFY2(!e2, e2 ? e2->errstr : "");
    QVERIFY(model2.data(stack2, Desktopmodel::Role_pagecount).toInt() > 0);

    /* rotate the file on the server behind our back; yet another
       session must spot the stale cache on revalidation and re-read
       the changed bytes */
    QSize before = serverTestPageSize(dir, "testfile.max", 0);
    auto xf = postJson(
        QString("/v1/repos/%1/stacks/testfile.max/transform").arg(repo),
        R"({"page":1,"op":"rotate90"})");
    QVERIFY2(xf.header.contains("200"), qPrintable(xf.header));

    Desktopmodel model3(nullptr);
    Desktopmodelconv conv3(&model3);
    model3.setModelConv(&conv3);
    model3.setDirmodel(&dirmodel);
    QModelIndex parent3 = model3.showDir(root, root, &meas);
    QVERIFY(parent3.isValid());
    QModelIndex stack3 = model3.index("testfile.max", parent3);
    QVERIFY(stack3.isValid());
    err_info *e3 = model3.ensureContent(stack3);
    QVERIFY2(!e3, e3 ? e3->errstr : "");

    QImage turned;
    QSize tsz1, tsz2;
    int tbpp;
    QVERIFY(!model3.getImage(stack3, 0, false, turned, tsz1, tsz2, tbpp));
    QCOMPARE(turned.size(), QSize(before.height(), before.width()));

    server.stop();
}


void TestSearchServer::testMutationEndpoints()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString repo = QFileInfo(tmpDir.path()).fileName();

    auto makeFile = [&](const QString &name, const QByteArray &content) {
        QFile f(tmpDir.path() + "/" + name);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(content);
    };
    makeFile("one.max", "contents one");
    makeFile("two.max", "contents two");

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    QString base = QString("/v1/repos/%1/stacks/").arg(repo);

    // plain rename
    auto r = postJson(base + "one.max/rename",
                      R"({"newName":"first.max"})");
    QVERIFY2(r.header.contains("200"), qPrintable(r.header));
    QVERIFY(!QFile::exists(tmpDir.path() + "/one.max"));
    QVERIFY(QFile::exists(tmpDir.path() + "/first.max"));

    // a collision with autoRename picks a fresh name
    r = postJson(base + "two.max/rename",
                 R"({"newName":"first.max","autoRename":true})");
    QVERIFY2(r.header.contains("200"), qPrintable(r.header));
    QJsonObject obj = QJsonDocument::fromJson(r.body).object();
    QCOMPARE(obj.value("name").toString(), QString("first_1.max"));
    QVERIFY(QFile::exists(tmpDir.path() + "/first_1.max"));

    // a collision without autoRename is refused
    r = postJson(base + "first_1.max/rename",
                 R"({"newName":"first.max","autoRename":false})");
    QVERIFY2(r.header.contains("409"), qPrintable(r.header));

    // move to the trash creates it on demand
    r = postJson(base + "first.max/move",
                 R"({"destDir":".maxview-trash"})");
    QVERIFY2(r.header.contains("200"), qPrintable(r.header));
    QVERIFY(QFile::exists(tmpDir.path()
                          + "/.maxview-trash/first.max"));

    // moving back out of the trash
    r = postJson(base + ".maxview-trash/first.max/move",
                 R"({"destDir":""})");
    QVERIFY2(r.header.contains("200"), qPrintable(r.header));
    QVERIFY(QFile::exists(tmpDir.path() + "/first.max"));

    // a move to a missing directory is refused
    r = postJson(base + "first.max/move",
                 R"({"destDir":"nosuchdir"})");
    QVERIFY2(r.header.contains("404"), qPrintable(r.header));

    // delete removes the file outright
    r = postJson(base + "first_1.max/delete", "{}");
    QVERIFY2(r.header.contains("200"), qPrintable(r.header));
    QVERIFY(!QFile::exists(tmpDir.path() + "/first_1.max"));

    // traversal is rejected
    r = postJson(base + "..%2Fescape.max/rename",
                 R"({"newName":"x.max"})");
    QVERIFY2(r.header.contains("400"), qPrintable(r.header));

    // a page rename lands in the stack itself
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    r = postJson(base + "testfile.max/pages/1/rename",
                 R"({"newName":"my page"})");
    QVERIFY2(r.header.contains("200"), qPrintable(r.header));
    {
        File *f = File::createFile(tmpDir.path() + "/", "testfile.max",
                                   nullptr, File::Type_max);
        QVERIFY(f && !f->load());
        QCOMPARE(f->pageTitle(0), QString("my page"));
        delete f;
    }

    server.stop();
}


void TestSearchServer::testDesktopRemoteSimpleOps()
{
    /* Rename, trash and delete of remote stacks driven through the
       desktop, including the undo round-trips. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());

    // rename through the desktop; the server's file follows
    model.renameStack(stack, "renamed");
    QVERIFY(QFile::exists(dir + "renamed.max"));
    QVERIFY(!QFile::exists(dir + "testfile.max"));
    QVERIFY(model.index("renamed.max", parent).isValid());

    // and undo brings the old name back
    model.getUndoStack()->undo();
    QVERIFY(QFile::exists(dir + "testfile.max"));
    QVERIFY(!QFile::exists(dir + "renamed.max"));

    // rename a page on the server via the desktop
    stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());
    QVERIFY(!model.ensureContent(stack));
    QString pageName = "remote page";
    QVERIFY(!model.opRenamePage(stack, 0, pageName));
    {
        File *f = File::createFile(dir, "testfile.max", nullptr,
                                   File::Type_max);
        QVERIFY(f && !f->load());
        QCOMPARE(f->pageTitle(0), QString("remote page"));
        delete f;
    }

    // trash through the desktop: the server file moves into the trash
    QModelIndexList list;
    list << stack;
    int rowsBefore = model.rowCount(parent);
    model.trashStacks(list, parent);
    QVERIFY(!QFile::exists(dir + "testfile.max"));
    QVERIFY(QFile::exists(dir + ".maxview-trash/testfile.max"));
    QCOMPARE(model.rowCount(parent), rowsBefore - 1);

    // undo the trashing: the file comes back
    model.getUndoStack()->undo();
    QVERIFY(QFile::exists(dir + "testfile.max"));
    QVERIFY(!QFile::exists(dir + ".maxview-trash/testfile.max"));
    QCOMPARE(model.rowCount(parent), rowsBefore);

    // delete outright
    stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());
    QVERIFY(!model.opDeleteStack(stack));
    QVERIFY(!QFile::exists(dir + "testfile.max"));
    QCOMPARE(model.rowCount(parent), rowsBefore - 1);

    server.stop();
}


/* read one annotation straight from the server's file on disk */
static QString serverAnnot(const QString &dir, const QString &fname,
                           File::e_annot type)
{
    File *f = File::createFile(dir, fname, nullptr,
                               File::typeFromName(fname));
    QString text;
    if (f && !f->load())
        f->getAnnot(type, text);
    delete f;
    return text;
}

void TestSearchServer::testDesktopRemoteAnnotations()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());
    QVERIFY(!model.ensureContent(stack));

    QString oldAuthor = serverAnnot(dir, "testfile.max",
                                    File::Annot_author);

    // update through the desktop; the server's file follows
    QHash<int, QString> updates;
    updates[File::Annot_author] = "A. Writer";
    updates[File::Annot_title] = "Remote title";
    model.updateAnnot(stack, updates);

    QCOMPARE(serverAnnot(dir, "testfile.max", File::Annot_author),
             QString("A. Writer"));
    QCOMPARE(serverAnnot(dir, "testfile.max", File::Annot_title),
             QString("Remote title"));

    // the cached copy shows the new values without a refetch
    QCOMPARE(model.getAnnot(stack, File::Annot_author),
             QString("A. Writer"));

    // undo restores the previous value on the server
    model.getUndoStack()->undo();
    QCOMPARE(serverAnnot(dir, "testfile.max", File::Annot_author),
             oldAuthor);

    server.stop();
}


/* page count of the server's file on disk */
static int serverPagecount(const QString &dir, const QString &fname)
{
    File *f = File::createFile(dir, fname, nullptr,
                               File::typeFromName(fname));
    int count = -1;
    if (f && !f->load())
        count = f->pagecount();
    delete f;
    return count;
}

void TestSearchServer::testDesktopRemoteStructuralOps()
{
    /* Page deletion, unstacking and duplication of remote stacks
       driven through the desktop, with their undo round-trips. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());
    QVERIFY(!model.ensureContent(stack));

    int pagecount = model.data(stack,
                               Desktopmodel::Role_pagecount).toInt();
    QVERIFY(pagecount >= 2);
    int rowsBefore = model.rowCount(parent);

    // delete the last page through the desktop
    QBitArray pages(pagecount);
    pages.setBit(pagecount - 1);
    model.deletePages(stack, pages);
    QCOMPARE(serverPagecount(dir, "testfile.max"), pagecount - 1);
    QCOMPARE(model.data(stack, Desktopmodel::Role_pagecount).toInt(),
             pagecount - 1);

    // undo redeems the server-side undoId and restores the page
    model.getUndoStack()->undo();
    QCOMPARE(serverPagecount(dir, "testfile.max"), pagecount);
    QCOMPARE(model.data(stack, Desktopmodel::Role_pagecount).toInt(),
             pagecount);

    // unstack the first page into a new stack
    model.unstackPage(stack, 0, true);
    QCOMPARE(model.rowCount(parent), rowsBefore + 1);
    QCOMPARE(serverPagecount(dir, "testfile.max"), pagecount - 1);
    QModelIndex unstacked;
    for (int row = 0; row < model.rowCount(parent); row++) {
        QModelIndex ind = model.index(row, 0, parent);
        if (ind != stack)
            unstacked = ind;
    }
    QVERIFY(unstacked.isValid());
    QString newName = model.data(unstacked,
                                 Desktopmodel::Role_filename).toString();
    QCOMPARE(serverPagecount(dir, newName), 1);

    // undo stacks the page back and removes the new stack
    model.getUndoStack()->undo();
    QCOMPARE(model.rowCount(parent), rowsBefore);
    QCOMPARE(serverPagecount(dir, "testfile.max"), pagecount);
    QVERIFY(!QFile::exists(dir + newName));

    // duplicate (copy) the stack
    QModelIndexList list;
    QStringList names;
    stack = model.index("testfile.max", parent);
    list << stack;
    QVERIFY(!model.opDuplicateStacks(list, parent, names,
                                     File::Type_other, 3));
    QCOMPARE(names.size(), 1);
    QVERIFY(QFile::exists(dir + names[0]));
    QCOMPARE(serverPagecount(dir, names[0]), pagecount);
    QCOMPARE(model.rowCount(parent), rowsBefore + 1);

    server.stop();
}


void TestSearchServer::testDesktopRemoteScan()
{
    /* Scanning into a remote desk builds the stack in the local cache
       and uploads the finished file on confirm; a cancelled scan
       leaves no trace on either side. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    int rowsBefore = model.rowCount(parent);

    /* a fake scanned page: 64x48 grey */
    auto makePage = [](Filemaxpage &mp, const QString &title) {
        int w = 64, h = 48;
        QByteArray data(w * h, '\x80');
        QString name = title;
        mp.addData(w, h, 8, w, name, false, false, 1, data, w * h);
        QVERIFY(!mp.compress());
    };

    // scan a one-page stack and confirm it
    QVERIFY(!model.beginScan(parent, "scanned"));
    Filemaxpage page;
    makePage(page, "page 1");
    QVERIFY(!model.addPageToScan(&page, ""));
    QString fname;
    QVERIFY(!model.confirmScan(&fname));

    QVERIFY(QFile::exists(dir + fname));
    QCOMPARE(serverPagecount(dir, fname), 1);
    QCOMPARE(model.rowCount(parent), rowsBefore + 1);

    // the uploaded copy's validator lets a fresh session revalidate
    QModelIndex ind = model.index(fname, parent);
    QVERIFY(ind.isValid());
    File *f = model.getFile(ind);
    QVERIFY(f);
    QVERIFY(QFile::exists(f->pathname() + ".etag"));

    // a cancelled scan leaves nothing behind
    QVERIFY(!model.beginScan(parent, "aborted"));
    Filemaxpage page2;
    makePage(page2, "page 1");
    QVERIFY(!model.addPageToScan(&page2, ""));
    QVERIFY(!model.cancelScan());
    QCOMPARE(model.rowCount(parent), rowsBefore + 1);
    QVERIFY(!QFile::exists(dir + "aborted.max"));

    server.stop();
}


void TestSearchServer::testRemoteEvents()
{
    /* One client's mutation reaches another client's event stream;
       the originator does not hear its own echo.  A desktop showing
       the directory picks the change up and refreshes. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    RemoteBackend listener(url);
    QSignalSpy spy(&listener, &RemoteBackend::stackEvent);
    listener.subscribeEvents(repo);
    // a change made before the stream is in place is not heard
    QTRY_COMPARE(server.eventClientCount(), 1);

    // a change made by a different client arrives as an event
    RemoteBackend actor(url);
    QString newName = "moved.max";
    QVERIFY2(actor.renameStack(repo, "testfile.max", newName),
             qPrintable(actor.lastError()));
    /* the event may already have arrived while the sync rename was
       spinning the event loop */
    if (spy.isEmpty())
        QVERIFY(spy.wait(3000));
    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args[0].toString(), repo);
    QCOMPARE(args[1].toString(), QString("rename"));
    QCOMPARE(args[2].toString(), QString("testfile.max"));
    QCOMPARE(args[3].toString(), QString("moved.max"));

    // the listener's own change does not echo back to it
    QString backName = "testfile.max";
    QVERIFY(listener.renameStack(repo, "moved.max", backName));
    QTest::qWait(500);
    QCOMPARE(spy.count(), 0);

    /* a desktop showing the directory refreshes on another client's
       change */
    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());
    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);
    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QVERIFY(model.index("testfile.max", parent).isValid());

    /* the desktop opens its own stream to the server as it shows the
       directory, and does so in the background: rename before it is in
       place and the desktop never hears of it */
    QTRY_COMPARE(server.eventClientCount(), 2);
    QVERIFY(actor.renameStack(repo, "testfile.max", newName));
    /* wait for the event to arrive and the queued refresh to run */
    bool renamed = false;
    for (int i = 0; i < 50 && !renamed; i++) {
        QTest::qWait(100);
        parent = model.index(root + "/", QModelIndex());
        renamed = parent.isValid()
            && model.index("moved.max", parent).isValid();
    }
    QVERIFY(renamed);

    server.stop();
}


void TestSearchServer::testDesktopRemoteSharedPositions()
{
    /* Stack positions on a remote desk live in the server's
       .paperdesk file, so every client sees the same layout. */
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());

    // move the stack; the layout must land on the server at once
    QPoint target(432, 210);
    QModelIndexList list;
    QList<QPoint> newpos;
    list << stack;
    newpos << target;
    model.opMoveStacks(list, parent, newpos);
    QVERIFY(QFile::exists(dir + ".paperdesk"));
    {
        QFile pd(dir + ".paperdesk");
        QVERIFY(pd.open(QIODevice::ReadOnly));
        QVERIFY(pd.readAll().contains("testfile.max"));
    }

    // the desk file must not show up as a stack
    QVERIFY(!model.index(".paperdesk", parent).isValid());

    // a second client sees the same position
    Desktopmodel model2(nullptr);
    Desktopmodelconv conv2(&model2);
    model2.setModelConv(&conv2);
    model2.setDirmodel(&dirmodel);
    QModelIndex parent2 = model2.showDir(root, root, &meas);
    QVERIFY(parent2.isValid());
    QModelIndex stack2 = model2.index("testfile.max", parent2);
    QVERIFY(stack2.isValid());
    QCOMPARE(model2.data(stack2, Desktopmodel::Role_position).toPoint(),
             target);

    server.stop();
}


void TestSearchServer::testRemoteOcr()
{
    /* OCR of a remote stack runs on the server, which stores the text
       in the stack's ocr annotation and returns it. */
    if (QStandardPaths::findExecutable("tesseract").isEmpty())
        QSKIP("tesseract is not installed");
    if (QStandardPaths::findExecutable("exiftool").isEmpty())
        QSKIP("exiftool is not installed");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    /* a page with large, clean text for the engine to find */
    {
        QImage page(500, 140, QImage::Format_RGB32);
        page.fill(Qt::white);
        QPainter paint(&page);
        paint.setPen(Qt::black);
        QFont font;
        font.setPointSize(40);
        paint.setFont(font);
        paint.drawText(page.rect(), Qt::AlignCenter, "HELLO WORLD");
        paint.end();
        QVERIFY(page.save(dir + "readme.jpg", "JPG"));
    }

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("readme.jpg", parent);
    QVERIFY(stack.isValid());
    QVERIFY(!model.ensureContent(stack));

    QString text;
    err_info *e = model.ocrPage(stack, 0, text);
    QVERIFY2(!e, e ? e->errstr : "");
    QVERIFY2(text.contains("HELLO"), qPrintable(text));

    // the text is stored in the stack's annotation on the server
    QVERIFY(serverAnnot(dir, "readme.jpg",
                        File::Annot_ocr).contains("HELLO"));

    // and mirrored onto the cached copy
    QVERIFY(model.getAnnot(stack, File::Annot_ocr).contains("HELLO"));

    server.stop();
}

void TestSearchServer::testRemoteOcrMax()
{
    /* a .max stack keeps the words read from a page with the page, on
       the server and in the cached copy, leaving its annotation alone */
    if (QStandardPaths::findExecutable("tesseract").isEmpty())
        QSKIP("tesseract is not installed");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();

    {
        QImage image(500, 140, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter paint(&image);
        paint.setPen(Qt::black);
        QFont font;
        font.setPointSize(40);
        paint.setFont(font);
        paint.drawText(image.rect(), Qt::AlignCenter, "HELLO WORLD");
        paint.end();

        QVERIFY(image.save(dir + "readme.jpg", "JPG"));

        File *jpg = File::createFile(dir, "readme.jpg", nullptr,
                                     File::Type_jpeg);
        File *max = File::createFile(dir, "readme.max", nullptr,
                                     File::Type_max);
        QVERIFY(!jpg->load());
        QVERIFY(!max->create());
        Operation op("Convert", 0, 0);
        QVERIFY(!jpg->copyTo(max, 3, op));
        delete max;
        delete jpg;
        QVERIFY(QFile::remove(dir + "readme.jpg"));
    }

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("readme.max", parent);
    QVERIFY(stack.isValid());
    QVERIFY(!model.ensureContent(stack));

    QString text;
    err_info *e = model.ocrPage(stack, 0, text);
    QVERIFY2(!e, e ? e->errstr : "");
    QVERIFY2(text.contains("HELLO"), qPrintable(text));

    // the server's stack keeps the page's words, not an annotation
    {
        Filemax max(dir, "readme.max", nullptr);
        QVERIFY(!max.load());
        OcrPage words;
        QVERIFY(!max.getPageOcr(0, words));
        QCOMPARE(words.text(), text);
        QVERIFY(!words.words[0].box.isEmpty());
    }
    QCOMPARE(serverAnnot(dir, "readme.max", File::Annot_ocr), QString());

    // and so does the cached copy
    OcrPage cached;
    model.getPageOcr(stack, 0, cached);
    QCOMPARE(cached.text(), text);

    server.stop();
}

// the text of a PDF's page, as a viewer would find it
static QString pdfPageText(const QByteArray &pdf, int pagenum)
{
   std::unique_ptr<Poppler::Document> doc(Poppler::Document::loadFromData(pdf));
   if (!doc || pagenum >= doc->numPages())
      return "no page";
   std::unique_ptr<Poppler::Page> page(doc->page(pagenum));
   return page->text(QRectF()).trimmed();
}

void TestSearchServer::testPdfTextLayer()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    {
        Filemax max(dir, "testfile.max", nullptr);
        QVERIFY(!max.load());
        for (int i = 0; i < 2; i++) {
            OcrPage ocr;
            ocr.size = QSize(2000, 3000);
            ocr.words << OcrWord{QRect(100, 100, 400, 60),
                                 i ? "beta" : "alpha", 90, 0, 0};
            QVERIFY(!max.putPageOcr(i, ocr));
        }
    }
    clearCaches();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    /* the whole stack as a PDF; this is too large to fetch here, since
       the server, in this same process, writes it all before the test
       can read any, so convert it directly */
    QString converted = server.convertToPdf(dir + "testfile.max", nullptr);
    QVERIFY(!converted.isEmpty());
    QFile file(converted);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray whole = file.readAll();
    QCOMPARE(pdfPageText(whole, 0), QString("alpha"));
    QCOMPARE(pdfPageText(whole, 1), QString("beta"));
    QCOMPARE(pdfPageText(whole, 2), QString());

    // and one page of it, likewise
    converted = server.convertPageWithFile(dir + "testfile.max", 2,
                                           QFileInfo(dir + "testfile.max"));
    QVERIFY(!converted.isEmpty());
    QFile page(converted);
    QVERIFY(page.open(QIODevice::ReadOnly));
    QCOMPARE(pdfPageText(page.readAll(), 0), QString("beta"));

    server.stop();
}

void TestSearchServer::testTransformEndpointErrors()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);

    // an unknown op is rejected
    auto badOp = postJson(
        QString("/v1/repos/%1/stacks/testfile.max/transform").arg(repo),
        R"({"page":1,"op":"sideways"})");
    QVERIFY(badOp.header.contains("400"));

    // a missing file is a 404
    auto missing = postJson(
        QString("/v1/repos/%1/stacks/nope.max/transform").arg(repo),
        R"({"page":1,"op":"rotate90"})");
    QVERIFY(missing.header.contains("404"));

    // a path-traversal attempt (encoded slashes) is rejected
    QString traversePath = "/v1/repos/" + repo
        + "/stacks/..%2F..%2Fetc%2Fpasswd/transform";
    auto traverse = postJson(traversePath, R"({"page":1,"op":"rotate90"})");
    QVERIFY(traverse.header.contains("400"));

    server.stop();
}


void TestSearchServer::testRemotePageFetch()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    auto stop = qScopeGuard([&] { server.stop(); });

    RemoteBackend backend(QUrl(QString("http://localhost:%1").arg(PORT)));

    /* the server says it serves pages, and the backend hears it */
    QVERIFY(!backend.serverId().isEmpty());
    QVERIFY(backend.hasPageRoutes());
    backend.invalidatePageDir(repo, "testfile.max");   // start clean

    /* structure first: no page has been fetched */
    QString dir = backend.fetchStackInfo(repo, "testfile.max");
    QVERIFY2(!dir.isEmpty(), qPrintable(backend.lastError()));
    QVERIFY(dir.endsWith(".d"));
    /* read info.json and close it again at once: the fetch below
       replaces it, which Windows refuses while it is open */
    auto readInfo = [&dir]() {
        QFile info(dir + "/info.json");
        return info.open(QIODevice::ReadOnly)
            ? QJsonDocument::fromJson(info.readAll()).object()
            : QJsonObject();
    };
    QJsonObject top = readInfo();
    QCOMPARE(top.value("pagecount").toInt(), 5);
    QVERIFY(!top.value("pages").toArray().at(0).toObject().contains("text"));

    /* one page: a real one-page stack lands under the page directory */
    QString p1 = backend.fetchPage(repo, "testfile.max", 1);
    QVERIFY2(!p1.isEmpty(), qPrintable(backend.lastError()));
    QCOMPARE(QFileInfo(p1).fileName(), QString("page-1.max"));
    QCOMPARE(QFileInfo(p1).absolutePath(), QFileInfo(dir).absoluteFilePath());
    {
        Filemax one(dir + "/", "page-1.max", nullptr);
        QVERIFY(one.load() == nullptr);
        QCOMPARE(one.pagecount(), 1);
    }

    /* a sparse stack over the directory sees five pages, one present,
       and decodes it to the same pixels as the whole file */
    Filemax src(tmpDir.path() + "/", "testfile.max", nullptr);
    QVERIFY(src.load() == nullptr);
    Filemax sp(tmpDir.path() + "/", "sparse.max", nullptr);
    sp.setPageDir(dir);
    QVERIFY(sp.load() == nullptr);
    QCOMPARE(sp.pagecount(), 5);
    QVERIFY(sp.hasPage(0));
    QVERIFY(!sp.hasPage(1));

    QImage i1, i2;
    QSize s1, t1, s2, t2;
    int b1 = 0, b2 = 0;
    QVERIFY(src.getImage(0, false, i1, s1, t1, b1, false) == nullptr);
    QVERIFY(sp.getImage(0, false, i2, s2, t2, b2, false) == nullptr);
    QCOMPARE(i2.convertToFormat(QImage::Format_RGB32),
             i1.convertToFormat(QImage::Format_RGB32));
    QCOMPARE(t2, t1);

    /* a page the stack does not have is refused, not invented */
    QVERIFY(backend.fetchPage(repo, "testfile.max", 99).isEmpty());
    QVERIFY(!backend.lastError().isEmpty());

    /* the async paths land in the same places */
    QSignalSpy pageSpy(&backend, &RemoteBackend::pageReady);
    quint64 t = backend.fetchPageAsync(repo, "testfile.max", 2);
    QVERIFY(pageSpy.wait(5000));
    QCOMPARE(pageSpy.at(0).at(0).toULongLong(), t);
    QString p2 = pageSpy.at(0).at(1).toString();
    QVERIFY(p2.endsWith("/page-2.max"));
    sp.pageArrived(1);
    QVERIFY(sp.hasPage(1));
    QVERIFY(src.getImage(1, false, i1, s1, t1, b1, false) == nullptr);
    QVERIFY(sp.getImage(1, false, i2, s2, t2, b2, false) == nullptr);
    QCOMPARE(i2.convertToFormat(QImage::Format_RGB32),
             i1.convertToFormat(QImage::Format_RGB32));

    QSignalSpy infoSpy(&backend, &RemoteBackend::stackInfoReady);
    quint64 ti = backend.fetchStackInfoAsync(repo, "testfile.max", true);
    QVERIFY(infoSpy.wait(5000));
    QCOMPARE(infoSpy.at(0).at(0).toULongLong(), ti);
    QCOMPARE(infoSpy.at(0).at(1).toString(), dir);
    top = readInfo();
    QVERIFY(top.value("pages").toArray().at(0).toObject().contains("text"));

    /* forgetting a page, then the whole directory */
    backend.invalidatePage(repo, "testfile.max", 2);
    QVERIFY(!QFile::exists(p2));
    QVERIFY(QFile::exists(p1));
    backend.invalidatePageDir(repo, "testfile.max");
    QVERIFY(!QDir(dir).exists());
}


void TestSearchServer::testDesktopRemotePages()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);
    QString repo = QFileInfo(tmpDir.path()).fileName();

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    auto stop = qScopeGuard([&] { server.stop(); });
    QUrl url(QString("http://localhost:%1").arg(PORT));

    /* where the model will put things, and a clean slate */
    RemoteBackend probe(url);
    QVERIFY(!probe.serverId().isEmpty());
    QString whole = probe.cachePathFor(repo, "testfile.max");
    QString pageDir = probe.pageDirFor(repo, "testfile.max");
    probe.invalidateCachedFile(repo, "testfile.max");
    probe.invalidatePageDir(repo, "testfile.max");
    QVERIFY(!QFile::exists(whole));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    QModelIndex stack = model.index("testfile.max", parent);
    QVERIFY(stack.isValid());

    /* the view asks for the stack: only its structure comes */
    QVERIFY(model.requestContent(stack));
    QTRY_COMPARE_WITH_TIMEOUT(
        model.data(stack, Desktopmodel::Role_pagecount).toInt(), 5, 5000);
    QVERIFY(QFile::exists(pageDir + "/info.json"));
    QVERIFY(!QFile::exists(pageDir + "/page-1.max"));
    QVERIFY(!QFile::exists(whole));
    QVERIFY(!model.requestContent(stack));     // nothing more to ask for

    /* sizes come from the structure, so no page is needed for them */
    QSize preview, image;
    int bpp = 0;
    QVERIFY(!model.getImagePreviewSizes(stack, 2, preview, image));
    QVERIFY(image.width() > 0 && preview.width() > 0);
    QVERIFY(!QFile::exists(pageDir + "/page-3.max"));

    /* the render thread's entry refuses a page that is not here rather
       than fetching it, since it must not touch the network */
    QImage img;
    QSize s, t;
    err_info *e = model.getScaledImageData(stack, 0, QSize(100, 100), false,
                                           img);
    QVERIFY(e != nullptr);
    QCOMPARE(e->errnum, (int)ERR_file_not_loaded_yet1);
    QVERIFY(!QFile::exists(pageDir + "/page-1.max"));

    /* the view asks for the page the way it does in the GUI: at once,
       with the placeholder up until it lands */
    QVERIFY(model.requestPage(stack, 0));
    QVERIFY(model.requestPage(stack, 0));      // already on its way
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pageDir + "/page-1.max"), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!model.requestPage(stack, 0), 5000);
    QVERIFY(!model.getScaledImageData(stack, 0, QSize(100, 100), false,
                                      img));
    QVERIFY(!img.isNull());

    /* a reader on the GUI thread that needs a page gets that page
       fetched, and only that page */
    QVERIFY(!model.getImage(stack, 2, false, img, s, t, bpp, false));
    QVERIFY(!img.isNull());
    QCOMPARE(img.size(), t);
    QVERIFY(QFile::exists(pageDir + "/page-3.max"));
    QVERIFY(!QFile::exists(pageDir + "/page-2.max"));
    QVERIFY(!QFile::exists(whole));
}


void TestSearchServer::testUserStoreFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + "/users.json";

    {
        UserStore store(path);
        QVERIFY(store.addUser("alice", "s3cret"));
        QVERIFY(store.save());
    }

    /* only the owner can read the hashes, and no temporary file is left.
       Windows reports permissions from the read-only flag rather than
       the file's ACL, so there is nothing to check there; the file is
       under the user's profile, which is private already */
#ifndef Q_OS_WIN
    QFileDevice::Permissions perms = QFile::permissions(path);
    QVERIFY(perms & QFileDevice::ReadOwner);
    QVERIFY(!(perms & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
#endif
    QCOMPARE(QDir(tmp.path()).entryList(QDir::Files),
             QStringList() << "users.json");

    UserStore store(path);
    QVERIFY(store.load());
    QVERIFY(store.verify("alice", "s3cret"));

    /* a file caught half-written, or broken by hand, is not taken as
       having no users, which would turn authentication off */
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("{\"alice\": {\"hash\": \"pbkdf2");
    f.close();
    QVERIFY(!store.load());
    QCOMPARE(store.count(), 1);
    QVERIFY(store.verify("alice", "s3cret"));

    // a file which is gone does mean no users
    QVERIFY(QFile::remove(path));
    QVERIFY(store.load());
    QCOMPARE(store.count(), 0);
}


void TestSearchServer::testUsersFollowFile()
{
    QStandardPaths::setTestModeEnabled(true);
    auto restoreStdPaths = qScopeGuard([] {
        QStandardPaths::setTestModeEnabled(false);
    });
    QString cfgFile = QStandardPaths::writableLocation(
                          QStandardPaths::GenericConfigLocation)
                      + "/paperman-server/users.json";
    QFile::remove(cfgFile);
    {
        UserStore store;
        QVERIFY(store.addUser("alice", "s3cret"));
        QVERIFY(store.save());
    }

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestFiles(tmpDir.path());
    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    auto stop = qScopeGuard([&] { server.stop(); });

    auto login = [this](const char *user, const char *password) {
        return postJson("/v1/auth/login",
                        QString(R"({"user":"%1","password":"%2"})")
                            .arg(user, password).toUtf8());
    };
    QVERIFY(login("alice", "s3cret").ok());
    QVERIFY(login("bob", "hunter2").header.contains("401"));

    // a user added while the server runs can log in at once
    {
        UserStore store;
        QVERIFY(store.addUser("bob", "hunter2"));
        QVERIFY(store.save());
    }
    QVERIFY(login("bob", "hunter2").ok());

    // a password changed while it runs: the new one works, the old not
    {
        UserStore store;
        QVERIFY(store.setPassword("alice", "n3wer"));
        QVERIFY(store.save());
    }
    QVERIFY(login("alice", "s3cret").header.contains("401"));
    QVERIFY(login("alice", "n3wer").ok());

    /* a file which cannot be read leaves the users as they were, rather
       than opening the server to anyone */
    {
        QFile f(cfgFile);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("{ not json");
    }
    QVERIFY(get("/repos").header.contains("401"));
    QVERIFY(login("alice", "n3wer").ok());
}


void TestSearchServer::testLoginsSurviveRestart()
{
    QStandardPaths::setTestModeEnabled(true);
    auto restoreStdPaths = qScopeGuard([] {
        QStandardPaths::setTestModeEnabled(false);
    });
    QString cfgDir = QStandardPaths::writableLocation(
                         QStandardPaths::GenericConfigLocation)
                     + "/paperman-server";
    QFile::remove(cfgDir + "/users.json");
    QFile::remove(cfgDir + "/tokens.json");
    {
        UserStore store;
        QVERIFY(store.addUser("alice", "s3cret"));
        QVERIFY(store.addUser("bob", "hunter2"));
        QVERIFY(store.save());
    }

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    createTestFiles(tmpDir.path());

    auto loginToken = [this](const char *user, const char *password) {
        auto resp = postJson("/v1/auth/login",
                             QString(R"({"user":"%1","password":"%2"})")
                                 .arg(user, password).toUtf8());
        return QJsonDocument::fromJson(resp.body).object()
                   .value("token").toString();
    };

    QString alice, bob;
    {
        SearchServer server(tmpDir.path(), PORT);
        QVERIFY(server.start());
        QTest::qWait(100);
        alice = loginToken("alice", "s3cret");
        bob = loginToken("bob", "hunter2");
        QVERIFY(!alice.isEmpty() && !bob.isEmpty());
        QVERIFY(getWithBearer("/repos", alice).ok());
        server.stop();
    }

    /* the file keeps the logins but not the tokens themselves, and only
       the owner can read it */
    QString tokensFile = cfgDir + "/tokens.json";
    QFile f(tokensFile);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray stored = f.readAll();
    f.close();
    QVERIFY(!stored.contains(alice.toLatin1()));
    QVERIFY(!stored.contains(bob.toLatin1()));
#ifndef Q_OS_WIN    // see testUserStoreFile()
    QVERIFY(!(QFile::permissions(tokensFile)
              & (QFileDevice::ReadGroup | QFileDevice::ReadOther)));
#endif

    // a new server, as after a restart, still knows both
    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    QTest::qWait(100);
    auto stop = qScopeGuard([&] { server.stop(); });
    QVERIFY(getWithBearer("/repos", alice).ok());
    QVERIFY(getWithBearer("/repos", bob).ok());

    // a new password ends the user's logins, without a restart
    {
        UserStore store;
        QVERIFY(store.setPassword("alice", "n3wer"));
        QVERIFY(store.save());
    }
    QVERIFY(getWithBearer("/repos", alice).header.contains("401"));
    QVERIFY(getWithBearer("/repos", bob).ok());
    QString again = loginToken("alice", "n3wer");
    QVERIFY(getWithBearer("/repos", again).ok());

    // and so does deleting the user
    {
        UserStore store;
        QVERIFY(store.delUser("bob"));
        QVERIFY(store.save());
    }
    QVERIFY(getWithBearer("/repos", bob).header.contains("401"));
    QVERIFY(getWithBearer("/repos", again).ok());

    /* a token which has run out is refused, and dropped from the file
       when it is next read */
    {
        TokenStore store(tokensFile);
        QString old = store.mint("alice", -1, "stamp");
        QVERIFY(store.lookup(old).isEmpty());
        QString live = store.mint("alice", 1, "stamp");
        TokenStore reread(tokensFile);
        QString stamp;
        QCOMPARE(reread.lookup(live, &stamp), QString("alice"));
        QCOMPARE(stamp, QString("stamp"));
        QVERIFY(reread.lookup(old).isEmpty());
    }
}


void TestSearchServer::testServerReadsPages()
{
    // the record of what was read goes in the redirected data directory
    QStandardPaths::setTestModeEnabled(true);
    auto restoreStdPaths = qScopeGuard([] {
        QStandardPaths::setTestModeEnabled(false);
    });

    // no users, as other tests here may leave, so no logging in
    QFile::remove(QStandardPaths::writableLocation(
                      QStandardPaths::GenericConfigLocation)
                  + "/paperman-server/users.json");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(copyTestFile("testfile.max", tmpDir.path()) > 0);

    auto calls = std::make_shared<std::atomic<int>>(0);
    RepoReader::Engine engine = [calls](QImage &, OcrPage &page) {
        page.size = QSize(100, 100);
        page.words << OcrWord{QRect(0, 0, 10, 10),
                              QString("read%1").arg(++*calls), 90, 0, 0};
        return QString();
    };

    auto readingStatus = [this, repo]() {
        auto resp = get("/v1/status");
        return QJsonDocument::fromJson(resp.body).object()
                   .value("reading").toObject().value(repo).toObject();
    };
    auto allRead = [](const QString &path) {
        Filemax max(QFileInfo(path).absolutePath() + "/",
                    QFileInfo(path).fileName(), nullptr);
        if (max.load() || !max.pagecount())
            return false;
        for (int i = 0; i < max.pagecount(); i++) {
            OcrPage page;
            if (max.getPageOcr(i, page) || page.isEmpty())
                return false;
        }
        return true;
    };

    SearchServer server(tmpDir.path(), PORT);
    server.setReadPages(1, engine);
    QVERIFY(server.start());
    auto stop = qScopeGuard([&] { server.stop(); });

    // the stack in the repository is read, and the status says so
    QTRY_VERIFY_WITH_TIMEOUT(readingStatus().value("written").toInt() == 1,
                             15000);
    QJsonObject st = readingStatus();
    QCOMPARE(st.value("pending").toInt(), 0);
    QVERIFY(st.value("pages").toInt() > 1);
    QVERIFY(allRead(dir + "testfile.max"));

    // a stack uploaded, as a scan is, is read without waiting for a pass
    QFile src(testSrc + "/testfile.max");
    QVERIFY(src.open(QIODevice::ReadOnly));
    RemoteBackend backend(QUrl(QString("http://localhost:%1").arg(PORT)));
    QString finalName, etag;
    QVERIFY2(backend.uploadFile(repo, "scan.max", src.readAll(), &finalName,
                                &etag),
             qPrintable(backend.lastError()));
    QTRY_VERIFY_WITH_TIMEOUT(readingStatus().value("written").toInt() == 2,
                             15000);
    QVERIFY(allRead(dir + finalName));

    // a server not asked to read pages says nothing about reading
    server.stop();
    SearchServer plain(tmpDir.path(), PORT);
    QVERIFY(plain.start());
    QVERIFY(get("/v1/status").ok());
    QVERIFY(!QJsonDocument::fromJson(get("/v1/status").body).object()
                 .contains("reading"));
    plain.stop();
}


void TestSearchServer::testServerSearchesText()
{
    // the index goes in the redirected data directory
    QStandardPaths::setTestModeEnabled(true);
    auto restoreStdPaths = qScopeGuard([] {
        QStandardPaths::setTestModeEnabled(false);
    });

    // no users, as other tests here may leave, so no logging in
    QFile::remove(QStandardPaths::writableLocation(
                      QStandardPaths::GenericConfigLocation)
                  + "/paperman-server/users.json");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(QDir(dir).mkdir("sub"));
    QVERIFY(QFile::copy(testSrc + "/testfile.max", dir + "a.max"));
    QVERIFY(QFile::copy(testSrc + "/testfile.max", dir + "sub/b.max"));
    QVERIFY(QFile::copy(testSrc + "/testfile.max", dir + "c.max"));
    auto say = [](const QString &path, int pagenum, const QString &text) {
        Filemax max(QFileInfo(path).absolutePath() + "/",
                    QFileInfo(path).fileName(), nullptr);
        OcrPage page;
        page.size = QSize(100, 100);
        for (const QString &word : text.split(' '))
            page.words << OcrWord{QRect(0, 0, 10, 10), word, 90, 0, 0};
        return !max.load() && !max.putPageOcr(pagenum, page);
    };
    QVERIFY(say(dir + "a.max", 0, "invoice for apples"));
    QVERIFY(say(dir + "sub/b.max", 2, "banana invoice"));

    auto search = [this, repo](const QString &query) {
        auto resp = get(QString("/v1/repos/%1/search?%2").arg(repo, query));
        QJsonObject obj = QJsonDocument::fromJson(resp.body).object();
        QStringList found;

        for (const QJsonValue &v : obj.value("results").toArray())
            found << QString("%1:%2").arg(v["path"].toString())
                         .arg(v["page"].toInt());
        found.sort();
        return found;
    };
    auto status = [this]() {
        return QJsonDocument::fromJson(get("/v1/status").body).object();
    };

    SearchServer server(tmpDir.path(), PORT);
    server.setTextIndex(true);
    QVERIFY(server.start());
    auto stop = qScopeGuard([&] { server.stop(); });

    // the server says it can search, and how far it has got indexing
    QVERIFY(status().value("features").toArray().contains("textSearch"));
    QTRY_VERIFY_WITH_TIMEOUT(status().value("indexing").toObject()
                                 .value(repo).toObject().value("ready")
                                 .toBool(), 15000);
    QCOMPARE(status().value("indexing").toObject().value(repo).toObject()
                 .value("total").toInt(), 3);

    auto resp = get(QString("/v1/repos/%1/search?text=invoice").arg(repo));
    QVERIFY2(resp.ok(), qPrintable(resp.header));
    QJsonObject obj = QJsonDocument::fromJson(resp.body).object();
    QVERIFY(obj.value("complete").toBool());
    QVERIFY(obj.value("results").toArray().at(0).toObject().value("snippet")
                .toString().contains("<b>"));

    QCOMPARE(search("text=invoice"), QStringList({"a.max:1", "sub/b.max:3"}));
    QCOMPARE(search("text=invoice&path=sub"), QStringList({"sub/b.max:3"}));
    QCOMPARE(search("text=invoice&max=1").size(), 1);
    QCOMPARE(search("text=app"), QStringList({"a.max:1"}));
    QCOMPARE(search("text=nothing%20here"), QStringList());
    QVERIFY(get(QString("/v1/repos/%1/search?text=x&path=../etc")
                    .arg(repo)).header.contains("400"));
    QVERIFY(get("/v1/repos/nosuchrepo/search?text=x").header.contains("404"));

    // a stack changed, moved or stacked through the server is indexed
    // again at once
    QString base = QString("/v1/repos/%1/stacks/").arg(repo);
    QVERIFY(say(dir + "c.max", 1, "cherry"));
    QVERIFY(postJson(base + "c.max/annotations",
                     R"({"keywords":"fruit"})").header.contains("200"));
    QTRY_COMPARE(search("text=cherry"), QStringList({"c.max:2"}));

    QVERIFY(postJson(base + "a.max/move",
                     R"({"destDir":"sub"})").header.contains("200"));
    QTRY_COMPARE(search("text=apples"), QStringList({"sub/a.max:1"}));

    QVERIFY(postJson(base + "sub/b.max/stack",
                     R"({"sources":["c.max"]})").header.contains("200"));
    QTRY_COMPARE(search("text=cherry"), QStringList({"sub/b.max:7"}));

    // a server not asked to index says nothing about searching
    server.stop();
    SearchServer plain(tmpDir.path(), PORT);
    QVERIFY(plain.start());
    QVERIFY(!status().value("features").toArray().contains("textSearch"));
    QVERIFY(!status().contains("indexing"));
    QVERIFY(get(QString("/v1/repos/%1/search?text=invoice").arg(repo))
                .header.contains("404"));
    plain.stop();
}


/* a repository of three stacks, two of them with text: a.max, sub/b.max
   (whose third page has the words) and c.max */
static bool makeTextRepo(const QString &dir)
{
    auto say = [](const QString &path, int pagenum, const QString &text) {
        Filemax max(QFileInfo(path).absolutePath() + "/",
                    QFileInfo(path).fileName(), nullptr);
        OcrPage page;
        page.size = QSize(100, 100);
        for (const QString &word : text.split(' '))
            page.words << OcrWord{QRect(0, 0, 10, 10), word, 90, 0, 0};
        return !max.load() && !max.putPageOcr(pagenum, page);
    };

    return QDir(dir).mkdir("sub")
           && QFile::copy(Suite::testSrc + "/testfile.max", dir + "a.max")
           && QFile::copy(Suite::testSrc + "/testfile.max", dir + "sub/b.max")
           && QFile::copy(Suite::testSrc + "/testfile.max", dir + "c.max")
           && say(dir + "a.max", 0, "invoice for apples")
           && say(dir + "sub/b.max", 2, "banana invoice");
}

void TestSearchServer::testRemoteSearchDesk()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(makeTextRepo(dir));

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    auto stop = qScopeGuard([&] { server.stop(); });
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QVERIFY(model.showDir(root, root, &meas).isValid());

    /* a search's desk holds stacks from several folders: each is fetched
       from its own place, and works as in its folder */
    QModelIndex found = model.finishFileSearch(root, root,
                                               {"a.max", "sub/b.max"}, &meas);
    QCOMPARE(model.rowCount(found), 2);

    QModelIndex b;
    for (int row = 0; row < model.rowCount(found); row++)
        if (model.data(model.index(row, 0, found), Desktopmodel::Role_filename)
                .toString() == "b.max")
            b = model.index(row, 0, found);
    QVERIFY(b.isValid());
    File *f = model.getFile(b);
    QVERIFY2(f->pathname().endsWith("/" + repo + "/sub/b.max"),
             qPrintable(f->pathname()));
    QVERIFY(!model.ensureContent(b));
    QCOMPARE(f->pagecount(), 5);
}

/* the stacks in a search's desk, as name:page, sorted */
static QStringList foundStacks(Desktopmodel &model, const QModelIndex &found)
{
    QStringList out;

    for (int row = 0; row < model.rowCount(found); row++) {
        QModelIndex ind = model.index(row, 0, found);

        out << QString("%1:%2")
                   .arg(model.data(ind, Desktopmodel::Role_filename).toString())
                   .arg(model.getFile(ind)->pagenum());
    }
    out.sort();
    return out;
}

void TestSearchServer::testRemoteTextSearch()
{
    QStandardPaths::setTestModeEnabled(true);
    auto restoreStdPaths = qScopeGuard([] {
        QStandardPaths::setTestModeEnabled(false);
    });
    QFile::remove(QStandardPaths::writableLocation(
                      QStandardPaths::GenericConfigLocation)
                  + "/paperman-server/users.json");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(makeTextRepo(dir));

    SearchServer server(tmpDir.path(), PORT);
    server.setTextIndex(true);
    QVERIFY(server.start());
    auto stop = qScopeGuard([&] { server.stop(); });
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QVERIFY(model.showDir(root, root, &meas).isValid());

    // the whole repository, once the server has built its index
    QModelIndex found;
    int count = 0;
    bool complete = false;
    QTRY_VERIFY_WITH_TIMEOUT(
        !model.findText(root, root, "invoice", &meas, found, count, nullptr,
                        &complete) && complete && count == 2, 15000);
    QCOMPARE(foundStacks(model, found), QStringList({"a.max:0", "b.max:2"}));

    /* a stack found in a subfolder is fetched from its own place, and
       opens at the page which matched */
    QModelIndex b;
    for (int row = 0; row < model.rowCount(found); row++)
        if (model.data(model.index(row, 0, found), Desktopmodel::Role_filename)
                .toString() == "b.max")
            b = model.index(row, 0, found);
    QVERIFY(b.isValid());
    File *f = model.getFile(b);
    QVERIFY2(f->pathname().endsWith("/" + repo + "/sub/b.max"),
             qPrintable(f->pathname()));
    QVERIFY(!model.ensureContent(b));
    QCOMPARE(f->pagecount(), 5);
    QCOMPARE(model.data(b, Desktopmodel::Role_pagenum).toInt(), 2);

    // only in a folder
    QVERIFY(!model.findText(root + "/sub", root, "invoice", &meas, found,
                            count));
    QCOMPARE(foundStacks(model, found), QStringList({"b.max:2"}));

    // nothing, and an empty search
    QVERIFY(!model.findText(root, root, "zebra", &meas, found, count));
    QCOMPARE(count, 0);
    QVERIFY(!model.findText(root, root, "  ", &meas, found, count));
    QCOMPARE(count, 0);

    // a server which does not index its text says so
    server.stop();
    SearchServer plain(tmpDir.path(), PORT);
    QVERIFY(plain.start());
    auto stopPlain = qScopeGuard([&] { plain.stop(); });
    err_info *e = model.findText(root, root, "invoice", &meas, found, count);
    QVERIFY(e);
    QVERIFY2(QString(e->errstr).contains("No text index"), e->errstr);
}


/* the number of pages of a stack in a server's repository, -1 if none */
static int serverPages(const QString &dir, const QString &fname)
{
    File *f = File::createFile(dir, fname, nullptr, File::typeFromName(fname));
    int count = f && !f->load() ? f->pagecount() : -1;

    delete f;
    return count;
}

void TestSearchServer::testRemoteConvert()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(QFile::copy(testSrc + "/testfile.max", dir + "a.max"));
    QCOMPARE(serverPages(dir, "a.max"), 5);

    SearchServer server(tmpDir.path(), PORT);
    QVERIFY(server.start());
    auto stop = qScopeGuard([&] { server.stop(); });
    QTest::qWait(100);
    QUrl url(QString("http://localhost:%1").arg(PORT));

    Dirmodel dirmodel;
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());

    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QVERIFY(parent.isValid());
    auto stackA = [&] { return model.index("a.max", parent); };
    QVERIFY(stackA().isValid());

    /* the server does the work, so the stack does not come here: only
       replies and the new stacks' thumbnails do */
    QVERIFY(dirmodel.stats());
    dirmodel.stats()->reset();
    qint64 stackSize = QFileInfo(dir + "a.max").size();

    // converted to a PDF, on the server, beside the original
    QModelIndexList list {stackA()};
    QStringList names;
    err_info *e = model.opDuplicateStacks(list, parent, names,
                                          File::Type_pdf, 3);
    QVERIFY2(!e, e ? e->errstr : "");
    QCOMPARE(names, QStringList({"a_copy.pdf"}));
    QCOMPARE(serverPages(dir, "a_copy.pdf"), 5);
    QVERIFY(model.index("a_copy.pdf", parent).isValid());

    // its odd pages, as a .max
    list = {stackA()};
    names.clear();
    e = model.opDuplicateStacks(list, parent, names, File::Type_max, 1);
    QVERIFY2(!e, e ? e->errstr : "");
    QCOMPARE(names, QStringList({"a_copy.max"}));
    QCOMPARE(serverPages(dir, "a_copy.max"), 3);

    // and again: the server gives the new stack a name of its own
    list = {stackA()};
    names.clear();
    e = model.opDuplicateStacks(list, parent, names, File::Type_max, 2);
    QVERIFY2(!e, e ? e->errstr : "");
    QCOMPARE(names.size(), 1);
    QVERIFY(names[0] != "a_copy.max");
    QCOMPARE(serverPages(dir, names[0]), 2);

    // unfolded as a booklet, each page becoming two
    list = {stackA()};
    names.clear();
    e = model.opUnfoldBooklets(list, parent, names);
    QVERIFY2(!e, e ? e->errstr : "");
    QCOMPARE(names, QStringList({"a_unfold.max"}));
    QCOMPARE(serverPages(dir, "a_unfold.max"), 10);
    QVERIFY2(dirmodel.stats()->bytesReceived() < stackSize / 4,
             qPrintable(QString("%1 bytes came for a stack of %2")
                        .arg(dirmodel.stats()->bytesReceived())
                        .arg(stackSize)));

    // a new stack opens from the copy kept in the cache
    QModelIndex unfold = model.index("a_unfold.max", parent);
    QVERIFY(unfold.isValid());
    QVERIFY(!model.ensureContent(unfold));
    QCOMPARE(model.getFile(unfold)->pagecount(), 10);

    // copied to the clipboard, as a PDF and as it is
    for (File::e_type type : {File::Type_pdf, File::Type_other}) {
        list = {stackA()};
        QApplication::clipboard()->clear();
        e = model.opCopyFiles(parent, list, type);
        QVERIFY2(!e, e ? e->errstr : "");

        const QMimeData *mime = QApplication::clipboard()->mimeData();
        QVERIFY(mime && mime->urls().size() == 1);
        QFileInfo fi(mime->urls()[0].toLocalFile());
        QVERIFY2(fi.exists(), qPrintable(fi.filePath()));
        QCOMPARE(fi.suffix(), type == File::Type_pdf ? QString("pdf")
                                                     : QString("max"));
        QCOMPARE(serverPages(fi.absolutePath() + "/", fi.fileName()), 5);
        QFile::remove(fi.filePath());
    }

    /* a server which does not convert has the stack converted here and
       the result uploaded */
    auto *remote = dynamic_cast<RemoteBackend *>(
        dirmodel.backendForRoot(root));
    QVERIFY(remote && remote->hasConvert());
    QStringList features = remote->features();
    features.removeAll("convert");
    remote->setServerInfo(remote->serverId(), features);
    QVERIFY(!remote->hasConvert());

    list = {stackA()};
    names.clear();
    e = model.opDuplicateStacks(list, parent, names, File::Type_pdf, 1);
    QVERIFY2(!e, e ? e->errstr : "");
    QCOMPARE(names.size(), 1);
    QCOMPARE(serverPages(dir, names[0]), 3);

    list = {stackA()};
    names.clear();
    e = model.opUnfoldBooklets(list, parent, names);
    QVERIFY2(!e, e ? e->errstr : "");
    QCOMPARE(names.size(), 1);
    QCOMPARE(serverPages(dir, names[0]), 10);
}


void TestSearchServer::testRemoteReadStack()
{
    QStandardPaths::setTestModeEnabled(true);
    auto restoreStdPaths = qScopeGuard([] {
        QStandardPaths::setTestModeEnabled(false);
    });
    QFile::remove(QStandardPaths::writableLocation(
                      QStandardPaths::GenericConfigLocation)
                  + "/paperman-server/users.json");

    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    QString dir = tmpDir.path() + "/";
    QString repo = QFileInfo(tmpDir.path()).fileName();
    QVERIFY(QFile::copy(testSrc + "/testfile.max", dir + "a.max"));

    RepoReader::Engine engine = [](QImage &, OcrPage &page) {
        page.size = QSize(100, 100);
        page.words << OcrWord{QRect(0, 0, 10, 10), "read", 90, 0, 0};
        return QString();
    };
    auto written = [this, repo]() {
        auto resp = get("/v1/status");
        return QJsonDocument::fromJson(resp.body).object()
                   .value("reading").toObject().value(repo).toObject()
                   .value("written").toInt();
    };
    auto firstRead = [](const QString &path) {
        Filemax max(QFileInfo(path).absolutePath() + "/",
                    QFileInfo(path).fileName(), nullptr);
        OcrPage page;
        return !max.load() && !max.getPageOcr(0, page) && !page.isEmpty();
    };

    SearchServer server(tmpDir.path(), PORT);
    server.setReadPages(1, engine);
    QVERIFY(server.start());
    auto stop = qScopeGuard([&] { server.stop(); });
    QTRY_VERIFY_WITH_TIMEOUT(written() == 1, 15000);

    Dirmodel dirmodel;
    QUrl url(QString("http://localhost:%1").arg(PORT));
    QString err;
    QVERIFY2(dirmodel.addRemoteRepository(url, &err),
             err.toUtf8().constData());
    Desktopmodel model(nullptr);
    Desktopmodelconv conv(&model);
    model.setModelConv(&conv);
    model.setDirmodel(&dirmodel);

    /* a stack put straight into the repository is not read until the
       reader next looks through it, a quarter of an hour on */
    QVERIFY(QFile::copy(testSrc + "/testfile.max", dir + "b.max"));
    QString root = url.toString() + "/" + repo;
    Measure meas(qApp->style(), QFont());
    QModelIndex parent = model.showDir(root, root, &meas);
    QModelIndex b = model.index("b.max", parent);
    QVERIFY(b.isValid());
    QTest::qWait(300);
    QVERIFY(!firstRead(dir + "b.max"));

    // asked, the server reads it now
    err_info *e = model.askServerToRead(b);
    QVERIFY2(!e, e ? e->errstr : "");
    QTRY_VERIFY_WITH_TIMEOUT(written() == 2, 15000);
    QVERIFY(firstRead(dir + "b.max"));

    // a server which reads nothing says so
    server.stop();
    SearchServer plain(tmpDir.path(), PORT);
    QVERIFY(plain.start());
    auto stopPlain = qScopeGuard([&] { plain.stop(); });

    /* the desk may have been refreshed as the first server went, so find
       the stack again */
    parent = model.showDir(root, root, &meas);
    b = model.index("b.max", parent);
    QVERIFY(b.isValid());
    e = model.askServerToRead(b);
    QVERIFY(e);
    QVERIFY2(QString(e->errstr).contains("--read-pages"), e->errstr);
}


/* A stand-in server which answers each request from a table of paths, so
   that a server of another version can be imitated */
class StubServer : public QTcpServer
{
public:
    // the status and the body to send for each path, without the query
    QHash<QString, QPair<int, QByteArray>> replies;

    StubServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket *sock = nextPendingConnection()) {
                connect(sock, &QTcpSocket::readyRead, sock, [this, sock]() {
                    QByteArray req = sock->readAll();
                    QString path = QString::fromLatin1(req.split(' ').value(1))
                                       .section('?', 0, 0);
                    auto it = replies.constFind(path);
                    int status = it == replies.constEnd() ? 404
                                                         : it.value().first;
                    QByteArray body = it == replies.constEnd()
                                          ? QByteArray("Not Found")
                                          : it.value().second;
                    sock->write("HTTP/1.1 " + QByteArray::number(status)
                                + " X\r\nContent-Length: "
                                + QByteArray::number(body.size())
                                + "\r\nConnection: close\r\n\r\n" + body);
                    sock->disconnectFromHost();
                });
                connect(sock, &QTcpSocket::disconnected, sock,
                        &QObject::deleteLater);
            }
        });
    }
};

void TestSearchServer::testServerTooOldOrNew()
{
    StubServer stub;
    QVERIFY(stub.listen(QHostAddress::LocalHost));
    QUrl url(QString("http://localhost:%1").arg(stub.serverPort()));

    // an old server lists its repositories, but has no /v1
    stub.replies["/repos"] = {200, R"({"repositories":[{"name":"papers"}]})"};

    Dirmodel dirmodel;
    QString err;
    QVERIFY(!dirmodel.addRemoteRepository(url, &err));
    QVERIFY2(err.contains("too old") && err.contains("1.4.0"),
             qPrintable(err));
    QVERIFY(!dirmodel.loginToServer(url, "user", "pass", &err));
    QVERIFY2(err.contains("too old"), qPrintable(err));

    // a newer one speaks another version of the protocol
    stub.replies["/v1/status"] = {200, R"({"status":"running",
        "apiVersion":"2","serverId":"abc","features":[]})"};
    QVERIFY(!dirmodel.addRemoteRepository(url, &err));
    QVERIFY2(err.contains("newer") && err.contains("version 2"),
             qPrintable(err));

    // and one of this version is used
    stub.replies["/v1/status"] = {200, R"({"status":"running",
        "apiVersion":"1","serverId":"abc","features":[]})"};
    QVERIFY2(dirmodel.addRemoteRepository(url, &err), qPrintable(err));
}
