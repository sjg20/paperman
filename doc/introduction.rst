Introduction
============

Paperman is an electronic filing cabinet: it scans paper into stacks of
pages, and lets you view, arrange, annotate, print, search and send them.
It reads and writes PDF, JPEG and its own variant of PaperPort's .max file
format.

Paperman has three parts:

The desktop program
   for scanning, filing and working with papers, on Linux, Windows and
   macOS (see :doc:`install` and :doc:`scanning`)

The search server, ``paperman-server``
   which serves repositories over the network, with a REST API, so that
   they can be reached from elsewhere: the desktop program shows a
   server's repositories beside its own and can change them, and the
   apps browse them (see :doc:`server` and :doc:`api`)

The Android and iPhone app
   for finding and reading papers from a phone or tablet, from a
   server's repositories, with a full-text search and each document
   shown as a PDF (see :doc:`app`)

So papers can be scanned at a desk, kept on one machine running the
server, and found from anywhere.

.. image:: 1.jpeg
   :width: 30%

.. image:: all.png
   :width: 30%

Features
--------

- Simple GUI based around stacks and pages
- View previews and browse through pages
- Move pages in and out of stacks
- Navigate through directories
- Move stacks between directories
- Print stacks and pages, including page annotations
- Full undo/redo
- Scanning from sheet-fed scanners, fast, with presets (see :doc:`scanning`):

  - pages cut to the sheet, and straightened, by the scanner
  - pages without colour stored as grey or mono, while a page with a
    coloured mark or note on it stays in colour
  - blank pages found and left out
  - sheets fed sideways turned upright, and sheets longer than any paper
    size scanned whole
  - a misfeed or jam waits to be cleared, and loses no pages
  - scanning from the command line (see :doc:`cli`)

- PDF, JPEG and TIFF conversion
- Unfolding scanned booklets into their pages
- OCR engine with full-text search
- Search server with REST API, whose repositories the desktop can show and
  change, with changes made elsewhere shown as they happen (see
  :doc:`server`)
- Android and iPhone app for browsing, searching and reading a server's
  repositories, with an offline demo mode (see :doc:`app`)
- Email files as PDF via Gmail (see `Emailing files`_)
- Linux, Windows (x64 and Arm) and macOS

Emailing files
--------------

The Email menu (Ctrl+Shift+E for "Email as PDF") converts the selected
files to the chosen format, copies the resulting file to the clipboard
as a ``text/uri-list`` and opens Gmail's compose window in the default
browser.  Once the compose window is up, click in the message body and
press Ctrl+V to attach the file.

The clipboard step is the only path that lands an actual attachment in
Gmail: ``mailto:`` URLs cannot carry attachments per RFC 6068, so Gmail
ignores any attachment argument on the compose URL itself.  The same
flow works with other webmail clients (Outlook on the web, Fastmail)
that accept file paste into compose, and with desktop Chrome / Firefox
generally.

If more than one file is selected they are packed into a single zip
first, since most compose paste handlers only take one file.

Copying files
-------------

The Copy command (Ctrl+C) converts the selected files to PDF and puts
the result on the clipboard as a ``text/uri-list``, the same way the
Email menu does, but without opening a browser.  Paste it wherever a
file is wanted: a Gmail compose window (Ctrl+V attaches it), a file
manager, or a chat window.  As with email, more than one file is packed
into a single zip first.
