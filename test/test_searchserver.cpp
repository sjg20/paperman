#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "../searchserver.h"
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
