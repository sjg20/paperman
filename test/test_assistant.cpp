#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QtTest/QtTest>

#include "assistant.h"
#include "test_assistant.h"

void TestAssistant::init()
{
   QSettings().remove("assistant");
}

void TestAssistant::testSettings()
{
   // it sends the user's papers away, so is off unless asked for
   QVERIFY(!Assistant::enabled());
   QCOMPARE(Assistant::command(), QString("claude"));
   QCOMPARE(Assistant::model(), QString(Assistant::default_model));

   Assistant::setEnabled(true);
   Assistant::setCommand(" /opt/bin/claude ");
   Assistant::setModel("claude-haiku-5-5");
   QVERIFY(Assistant::enabled());
   QCOMPARE(Assistant::command(), QString("/opt/bin/claude"));
   QCOMPARE(Assistant::model(), QString("claude-haiku-5-5"));

   // an empty setting means the default
   Assistant::setCommand("");
   Assistant::setModel("");
   QCOMPARE(Assistant::command(), QString("claude"));
   QCOMPARE(Assistant::model(), QString(Assistant::default_model));
   QSettings().remove("assistant");
}

void TestAssistant::testNamePrompt()
{
   QStringList examples;

   for (int i = 0; i < Assistant::name_examples + 5; i++)
      examples << QString("Bill %1").arg(i);

   QString prompt = Assistant::namePrompt("Tax invoice", examples);

   QVERIFY(prompt.contains("Tax invoice"));
   QVERIFY(prompt.contains("- Bill 0\n"));
   QVERIFY(prompt.contains(QString("- Bill %1\n")
                              .arg(Assistant::name_examples - 1)));
   QVERIFY(!prompt.contains(QString("- Bill %1\n")
                               .arg(Assistant::name_examples)));

   // with no other names there is no list of them
   QVERIFY(!Assistant::namePrompt("Tax invoice", QStringList())
               .contains("Names of other"));

   // only the start of a long text is sent
   QString longer = QString(Assistant::name_text_chars, 'a') + "ZZZ";
   QVERIFY(!Assistant::namePrompt(longer, QStringList()).contains("ZZZ"));
}

void TestAssistant::testCleanName()
{
   QCOMPARE(Assistant::cleanName("  Power bill\n Mar 2026 "),
            QString("Power bill Mar 2026"));
   QCOMPARE(Assistant::cleanName("Rates 2025/26: final?"),
            QString("Rates 202526 final"));
   QCOMPARE(Assistant::cleanName("Statement.PDF"), QString("Statement"));
   QCOMPARE(Assistant::cleanName("..hidden"), QString("hidden"));
   QCOMPARE(Assistant::cleanName(QString(200, 'x')).size(), 80);
   QCOMPARE(Assistant::cleanName(" / "), QString());
}

void TestAssistant::testSuggestName()
{
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);
   QString name, error;
   int calls = 0;
   auto done = [&](const QString &n, const QString &e) {
      name = n;
      error = e;
      calls++;
   };

   backend->_answer["name"] = "Power bill: Mar 2026.pdf";
   assistant.suggestName("Tax invoice", {"Water bill Jan 2026"}, this, done);
   QCOMPARE(calls, 1);
   QCOMPARE(name, QString("Power bill Mar 2026"));
   QVERIFY(error.isEmpty());

   // it was asked for a name, as JSON, with the text and the other names
   QVERIFY(backend->_prompt.contains("Tax invoice"));
   QVERIFY(backend->_prompt.contains("Water bill Jan 2026"));
   QVERIFY(!backend->_system.isEmpty());
   QCOMPARE(backend->_schema.value("required").toArray(),
            QJsonArray{"name"});

   // a name with nothing left of it is no name
   backend->_answer["name"] = "??";
   assistant.suggestName("Tax invoice", QStringList(), this, done);
   QVERIFY(name.isEmpty());
   QVERIFY(!error.isEmpty());

   // nor is a failure, whose reason is passed on
   backend->_answer = QJsonObject();
   backend->_error = "no network";
   assistant.suggestName("Tax invoice", QStringList(), this, done);
   QVERIFY(name.isEmpty());
   QCOMPARE(error, QString("no network"));
   QCOMPARE(calls, 3);
}

