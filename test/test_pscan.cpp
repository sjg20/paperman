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

static void chooseSize(Pscan &pscan, const QString &prefix)
{
   for (int row = 0; row < pscan.pageSize->count(); row++)
      if (pscan.pageSize->itemText(row).startsWith(prefix)) {
         pscan.pageSize->setCurrentIndex(row);
         pscan.size_activated(row);
         return;
      }
   QFAIL(qPrintable("no size " + prefix));
}

static void setBright(Pscan &pscan, int value)
{
   pscan.bright->setValue(value);
   pscan.brightChanged(value);
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
   QString usual = pscan.pageSize->currentText();

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
      {"size", [&] { chooseSize(pscan, "A5"); },
               [&] { chooseSize(pscan, usual); }},
      {"feed", [&] { pscan.sideways->setCurrentIndex(1);
                     pscan.sideways_activated(1); },
               [&] { pscan.sideways->setCurrentIndex(0);
                     pscan.sideways_activated(0); }},
      {"auto-size", [&] { pscan.autosize->click(); },
                    [&] { pscan.autosize->click(); }},
      {"straighten", [&] { pscan.deskew->click(); },
                     [&] { pscan.deskew->click(); }},
      {"exposure", [&] { setBright(pscan, 60); },
                   [&] { setBright(pscan, 127); }},
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

   // one of our own, with all of the settings
   pscan.grey->click();
   pscan.duplex->click();
   chooseSize(pscan, "A5");
   pscan.autosize->click();
   pscan.deskew->click();
   setBright(pscan, 40);
   pscan.contrast->setValue(-20);
   pscan.contrastChanged(-20);
   QCOMPARE(pscan.presetAddNamed("Everything"), QString());

   choose(pscan, 0);
   QCOMPARE(fake._format, QScanner::mono);
   QVERIFY(fake._duplex);
   QVERIFY(!fake._autosize);
   QVERIFY(!fake._deskew);

   choose(pscan, 2);
   QCOMPARE(fake._format, QScanner::grey);
   QVERIFY(!fake._duplex);
   QVERIFY(fake.size().startsWith("A5"));
   QVERIFY(pscan.pageSize->currentText().startsWith("A5"));
   QVERIFY(fake._autosize);
   QVERIFY(fake._deskew);
   QCOMPARE(fake._bright, 40);
   QCOMPARE(fake._contrast, -20);
   QCOMPARE(pscan.preset->currentIndex(), 2);
}

void TestPscan::testOldPresetGivesUsual()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   QString usual = pscan.pageSize->currentText();

   pscan.autosize->click();
   chooseSize(pscan, "A5");
   setBright(pscan, 60);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));

   // the standard presets have none of these
   choose(pscan, 0);
   QVERIFY(!fake._autosize);
   QVERIFY(!pscan.autosize->isChecked());
   QCOMPARE(pscan.pageSize->currentText(), usual);
   QCOMPARE(fake._exposure, 127);
   QCOMPARE(pscan.preset->currentIndex(), 0);

   // in colour, brightness and contrast
   fake._bright = 50;
   fake._contrast = 50;
   choose(pscan, 1);
   QCOMPARE(fake._bright, 0);
   QCOMPARE(fake._contrast, 0);
   QCOMPARE(pscan.preset->currentIndex(), 1);
}

