#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "../searchserver.h"
#include "../serverlog.h"
#include "test.h"

#include "test_searchserver.h"

QString TestSearchServer::httpGet(const QString& url)
{
    // Parse URL to get host, port, and path
    QUrl qurl(url);
    QString host = qurl.host();
    int port = qurl.port();
    QString path = qurl.path();
    if (qurl.hasQuery())
        path += "?" + qurl.query();

    QTcpSocket socket;
    socket.connectToHost(host, port);

    // Process events to allow server to accept connection
    QCoreApplication::processEvents();

    if (!socket.waitForConnected(2000)) {
        qWarning() << "Failed to connect:" << socket.errorString();
        return QString();
    }

    // Send HTTP GET request
    QString request = QString("GET %1 HTTP/1.1\r\n"
                             "Host: %2\r\n"
                             "Connection: close\r\n"
                             "\r\n").arg(path, host);

    socket.write(request.toUtf8());
    socket.flush();

    // Process events to allow server to handle request
    QCoreApplication::processEvents();

    // Wait for response with timeout
    int totalWait = 0;
    const int maxWait = 3000; // 3 second timeout
    while (socket.bytesAvailable() == 0 && totalWait < maxWait) {
        // Process events to allow server to send response
        QCoreApplication::processEvents();

        if (!socket.waitForReadyRead(200)) {
            if (socket.state() != QAbstractSocket::ConnectedState) {
                break;
            }
        }
        totalWait += 200;
    }

    // Read all data
    QByteArray response;
    response += socket.readAll();

    // Wait a bit more for any remaining data
    totalWait = 0;
    while (socket.state() == QAbstractSocket::ConnectedState && totalWait < 1000) {
        if (socket.waitForReadyRead(100)) {
            response += socket.readAll();
        }
        totalWait += 100;
    }

    socket.close();

    if (response.isEmpty()) {
        qWarning() << "No response data received";
        return QString();
    }

    // Return full HTTP response (including headers) for status code checking
    return QString::fromUtf8(response);
}

QByteArray TestSearchServer::httpGetRaw(const QString& url, int timeoutMs)
{
    // Parse URL to get host, port, and path
    QUrl qurl(url);
    QString host = qurl.host();
    int port = qurl.port();
    QString path = qurl.path();
    if (qurl.hasQuery())
        path += "?" + qurl.query();

    QTcpSocket socket;
    socket.connectToHost(host, port);

    // Process events to allow server to accept connection
    QCoreApplication::processEvents();

    if (!socket.waitForConnected(2000)) {
        qWarning() << "Failed to connect:" << socket.errorString();
        return QByteArray();
    }

    // Send HTTP GET request
    QString request = QString("GET %1 HTTP/1.1\r\n"
                             "Host: %2\r\n"
                             "Connection: close\r\n"
                             "\r\n").arg(path, host);

    socket.write(request.toUtf8());
    socket.flush();

    // Process events to allow server to handle request
    QCoreApplication::processEvents();

    // Wait for response with timeout
    int totalWait = 0;
    while (socket.bytesAvailable() == 0 && totalWait < timeoutMs) {
        QCoreApplication::processEvents();

        if (!socket.waitForReadyRead(200)) {
            if (socket.state() != QAbstractSocket::ConnectedState)
                break;
        }
        totalWait += 200;
    }

    // Read all data
    QByteArray response;
    response += socket.readAll();

    // Wait for remaining data (proportional to main timeout)
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

void TestSearchServer::testServerStartStop()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    qDebug() << "Creating server on port 9876";
    SearchServer server(tmpDir.path(), 9876);

    qDebug() << "Starting server...";
    QVERIFY(server.start());
    qDebug() << "Server started";
    QCOMPARE(server.port(), (quint16)9876);
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
    SearchServer server(tmpDir.path(), 9877);
    QVERIFY(server.start());

    // Give server a moment to fully initialize
    QTest::qWait(100);

    qDebug() << "Making HTTP request to /status";
    QString response = httpGet("http://localhost:9877/status");
    qDebug() << "Response:" << response;
    QVERIFY(!response.isEmpty());
    QVERIFY(response.contains("\"status\""));
    QVERIFY(response.contains("\"running\""));
    QVERIFY(response.contains("\"repository\""));
    QVERIFY(response.contains(tmpDir.path()));

    server.stop();
    qDebug() << "testStatusEndpoint PASSED";
}

