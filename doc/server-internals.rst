Server Internals
================

How ``paperman-server`` works, for anyone changing it or writing a client
for it. See :doc:`server` for running it, :doc:`api` for its routes and
:doc:`client-server` for how the desktop uses it.

Reading Pages
-------------

With ``--read-pages``, each repository has a ``RepoReader``, which reads
stacks on worker threads, ``n`` at a time, running tesseract at a low
priority, one thread to a stack. Stacks which change or arrive through
the server, and those a client asks for (``…/stacks/{path}/read``), go
to the front of the queue; otherwise the newest are read first. The
trash and any hidden folder are left alone.

Each stack is written in one step: the server copies it, puts the words
into the copy and puts the copy in place of the stack, keeping its mode
and group. A stack which changes while it is being read is left as it
is and read again. Pages which already have words are not read again.

The reader keeps a record of which stacks it has read, so that a
restart carries on where it left off and a stack with nothing to read
is not read again until it changes. The record is kept on the server's
own disk, not in the repository (which may be on a network filesystem,
where SQLite cannot rely on locking), in
``~/.local/share/paperman-server/read-<repo>-<hash>.db``, one per
repository. Removing it makes the server look at every stack again,
though without reading pages which already have words.
``GET /v1/status`` reports its progress under ``reading``.

The Text Index
--------------

With ``--index``, each repository has a ``RepoIndex``, which keeps the
text of every stack's pages in a SQLite FTS5 index, as the desktop's
``.paperindex`` does for a local repository (``SearchIndex``). The text
is what OCR read from each page, whether the desktop or the server read
it, or a PDF's own text. The trash and hidden folders are left out.

The index is kept on the server's own disk, in
``~/.local/share/paperman-server/index-<repo>-<hash>.db``, opened with a
write-ahead log so that searches do not wait for the writer, and is
brought up to date on a thread of its own: the whole repository when the
server starts and every 15 minutes after that, committing every 200
stacks so that a first build can be searched meanwhile, and a stack at
once when it is changed through the server or the reader has read it.
After the first build only stacks which have changed are read. Removing
the file makes the server build it afresh. ``GET /v1/status`` reports
its progress under ``indexing``.

Fetching Pages
--------------

When the server serves stacks a page at a time (it lists ``pages`` among
its features in ``/v1/status``), the desktop opens a remote stack by
fetching only its structure, which is enough to lay the pages out, and
then fetches each page the first time it is shown. Against an older
server the whole file is fetched.

The pages are kept under the desktop's cache, beside where the whole
file would go:

.. code:: text

   ~/.cache/paperman/<serverId>/<repo>/<stack>.max.d/
       info.json      the stack's structure, from /info
       page-7.max     each page fetched so far, a one-page stack

A page is fetched again after it is changed on the server. The cache
grows by a page for every page viewed and is not yet trimmed
automatically; it is safe to delete any of it at any time, since
anything missing is simply fetched again.

Logins
------

A server with users only answers a client which has logged in with
``POST /v1/auth/login``, which gives a bearer token lasting 30 days, or
which gives an API key. The desktop and ``paperman-client`` keep the
token in ``~/.config/paperman/<serverId>.token``, with 0600
permissions, keyed by the server's own id, so ``client.conf`` holds no
credentials.

Page Delivery
-------------

When an individual page is requested (``/file?path=...&page=N``), the
server converts it to a single-page PDF. The compression depends on the
page:

- **Greyscale and colour pages** (8 or 24 bpp) use JPEG compression
  (DCTDecode) at quality 80. This gives a 3--5x size reduction for
  greyscale and up to 13x for colour pages which are really greyscale
  with scanner noise.
- **Monochrome pages** (1 bpp) keep FlateDecode (zlib). JPEG is
  unsuitable for hard black and white edges, and FlateDecode already
  compresses 1-bit data very well (about 11 KB a page).

Scanned "colour" pages whose RGB channels differ by no more than 10
levels are taken as greyscale and converted before JPEG encoding.

The PDFs the server makes have the words read from each page as an
invisible layer of text, so they can be searched.

Supported File Types
--------------------

The server searches for and lists these file types:

- ``.max`` - Paperman/Maxview native format
- ``.pdf`` - PDF documents
- ``.jpg``, ``.jpeg`` - JPEG images
- ``.tiff``, ``.tif`` - TIFF images

CORS and Errors
---------------

The server includes CORS headers (``Access-Control-Allow-Origin: *``) to
allow access from web applications.

Errors are returned with an HTTP status code and a JSON message:

.. code:: json

   {
     "success": false,
     "error": "Directory does not exist"
   }

The common codes are ``400 Bad Request`` (missing or invalid
parameters), ``404 Not Found`` (no such route or file) and ``405 Method
Not Allowed``.

All file paths are relative to the repository root, so a client cannot
reach outside it.

The Original Routes
-------------------

Before the ``/v1/`` API (see :doc:`api`), the server had a few read-only
routes, which it still answers.

``GET /status`` gives the server's status and repository:

.. code:: json

   {
     "status": "running",
     "repository": "/home/user/Documents/papers"
   }

``GET /search`` finds files whose names contain a pattern. Its query
parameters are ``q`` (required: the pattern, matched without regard to
case), ``path`` (a folder to search in, relative to the repository) and
``recursive`` (whether to search subfolders; the default is true):

.. code:: bash

   curl "http://localhost:8080/search?q=invoice"
   curl "http://localhost:8080/search?q=2024&path=archive&recursive=true"

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

``GET /list`` lists the files in a folder, given by ``path`` (relative
to the repository; the default is its top):

.. code:: bash

   curl "http://localhost:8080/list"
   curl "http://localhost:8080/list?path=2024/invoices"

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

From Python:

.. code:: python

   import requests

   response = requests.get('http://localhost:8080/search',
                           params={'q': 'invoice'})
   results = response.json()
   print(f"Found {results['count']} files")
   for file in results['results']:
       print(f"  {file['name']} - {file['size']} bytes")

From JavaScript (in a browser or Node.js):

.. code:: javascript

   fetch('http://localhost:8080/search?q=invoice')
     .then(response => response.json())
     .then(data => {
       console.log(`Found ${data.count} files`);
       data.results.forEach(file => {
         console.log(`  ${file.name} - ${file.size} bytes`);
       });
     });
