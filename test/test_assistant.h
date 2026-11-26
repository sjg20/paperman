#ifndef TEST_ASSISTANT_H
#define TEST_ASSISTANT_H

#include <functional>
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
      _asked++;

      // a name for each request in turn, if any are lined up
      if (!_names.isEmpty())
         _answer["name"] = _names.takeFirst();

      // held back until release(), as a model which takes its time
      if (_hold) {
         QJsonObject answer = _answer;
         QString error = _error;

         _held << [done, answer, error] { done(answer, error); };
         return;
      }
      done(_answer, _error);
   }

   //! give the answers held back
   void release()
   {
      QList<std::function<void ()>> held = _held;

      _held.clear();
      _hold = false;
      for (const auto &answer : held)
         answer();
   }

   bool _hold = false;     //!< hold answers back until release()
   QList<std::function<void ()>> _held;

   QString _system, _prompt, _error;
   QJsonObject _schema, _answer;
   QStringList _names;     //!< the name to answer each request with
   int _asked = 0;         //!< how many requests there have been
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
   void testNameText();
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
