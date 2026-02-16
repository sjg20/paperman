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

Prerequisites
-------------

Flutter SDK
~~~~~~~~~~~

Download and extract the Flutter SDK:

.. code:: bash

   curl -fSL -o flutter.tar.xz \
     https://storage.googleapis.com/flutter_infra_release/releases/stable/linux/flutter_linux_3.41.1-stable.tar.xz
   tar xf flutter.tar.xz -C ~/
   rm flutter.tar.xz
   export PATH="$HOME/flutter/bin:$PATH"

Add the ``export PATH`` line to your shell profile to make it permanent.

Android SDK
~~~~~~~~~~~

If you don't have Android Studio installed, set up the command-line
tools manually:

.. code:: bash

   curl -fSL -o cmdline-tools.zip \
     https://dl.google.com/android/repository/commandlinetools-linux-11076708_latest.zip
   mkdir -p ~/android-sdk/cmdline-tools
   unzip -q cmdline-tools.zip -d ~/android-sdk/cmdline-tools
   mv ~/android-sdk/cmdline-tools/cmdline-tools ~/android-sdk/cmdline-tools/latest
   rm cmdline-tools.zip
   export ANDROID_HOME=~/android-sdk
   export PATH="$ANDROID_HOME/cmdline-tools/latest/bin:$ANDROID_HOME/platform-tools:$PATH"

Accept the licences and install the required SDK components:

.. code:: bash

   sdkmanager --licenses
   sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0"
   flutter config --android-sdk ~/android-sdk

Verify that both toolchains are working:

.. code:: bash

   flutter doctor

Building
--------

From the top-level directory:

.. code:: bash

   make app

This builds both the Android APK and the Linux desktop binary, passing
the current date as the build date. The outputs are:

-  ``app/build/app/outputs/flutter-apk/app-release.apk``
-  ``app/build/linux/x64/release/bundle/paperman``

To build just one target, use ``make app-apk`` or ``make app-linux``.

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

Publishing to the Play Store
-----------------------------

Signing
~~~~~~~

Release builds are signed using a keystore referenced by
``app/android/key.properties`` (gitignored). To create a new keystore:

.. code:: bash

   keytool -genkey -v \
     -keystore app/android/app/upload-keystore.jks \
     -keyalg RSA -keysize 2048 -validity 10000 \
     -alias upload

Then create ``app/android/key.properties``:

.. code:: ini

   storePassword=<your password>
   keyPassword=<your password>
   keyAlias=upload
   storeFile=upload-keystore.jks

Keep the keystore file safe -- you cannot update the app without it.

To change the passwords later:

.. code:: bash

   keytool -storepasswd -keystore app/android/app/upload-keystore.jks
   keytool -keypasswd -alias upload -keystore app/android/app/upload-keystore.jks

Building an app bundle
~~~~~~~~~~~~~~~~~~~~~~

The Play Store prefers an Android App Bundle (``.aab``) over an APK:

.. code:: bash

   cd app && flutter build appbundle

The output is at ``app/build/app/outputs/bundle/release/app-release.aab``

Uploading
~~~~~~~~~

1. Register a `Google Play Developer account
   <https://play.google.com/console>`_ ($25 one-time fee)
2. Create a new app in the Play Console
3. Fill in the store listing: app name, description, category,
   screenshots (phone + tablet), a 512x512 icon, and a privacy policy URL
4. Complete the content rating questionnaire and data safety form
5. Upload the ``.aab`` to a release track (start with internal testing)
6. Submit for review

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