void TestSearchServer::testSearchEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    createTestFiles(tmpDir.path());

    SearchServer server(tmpDir.path(), 9878);
    QVERIFY(server.start());

    // Search for "invoice"
    QString response = httpGet("http://localhost:9878/search?q=invoice");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("invoice-2024.pdf"));
    QVERIFY(response.contains("\"count\":1"));

    // Search for "test"
    response = httpGet("http://localhost:9878/search?q=test");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("test-document.max"));

    // Search for something that doesn't exist
    response = httpGet("http://localhost:9878/search?q=nonexistent");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("\"count\":0"));

    server.stop();
}

void TestSearchServer::testListEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    createTestFiles(tmpDir.path());

    SearchServer server(tmpDir.path(), 9879);
    QVERIFY(server.start());

    // List files in root
    QString response = httpGet("http://localhost:9879/list");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("test-document.max"));
    QVERIFY(response.contains("invoice-2024.pdf"));
    QVERIFY(response.contains("photo.jpg"));
    QVERIFY(response.contains("\"count\":3"));

    // List files in subdirectory
    response = httpGet("http://localhost:9879/list?path=archive");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("old-doc.max"));
    QVERIFY(response.contains("\"count\":1"));

    server.stop();
}

void TestSearchServer::testInvalidEndpoint()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SearchServer server(tmpDir.path(), 9880);
    QVERIFY(server.start());

    QString response = httpGet("http://localhost:9880/invalid");
    QVERIFY(response.contains("404") || response.contains("Not Found"));

    server.stop();
}

void TestSearchServer::testMissingSearchParameter()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    SearchServer server(tmpDir.path(), 9881);
    QVERIFY(server.start());

    // Search without 'q' parameter should return error
    QString response = httpGet("http://localhost:9881/search");
    QVERIFY(response.contains("\"success\":false") || response.contains("error") || response.contains("400"));

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
    SearchServer server(repos, 9882);
    QVERIFY(server.start());

    // Request repository list
    QString response = httpGet("http://localhost:9882/repos");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("\"count\":2"));
    QVERIFY(response.contains(tmpDir1.path()));
    QVERIFY(response.contains(tmpDir2.path()));
    QVERIFY(response.contains("\"repositories\""));
    QVERIFY(response.contains("\"exists\":true"));

    server.stop();

    // Test with single repository (backward compatibility)
    SearchServer server2(tmpDir1.path(), 9883);
    QVERIFY(server2.start());

    response = httpGet("http://localhost:9883/repos");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("\"count\":1"));
    QVERIFY(response.contains(tmpDir1.path()));

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
    SearchServer server(repos, 9884);
    QVERIFY(server.start());

    // Get repository names from paths
    QString repo1Name = QFileInfo(tmpDir1.path()).fileName();
    QString repo2Name = QFileInfo(tmpDir2.path()).fileName();

    // Search without repo parameter (should search in default/first repo)
    QString response = httpGet("http://localhost:9884/search?q=repo1");
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("repo1-file.max"));
    QVERIFY(!response.contains("repo2-file.max"));

    // Search in specific repo (repo2)
    response = httpGet(QString("http://localhost:9884/search?q=repo2&repo=%1").arg(repo2Name));
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("repo2-file.max"));
    QVERIFY(!response.contains("repo1-file.max"));

    // Search in specific repo (repo1)
    response = httpGet(QString("http://localhost:9884/search?q=repo1&repo=%1").arg(repo1Name));
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("repo1-file.max"));

    // Search in non-existent repo
    response = httpGet("http://localhost:9884/search?q=test&repo=nonexistent");
    QVERIFY(response.contains("404") || response.contains("Repository not found"));

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

    SearchServer server(tmpDir.path(), 9885);
    QVERIFY(server.start());

    // Test retrieving existing file
    QString response = httpGet("http://localhost:9885/file?path=test-file.pdf");
    QVERIFY(response.contains("200 OK"));
    QVERIFY(response.contains("Content-Type: application/pdf"));
    QVERIFY(response.contains(testContent));

    // Test missing path parameter
    response = httpGet("http://localhost:9885/file");
    QVERIFY(response.contains("400") || response.contains("Missing 'path' parameter"));

    // Test non-existent file
    response = httpGet("http://localhost:9885/file?path=nonexistent.pdf");
    QVERIFY(response.contains("404") || response.contains("File not found"));

    // Test directory traversal prevention
    response = httpGet("http://localhost:9885/file?path=../etc/passwd");
    QVERIFY(response.contains("400") || response.contains("Invalid file path"));

    // Test absolute path prevention
    response = httpGet("http://localhost:9885/file?path=/etc/passwd");
    QVERIFY(response.contains("400") || response.contains("Invalid file path"));

    server.stop();
}

