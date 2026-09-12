#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "../clientconf.h"

#include "test_clientconf.h"


/* Point ClientConfig at a file of our own for the duration of a test,
   so nothing here touches the user's real configuration. */
class ConfigFile
{
public:
   ConfigFile(QTemporaryDir &dir, const QByteArray &content = QByteArray())
   {
      _path = dir.path() + "/client.conf";
      if (!content.isNull())
         write(content);
      qputenv("PAPERMAN_CONFIG", _path.toLocal8Bit());
   }

   ~ConfigFile() { qunsetenv("PAPERMAN_CONFIG"); }

   void write(const QByteArray &content)
   {
      QFile f(_path);
      f.open(QIODevice::WriteOnly | QIODevice::Truncate);
      f.write(content);
   }

   QByteArray read() const
   {
      QFile f(_path);
      f.open(QIODevice::ReadOnly);
      return f.readAll();
   }

   QString path() const { return _path; }

private:
   QString _path;
};


void TestClientConf::testMissingFile()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());

   /* Name a file that was never created */
   QString path = tmp.path() + "/absent.conf";
   qputenv("PAPERMAN_CONFIG", path.toLocal8Bit());

   QCOMPARE(ClientConfig::configPath(), path);
   QVERIFY(ClientConfig::load().isEmpty());

   qunsetenv("PAPERMAN_CONFIG");
}


void TestClientConf::testParsing()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   ConfigFile conf(tmp,
      "# a comment\n"
      "\n"
      "http://localhost:8080\n"
      "Office   =   https://paper.example.com\n"
      "   Home = https://nas.local:8443\n"
      "   # an indented comment\n");

   QList<ServerEntry> list = ClientConfig::load();
   QCOMPARE(list.size(), 3);

   /* A bare URL is labelled with its host */
   QCOMPARE(list[0].name, QString("localhost"));
   QCOMPARE(list[0].url, QUrl("http://localhost:8080"));

   /* Space around the name and the URL is trimmed */
   QCOMPARE(list[1].name, QString("Office"));
   QCOMPARE(list[1].url, QUrl("https://paper.example.com"));

   QCOMPARE(list[2].name, QString("Home"));
   QCOMPARE(list[2].url, QUrl("https://nas.local:8443"));
}


void TestClientConf::testBadLines()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   ConfigFile conf(tmp,
      "http://good.example.com\n"
      "not-a-url\n"
      "Broken =\n");

   QStringList bad;
   QList<ServerEntry> list = ClientConfig::load(&bad);

   /* The good line still loads; the other two are reported */
   QCOMPARE(list.size(), 1);
   QCOMPARE(list[0].url, QUrl("http://good.example.com"));
   QCOMPARE(bad.size(), 2);
   QVERIFY(bad.contains("not-a-url"));
}


void TestClientConf::testFindByName()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   ConfigFile conf(tmp, "Office = https://paper.example.com\n");

   ServerEntry entry;
   QVERIFY(ClientConfig::findByName("Office", &entry));
   QCOMPARE(entry.url, QUrl("https://paper.example.com"));

   /* The name is matched without regard to case */
   QVERIFY(ClientConfig::findByName("office", &entry));
   QCOMPARE(entry.url, QUrl("https://paper.example.com"));

   QVERIFY(!ClientConfig::findByName("nosuch", &entry));
}


void TestClientConf::testAdd()
{
   QTemporaryDir tmp;
   QVERIFY(tmp.isValid());
   ConfigFile conf(tmp, QByteArray());

   bool added = false;
   QVERIFY(ClientConfig::add(QUrl("http://localhost:8080"), QString(),
                             nullptr, &added));
   QVERIFY(added);

   QVERIFY(ClientConfig::add(QUrl("https://paper.example.com"), "Office",
                             nullptr, &added));
   QVERIFY(added);
   QCOMPARE(ClientConfig::load().size(), 2);

   /* A named entry is written so it parses back with its name */
   ServerEntry entry;
   QVERIFY(ClientConfig::findByName("Office", &entry));

   /* Adding the same server again is a success but writes nothing */
   QVERIFY(ClientConfig::add(QUrl("http://localhost:8080"), QString(),
                             nullptr, &added));
   QVERIFY(!added);
   QCOMPARE(ClientConfig::load().size(), 2);

   /* ...even when it differs by a trailing slash or the host's case */
   QVERIFY(ClientConfig::add(QUrl("http://localhost:8080/"), QString(),
                             nullptr, &added));
   QVERIFY(!added);
   QVERIFY(ClientConfig::add(QUrl("http://LOCALHOST:8080"), QString(),
                             nullptr, &added));
   QVERIFY(!added);
   QCOMPARE(ClientConfig::load().size(), 2);
}


void TestClientConf::testSameServer()
{
   /* Differences that do not change what is addressed */
   QVERIFY(ClientConfig::sameServer(QUrl("http://host:8080"),
                                    QUrl("http://host:8080/")));
   QVERIFY(ClientConfig::sameServer(QUrl("http://HOST:8080"),
                                    QUrl("http://host:8080")));
   QVERIFY(ClientConfig::sameServer(QUrl("https://h/base/"),
                                    QUrl("https://h/base")));

   /* ...and differences that do */
   QVERIFY(!ClientConfig::sameServer(QUrl("http://host:8080"),
                                     QUrl("http://host:9090")));
   QVERIFY(!ClientConfig::sameServer(QUrl("http://host:8080"),
                                     QUrl("https://host:8080")));
   QVERIFY(!ClientConfig::sameServer(QUrl("http://a.example.com"),
                                     QUrl("http://b.example.com")));
}
