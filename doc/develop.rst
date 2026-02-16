Development
===========

This guide covers building paperman from source and working on the codebase.

Prerequisites
-------------

Install the required development libraries:

.. code:: bash

   sudo apt-get install -y \
     build-essential qt5-qmake qtbase5-dev libqt5sql5-sqlite \
     libpoppler-qt5-dev libpodofo-dev libtiff-dev libjpeg-dev \
     libsane-dev zlib1g-dev

For the documentation you also need Sphinx:

.. code:: bash

   sudo apt-get install -y python3-sphinx python3-sphinx-rtd-theme

Building
--------

Generate the qmake Makefile and build:

.. code:: bash

   qmake "CONFIG+=test" paperman.pro
   make

The ``CONFIG+=test`` flag compiles in the built-in test suites.  Without it
the ``-t`` option is not available.

The top-level ``GNUmakefile`` wraps the qmake-generated ``Makefile`` and adds
extra targets.  GNU make picks it up automatically, so a plain ``make`` in the
project root builds everything including the server, Flutter app and Sphinx
documentation.  Run ``make help`` to list all targets.

Project Layout
--------------

::

   *.cpp / *.h           Desktop app sources (Qt Widgets)
   qi/                   Scanner interface widgets
   test/                 Test sources and generated test data
   scripts/              Helper scripts (test-file generation, etc.)
   doc/                  Sphinx documentation
   app/                  Flutter mobile app
   paperman.pro          Qt project file (desktop app)
   paperman-server.pro   Qt project file (search server)
   GNUmakefile           Top-level build wrapper

Key Source Files
~~~~~~~~~~~~~~~~

``desktopmodel.cpp``
   Central model that manages the stack/page tree shown in the GUI.

``file.cpp``, ``filemax.cpp``, ``filepdf.cpp``, ``filejpeg.cpp``
   File-format backends.  ``File`` is the abstract base; the subclasses
   handle .max, .pdf and .jpg respectively.

``dirmodel.cpp``
   Directory tree model for the left-hand panel.

``desktopundo.cpp``, ``dmop.cpp``, ``dmuserop.cpp``
   Undo/redo framework and desktop-model operations.

``searchserver.cpp``
   Embedded HTTP server for the REST API.

``searchindex.cpp``
   SQLite FTS5 index used by the search server.

Testing
-------

See :doc:`testing` for full details on running and writing tests.

Quick reference:

.. code:: bash

   # Run all suites
   QT_QPA_PLATFORM=offscreen ./paperman -t

   # Run a single suite
   QT_QPA_PLATFORM=offscreen ./paperman -t TestOps

   # List available suites
   ./paperman -t list

   # Generate test files (also done automatically by 'make test')
   make test-setup

Test suites live in ``test/`` and are registered in ``test/suite.cpp``.  Each
suite is a ``QObject`` subclass using the Qt Test framework (``QCOMPARE``,
``QVERIFY``, etc.).  Test data files are generated at build time by
``scripts/make_test_files.py`` and are not tracked in git.

Build Targets
-------------

The most useful targets during development:

.. code:: bash

   make paperman          # Build the desktop app only
   make test              # Build and run all tests (includes test-setup)
   make test-setup        # Generate test files without running tests
   make app-test          # Run Flutter widget tests
   make docs              # Build the Sphinx documentation
   make clean             # Remove all build artefacts

Coding Style
------------

- C++ sources use Qt naming conventions (camelCase for methods, CamelCase
  for classes)
- Indentation is 3 spaces in most files
- Commit messages use present/imperative tense and British spelling
