#include <QtTest/QtTest>
#include <QSettings>
#include <QStandardItemModel>

#include "fakecontrol.h"
#include "pscan.h"
#include "qxmlconfig.h"
#include "scansettings.h"
#include "test_pscan.h"

// the settings the panel keeps in the program's configuration
static const char *const saved_keys[] = {
   "LAST_DEVICE", "SCAN_SIDEWAYS", "SCAN_AUTO_COLOUR",
};
static QMap<QString, QString> saved;

void TestPscan::init()
{
   ensureXmlConfig();
   for (const char *key : saved_keys)
      saved[key] = xmlConfig->stringValue(key);

   // a later start, with a scanner chosen before, fed upright
   xmlConfig->setStringValue("LAST_DEVICE", "fake:scanner");
   xmlConfig->setIntValue("SCAN_SIDEWAYS", 0);
   xmlConfig->setBoolValue("SCAN_AUTO_COLOUR", false);

   // just the standard presets
   QSettings().remove("preset");
}

void TestPscan::cleanup()
{
   for (const char *key : saved_keys)
      xmlConfig->setStringValue(key, saved[key]);
   QSettings().remove("preset");
}

/* Put the panel in front of the fake scanner as the program does: the
   main window is set up first, then the scanner is opened, and whenever
   the scanner says an option has changed the panel is refreshed */
static void attach(Pscan &pscan, FakeControl &fake)
{
   fake.changed = [&pscan] { pscan.refresh(); };
   pscan.setMainwidget(nullptr);
   pscan.setControl(&fake);
}

// pick a preset from the list, as the user does
static void choose(Pscan &pscan, int item)
{
   pscan.preset->setCurrentIndex(item);
   pscan.on_preset_activated(item);
}

// the list's <custom> item, shown when the panel matches no preset
static int custom(Pscan &pscan)
{
   return pscan.preset->count() - 1;
}

//! true if an item in the preset list after the presets can be picked
static bool enabled(Pscan &pscan, int count, Preset::preset_item_t index)
{
   auto model = qobject_cast<QStandardItemModel *>(pscan.preset->model());

   return model->item(count + index)->isEnabled();
}

void TestPscan::testLaterStartFollowsPanel()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);

   // the scanner is as the mono preset has it
   QCOMPARE(pscan.preset->currentIndex(), 0);

   pscan.grey->click();
   QCOMPARE(fake._format, QScanner::grey);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));

   pscan.mono->click();
   QCOMPARE(pscan.preset->currentIndex(), 0);
}

void TestPscan::testPresetFollowsPanel()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   choose(pscan, 0);
   QCOMPARE(pscan.preset->currentIndex(), 0);

   // each change, and how to put it back
   struct Change {
      const char *what;
      std::function<void()> make, undo;
   } changes[] = {
      {"mode", [&] { pscan.dither->click(); },
               [&] { pscan.mono->click(); }},
      {"dpi", [&] { pscan.res->setCurrentIndex(0); pscan.res_activated(0); },
              [&] { pscan.res->setCurrentIndex(1); pscan.res_activated(1); }},
      {"duplex", [&] { pscan.duplex->click(); },
                 [&] { pscan.duplex->click(); }},
   };

   for (auto &change : changes) {
      change.make();
      QVERIFY2(pscan.preset->currentIndex() == custom(pscan), change.what);
      change.undo();
      QVERIFY2(pscan.preset->currentIndex() == 0, change.what);
   }

   // Colour and Auto are the same to the scanner, but not to a preset
   choose(pscan, 1);
   QCOMPARE(pscan.preset->currentIndex(), 1);
   pscan.colour->click();
   QCOMPARE(fake._format, QScanner::colour);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));
   pscan.autoMode->click();
   QCOMPARE(pscan.preset->currentIndex(), 1);
}

void TestPscan::testPresetSelect()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);

   choose(pscan, 1);
   QCOMPARE(fake._format, QScanner::colour);
   QCOMPARE(fake._dpi, 200);
   QVERIFY(fake._duplex);
   QVERIFY(xmlConfig->boolValue("SCAN_AUTO_COLOUR"));
   QVERIFY(pscan.autoMode->isChecked());
   QCOMPARE(pscan.res->currentText(), QString("200"));
   QCOMPARE(pscan.preset->currentIndex(), 1);

   // Ctrl-1
   pscan.presetShortcut(1);
   QCOMPARE(fake._format, QScanner::mono);
   QCOMPARE(fake._dpi, 300);
   QVERIFY(!xmlConfig->boolValue("SCAN_AUTO_COLOUR"));
   QCOMPARE(pscan.preset->currentIndex(), 0);

   // one of our own
   pscan.grey->click();
   pscan.duplex->click();
   QCOMPARE(pscan.presetAddNamed("Grey"), QString());

   choose(pscan, 0);
   QCOMPARE(fake._format, QScanner::mono);
   QVERIFY(fake._duplex);

   choose(pscan, 2);
   QCOMPARE(fake._format, QScanner::grey);
   QVERIFY(!fake._duplex);
   QCOMPARE(pscan.preset->currentIndex(), 2);
}

void TestPscan::testPresetEdit()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   choose(pscan, 0);

   int count = pscan._presets.size();
   QVERIFY(enabled(pscan, count, Preset::add));
   QVERIFY(enabled(pscan, count, Preset::delete_it));
   QVERIFY(!enabled(pscan, count, Preset::custom));

   // Add
   pscan.grey->click();
   QVERIFY(!pscan.presetAddNamed("").isEmpty());
   QCOMPARE(pscan.presetAddNamed("Grey"), QString());
   QCOMPARE(pscan._presets.size(), 3u);
   QCOMPARE(pscan.preset->itemText(2), QString("Grey"));
   QCOMPARE(pscan.preset->currentIndex(), 2);
   QVERIFY(pscan.presetAddNamed("Again").contains("Grey"));
   QCOMPARE(pscan._presets.size(), 3u);

   // it is saved
   {
      Pscan again;

      QCOMPARE(again._presets.size(), 3u);
      QCOMPARE(again._presets[2]._name, QString("Grey"));
      QCOMPARE(again._presets[2]._format, QScanner::grey);
   }

   // Delete, after which the panel matches none
   pscan.presetDelete(2);
   QCOMPARE(pscan._presets.size(), 2u);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));
   QVERIFY(!enabled(pscan, 2, Preset::delete_it));
   {
      Pscan again;

      QCOMPARE(again._presets.size(), 2u);
   }
}

void TestPscan::testReset()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   choose(pscan, 0);

   pscan.reset_clicked();
   QCOMPARE(pscan.preset->currentIndex(), pscan.defaultPreset());
   QCOMPARE(fake._format, QScanner::colour);
   QCOMPARE(fake._dpi, 200);
}

void TestPscan::testNoScanner()
{
   FakeControl fake;
   Pscan pscan;

   fake._present = false;
   attach(pscan, fake);

   QVERIFY(!pscan.mono->isEnabled());
   QVERIFY(!pscan.res->isEnabled());
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));
   QVERIFY(!pscan.presetAddNamed("Nothing").isEmpty());
   QCOMPARE(pscan._presets.size(), 2u);
}
