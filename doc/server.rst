Paperman Search Server
======================

A lightweight HTTP server for searching and listing files in a Paperman
paper repository.

Overview
--------

The Paperman Search Server provides a REST API to search for and list
document files (.max, .pdf, .jpg, .jpeg, .tiff) in a Paperman
repository. It’s designed to be used by external applications that need
to query the paper repository without direct filesystem access.

Building
--------

The server is built with Qt 5 or Qt 6, using qmake:

.. code:: bash

   qmake6 paperman-server.pro -o server.mk
   make -f server.mk

This will produce the ``paperman-server`` executable.

Running the Server
------------------

Basic Usage
~~~~~~~~~~~

.. code:: bash

   ./paperman-server <repository-path>

Example:

.. code:: bash

   ./paperman-server /home/user/Documents/papers

Options
~~~~~~~

-  ``-p, --port <port>`` - Port to listen on (default: 8080)
-  ``-C, --no-cache`` - Skip building the file cache at startup
-  ``-i, --index`` - Keep an index of the text on the pages, so that
   clients can search it; see `Searching Text`_
-  ``-r, --read-pages <n>`` - Read the pages of every stack in the
   background (OCR), ``n`` stacks at a time; see `Reading Pages`_
-  ``-h, --help`` - Show help message

Example with custom port:

.. code:: bash

   ./paperman-server -p 9000 /home/user/Documents/papers

Reading Pages
~~~~~~~~~~~~~

With ``--read-pages`` the server reads the pages of every stack in its
repositories with tesseract, as it has time, and puts the words into the
stacks, so that the desktop and the server can find a stack by the text
on its pages. Scans which come in, and stacks which change, are read
before the rest; otherwise the newest stacks are read first. The trash
and any hidden folder are left alone.

Each stack is written in one step: the server copies it, puts the words
into the copy and puts the copy in place of the stack, keeping its mode
and group. A stack which changes while it is being read is left as it
is and read again. Pages which already have words are not read again.

The server keeps a record of which stacks it has read, so that a
restart carries on where it left off and a stack with nothing to read
is not read again until it changes. The record is kept on the server's
own disk, not in the repository (which may be on a network filesystem),
in ``~/.local/share/paperman-server/read-<repo>-<hash>.db``, one per
repository. Removing it makes the server look at every stack again,
though without reading pages which already have words.

Tesseract runs at a low priority, one thread to a stack, so ``n`` can
be about the number of CPU cores which the server can spare. Reading
takes around a second or two a page per core, so a large repository
takes some hours at first; ``GET /v1/status`` shows how far it has got.
With systemd, add the option to the service:

.. code:: ini

   ExecStart=/opt/paperman/paperman-server -p 8081 -i -r 8 /srv/papers

Searching Text
~~~~~~~~~~~~~~