static QJsonObject docsAnswer(const QList<QPair<int, QString>> &docs)
{
   QJsonArray arr;

   for (const auto &doc : docs) {
      QJsonObject obj;

      obj["first_page"] = doc.first;
      obj["name"] = doc.second;
      arr << obj;
   }

   QJsonObject answer;

   answer["documents"] = arr;
   return answer;
}

void TestAssistant::testSplitPrompt()
{
   QStringList pages{"Power bill", "", "  Dear Sir  "};
   QString prompt = Assistant::splitPrompt(pages, {"Water bill Jan 2026"});

   QVERIFY(prompt.contains("- Water bill Jan 2026\n"));
   QVERIFY(prompt.contains("The stack has 3 pages"));
   QVERIFY(prompt.contains("=== Page 1 ===\nPower bill\n"));
   QVERIFY(prompt.contains("=== Page 2 ===\n(no text)\n"));
   QVERIFY(prompt.contains("=== Page 3 ===\nDear Sir\n"));
   QVERIFY(prompt.indexOf("Page 1") < prompt.indexOf("Page 2"));

   // only the start of a long page is sent
   pages[0] = QString(Assistant::split_page_chars, 'a') + "ZZZ";
   QVERIFY(!Assistant::splitPrompt(pages, QStringList()).contains("ZZZ"));

   /* with many pages each gets less, so that the whole stays within
      what may be sent, but never less than the least */
   pages.clear();
   for (int i = 0; i < Assistant::split_pages; i++)
      pages << QString(Assistant::split_page_chars, 'b');
   prompt = Assistant::splitPrompt(pages, QStringList());
   QVERIFY(prompt.count('b') <= Assistant::split_text_chars);
   QVERIFY(prompt.count('b') >= Assistant::split_pages
                                   * Assistant::split_page_min_chars);
}

void TestAssistant::testSplitFromAnswer()
{
   typedef Assistant::Doc Doc;
   QList<Doc> docs;

   // pages are numbered from 1 in the answer, and from 0 here
   docs = Assistant::splitFromAnswer(
      docsAnswer({{1, "Power bill"}, {3, "Letter: bank.pdf"}}), 5);
   QCOMPARE(docs, (QList<Doc>{{0, "Power bill"}, {2, "Letter bank"}}));

   // out of order, twice at one page and outside the stack
   docs = Assistant::splitFromAnswer(
      docsAnswer({{4, "D"}, {1, "A"}, {4, "again"}, {9, "far"}, {0, "no"},
                  {2, "B"}}), 5);
   QCOMPARE(docs, (QList<Doc>{{0, "A"}, {1, "B"}, {3, "D"}}));

   // the first document has the pages before it too
   docs = Assistant::splitFromAnswer(docsAnswer({{2, "A"}, {3, "B"}}), 5);
   QCOMPARE(docs, (QList<Doc>{{0, "A"}, {2, "B"}}));

   // a document with no usable name is still a document
   docs = Assistant::splitFromAnswer(docsAnswer({{1, "A"}, {2, "??"}}), 5);
   QCOMPARE(docs, (QList<Doc>{{0, "A"}, {1, ""}}));

   // nothing usable gives nothing
   QVERIFY(Assistant::splitFromAnswer(QJsonObject(), 5).isEmpty());
   QVERIFY(Assistant::splitFromAnswer(docsAnswer({{7, "A"}}), 5).isEmpty());
}

