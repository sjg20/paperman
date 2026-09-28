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

Flutter Builds
~~~~~~~~~~~~~~

Always use ``make`` targets (``make app``, ``make app-linux``, etc.) for
Flutter builds rather than running ``flutter`` directly.  The makefile sets
up Dart defines (``BUILD_DATE`` and others) that the app needs at runtime.
Running ``flutter build`` directly skips these, causing the app to show
"Built: unknown".

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

AppImage
--------

``make appimage`` makes ``dist/Paperman-VERSION-ARCH.AppImage``, using
`linuxdeploy <https://github.com/linuxdeploy/linuxdeploy>`_, which
``scripts/appimage.sh`` fetches the first time. It builds paperman
afresh with Qt 6 in ``build-appimage``, without the tests. An AppImage
runs only on a Linux with the C library it was built with or a later one,
so one built on a recent Linux is only of use there; the release builds
it on Debian 12, in a container:

.. code:: bash

   docker run --rm -v $PWD:/src -w /src debian:12 bash -c '
      apt-get update && apt-get install -y build-essential qmake6 \
         qt6-base-dev qt6-base-dev-tools libqt6sql6-sqlite qt6-scxml-dev \
         libqt6statemachine6 qt6-gtk-platformtheme libpoppler-qt6-dev libpodofo-dev libtiff-dev \
         libsane-dev libjpeg-dev zlib1g-dev pkg-config file curl
      scripts/appimage.sh'

SANE is left out, so that paperman uses the machine's own scanner back
ends; the script checks that the AppImage starts and that it loads the
machine's fujitsu back end.

Snap
----

``snap/snapcraft.yaml`` builds the snap: run ``snapcraft pack`` at the top
of a clean checkout (snapcraft copies in the whole directory) and it
builds in an LXD container, making ``paperman_VERSION_ARCH.snap``. Qt 6
comes from KDE's runtime, through the ``kde-neon-6`` extension, and
Poppler with its Qt bindings and SANE, from the fork's ``all-work``
branch, are built as parts. To try it:

.. code:: bash

   sudo snap install --dangerous paperman_*.snap
   sudo snap connect paperman:raw-usb

When run as a snap, paperman points libsane at the snap's back ends and
configuration, with ``QScanner::useSaneIn()``.

Windows
-------

Paperman builds on Windows with the MSYS2 toolchain, which is also what
CI uses. In a MINGW64 shell (CLANGARM64 on Windows on Arm):

.. code:: bash

   pacman -S make mingw-w64-x86_64-gcc mingw-w64-x86_64-pkgconf \
       mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-scxml \
       mingw-w64-x86_64-poppler-qt6 mingw-w64-x86_64-podofo \
       mingw-w64-x86_64-libtiff mingw-w64-x86_64-libjpeg-turbo
   qmake6 paperman.pro -o Makefile.win
   make -f Makefile.win -j$(nproc)

With `Inno Setup <https://jrsoftware.org/isinfo.php>`_ installed,
``make -f Makefile.win installer`` also builds the installer; see
:doc:`releasing`. Scanners are reached through TWAIN rather than SANE,
by ``win32/twainsane.cpp``.

macOS
-----

Paperman builds on macOS with Qt 6 and Homebrew. Homebrew's Poppler has
no Qt bindings, so Poppler is built separately:

.. code:: bash

   brew install pkgconf podofo libtiff jpeg-turbo sane-backends cmake \
       ninja openjpeg little-cms2 fontconfig
   # Qt 6 from qt.io, or with aqtinstall:
   #    aqt install-qt mac desktop 6.11.3 clang_64 -m qtscxml -O ~/Qt
   curl -LO https://poppler.freedesktop.org/poppler-26.09.0.tar.xz
   tar xf poppler-26.09.0.tar.xz
   cmake -S poppler-26.09.0 -B pbuild -G Ninja -DCMAKE_BUILD_TYPE=Release \
       -DCMAKE_INSTALL_PREFIX=$HOME/poppler \
       -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.3/macos;$(brew --prefix)" \
       -DENABLE_QT5=OFF -DENABLE_GLIB=OFF -DENABLE_NSS3=OFF \
       -DENABLE_GPGME=OFF -DENABLE_LIBCURL=OFF -DENABLE_BOOST=OFF \
       -DENABLE_HARFBUZZ=OFF
   ninja -C pbuild install
   export PKG_CONFIG_PATH=$HOME/poppler/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig
   ~/Qt/6.11.3/macos/bin/qmake6 paperman.pro -o Makefile
   make -f Makefile

That makes ``paperman.app``; a build with ``CONFIG+=test`` makes a plain
``paperman`` instead, for the tests. Use ``make -f Makefile``, since the
``GNUmakefile`` builds the other programs too. On an Intel Mac with the
latest macOS, Homebrew has no ready-built packages, so each needs
``brew install --build-from-source``.
