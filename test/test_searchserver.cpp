#include <QtTest/QtTest>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>

#include "../searchserver.h"
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
