/*
License: GPL-2
  An electronic filing cabinet: scan, print, stack, arrange
 Copyright (C) 2026 Simon Glass, chch-kiwi@users.sourceforge.net
*/

#pragma once

#include <QJsonArray>
#include <QPoint>

#include "suite.h"

class Mainwindow;
class QWidget;

/** Plays Paperman being used, at a person's pace, for scripts/make_video.sh
    to record: browsing the demo papers, scanning into a folder, searching
    and filing. The real pointer moves to each thing clicked, and text is
    typed a letter at a time. Each scene's caption is noted with the time it
    starts, for the script to put on the video.

    The suite is skipped unless PAPERMAN_VIDEO_REPO, the demo papers,
    PAPERMAN_VIDEO_SHEETS, pictures of the sheets to scan, and
    PAPERMAN_VIDEO_DIR, where the captions go, are set */
class TestVideo : public Suite
   {
   Q_OBJECT

public:
   using Suite::Suite;

private slots:
   void init ();

   //! the whole video, as one test, since each scene follows the last
   void testVideo ();

private:
   //! note that a scene with @p caption starts now
   void cue (const QString &caption);

   //! move the pointer to @p pos in @p widget, taking about @p ms
   void glide (QWidget *widget, QPoint pos, int ms = 600);

   //! move to @p pos in @p widget and click there, as a person does
   void click (QWidget *widget, QPoint pos, bool twice = false);

   //! type @p text into @p widget a letter at a time
   void type (QWidget *widget, const QString &text);

   //! let what has been done be seen, for @p ms
   void pause (int ms);

   QString _repo;          //!< the demo papers
   QString _sheets;        //!< pictures of the sheets to scan
   QString _out;           //!< where the captions go
   QJsonArray _cues;       //!< each scene's caption and its time
   };
