Paperman Mobile App
====================

A Flutter app for Android and iPhone for browsing, viewing and scanning
documents on a paperman server.

To build it or change it, see :doc:`app-develop`.

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
   directory, by name or by the text on their pages
-  **View** documents as PDF (server converts .max, .jpg, .tiff on the fly)
-  **Scan** pages with the phone's camera into the folder shown, as a PDF
-  **Multiple repositories** with a switcher in the toolbar
-  **Logging in** to a server with accounts of its own, or **HTTP Basic
   Auth** for servers behind nginx authentication
-  **Demo mode** for trying the app without a server (for Play Store
   reviewers)
-  **Dark mode** follows the system theme
-  **Local URL** for fast downloads when on the same LAN as the server
-  Server URL and user name saved locally for auto-reconnect, with the
   password in the phone's secure storage (the Android Keystore or the
   iPhone's Keychain)

Scanning
--------

The scan button at the foot of a folder opens the phone's document
scanner: Google's on Android (7 or later), Apple's on iPhone. It finds the
edges of each page and straightens it, and takes as many pages as wanted
in one go. The pages are made into a PDF, named for the day as the desktop
names a scan (which can be changed), and uploaded into the folder shown,
where any desktop showing it sees it at once. A stack of that name already
there is not overwritten: the scan is given a new name. Should the upload
fail, as when the phone loses its connection, it can be tried again
without scanning again.

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

Demo Mode
---------

The app includes a demo mode so that Google Play reviewers (and anyone
without a paperman server) can explore the full UI.  Tap **Try Demo** on
the login screen to enter demo mode.  No server connection is needed --
all data comes from bundled assets.

The demo catalogue contains one repository ("Sample Documents") with two
directories and three documents:

- ``Reports/Annual Report.pdf`` -- 100 pages with coloured chapter headings
- ``Reports/Summary.pdf`` -- 5-page colour PDF with text and tables
- ``Photos/Image Document.pdf`` -- 2 pages with an embedded image and text

Browsing, search and the PDF viewer all work normally in demo mode.
Tapping the disconnect (logout) button returns to the login screen and
leaves demo mode.
