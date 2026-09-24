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
It is built with the tests, and libsane loads it as it would a real
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
for. A sheet narrower than the window has backing either side of it, and
one shorter than the window has backing below it, as on the real scanner.
When the hopper is empty the scanner says so, as a real one does.

The ``TestFakescan`` suite checks the scanner itself and shows how to use
it.

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
