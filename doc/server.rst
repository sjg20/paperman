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

The server requires Qt 5 (or Qt 4 with reduced functionality) and is
built using qmake:

.. code:: bash

   qmake paperman-server.pro
   make

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
-  ``-h, --help`` - Show help message

Example with custom port:

.. code:: bash

   ./paperman-server -p 9000 /home/user/Documents/papers

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
- Endpoint not found - ``405 Method Not Allowed`` - Only GET requests
are supported

Security Notes
--------------

1. The server only provides read-only access to the repository
2. All file paths are relative to the repository root to prevent
   directory traversal
3. No authentication is currently implemented - use firewall rules or
   reverse proxy for access control
4. Consider running behind a reverse proxy (nginx, Apache) for
   production use

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
