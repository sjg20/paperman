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

Large test files live in ``test/files/`` and are not tracked in git. The
search-server tests copy them into temporary directories for each run so the
originals are never modified.
