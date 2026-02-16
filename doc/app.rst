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

Java JDK
~~~~~~~~

Gradle requires Java 17 or newer with a full JDK (not just a JRE).
Install the headless JDK package:

.. code:: bash

   sudo apt-get install -y openjdk-21-jdk-headless

Then set ``JAVA_HOME`` so Gradle finds the compiler:

.. code:: bash

   export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64

Add this to your shell profile to make it permanent.  You can verify
that ``javac`` is available:

.. code:: bash

   $JAVA_HOME/bin/javac -version

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

   yes | sdkmanager --licenses
   sdkmanager "platform-tools" "platforms;android-35" "build-tools;35.0.0"
   flutter config --android-sdk ~/android-sdk

Gradle auto-installs extra components (NDK, CMake, additional
platforms) on the first build, so an internet connection is needed.

Verify that both toolchains are working:

.. code:: bash

   flutter doctor

Troubleshooting
~~~~~~~~~~~~~~~

**Stale build directory** -- If the ``app/build/`` tree was created on a
different machine (e.g. a CI container), Gradle caches contain
hard-coded paths that cause ``Failed to create parent directory``
errors.  Delete ``app/build/`` and rebuild:

.. code:: bash

   rm -rf app/build

**"Does not provide the required capabilities: [JAVA_COMPILER]"** --
Gradle found a JRE but not a JDK.  Make sure
``openjdk-21-jdk-headless`` (not just the JRE) is installed and
``JAVA_HOME`` points to it.

**"Toolchain installation ... does not provide the required
capabilities"** after installing the JDK -- Gradle may have cached
the old JDK probe results.  Clear the Gradle caches and retry:

.. code:: bash

   rm -rf ~/.gradle/caches ~/.gradle/daemon ~/.gradle/native

Building
--------

Make sure the following environment variables are set (from the
prerequisites above):

.. code:: bash

   export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
   export ANDROID_HOME=~/android-sdk
   export PATH="$HOME/flutter/bin:$JAVA_HOME/bin:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"

Then, from the top-level directory:

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

Architecture
------------

The app uses Provider-based dependency injection with a single ``ApiService``
instance created at the root of the widget tree.  There are four screens
connected by imperative ``Navigator.push`` / ``pushReplacement`` navigation.
All HTTP calls go through the shared ``ApiService``, and the UI follows
Material 3 with automatic dark-mode support driven by the system theme.

Data Models
-----------

Five model classes live in ``models/models.dart``.  Each has a
``fromJson()`` factory constructor for deserialising server responses.

``Repository``
   A paper repository on the server.  Fields: ``path`` (filesystem path),
   ``name`` (display name) and ``exists`` (whether the path is valid).

``DirectoryEntry``
   A subdirectory inside a repository.  Fields: ``name``, ``path`` and
   ``count`` (number of items inside).

``FileEntry``
   A single document.  Fields: ``name``, ``path``, ``size`` (bytes) and
   ``modified`` (date string).

``BrowseResult``
   Response from the ``/browse`` endpoint.  Fields: ``path`` (the directory
   that was listed), ``directories`` (list of ``DirectoryEntry``) and
   ``files`` (list of ``FileEntry``).

``SearchResult``
   Response from the ``/search`` endpoint.  Fields: ``count`` (total hits)
   and ``results`` (list of ``FileEntry``).

API Service
-----------

``api_service.dart`` contains the ``ApiService`` class and a small
``ApiException`` class.

Base URL handling
   The constructor and ``updateConfig()`` strip trailing slashes from the
   URL.  ``ConnectionScreen`` auto-prepends ``https://`` when the user
   enters a bare hostname.

Authentication
   When a username is configured, ``_basicAuth`` produces a Base64-encoded
   ``Authorization: Basic`` header.  The ``_headers`` getter attaches this
   header (plus ``Accept: application/json``) to every request.  The public
   ``basicAuth`` getter lets widgets such as ``FileTile`` pass the same
   credentials to ``CachedNetworkImage``.

``_getJson()`` helper
   Performs a GET request, checks for 401 and other error codes, parses the
   JSON body and returns it.  All typed endpoint methods (``getRepos()``,
   ``browse()``, ``search()``) are thin wrappers around this helper.

URL builders
   ``getFileUrl()`` and ``getThumbnailUrl()`` return ``Uri`` objects without
   embedded credentials -- widgets use them together with auth headers.

Streaming downloads
   ``downloadFilePageStreamed()`` uses ``http.Client.send()`` to stream a
   single-page PDF.  It accepts an ``onProgress`` callback that reports
   bytes received versus ``Content-Length``, which ``ViewerScreen`` turns
   into a percentage indicator.

Page-count query
   ``getPageCount()`` calls the ``/file`` endpoint with ``pages=true`` to
   retrieve the number of pages in a document without downloading it.

Screens
-------

ConnectionScreen
~~~~~~~~~~~~~~~~

Login form with URL, username and password fields.  On startup it loads
saved values from ``SharedPreferences`` (keys ``server_url``, ``username``,
``password``) and, if a URL exists, auto-connects by calling
``ApiService.checkStatus()``.  The ``autoConnect`` flag (default ``true``)
is set to ``false`` when the user disconnects, preventing an immediate
reconnect loop.  After a successful status check the credentials are
persisted and the screen is replaced (``pushReplacement``) with
``BrowseScreen``.  The app version and build date appear at the bottom.