void TestSearchServer::testFilePageCount()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Copy test PDF into temp directory
    QString srcPdf = testSrc + "/testpdf.pdf";
    QString dstPdf = tmpDir.path() + "/testpdf.pdf";
    QVERIFY2(QFile::copy(srcPdf, dstPdf),
             qPrintable("Failed to copy " + srcPdf + " to " + dstPdf));

    SearchServer server(tmpDir.path(), 9886);
    QVERIFY(server.start());
    QTest::qWait(100);

    // Request page count
    QString response = httpGet(
        "http://localhost:9886/file?path=testpdf.pdf&pages=true");
    qDebug() << "Page count response:" << response;
    QVERIFY(response.contains("200 OK"));
    QVERIFY(response.contains("\"success\":true"));
    QVERIFY(response.contains("\"pages\":5"));

    server.stop();
}

void TestSearchServer::testFilePageExtract()
{
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());

    // Copy test PDF into temp directory
    QString srcPdf = testSrc + "/testpdf.pdf";
    QString dstPdf = tmpDir.path() + "/testpdf.pdf";
    QVERIFY2(QFile::copy(srcPdf, dstPdf),
             qPrintable("Failed to copy " + srcPdf + " to " + dstPdf));

    // Get full file size for comparison
    qint64 fullSize = QFileInfo(dstPdf).size();

    SearchServer server(tmpDir.path(), 9887);
    QVERIFY(server.start());
    QTest::qWait(100);

    // Request single page
    QByteArray raw = httpGetRaw(
        "http://localhost:9887/file?path=testpdf.pdf&page=1");
    QVERIFY(!raw.isEmpty());

    QString header = QString::fromUtf8(raw.left(raw.indexOf("\r\n\r\n")));
    qDebug() << "Page extract header:" << header;
    QVERIFY(header.contains("200 OK"));
    QVERIFY(header.contains("application/pdf"));

    // Body should be smaller than the full file
    int bodyStart = raw.indexOf("\r\n\r\n") + 4;
    qint64 bodySize = raw.size() - bodyStart;
    qDebug() << "Full file:" << fullSize << "bytes, page 1:" << bodySize << "bytes";
    QVERIFY2(bodySize < fullSize,
             qPrintable(QString("Page (%1) should be smaller than full file (%2)")
                       .arg(bodySize).arg(fullSize)));

    server.stop();
}

