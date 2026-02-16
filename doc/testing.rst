Testing
=======

Paperman includes a built-in test suite that exercises the desktop operations,
directory model, search server, and OCR indexing.

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

Flutter Widget Tests
--------------------

The Flutter app has its own widget tests in ``app/test/``.  Run them with:

.. code:: bash

   make app-test

This runs ``flutter test`` inside the ``app/`` directory.  The tests use
`mocktail <https://pub.dev/packages/mocktail>`_ to mock ``ApiService`` and
cover the ``ViewerScreen`` UI states: loading indicator, error/retry, page
counter, and page slider behaviour.
