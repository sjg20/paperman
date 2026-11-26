#ifndef TEST_ASSISTANT_H
#define TEST_ASSISTANT_H

#include <QObject>

#include "assistant.h"
#include "suite.h"

/* answers as it is told to, and keeps what it was asked */
class FakeBackend : public AssistantBackend
{
public:
   void ask(const QString &system, const QString &prompt,
            const QJsonObject &schema, QObject *, Done done) override
   {
      _system = system;
      _prompt = prompt;
      _schema = schema;
      done(_answer, _error);
   }

   QString _system, _prompt, _error;
   QJsonObject _schema, _answer;
};

/** Tests for the AI assistant, with a backend which stands in for the
    model, so that nothing leaves the computer */
class TestAssistant : public Suite
{
   Q_OBJECT
public:
   using Suite::Suite;

private slots:
   //! each test starts with nothing set
   void init();

   //! The assistant is off until turned on, with a command and model
   void testSettings();

   //! The question holds the text and the names to follow, within limits
   void testNamePrompt();

   //! A name is made fit to be a stack's name
   void testCleanName();

   //! A name comes back from the backend, cleaned, or the reason for none
   void testSuggestName();

   //! The question for dividing a stack holds each page's text, in order
   void testSplitPrompt();

   //! Whatever the model answers, the documents are in order and in range
   void testSplitFromAnswer();

   //! Documents come back from the backend, or the reason for none
   void testSuggestSplit();

   //! The command runs with no tools and the schema, and its reply is read
   void testClaudeCommand();

   //! A command which is not there says so
   //! a request can be stopped from the list of work going on
   void testCommandStopped();

   void testCommandMissing();
};

#endif // TEST_ASSISTANT_H
