Paperman in a Web Browser
=========================

Paperman's desktop can be built to run in a web browser, as WebAssembly.
The page is the same program as the desktop, with the same windows and
menus, working on the papers of a ``paperman-server``.

This is at an early stage: see `What works`_.

How it fits together
--------------------

A page in a browser cannot open a folder of papers, run another program
or reach a scanner. So the browser build is always a client of a
server:

- ``paperman-server`` holds the papers, reads their text and searches
  them
- the same server hands the page to the browser, with ``--web`` (see
  :doc:`server`), so that the page and the server are one site
- when the page opens, it shows the papers of the server it came from,
  with nothing to set up

Building it
-----------

The build needs:

- the **Emscripten SDK**, at the version which the Qt in use asks for
  (Qt 6.10 pairs with Emscripten 4.0.7), set up in the shell with its
  ``emsdk_env.sh``
- **Qt for WebAssembly**, the multi-threaded kind, with the host tools
  of the same version. ``aqtinstall`` fetches both::

     aqt install-qt linux desktop 6.10.3 linux_gcc_64 -m qtscxml
     aqt install-qt all_os wasm 6.10.3 wasm_multithread -m qtscxml

- **libtiff**'s source, which the script builds for the browser. Pages
  in ``.max`` files are kept with its fax coding. JPEG and zlib come
  with Emscripten

Then::

   scripts/build_wasm.sh -q <qt>/6.10.3/wasm_multithread \
       -t <tiff-source> <outdir>

which leaves the files to serve in ``<outdir>/web``. Serve them with::

   paperman-server --web <outdir>/web <repository>

and open ``http://localhost:<port>/app/`` on the same machine.

From any other machine the page has to come over HTTPS. A browser lets
a page use threads only where it counts the page as secure, which is
HTTPS or ``localhost``: over plain ``http`` from another machine the page
stays at "Loading...". ``paperman-server`` does not do HTTPS itself, so
put a web server in front of it which does: see :doc:`deployment`.

The program is about 38 MB, and 11 MB as the browser fetches it, since
the script makes compressed copies which the server sends. The browser
keeps it after the first visit.

What is different in the build
------------------------------

``paperman.pro`` has a section for this target, which Qt calls ``wasm``.
The rest of the code is as it is for the other builds, with stand-ins
for what a browser lacks:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - A browser has no
     - So the build
   * - Scanner library
     - Builds the scanner code as it is against the SANE headers kept for
       Windows, with ``wasm/nosane.cpp``: a SANE library which has no
       scanners
   * - Way to run a program
     - Uses ``wasm/noprocess.h`` in place of ``QProcess``: a program
       which can never be started, so that code which runs one takes the
       path it has for a program which is not installed. This covers
       text reading, mail, the assistant's command and ``exiftool``
   * - PDF libraries
     - Leaves PDF stacks out (``CONFIG_no_pdf``): they are listed, but
       cannot be opened
   * - Folders of papers
     - Shows the server's papers when it starts, and hides **Add
       repository**
   * - Waiting dialogs
     - Is linked with Asyncify, which lets a dialog wait for its answer
       as it does elsewhere, at some cost in size and speed

What works
----------

Tried in Chrome, against a server on the same computer:

- the window, menus, toolbars and dialogs
- showing the server's folders, stacks, thumbnails and ``.max`` pages
- renaming a stack

Not yet tried: most other changes to stacks, searching, logging in to a
server with users, and other browsers.

Known not to work:

- **PDF stacks** cannot be opened. The server would need to draw their
  pages, as it does for ``.max`` stacks, or the PDF libraries be built
  for the browser
- **Reading text** from the page: the server has to do it
  (``--read-pages``, or the **OCR** button, which asks the server)
- **Scanning**. A scanner could be reached either through the server it
  is plugged into, or through the browser's own access to USB devices,
  which only some browsers have
- **Printing**, which Qt does not support in a browser, and **email**
- **The assistant**, which runs a command on the computer

Nothing here builds the browser version in the project's automated
checks yet, so a change elsewhere can break it unnoticed.
