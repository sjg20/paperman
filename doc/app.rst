Paperman Mobile App
====================

A Flutter app for browsing and viewing documents on a paperman server.

Overview
--------

The Paperman mobile app provides a convenient way to access your paper
repositories from an Android device. It connects to a running paperman
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

Building
--------

Requires Flutter SDK (tested with 3.41.x) and the Android SDK.

.. code:: bash

   cd app
   flutter pub get
   flutter build apk --debug

The APK is written to ``build/app/outputs/flutter-apk/app-debug.apk``.

You can also build the app from the top-level directory:

.. code:: bash

   make app

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