void TestSearchServer::testLargePdfProgressive()
{
    // Test progressive loading with a large real-world PDF (22 MB, 546 pages)
    QString largePdf = "/vid/homepaper/other/books/BBCMicroCompendium.pdf";
    if (!QFile::exists(largePdf)) {
        QSKIP("Large test PDF not available");
    }

    qint64 fullFileSize = QFileInfo(largePdf).size();
    qDebug() << "Large PDF:" << largePdf << "(" << fullFileSize << "bytes)";

    // Point the server at the directory containing the PDF
    QString repoPath = QFileInfo(largePdf).absolutePath();
    QString fileName = QFileInfo(largePdf).fileName();

    // Clear on-disk page cache so results are deterministic
    QDir pageCache("/tmp/paperman-pages");
    if (pageCache.exists()) {
        foreach (const QString &f, pageCache.entryList(QDir::Files))
            pageCache.remove(f);
    }

    SearchServer server(repoPath, 9888, nullptr, true);
    QVERIFY(server.start());
    QTest::qWait(100);

    // 1. Page count — should return 546 and log PageCount
    ServerLog::clear();
    QString response = httpGet(
        QString("http://localhost:9888/file?path=%1&pages=true")
            .arg(fileName));
    QVERIFY(response.contains("200 OK"));
    QVERIFY(response.contains("\"pages\":546"));

    QList<ServerLog::Entry> log = ServerLog::entries();
    QCOMPARE(log.size(), 1);
    QCOMPARE(log[0].action, ServerLog::PageCount);
    QCOMPARE(log[0].detail, 546);

    // 2. Extract page 1 — should log PageExtract
    ServerLog::clear();
    QByteArray raw = httpGetRaw(
        QString("http://localhost:9888/file?path=%1&page=1").arg(fileName),
        10000);
    QVERIFY(!raw.isEmpty());

    QString header = QString::fromUtf8(raw.left(raw.indexOf("\r\n\r\n")));
    QVERIFY2(header.contains("200 OK"),
             qPrintable("Page 1 extraction failed: " + header));
    QVERIFY(header.contains("application/pdf"));

    int bodyStart = raw.indexOf("\r\n\r\n") + 4;
    qint64 page1Size = raw.size() - bodyStart;
    QVERIFY2(page1Size < fullFileSize / 5,
             qPrintable(QString("Page 1 (%1 bytes) should be < 1/5 of full "
                                "file (%2)")
                       .arg(page1Size).arg(fullFileSize)));

    QByteArray body = raw.mid(bodyStart);
    QVERIFY2(body.startsWith("%PDF"),
             "Page 1 should be a valid PDF");

    log = ServerLog::entries();
    QCOMPARE(log.size(), 1);
    QCOMPARE(log[0].action, ServerLog::PageExtract);
    QCOMPARE(log[0].detail, 1);

    // 3. Request page 1 again — should log PageCacheHit
    ServerLog::clear();
    raw = httpGetRaw(
        QString("http://localhost:9888/file?path=%1&page=1").arg(fileName),
        10000);
    QVERIFY(!raw.isEmpty());
    header = QString::fromUtf8(raw.left(raw.indexOf("\r\n\r\n")));
    QVERIFY2(header.contains("200 OK"),
             qPrintable("Page 1 cache-hit failed: " + header));

    log = ServerLog::entries();
    QCOMPARE(log.size(), 1);
    QCOMPARE(log[0].action, ServerLog::PageCacheHit);
    QCOMPARE(log[0].detail, 1);

    // 4. Extract page 100 — should log PageExtract
    ServerLog::clear();
    raw = httpGetRaw(
        QString("http://localhost:9888/file?path=%1&page=100").arg(fileName),
        10000);
    QVERIFY(!raw.isEmpty());
    header = QString::fromUtf8(raw.left(raw.indexOf("\r\n\r\n")));
    QVERIFY2(header.contains("200 OK"),
             qPrintable("Page 100 extraction failed: " + header));

    body = raw.mid(raw.indexOf("\r\n\r\n") + 4);
    QVERIFY2(body.startsWith("%PDF"),
             "Page 100 should be a valid PDF");

    log = ServerLog::entries();
    QCOMPARE(log.size(), 1);
    QCOMPARE(log[0].action, ServerLog::PageExtract);
    QCOMPARE(log[0].detail, 100);

    server.stop();
}