void TestAssistant::testSuggestSplit()
{
   FakeBackend *backend = new FakeBackend;
   Assistant assistant(backend);
   QList<Assistant::Doc> docs;
   QString error;
   int calls = 0;
   auto done = [&](const QList<Assistant::Doc> &d, const QString &e) {
      docs = d;
      error = e;
      calls++;
   };
   QStringList pages{"Power bill", "page 2 of 2", "Dear Sir"};

   backend->_answer = docsAnswer({{1, "Power bill Mar 2026"},
                                  {3, "Letter from the bank"}});
   assistant.suggestSplit(pages, {"Water bill Jan 2026"}, this, done);
   QCOMPARE(calls, 1);
   QVERIFY(error.isEmpty());
   QCOMPARE(docs.size(), 2);
   QCOMPARE(docs[1].first, 2);
   QCOMPARE(docs[1].name, QString("Letter from the bank"));

   // it was asked for documents, as JSON, with the pages and other names
   QVERIFY(backend->_prompt.contains("=== Page 3 ===\nDear Sir"));
   QVERIFY(backend->_prompt.contains("Water bill Jan 2026"));
   QVERIFY(!backend->_system.isEmpty());
   QCOMPARE(backend->_schema.value("required").toArray(),
            QJsonArray{"documents"});

   // an answer with no documents is a failure, as is one from the backend
   backend->_answer = QJsonObject();
   assistant.suggestSplit(pages, QStringList(), this, done);
   QVERIFY(docs.isEmpty());
   QVERIFY(!error.isEmpty());

   backend->_error = "no network";
   assistant.suggestSplit(pages, QStringList(), this, done);
   QVERIFY(docs.isEmpty());
   QCOMPARE(error, QString("no network"));

   // a stack with too many pages is not sent at all
   backend->_prompt.clear();
   backend->_error.clear();
   pages.clear();
   for (int i = 0; i <= Assistant::split_pages; i++)
      pages << "page";
   assistant.suggestSplit(pages, QStringList(), this, done);
   QVERIFY(docs.isEmpty());
   QVERIFY(!error.isEmpty());
   QVERIFY(backend->_prompt.isEmpty());
   QCOMPARE(calls, 4);
}

void TestAssistant::testClaudeCommand()
{
   ClaudeCommandBackend backend("claude", "claude-opus-5-5");
   QJsonObject schema;

   schema["type"] = "object";

   QStringList args = backend.arguments("Name it", schema);

   // it may read and answer, and nothing else
   int tools = args.indexOf("--tools");
   QVERIFY(tools >= 0);
   QCOMPARE(args.value(tools + 1), QString());
   QVERIFY(args.contains("--print"));
   QVERIFY(args.contains("--no-session-persistence"));
   QCOMPARE(args.value(args.indexOf("--system-prompt") + 1),
            QString("Name it"));
   QCOMPARE(args.value(args.indexOf("--json-schema") + 1),
            QString("{\"type\":\"object\"}"));
   QCOMPARE(args.value(args.indexOf("--model") + 1),
            QString("claude-opus-5-5"));

   // with no model named, the command's own choice is used
   QVERIFY(!ClaudeCommandBackend("claude", QString())
               .arguments("Name it", schema).contains("--model"));

   QJsonObject answer;

   QVERIFY(ClaudeCommandBackend::parse(
      "{\"is_error\":false,\"structured_output\":{\"name\":\"A bill\"}}",
      answer).isEmpty());
   QCOMPARE(answer.value("name").toString(), QString("A bill"));

   QString error = ClaudeCommandBackend::parse(
      "{\"is_error\":true,\"result\":\"Not logged in\"}", answer);
   QVERIFY(error.contains("Not logged in"));
   QVERIFY(answer.isEmpty());

   QVERIFY(!ClaudeCommandBackend::parse("{\"is_error\":false}", answer)
               .isEmpty());
   QVERIFY(!ClaudeCommandBackend::parse("not json", answer).isEmpty());
}

void TestAssistant::testCommandMissing()
{
   ClaudeCommandBackend backend("/nonexistent/paperman-no-such-command",
                                QString());
   QString error;
   bool called = false;

   backend.ask("Name it", "Text", QJsonObject(), this,
               [&](const QJsonObject &, const QString &e) {
      error = e;
      called = true;
   });
   QTRY_VERIFY(called);
   QVERIFY(error.contains("paperman-no-such-command"));
}
