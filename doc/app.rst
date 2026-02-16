Paperman Mobile App
====================

A Flutter app for browsing and viewing documents on a paperman server.

Overview
--------

The Paperman mobile app provides a convenient way to access your paper
repositories from an Android or iOS device. It connects to a running paperman
server and lets you browse directories, search for documents, and view
files as PDFs. The server converts .max, .jpg and .tiff files to PDF on
the fly, so all document types are viewable directly in the app.

Features
--------

-  **Browse** directories with thumbnails, breadcrumb navigation and
   pull-to-refresh
-  **Search** documents across the whole repository or within the current
   directory
-  **View** documents as PDF (server converts .max, .jpg, .tiff on the fly)
-  **Multiple repositories** with a switcher in the toolbar
-  **HTTP Basic Auth** for servers behind nginx authentication
-  **Dark mode** follows the system theme
-  Credentials and server URL saved locally for auto-reconnect

Installing Flutter
------------------

Install the system dependencies:

.. code:: bash

   sudo apt-get install cmake ninja-build clang lld pkg-config libgtk-3-dev

Download and extract the Flutter SDK:

.. code:: bash

   curl -fSL -o flutter.tar.xz \
     https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.41.1-stable.tar.xz
   tar xf flutter.tar.xz -C ~/
   rm flutter.tar.xz
   export PATH="$HOME/flutter/bin:$PATH"

Add the ``export PATH`` line to your shell profile to make it permanent.

Verify the installation:

.. code:: bash

   flutter doctor

Building
--------

Requires Flutter SDK (tested with 3.41.x). Start by fetching the
dependencies:

.. code:: bash

   cd app
   flutter pub get

Linux
~~~~~

.. code:: bash

   flutter build linux --dart-define=BUILD_DATE=$(date +%Y-%m-%d)

The binary is written to ``build/linux/x64/release/bundle/paperman``.

You can also build from the top-level directory (this passes the build
date automatically):

.. code:: bash

   make app

Android
~~~~~~~

Requires the Android SDK.

.. code:: bash

   flutter build apk --dart-define=BUILD_DATE=$(date +%Y-%m-%d)

The APK is written to ``build/app/outputs/flutter-apk/app-release.apk``.

iOS
~~~

Requires Xcode and CocoaPods on macOS. Install the CocoaPods
dependencies first, then build:

.. code:: bash

   cd ios
   pod install
   cd ..
   flutter build ios --debug

To run on a connected device:

.. code:: bash

   flutter run

For a release build you need an Apple Developer account and a valid
signing configuration in Xcode. Open ``ios/Runner.xcworkspace`` in
Xcode, set your team and provisioning profile under **Signing &
Capabilities**, then build with:

.. code:: bash

   flutter build ipa

Project Structure
-----------------

::

   lib/
     main.dart                  Entry point, Provider setup, theme
     models/models.dart         Data classes (Repository, FileEntry, etc.)
     services/api_service.dart  REST client for all paperman endpoints
     screens/
       connection_screen.dart   Server URL + credentials input
       browse_screen.dart       Directory listing with breadcrumbs
       search_screen.dart       Full-text search
       viewer_screen.dart       PDF viewer with page navigation
     widgets/
       file_tile.dart           Thumbnail + filename list item
       directory_tile.dart      Folder list item

Server API Endpoints
--------------------

The app talks to the following paperman server endpoints:

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Endpoint
     - Purpose
   * - ``/status``
     - Health check
   * - ``/repos``
     - List repositories
   * - ``/browse``
     - List directories and files
   * - ``/search``
     - Search by filename
   * - ``/file``
     - Download or convert a file to PDF
   * - ``/thumbnail``
     - Get a JPEG thumbnail for a file page

See :doc:`api` for full details on each endpoint.

Connecting to the Server
------------------------

1. Start the paperman server (see :doc:`server`)
2. Open the app and enter the server URL
   (e.g. ``http://192.168.1.10:8080``)
3. If the server is behind nginx with HTTP Basic Auth, enter the
   username and password
4. Tap **Connect** -- the app checks ``/status`` to verify the server is
   reachable
5. Browse repositories and directories, search for documents, or tap a
   file to view it as PDF
