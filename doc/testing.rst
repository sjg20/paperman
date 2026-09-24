Testing
=======

Paperman includes a built-in test suite that exercises the desktop operations,
directory model, search server, and OCR indexing.

Two suites drive the application through real UI events rather than calling
operations directly: ``TestDesktopUi`` covers desktop interactions (clicking
and double-clicking stacks, toolbar navigation, filtering, folder search and
the directory tree) and ``TestPagewidget`` covers the page view (thumbnail
selection, zoom and display rotation). These run the ``Mainwindow`` with the
offscreen platform plugin and use ``QTest`` mouse and keyboard events, so
they check the behaviour the user actually sees.

Running Tests
-------------

Build with test support enabled (the default ``qmake CONFIG+=test`` build) and
run:

.. code:: bash

   QT_QPA_PLATFORM=offscreen ./paperman -t

This runs every test suite in sequence and prints results for each one.

Running a Single Suite
----------------------

Pass the class name after ``-t`` to run only that suite:

.. code:: bash

   QT_QPA_PLATFORM=offscreen ./paperman -t TestSearchServer

Listing Available Suites
------------------------

Use ``-t list`` to print the registered suite names:

.. code:: bash

   ./paperman -t list

Test Files
----------

All test files live in ``test/files/`` and are generated at build time, not
tracked in git. Run ``make test-setup`` to create them:

.. code:: bash

   make test-setup

This calls ``scripts/make_test_files.py`` which generates PDFs, ``.max``
files, and a plasma JPEG. The ``make test`` target depends on ``test-setup``
so the files are created automatically before running tests.

The search-server tests copy them into temporary directories for each run so
the originals are never modified.

The Fake Scanner
----------------

The tests have a scanner of their own: ``test/fakescan`` is a SANE back end
which answers as the fujitsu back end does for an fi-8170, with the same
option names and values and a window placed on the sheet in the same way.
It is built with paperman, and libsane loads it as it would a real
scanner's back end, so paperman drives it through the same calls.

Before any suite runs, ``libsane`` is given a configuration listing only
this back end. The tests therefore cannot reach a real scanner, nor wait
while other back ends look for theirs. Set ``PAPERMAN_TEST_DEVICE`` to a
real scanner and this is left out, for the few tests which use one.

What the scanner scans is whatever a test puts in its hopper, through the
back door in ``test/fakescan/fakescan.h``. A test does that with the
``Fakescan`` helper in ``test/test_fakescan.h``:

.. code:: c++

   QImage front (85, 110, QImage::Format_RGB32);   // US letter at 10dpi

   front.fill (Qt::white);
   Fakescan::loadSheet (front, QImage (), 10);     // plain paper behind

Each sheet is a picture of the paper, laid on the scanner's backing inside
the window and sent at the resolution and in the mode the front end asks
for, as raw lines or, with ``compression`` set to JPEG, as a JPEG. A sheet
narrower than the window has backing either side of it, and one shorter
than the window has backing below it, as on the real scanner. A sheet can
go through askew, by giving ``loadSheet()`` an angle. When the hopper is
empty the scanner says so, as a real one does.

The settings with which a Fujitsu scanner finds the size of the sheet
work as they do on an fi-8170, awkward parts included:

``ald``
   The page ends at the foot of the sheet, and the scanner says the
   length is unknown (``lines`` is -1) until it gets there. A JPEG's
   header still promises the height of the whole window, with the picture
   ending short of it.

``hwdeskewcrop``
   The sheet comes back straightened and cut down to itself, and the
   scanner can only say its size once it has read the whole of it. A JPEG
   of a sheet which went through askew is bigger than that size: it holds
   the box the sheet went through in, with the sheet upright in the middle
   of it on a black ground.

``buffermode`` and ``stop-feed``
   With buffer mode on, the feeder takes sheets from the hopper four ahead
   of the one being read, and a cancel sends those through unscanned.
   ``stop-feed`` stops the feeder but keeps what it has taken, and the
   batch ends once those have been read.

A test can also have things go wrong, with ``Fakescan::addFault()``, at a
given sheet and side, either when the side starts or part-way down it: a
jam or double feed, the cover opening, a frame which ends early or has
nothing in it, a spoilt JPEG, a scanner which stays busy and takes its
time saying so, and one which stops answering. The scanner then stays as
a real one would until ``Fakescan::clear()``, which is the person at the
scanner clearing the paper path. ``Fakescan::press()`` presses a button
on it, and ``Fakescan::log()`` gives what paperman asked of it, which
also shows if paperman ever made two calls on it at once.

A person has half a minute to clear a jam, and a test does not want to
wait that long, so ``TestFakescan`` makes paperman's waits for the
scanner shorter with ``Paperscan::setTimeScale()``.

The tests of the scanner in ``TestFakescan`` show how to drive it, and its
tests of paperman scanning into a stack check what paperman does with the
pages it sends, including when something goes wrong.

Driving it by hand
~~~~~~~~~~~~~~~~~~

paperman offers the fake scanner beside the real ones when it is given a
directory to feed it from:

.. code:: bash

   ./paperman --fake-scanner /tmp/fs

or with ``PAPERMAN_FAKE_SCANNER=/tmp/fs`` in the environment. It then
shows in the list of scanners as ``FUJITSU fi-8170 (fake)``, named
``fakefujitsu:fi-8170:00001``, and can be chosen like any other.

Put pictures of sheets in ``/tmp/fs/hopper``: ``page.png`` is the front of
a sheet and ``page.back.png``, if there is one, its back. They are fed in
order of name and moved to ``/tmp/fs/fed`` as they are taken. A picture is
taken to be at the resolution it says, or at 300dpi if it says less than
100dpi, which is usually a default. ``touch /tmp/fs/press-scan`` presses
the Scan button.

The fake scanner is built on Linux, beside paperman in ``test/fakescan``,
where paperman looks for it. Another SANE front end can use it through a
script, which gives libsane a configuration listing only the fake:

.. code:: bash

   scripts/fakescan.sh /tmp/fs scanimage -d fakefujitsu:fi-8170:00001 \
      --source "ADF Duplex" --batch

Code Coverage
-------------

To see how much of the C++ code the tests exercise, run:

.. code:: bash

   make coverage

This builds the test binary with gcov instrumentation in
``build-coverage/`` (a shadow build, so the normal build directories are
untouched), runs the whole suite headless and writes an annotated
per-file report to ``build-coverage/coverage.html`` plus a plain-text
summary to ``build-coverage/coverage.txt``.  The report covers the
application code only: the tests themselves, generated moc/qrc/ui files
and the bundled QuiteInsane scanner code under ``qi/`` are excluded.

The report needs `gcovr <https://gcovr.com/>`_ (``apt install gcovr``).
``scripts/coverage.sh`` accepts ``QMAKE``, ``JOBS`` and ``SUITE``
environment variables; set ``SUITE`` to a suite name (as accepted by
``paperman -t``) to measure the coverage of a single suite:

.. code:: bash

   SUITE=TestSearchServer scripts/coverage.sh

Flutter Widget Tests
--------------------

The Flutter app has its own widget tests in ``app/test/``.  Run them with:

.. code:: bash

   make app-test

This runs ``flutter test`` inside the ``app/`` directory.  The tests use
`mocktail <https://pub.dev/packages/mocktail>`_ to mock ``ApiService`` and
cover the ``ViewerScreen`` UI states: loading indicator, error/retry, page
counter, and page slider behaviour.