BrowseScreen
~~~~~~~~~~~~

Main navigation screen.  Loads the repository list on init and selects the
first one.  A ``PopupMenuButton`` in the app bar lets the user switch
repositories (only shown when more than one exists).  The directory listing
is wrapped in a ``RefreshIndicator`` for pull-to-refresh.

Breadcrumbs are rendered in a horizontal ``ListView`` at the top of the
screen.  Tapping a breadcrumb segment navigates to that directory.  Below
the breadcrumbs the listing shows directories (``DirectoryTile``) followed
by files (``FileTile``).  Tapping a directory calls ``_browse()`` with the
new path; tapping a file pushes ``ViewerScreen``.  The search icon pushes
``SearchScreen`` with the current repo and path.

SearchScreen
~~~~~~~~~~~~

Takes the current repository and path as constructor arguments.  A
checkbox controls whether the search is scoped to the current directory or
runs across the whole repository.  Results are displayed in a
``ListView.builder`` of ``FileTile`` widgets with ``showFullPath: true``
so the user can see where each match lives.  Tapping a result pushes
``ViewerScreen``.

ViewerScreen
~~~~~~~~~~~~

The most complex screen.  It receives the file path, display name and
optional repo, then:

1. Queries ``getPageCount()`` to learn how many pages the document has.
2. Immediately fetches page 1 via ``downloadFilePageStreamed()``.
3. Writes the returned bytes to a temp file under the pattern
   ``paperman_<safeName>_p<N>.pdf`` in the system temp directory.
4. Displays each page inside a horizontal ``PageView.builder``.  Each page
   item is either a ``PDFView`` widget (from ``flutter_pdfview``) when the
   file is ready, or a ``CircularProgressIndicator`` with a download
   percentage while streaming.
5. On every page change, ``_prefetchAround()`` downloads pages in a window
   of one page before and four pages ahead of the current position.  The
   ``_fetching`` set prevents duplicate requests.

Widgets
-------

FileTile
~~~~~~~~

A ``ListTile`` showing a ``CachedNetworkImage`` thumbnail (48 x 48), the
filename (or full path when ``showFullPath`` is ``true``) and a
human-readable file size (B / KB / MB).  The thumbnail URL is built by
``ApiService.getThumbnailUrl()`` and the ``Authorization`` header is passed
through via the ``httpHeaders`` parameter of the image widget.

DirectoryTile
~~~~~~~~~~~~~

A ``ListTile`` with an amber folder icon, the directory name and an item
count shown in the trailing position.

Navigation Flow
---------------

::

   ConnectionScreen ──pushReplacement──> BrowseScreen ──push──> ViewerScreen
                   <──pushReplacement──          │
                                                 └────push──> SearchScreen

``pushReplacement`` is used between ``ConnectionScreen`` and
``BrowseScreen`` so that the back button on the browse screen does not
return to the login form.  ``push`` is used for ``ViewerScreen`` and
``SearchScreen`` so the user can pop back to the directory listing.

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

   make app-aab

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

Automated upload
~~~~~~~~~~~~~~~~

The build system includes the `Gradle Play Publisher
<https://github.com/Triple-T/gradle-play-publisher>`_ plugin so you can
upload an app bundle to the Play Store internal testing track from the
command line.

**Service account setup**

You can use an existing Google Cloud service account or create a new one.
Either way it needs a JSON key and the right Play Console permissions:

1. In the `Google Cloud Console <https://console.cloud.google.com/>`_, go to
   **IAM & Admin -> Service Accounts**.

   - To use an existing account, click it and skip to step 3.
   - To create a new one, click **Create Service Account**, give it a name
     (e.g. ``play-publisher``) and click **Done** (no extra Cloud IAM roles
     are needed -- permissions are granted in Play Console instead).

2. In the `Google Play Console <https://play.google.com/console>`_, go to
   **Users and permissions -> Invite new users**.  Paste the service
   account's email address (it looks like
   ``name@project.iam.gserviceaccount.com``).  Under **Account
   permissions**, grant at least **Release to production, exclude devices,
   and use Play App Signing** (which covers all release tracks including
   internal testing).  Click **Invite user** and confirm.

   .. note::

      Only the Play Console **account owner** can invite service accounts.
      It can take up to 24 hours for Google's servers to activate a newly
      invited service account.

3. Back in the Cloud Console, click the service account, go to
   **Keys -> Add Key -> Create new key -> JSON** and download the file.
   Save it as ``app/android/play-account.json`` (this path is gitignored).

**Running the upload**

.. code:: bash

   make app-publish

This builds the AAB (``make app-aab``) then runs
``./gradlew publishReleaseBundle``, which uploads it to the internal
testing track.  Check the Play Console under **Internal testing** for the
new release.

Quick install via Google Drive
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Play Store processing can take up to an hour.  For faster testing, upload
the APK to Google Drive and install it directly on the device:

.. code:: bash

   make app-upload

This builds the APK (``make app-apk``) and copies it to the ``apps``
folder on Google Drive using ``rclone``.  Open the file from Google Drive
on the device to install it.

**rclone setup** (one-time):

1. Install rclone (``sudo apt install rclone``).
2. On a machine with a browser, run ``rclone authorize "drive"`` and
   complete the OAuth flow.
3. On the build machine, run ``rclone config``, create a remote called
   ``gdrive`` of type ``drive``, and when asked for auto config answer
   **n** and paste the token from step 2.

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