void TestPscan::testPresetEdit()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   choose(pscan, 0);

   int count = pscan._presets.size();
   QVERIFY(enabled(pscan, count, Preset::add));
   QVERIFY(enabled(pscan, count, Preset::update));
   QVERIFY(enabled(pscan, count, Preset::rename));
   QVERIFY(enabled(pscan, count, Preset::delete_it));
   QVERIFY(!enabled(pscan, count, Preset::custom));
   QCOMPARE(pscan.preset->itemText(count + Preset::update),
            QString("Update 'Monochrome 300dpi duplex'"));

   // Add
   pscan.grey->click();
   chooseSize(pscan, "A5");
   QVERIFY(!pscan.presetAddNamed("").isEmpty());
   QCOMPARE(pscan.presetAddNamed("Grey"), QString());
   QCOMPARE(pscan._presets.size(), 3u);
   QCOMPARE(pscan.preset->itemText(2), QString("Grey"));
   QCOMPARE(pscan.preset->currentIndex(), 2);
   QCOMPARE(pscan._chosen, 2);
   QCOMPARE(pscan.preset->itemText(3 + Preset::update),
            QString("Update 'Grey'"));
   QVERIFY(pscan.presetAddNamed("Again").contains("Grey"));
   QCOMPARE(pscan._presets.size(), 3u);

   // Update keeps its place, though the panel was changed away from it
   setBright(pscan, 90);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));
   pscan.presetUpdateUser();
   QCOMPARE(pscan._presets.size(), 3u);
   QCOMPARE(pscan._presets[2]._bright, 90);
   QCOMPARE(pscan.preset->currentIndex(), 2);

   // Rename
   pscan.presetRename(2, "Receipts");
   QCOMPARE(pscan.preset->itemText(2), QString("Receipts"));
   QCOMPARE(pscan.preset->itemText(3 + Preset::update),
            QString("Update 'Receipts'"));

   // it is all saved
   {
      Pscan again;

      QCOMPARE(again._presets.size(), 3u);
      QCOMPARE(again._presets[2]._name, QString("Receipts"));
      QCOMPARE(again._presets[2]._format, QScanner::grey);
      QCOMPARE(again._presets[2]._bright, 90);
      QVERIFY(again._presets[2]._size.startsWith("A5"));
   }

   // Delete, after which the panel matches none and nothing is chosen
   pscan.presetDelete(2);
   QCOMPARE(pscan._presets.size(), 2u);
   QCOMPARE(pscan._chosen, -1);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));
   QVERIFY(!enabled(pscan, 2, Preset::update));
   QVERIFY(!enabled(pscan, 2, Preset::rename));
   QVERIFY(!enabled(pscan, 2, Preset::delete_it));
   QCOMPARE(pscan.preset->itemText(2 + Preset::update),
            QString("Update preset"));
   {
      Pscan again;

      QCOMPARE(again._presets.size(), 2u);
   }
}

void TestPscan::testPresetMenu()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   choose(pscan, 0);
   setBright(pscan, 90);

   // Update, as though picked from the list
   int update = pscan._presets.size() + Preset::update;
   choose(pscan, update);
   QCOMPARE(pscan._presets[0]._bright, 90);
   QCOMPARE(pscan.preset->currentIndex(), 0);

   // with the scanner gone there is nothing to update from
   fake._present = false;
   pscan.refresh();
   choose(pscan, update);
   QCOMPARE(pscan.preset->currentIndex(), custom(pscan));
}

void TestPscan::testBrightShown()
{
   // outside the range the sliders start with
   {
      FakeControl fake;
      Pscan pscan;

      fake._exposure = 200;
      attach(pscan, fake);
      QCOMPARE(pscan.bright->value(), 200);
   }
   {
      FakeControl fake;
      Pscan pscan;

      fake._format = QScanner::grey;
      fake._bright = -50;
      fake._contrast = 60;
      attach(pscan, fake);
      QCOMPARE(pscan.bright->value(), -50);
      QCOMPARE(pscan.contrast->value(), 60);
   }
}

void TestPscan::testReset()
{
   FakeControl fake;
   Pscan pscan;

   attach(pscan, fake);
   choose(pscan, 0);
   chooseSize(pscan, "A5");

   pscan.reset_clicked();
   QCOMPARE(pscan.preset->currentIndex(), pscan.defaultPreset());
   QCOMPARE(fake._format, QScanner::colour);
   QCOMPARE(fake._dpi, 200);
   QVERIFY(!pscan.pageSize->currentText().startsWith("A5"));
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

void TestPscan::testPresetBeforeScanner()
{
   FakeControl fake;
   Pscan pscan;

   /* A first run, with no scanner chosen yet, starts with the default
      preset, before the scanner is chosen and opened */
   xmlConfig->setStringValue("LAST_DEVICE", "");
   fake._present = false;
   attach(pscan, fake);
   QCOMPARE(pscan.preset->currentIndex(), pscan.defaultPreset());

   /* the scanner opens as it was made, which is no preset, and is shown
      before it can be set, as the program opens the scanner before
      making the dialog which sets it */
   fake._present = true;
   fake._settable = false;
   fake._format = QScanner::mono;
   fake._dpi = 600;
   fake._duplex = false;
   pscan.refresh();
   QCOMPARE(fake._dpi, 600);
   QCOMPARE(pscan.preset->currentIndex(), pscan.defaultPreset());

   // once it can be set, it is given the preset
   fake._settable = true;
   pscan.refresh();
   QCOMPARE(fake._format, QScanner::colour);
   QCOMPARE(fake._dpi, 200);
   QVERIFY(fake._duplex);
   QCOMPARE(pscan.preset->currentIndex(), pscan.defaultPreset());

   // and only the once: after that, the scanner is left as it is set
   fake._dpi = 300;
   pscan.refresh();
   QCOMPARE(fake._dpi, 300);
}
