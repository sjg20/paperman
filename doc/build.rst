Building
========

This page is a quick reference for building all paperman components.  See
:doc:`develop` for prerequisites and project layout, and :doc:`app` for
Flutter-specific setup (Java, Flutter SDK, Android SDK).

Environment
-----------

Make sure these are set before building:

.. code:: bash

   export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
   export ANDROID_HOME=~/android-sdk
   export PATH="$HOME/flutter/bin:$JAVA_HOME/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"

Build Everything
----------------

A plain ``make`` in the project root builds all components -- the desktop app,
server, Flutter app and Sphinx documentation:

.. code:: bash

   make

The ``GNUmakefile`` wraps the qmake-generated ``Makefile`` and adds the extra
targets listed below.

Build Date
~~~~~~~~~~

The Flutter targets pass the current date and time as a Dart define
(``BUILD_DATE``) so it appears in the app UI.  This happens automatically
when building through ``make``.  Running ``flutter build`` directly skips
this, causing the app to show "Built: unknown".

Build Targets
-------------

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Target
     - Description
   * - ``make``
     - Build everything (desktop app, server, Flutter app, docs)
   * - ``make paperman``
     - Build the Qt desktop app only
   * - ``make paperman-server``
     - Build the standalone search server
   * - ``make app``
     - Build the Flutter app (Android APK + Linux)
   * - ``make app-apk``
     - Build the Android APK only
   * - ``make app-aab``
     - Build the Android App Bundle only
   * - ``make app-linux``
     - Build the Flutter Linux binary only
   * - ``make app-demo``
     - Generate demo assets (PDFs + thumbnails)
   * - ``make docs``
     - Build the Sphinx documentation

Test targets:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Target
     - Description
   * - ``make test``
     - Run all tests (builds desktop app and server first)
   * - ``make test-setup``
     - Generate test files without running tests
   * - ``make test-progressive``
     - Run progressive-loading tests
   * - ``make test-parallel``
     - Run parallel tests

Publishing and upload targets:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Target
     - Description
   * - ``make app-publish``
     - Build AAB and upload to Play Store internal testing
   * - ``make app-upload``
     - Build APK and upload to Google Drive via rclone
   * - ``make app-scp``
     - Build APK and copy to a web server via scp
   * - ``make app-scp-only``
     - Copy a previously built APK without rebuilding

Clean targets:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Target
     - Description
   * - ``make clean``
     - Remove all build artefacts
   * - ``make app-clean``
     - Clean Flutter build outputs only
   * - ``make docs-clean``
     - Clean Sphinx build outputs only

Other:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Target
     - Description
   * - ``make info``
     - List built binaries and their sizes
   * - ``make help``
     - Print a summary of all targets

Output Locations
----------------

::

   paperman                                             Desktop app
   paperman-server                                      Search server
   app/build/app/outputs/flutter-apk/app-release.apk    Android APK
   app/build/app/outputs/bundle/release/app-release.aab  Android App Bundle
   app/build/linux/x64/release/bundle/paperman           Linux Flutter binary
   doc/_build/html/                                      Documentation
