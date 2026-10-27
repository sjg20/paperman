#include <QtTest/QtTest>
#include <QSettings>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>

#include "../updatecheck.h"

#include "test_updatecheck.h"

/** A web server which answers every request with the same reply, as the
    website answers with its latest.json */
class FakeSite : public QTcpServer
{
public:
   FakeSite(const QByteArray &body) : _body(body)
   {
      listen(QHostAddress::LocalHost);
      connect(this, &QTcpServer::newConnection, this, &FakeSite::answer);
   }

   QUrl url() const
   {
      return QUrl(QString("http://127.0.0.1:%1/latest.json")
                  .arg(serverPort()));
   }

   int requests = 0;      //!< how many requests have come
   QByteArray target;     //!< the path and query of the last one

private:
   void answer()
   {
      QTcpSocket *sock = nextPendingConnection();

      connect(sock, &QTcpSocket::readyRead, sock, [this, sock]() {
         QByteArray req = sock->readAll();

         // only the request line matters: GET <target> HTTP/1.1
         target = req.split(' ').value(1);
         requests++;
         sock->write("HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: " + QByteArray::number(_body.size())
                     + "\r\nConnection: close\r\n\r\n" + _body);
         sock->disconnectFromHost();
      });
      connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
   }

   QByteArray _body;
};

static const QByteArray newer_reply =
   "{\"version\": \"9.1.0\", \"url\": \"https://getpaperman.com/download/\"}";

/** check against a site, waiting for the reply if a request is sent

   \returns true if a request was sent */
static bool check(UpdateCheck &uc, const FakeSite &site)
{
   QSignalSpy done(&uc, &UpdateCheck::done);

   uc.setBaseUrl(site.url());
   if (!uc.start())
      return false;
   if (!done.wait(10000))
      qWarning() << "no reply from the fake site";
   return true;
}

void TestUpdateCheck::init()
{
   QSettings().remove("updates");
   qunsetenv("SNAP");
}

void TestUpdateCheck::testRequest()
{
   UpdateCheck uc;

   uc.setBaseUrl(QUrl("https://example.com/latest.json"));
   uc.setVersion("1.4.0");

   QUrl url = uc.requestUrl();
   QUrlQuery query(url);

   QCOMPARE(url.path(), QString("/latest.json"));
   QCOMPARE(query.queryItemValue("v"), QString("1.4.0"));
   QCOMPARE(query.queryItemValue("os"), QSysInfo::productType());
   QCOMPARE(query.queryItemValue("arch"),
            QSysInfo::currentCpuArchitecture());
   QCOMPARE(query.queryItemValue("pkg"), UpdateCheck::packageKind());

   // and nothing else
   QCOMPARE(query.queryItems().size(), 4);
}

void TestUpdateCheck::testNewer()
{
   FakeSite site(newer_reply);
   UpdateCheck uc;
   QSignalSpy newer(&uc, &UpdateCheck::newer);

   uc.setVersion("1.4.0");
   QVERIFY(check(uc, site));
   QCOMPARE(site.requests, 1);
   QVERIFY(site.target.startsWith("/latest.json?v=1.4.0&"));

   QTRY_COMPARE(newer.size(), 1);
   QCOMPARE(newer.at(0).at(0).toString(), QString("9.1.0"));
   QCOMPARE(newer.at(0).at(1).toUrl(),
            QUrl("https://getpaperman.com/download/"));
}

void TestUpdateCheck::testNotNewer()
{
   FakeSite site(newer_reply);
   UpdateCheck uc;
   QSignalSpy newer(&uc, &UpdateCheck::newer);

   uc.setVersion("9.1.0");
   QVERIFY(check(uc, site));
   QTest::qWait(50);
   QCOMPARE(newer.size(), 0);

   UpdateCheck later;
   QSignalSpy later_newer(&later, &UpdateCheck::newer);

   QSettings().remove("updates/checked");
   later.setVersion("10.0");
   QVERIFY(check(later, site));
   QTest::qWait(50);
   QCOMPARE(later_newer.size(), 0);
}

void TestUpdateCheck::testOncePerDay()
{
   FakeSite site(newer_reply);
   UpdateCheck first;

   first.setVersion("1.4.0");
   QVERIFY(check(first, site));
   QCOMPARE(site.requests, 1);

   // started again soon after: nothing is asked, but the news is told
   UpdateCheck second;
   QSignalSpy newer(&second, &UpdateCheck::newer);

   second.setVersion("1.4.0");
   QVERIFY(!check(second, site));
   QTRY_COMPARE(newer.size(), 1);
   QCOMPARE(newer.at(0).at(0).toString(), QString("9.1.0"));
   QCOMPARE(site.requests, 1);

   // a day later it asks again
   QSettings().setValue("updates/checked",
                        QDateTime::currentDateTimeUtc().addDays(-1));
   QVERIFY(check(second, site));
   QCOMPARE(site.requests, 2);
}

void TestUpdateCheck::testDisabled()
{
   FakeSite site(newer_reply);
   UpdateCheck uc;

   QVERIFY(UpdateCheck::enabled());
   UpdateCheck::setEnabled(false);
   QVERIFY(!UpdateCheck::enabled());

   uc.setVersion("1.4.0");
   QVERIFY(!check(uc, site));
   QTest::qWait(50);
   QCOMPARE(site.requests, 0);

   UpdateCheck::setEnabled(true);
}

void TestUpdateCheck::testBadReply()
{
   const QByteArray replies[] = {
      "not json",
      "{\"version\": \"soon\", \"url\": \"https://getpaperman.com/\"}",
      "{\"version\": \"9.1.0\", \"url\": \"file:///etc/passwd\"}",
      "{\"version\": \"9.1.0\"}",
   };

   for (const QByteArray &reply : replies) {
      FakeSite site(reply);
      UpdateCheck uc;
      QSignalSpy newer(&uc, &UpdateCheck::newer);

      QSettings().remove("updates");
      uc.setVersion("1.4.0");
      QVERIFY(check(uc, site));
      QTest::qWait(50);
      QCOMPARE(newer.size(), 0);
      QVERIFY(!QSettings().contains("updates/latest"));
   }
}

void TestUpdateCheck::testSnap()
{
   FakeSite site(newer_reply);
   UpdateCheck uc;
   QSignalSpy newer(&uc, &UpdateCheck::newer);

   qputenv("SNAP", "/snap/paperman/current");
   QCOMPARE(UpdateCheck::packageKind(), QString("snap"));

   uc.setVersion("1.4.0");
   QVERIFY(check(uc, site));
   QVERIFY(site.target.contains("pkg=snap"));
   QTest::qWait(50);
   QCOMPARE(newer.size(), 0);

   qunsetenv("SNAP");
}