With ``--index`` the server keeps an index of the text on the pages of
every stack in its repositories, as the desktop does for a folder on
the computer it runs on, so that a client can find a stack in a remote
repository by what is written on it (see the API's *Text Search*). The
text is what OCR read from each page, whether the desktop or the server
read it, or a PDF's own text. The trash and hidden folders are left out.

The index is kept on the server's own disk, in
``~/.local/share/paperman-server/index-<repo>-<hash>.db``, one per
repository, and brought up to date on a thread of its own: the whole
repository when the server starts and every 15 minutes after that, and
a stack at once when it is changed through the server or the server has
read its pages. Building the index first means reading every stack, so
takes a while for a large repository, though it can be searched
meanwhile; after that, only stacks which have changed are read.
Removing the file makes the server build it afresh.

Together with ``--read-pages`` this lets clients search the text of
stacks which nobody has opened.

API Endpoints
-------------

All endpoints return JSON responses.

GET /status
~~~~~~~~~~~

Get server status and repository information.

**Response:**

.. code:: json

   {
     "status": "running",
     "repository": "/home/user/Documents/papers"
   }

GET /search
~~~~~~~~~~~

Search for files matching a pattern.

**Query Parameters:** - ``q`` (required) - Search pattern
(case-insensitive substring match) - ``path`` (optional) - Subdirectory
to search in (relative to repository root) - ``recursive`` (optional) -
Search subdirectories (default: true)

**Example:**

.. code:: bash

   curl "http://localhost:8080/search?q=invoice"
   curl "http://localhost:8080/search?q=2024&path=archive&recursive=true"

**Response:**

.. code:: json

   {
     "success": true,
     "count": 2,
     "results": [
       {
         "path": "invoice-2024.max",
         "name": "invoice-2024.max",
         "size": 293568,
         "modified": "2024-01-15T10:29:22"
       },
       {
         "path": "archive/invoice-2023.pdf",
         "name": "invoice-2023.pdf",
         "size": 150234,
         "modified": "2023-12-31T15:30:00"
       }
     ]
   }

GET /list
~~~~~~~~~

List all files in a directory.

**Query Parameters:** - ``path`` (optional) - Directory to list
(relative to repository root, default: root)

**Example:**

.. code:: bash

   curl "http://localhost:8080/list"
   curl "http://localhost:8080/list?path=2024/invoices"

**Response:**

.. code:: json

   {
     "success": true,
     "path": "",
     "count": 20,
     "files": [
       {
         "name": "document1.max",
         "path": "document1.max",
         "size": 293568,
         "modified": "2024-01-15T10:29:22"
       }
     ]
   }

Page Delivery
-------------

When an individual page is requested (``/file?path=...&page=N``),
the server converts it to a single-page PDF.  The compression
strategy depends on the page content:

- **Greyscale/colour pages** (8 or 24 bpp) use JPEG compression
  (DCTDecode) at quality 80.  This gives a 3--5x size reduction
  for greyscale and up to 13x for colour pages that are really
  greyscale with scanner noise.
- **Monochrome pages** (1 bpp) keep FlateDecode (zlib).  JPEG is
  unsuitable for hard black/white edges and FlateDecode already
  compresses 1-bit data very well (~11 KB per page).

Scanner-produced "colour" pages whose RGB channels differ by no more
than 10 levels are automatically detected as greyscale and converted
before JPEG encoding.

Supported File Types
--------------------

The server searches for and lists the following file types: - ``.max`` -
Paperman/Maxview native format - ``.pdf`` - PDF documents - ``.jpg``,
``.jpeg`` - JPEG images - ``.tiff``, ``.tif`` - TIFF images

CORS Support
------------

The server includes CORS headers (``Access-Control-Allow-Origin: *``) to
allow access from web applications.

Error Handling
--------------

Errors are returned with appropriate HTTP status codes and JSON error
messages:

.. code:: json

   {
     "success": false,
     "error": "Directory does not exist"
   }

Common HTTP status codes: - ``200 OK`` - Request successful -
``400 Bad Request`` - Missing or invalid parameters - ``404 Not Found``
- Endpoint not found - ``405 Method Not Allowed`` - The method is not
supported by the endpoint

Security Notes
--------------

1. The ``/v1/`` API can change the repository as well as read it, for
   the desktop program (see :doc:`api`)
2. All file paths are relative to the repository root to prevent
   directory traversal
3. Set up users (see `Authentication`_) before letting anything but
   the local machine reach the server
4. Run it behind a reverse proxy (nginx, Apache) with HTTPS for use
   from outside (see :doc:`deployment`)

Integration Examples
--------------------

Using curl
~~~~~~~~~~

.. code:: bash

   # Search for files containing "invoice"
   curl "http://localhost:8080/search?q=invoice"

   # List files in root directory
   curl "http://localhost:8080/list"

Using Python
~~~~~~~~~~~~

.. code:: python

   import requests

   # Search for files
   response = requests.get('http://localhost:8080/search', params={'q': 'invoice'})
   results = response.json()
   print(f"Found {results['count']} files")
   for file in results['results']:
       print(f"  {file['name']} - {file['size']} bytes")

Using JavaScript (browser or Node.js)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: javascript

   // Search for files
   fetch('http://localhost:8080/search?q=invoice')
     .then(response => response.json())
     .then(data => {
       console.log(`Found ${data.count} files`);
       data.results.forEach(file => {
         console.log(`  ${file.name} - ${file.size} bytes`);
       });
     });

Connecting Clients
------------------

Both the GUI (``paperman``) and the command-line client
(``paperman-client``) find servers through a shared configuration file:

.. code:: text

   ~/.config/paperman/client.conf

List one server per line, either as a bare URL or as a name and a URL
separated by ``=``. Blank lines and lines starting with ``#`` are
ignored:

.. code:: text

   # Servers this machine knows about
   http://localhost:8080
   Office = https://paper.office.example.com
   Home   = https://nas.local:8443

The name is a convenient label: it is shown in the GUI's directory tree
and can be given to ``--server`` in place of the full URL. When no name
is supplied the server's host is used.

Set ``PAPERMAN_CONFIG`` to use a different file, which is handy for
testing against a throwaway configuration.

Adding a server
~~~~~~~~~~~~~~~

Edit the file by hand, or let the client append to it:

.. code:: bash

   paperman-client add-server http://localhost:8080
   paperman-client add-server https://paper.office.example.com Office

A server that is already listed is not added twice; URLs that differ
only by a trailing slash or by the case of the host are recognised as
the same server.

Listing servers
~~~~~~~~~~~~~~~

.. code:: bash

   paperman-client servers

The entry the next command would talk to is marked with ``*``.

Choosing a server
~~~~~~~~~~~~~~~~~

``--server`` accepts either a URL or the name of a configured server:

.. code:: bash

   paperman-client --server Office ls repo
   paperman --server Office

Without ``--server``, ``paperman-client`` uses ``$PAPERMAN_SERVER`` if
it is set, otherwise the first server in ``client.conf``, otherwise
``http://localhost:8080``.

The GUI attaches *every* server listed in ``client.conf`` at startup, so
remote repositories appear in the directory tree alongside local ones. A
server that cannot be reached is reported once and the rest of the
program carries on, so an unavailable machine does not stop Paperman
from starting.

Authentication
~~~~~~~~~~~~~~

``client.conf`` holds no credentials. Log in once per server and the
bearer token is cached, keyed by the server's own id:

.. code:: bash

   paperman-client --server Office login simon

The token is written to ``~/.config/paperman/<serverId>.token`` with
0600 permissions and is picked up by both the CLI and the GUI, so
logging in from either one is enough. The GUI prompts for credentials
itself when it attaches to a server that needs them.

Fetching pages
~~~~~~~~~~~~~~

When the server serves stacks a page at a time (it lists ``pages``
among its features in ``/v1/status``), the GUI opens a remote stack by
fetching only its structure, which is enough to lay the pages out, and
then fetches each page the first time it is shown. Against an older
server the whole file is fetched, as before.

The pages are kept under the cache, beside where the whole file would
go:

.. code:: text

   ~/.cache/paperman/<serverId>/<repo>/<stack>.max.d/
       info.json      the stack's structure, from /info
       page-7.max     each page fetched so far, a one-page stack

A page is fetched again after it is changed on the server. The cache
grows by a page for every page viewed and is not yet trimmed
automatically; it is safe to delete any of it at any time, since
anything missing is simply fetched again.

Troubleshooting
---------------

**Server won’t start:** - Check if the port is already in use:
``netstat -ln | grep 8080`` - Verify the repository path exists and is
accessible - Check file permissions

**No results returned:** - Verify files exist in the repository - Check
file extensions match supported types - Try a broader search pattern

**Connection refused:** - Ensure the server is running - Check firewall
settings - Verify you’re connecting to the correct host and port

License
-------

GPL-2 (same as Paperman)

Copyright (C) 2009 Simon Glass
