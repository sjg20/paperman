Paperman Search Server API Documentation
========================================

Overview
--------

The Paperman Search Server provides a REST API for searching, listing,
and retrieving files from paper repositories. It supports on-the-fly PDF
conversion from various formats.

**Base URL**: ``http://localhost:8080``

**Version**: 1.0

All endpoints use the ``GET`` HTTP method and return JSON responses
(except file downloads which return the file content).

Authentication
--------------

The server supports **optional API key authentication** via the
``X-API-Key`` header.

Enabling Authentication
~~~~~~~~~~~~~~~~~~~~~~~

Set the ``PAPERMAN_API_KEY`` environment variable when starting the
server:

.. code:: bash

   export PAPERMAN_API_KEY="your-secret-key-here"
   ./paperman-server /path/to/repository

Or with systemd:

.. code:: bash

   # Edit /etc/systemd/system/paperman-server.service
   [Service]
   Environment="PAPERMAN_API_KEY=your-secret-key-here"

Using Authentication
~~~~~~~~~~~~~~~~~~~~

Once enabled, all endpoints (except ``/status``) require the API key:

.. code:: bash

   # Without API key - fails
   curl http://localhost:8080/search?q=test
   # Response: {"error":"Invalid or missing API key...","success":false}

   # With API key - works
   curl -H "X-API-Key: your-secret-key-here" http://localhost:8080/search?q=test

Authentication Behavior
~~~~~~~~~~~~~~~~~~~~~~~

-  **Disabled by default**: If ``PAPERMAN_API_KEY`` is not set, no
   authentication is required
-  **Status endpoint exempt**: ``/status`` always works without
   authentication (for health checks)
-  **All other endpoints protected**: When enabled, ``/search``,
   ``/list``, ``/file``, ``/repos`` require valid API key
-  **401 Unauthorized**: Invalid or missing API key returns HTTP 401
   with JSON error

**Security Note**: Always use HTTPS (SSL/TLS) when accessing the server
over a network to prevent API key interception.

Per-User Accounts (v1)
~~~~~~~~~~~~~~~~~~~~~~

The server also supports per-user accounts managed via the
``paperman-server`` CLI.  Once any user is registered, authentication is
enforced for non-status endpoints even if ``PAPERMAN_API_KEY`` is unset.

Manage users:

.. code:: bash

   paperman-server useradd alice            # prompts for password
   paperman-server passwd alice             # change password
   paperman-server userdel alice
   paperman-server usermod alice --repos work,personal   # 'all' for unrestricted
   paperman-server userlist

Accounts live in ``~/.config/paperman-server/users.json`` (0600).  The
CLI subcommands above are the supported way to edit it, but the format
is plain JSON so it can also be read or restored from a backup:

.. code:: json

   {
     "alice": {
       "hash": "pbkdf2-sha256$200000$<salt-b64>$<hash-b64>",
       "repos": [],
       "admin": false
     },
     "bob": {
       "hash": "pbkdf2-sha256$200000$<salt-b64>$<hash-b64>",
       "repos": ["work", "personal"],
       "admin": false
     }
   }

Top-level keys are user names.  Each record has:

-  ``hash`` — ``pbkdf2-sha256$<iters>$<salt-b64>$<hash-b64>``.  The
   salt is 16 random bytes; the derived key is 32 bytes; both are
   standard base64.  ``<iters>`` is the PBKDF2 iteration count used
   when the password was set, so old hashes keep verifying after the
   default is bumped.
-  ``repos`` — list of repository names this user may access.  Match
   is exact against the repository directory's basename.  An empty
   list means "all repositories".
-  ``admin`` — reserved for future use; currently ignored.

The server reads the file once at startup, so out-of-band edits require
a restart to take effect.

To obtain a bearer token, POST to ``/v1/auth/login``:

.. code:: bash

   curl -X POST -H "Content-Type: application/json" \
        -d '{"user":"alice","password":"s3cret"}' \
        http://localhost:8080/v1/auth/login
   # {"token":"...","user":"alice","expiry":"2026-07-01T..."}

Then pass the token in subsequent requests:

.. code:: bash

   curl -H "Authorization: Bearer <token>" http://localhost:8080/repos

Tokens are held in memory and invalidated on server restart (default
TTL: 30 days).  ``X-API-Key`` continues to work in parallel and bypasses
per-user repo gating.

Common Response Format
----------------------

Success Response
~~~~~~~~~~~~~~~~

.. code:: json

   {
     "success": true,
     "data": "...",
     "count": 0
   }

Error Response
~~~~~~~~~~~~~~

.. code:: json

   {
     "success": false,
     "error": "Error message description"
   }

Endpoints
---------

1. Server Status
~~~~~~~~~~~~~~~~

Get the current server status and repository information.

**Endpoint**: ``GET /status``

**Parameters**: None

**Response**:

.. code:: json

   {
     "status": "running",
     "repository": "/path/to/repository"
   }

**Example**:

.. code:: bash

   curl http://localhost:8080/status

--------------

1a. Server Status (v1)
~~~~~~~~~~~~~~~~~~~~~~

Versioned status endpoint for the new ``/v1/`` API.  Returns the API
version, a stable per-server UUID (``serverId``) that clients should use
to key their local caches, and a list of optional features the server
supports.  Auth-free so clients can probe before they have a token.

**Endpoint**: ``GET /v1/status``

**Parameters**: None

**Response**:

.. code:: json

   {
     "status": "running",
     "apiVersion": "1",
     "serverId": "f6a8e7c4-2c4b-4d5e-9aab-23a1b9d0e7f0",
     "version": "<server build version>",
     "features": []
   }

**Example**:

.. code:: bash

   curl http://localhost:8080/v1/status

--------------

1b. Login (v1)
~~~~~~~~~~~~~~

Exchange a username and password for a bearer token.  See the
*Per-User Accounts (v1)* section above for how to manage accounts.

**Endpoint**: ``POST /v1/auth/login``

**Body** (JSON):

.. code:: json

   {"user": "alice", "password": "s3cret"}

**Response** (200):

.. code:: json

   {
     "token": "<opaque bearer token>",
     "user": "alice",
     "expiry": "2026-07-01T12:34:56"
   }

**Errors**: 400 (missing fields), 401 (bad credentials).  Tokens are
held in memory and lost on server restart.

--------------

2. List Repositories
~~~~~~~~~~~~~~~~~~~~

Get a list of all configured repositories.

**Endpoint**: ``GET /repos``

**Parameters**: None

**Response**:

.. code:: json

   {
     "success": true,
     "count": 2,
     "repositories": [
       {
         "path": "/home/user/papers",
         "name": "papers",
         "exists": true
       },
       {
         "path": "/home/user/archive",
         "name": "archive",
         "exists": true
       }
     ]
   }

**Example**:

.. code:: bash

   curl http://localhost:8080/repos

--------------

3. Search Files
~~~~~~~~~~~~~~~

Search for files matching a pattern in the repository.

**Endpoint**: ``GET /search``

**Parameters**:

+---------+------+-------+------+-------------------------------------+
| Pa      | Type | Req   | Def  | Description                         |
| rameter |      | uired | ault |                                     |
+=========+======+=======+======+=====================================+
| ``q``   | st   | Yes   | -    | Search pattern (partial filename    |
|         | ring |       |      | match)                              |
+---------+------+-------+------+-------------------------------------+
| `       | st   | No    | F    | Repository name to search in        |
| `repo`` | ring |       | irst |                                     |
+---------+------+-------+------+-------------------------------------+
| `       | st   | No    | Root | Directory path to search in         |
| `path`` | ring |       |      | (relative to root)                  |
+---------+------+-------+------+-------------------------------------+
| ``recu  | boo  | No    | f    | Search subdirectories               |
| rsive`` | lean |       | alse |                                     |
+---------+------+-------+------+-------------------------------------+

**Response**:

.. code:: json

   {
     "success": true,
     "pattern": "invoice",
     "path": "/home/user/papers",
     "count": 3,
     "files": [
       {
         "name": "invoice-2023-01.pdf",
         "path": "invoices/invoice-2023-01.pdf",
         "size": 45632,
         "modified": "2023-01-15T10:30:00"
       },
       {
         "name": "invoice-2023-02.pdf",
         "path": "invoices/invoice-2023-02.pdf",
         "size": 52441,
         "modified": "2023-02-12T14:22:00"
       }
     ]
   }

**Examples**:

.. code:: bash

   # Basic search
   curl "http://localhost:8080/search?q=invoice"

   # Search in specific repository
   curl "http://localhost:8080/search?q=invoice&repo=papers"

   # Search in subdirectory
   curl "http://localhost:8080/search?q=report&path=2023"

   # Recursive search
   curl "http://localhost:8080/search?q=contract&recursive=true"

**Notes**: - Pattern matching is case-insensitive - Searches for partial
filename matches - Only returns files with supported extensions (.max,
.pdf, .jpg, .tiff)

--------------

4. List Directory Contents
~~~~~~~~~~~~~~~~~~~~~~~~~~

List all files in a specific directory.

**Endpoint**: ``GET /list``

**Parameters**:

+--------+------+--------+-------+------------------------------------+
| Par    | Type | Re     | De    | Description                        |
| ameter |      | quired | fault |                                    |
+========+======+========+=======+====================================+
| ``     | st   | No     | Root  | Directory path (relative to        |
| path`` | ring |        |       | repository)                        |
+--------+------+--------+-------+------------------------------------+
| ``     | st   | No     | First | Repository name                    |
| repo`` | ring |        |       |                                    |
+--------+------+--------+-------+------------------------------------+

**Response**:

.. code:: json

   {
     "success": true,
     "path": "invoices",
     "count": 5,
     "files": [
       {
         "name": "invoice-2023-01.pdf",
         "path": "invoices/invoice-2023-01.pdf",
         "size": 45632,
         "modified": "2023-01-15T10:30:00"
       },
       {
         "name": "invoice-2023-02.pdf",
         "path": "invoices/invoice-2023-02.pdf",
         "size": 52441,
         "modified": "2023-02-12T14:22:00"
       }
     ]
   }

**Examples**:

.. code:: bash

   # List root directory
   curl "http://localhost:8080/list"

   # List subdirectory
   curl "http://localhost:8080/list?path=invoices"

   # List in specific repository
   curl "http://localhost:8080/list?path=2023&repo=archive"

--------------

5. Get File Content
~~~~~~~~~~~~~~~~~~~

Retrieve a file’s content, optionally converting it to PDF.

**Endpoint**: ``GET /file``

**Parameters**:

+-----------+------+-------+-----------+------------------------------------+
| Parameter | Type | Req   | Default   | Description                        |
|           |      | uired |           |                                    |
+===========+======+=======+===========+====================================+
| ``path``  | str  | Yes   | -         | File path (relative to repository) |
|           | ing  |       |           |                                    |
+-----------+------+-------+-----------+------------------------------------+
| ``repo``  | str  | No    | First     | Repository name                    |
|           | ing  |       |           |                                    |
+-----------+------+-------+-----------+------------------------------------+
| ``type``  | str  | No    | ``orig    | Output type: ``original`` or       |
|           | ing  |       | inal``    | ``pdf``                            |
+-----------+------+-------+-----------+------------------------------------+
| ``page``  | int  | No    | 0         | Extract a single page from a PDF   |
|           |      |       |           | (1-based). Returns a standalone    |
|           |      |       |           | single-page PDF.                   |
+-----------+------+-------+-----------+------------------------------------+
| ``pages`` | str  | No    | -         | Set to ``true`` to return the page |
|           | ing  |       |           | count as JSON instead of file      |
|           |      |       |           | content. PDF files only.           |
+-----------+------+-------+-----------+------------------------------------+

**Response**: - **Success**: Binary file content with appropriate
``Content-Type`` header - **Error**: JSON error response

When ``pages=true`` is given, the response is JSON:

.. code:: json

   {
     "success": true,
     "pages": 5
   }

When ``page=N`` is given, the response is a single-page PDF
(``application/pdf``). Extracted pages are cached in
``/tmp/paperman-pages/`` with the same 7-day expiry as thumbnails.

**Content-Type Headers**: - ``.pdf`` → ``application/pdf`` - ``.jpg``,
``.jpeg`` → ``image/jpeg`` - ``.tif``, ``.tiff`` → ``image/tiff`` -
``.max`` → ``application/octet-stream`` - PDF conversion →
``application/pdf``

**Examples**:

.. code:: bash

   # Download original file
   curl "http://localhost:8080/file?path=invoice.pdf" -o invoice.pdf

   # Download from specific repository
   curl "http://localhost:8080/file?path=document.pdf&repo=archive" -o document.pdf

   # Convert JPEG to PDF on-the-fly
   curl "http://localhost:8080/file?path=scan.jpg&type=pdf" -o scan.pdf

   # Convert .max file to PDF
   curl "http://localhost:8080/file?path=document.max&type=pdf" -o document.pdf

   # Get page count for a PDF
   curl "http://localhost:8080/file?path=document.pdf&pages=true"

   # Download just page 1 (for fast initial display)
   curl "http://localhost:8080/file?path=document.pdf&page=1" -o page1.pdf

**PDF Conversion**: - Supports: ``.max``, ``.jpg``, ``.jpeg``, ``.tif``,
``.tiff`` - Conversion timeout: 30 seconds - Uses paperman’s built-in
conversion engine - Maintains image quality and metadata

**Error Responses**:

.. code:: json

   // File not found
   {
     "success": false,
     "error": "File not found"
   }

   // Invalid path (directory traversal attempt)
   {
     "success": false,
     "error": "Invalid file path"
   }

   // Conversion failed
   {
     "success": false,
     "error": "PDF conversion failed: <error details>"
   }

   // Conversion timeout
   {
     "success": false,
     "error": "PDF conversion timed out (30s limit)"
   }

--------------

Supported File Types
--------------------

The server handles the following file types:

+----------------+-------------------------+---------------+------------+
| Extension      | Description             | PDF           | Direct     |
|                |                         | Conversion    | View       |
+================+=========================+===============+============+
| ``.max``       | Paperman format         | ✅            | ❌         |
+----------------+-------------------------+---------------+------------+
| ``.pdf``       | PDF document            | N/A           | ✅         |
+----------------+-------------------------+---------------+------------+
| ``.jpg``,      | JPEG image              | ✅            | ✅         |
| ``.jpeg``      |                         |               |            |
+----------------+-------------------------+---------------+------------+
| ``.tif``,      | TIFF image              | ✅            | ✅         |
| ``.tiff``      |                         |               |            |
+----------------+-------------------------+---------------+------------+

--------------

Error Codes
-----------

+--------+------------------------+-----------------------------------+
| HTTP   | Description            | Common Causes                     |
| Code   |                        |                                   |
+========+========================+===================================+
| 200    | OK                     | Request successful                |
+--------+------------------------+-----------------------------------+
| 400    | Bad Request            | Invalid path, missing parameters  |
+--------+------------------------+-----------------------------------+
| 401    | Unauthorized           | Invalid or missing API key        |
+--------+------------------------+-----------------------------------+
| 404    | Not Found              | File/endpoint not found           |
+--------+------------------------+-----------------------------------+
| 405    | Method Not Allowed     | Non-GET request                   |
+--------+------------------------+-----------------------------------+
| 500    | Internal Server Error  | Conversion failed, file read      |
|        |                        | error                             |
+--------+------------------------+-----------------------------------+
| 501    | Not Implemented        | Unsupported conversion            |
|        |                        | (deprecated)                      |
+--------+------------------------+-----------------------------------+

--------------

CORS
----

All endpoints include CORS headers:

::

   Access-Control-Allow-Origin: *

This allows web applications from any origin to access the API.

--------------

Rate Limiting
-------------

Currently, no rate limiting is implemented. The server is designed for
trusted local or network use.

--------------

Examples
--------

JavaScript/Fetch API
~~~~~~~~~~~~~~~~~~~~

.. code:: javascript

   const API_KEY = 'your-secret-key-here';  // Set if authentication is enabled

   // Search for files
   fetch('http://localhost:8080/search?q=invoice', {
     headers: {
       'X-API-Key': API_KEY  // Include if auth enabled
     }
   })
     .then(response => response.json())
     .then(data => {
       console.log(`Found ${data.count} files`);
       data.files.forEach(file => {
         console.log(`- ${file.name} (${file.size} bytes)`);
       });
     });

   // Download a file
   fetch('http://localhost:8080/file?path=document.pdf', {
     headers: {
       'X-API-Key': API_KEY
     }
   })
     .then(response => response.blob())
     .then(blob => {
       const url = URL.createObjectURL(blob);
       const a = document.createElement('a');
       a.href = url;
       a.download = 'document.pdf';
       a.click();
     });

   // Convert to PDF
   fetch('http://localhost:8080/file?path=scan.jpg&type=pdf', {
     headers: {
       'X-API-Key': API_KEY
     }
   })
     .then(response => response.blob())
     .then(blob => {
       const url = URL.createObjectURL(blob);
       window.open(url, '_blank');
     });

Python
~~~~~~

.. code:: python

   import requests

   API_KEY = 'your-secret-key-here'  # Set if authentication is enabled
   headers = {'X-API-Key': API_KEY}  # Include if auth enabled

   # Search for files
   response = requests.get('http://localhost:8080/search',
                          params={'q': 'invoice'},
                          headers=headers)
   data = response.json()
   print(f"Found {data['count']} files")

   # Download a file
   response = requests.get('http://localhost:8080/file',
                          params={'path': 'document.pdf'},
                          headers=headers)
   with open('document.pdf', 'wb') as f:
       f.write(response.content)

   # Convert to PDF
   response = requests.get('http://localhost:8080/file',
                          params={'path': 'scan.jpg', 'type': 'pdf'},
                          headers=headers)
   with open('scan.pdf', 'wb') as f:
       f.write(response.content)

cURL
~~~~

.. code:: bash

   # Set API key if authentication is enabled
   API_KEY="your-secret-key-here"

   # Get server status (no auth required)
   curl http://localhost:8080/status

   # Search files (with auth)
   curl -H "X-API-Key: $API_KEY" "http://localhost:8080/search?q=invoice" | jq

   # List directory (with auth)
   curl -H "X-API-Key: $API_KEY" "http://localhost:8080/list?path=2023" | jq

   # Download file (with auth)
   curl -H "X-API-Key: $API_KEY" "http://localhost:8080/file?path=document.pdf" -o document.pdf

   # Convert to PDF (with auth)
   curl -H "X-API-Key: $API_KEY" "http://localhost:8080/file?path=scan.jpg&type=pdf" -o scan.pdf

   # Pretty print JSON response (with auth)
   curl -s -H "X-API-Key: $API_KEY" http://localhost:8080/repos | jq .

--------------

6. Mutation API (v1)
~~~~~~~~~~~~~~~~~~~~

The ``/v1/`` namespace adds write operations, used by the desktop
client when a repository is remote.  All are ``POST`` requests with a
JSON body (unless noted) to paths of the form::

   /v1/repos/{repo}/stacks/{path}{verb}

where ``{path}`` is the stack's percent-encoded repo-relative path.
Authentication follows the same rules as the read API; the per-user
repository allowlist is enforced.  Every response carries
``{"success": true|false}`` plus the fields shown.

``/rename``
   Body ``{"newName": ..., "autoRename": true}``.  Renames the stack
   within its directory; with ``autoRename`` a colliding name gets
   ``_1``, ``_2``... appended.  Returns ``{"name"}`` with the final
   name.

``/pages/{n}/rename``
   Body ``{"newName": ...}``.  Renames 1-based page ``n``.

``/move``
   Body ``{"destDir": ..., "copy": false}``.  Moves (or copies) the
   stack to another directory in the same repository; ``destDir`` is
   repo-relative and ``""`` means the root.  The shared trash
   directory ``.maxview-trash`` is created on demand; any other
   destination must exist.  Returns ``{"name"}``.

``/delete``
   Removes the file outright (no trash).

``/annotations``
   Body with any subset of ``author``/``title``/``keywords``/
   ``notes``/``ocr``.  Writes the fields into the stack's annotation
   block.

``/pages/delete``
   Body ``{"pages": [n, ...]}`` (1-based).  Removes the pages and
   keeps the recovery data server-side.  Returns ``{"undoId"}``, an
   opaque token for ``/pages/undelete``.  Tokens do not survive a
   server restart, so treat the undo as best-effort.

``/pages/undelete``
   Body ``{"undoId": ...}``.  Restores the pages removed by the
   matching ``/pages/delete``.  An unknown or already-redeemed token
   is ``410 Gone``.

``/unstack``
   Body ``{"pagenum": n, "pagecount": k, "remove": true,
   "newName": ...}``.  Splits ``k`` pages starting at 1-based ``n``
   into a new stack in the same directory, removing them from the
   source when ``remove`` is set.  Returns ``{"name"}``.

``/stack``
   Body ``{"sources": [path, ...], "insertPage": n}``.  Appends the
   pages of each source stack into the target at 1-based
   ``insertPage`` (absent means the end) and deletes the source
   files.  All stacks must be the same type.

``/duplicate``
   Copies the stack's file within its directory under a fresh name.
   Returns ``{"name"}``.

``/upload``
   The raw request body is the whole file's bytes (e.g. a stack
   scanned on a client).  Never overwrites: a name clash gets a fresh
   name.  Returns ``{"name", "etag"}``.

Whole-file downloads via ``/file`` carry an ``ETag`` header (size and
mtime).  A request with ``If-None-Match`` returns ``304 Not Modified``
when the tag still matches, so clients can revalidate cached copies
cheaply.

7. Change Events (v1)
~~~~~~~~~~~~~~~~~~~~~

**Endpoint**: ``GET /v1/repos/{repo}/events``

A long-lived ``text/event-stream`` response.  Every successful
mutation in the repository sends a frame::

   event: stackChanged
   data: {"repo": ..., "op": ..., "path": ..., "name": ..., "origin": ...}

``op`` is the operation (``rename``, ``move``, ``delete``,
``transform``, ``annotations``, ``pagesDeleted``, ``pagesRestored``,
``unstack``, ``stack``, ``duplicate``, ``upload``), ``name`` the
resulting name where one exists, and ``origin`` echoes the
``X-Client-Id`` header of the originating request so a client can
ignore its own changes.  Behind nginx the events path needs
``proxy_buffering off`` and a long ``proxy_read_timeout``.

--------------

Security Considerations
-----------------------

Path Traversal Prevention
~~~~~~~~~~~~~~~~~~~~~~~~~

The server prevents directory traversal attacks: - Paths containing
``..`` are rejected - Absolute paths starting with ``/`` are rejected -
All paths are resolved relative to the repository root

Network Security
~~~~~~~~~~~~~~~~

For production use, consider: 1. **Firewall**: Restrict access to
trusted IPs 2. **Reverse Proxy**: Use nginx/apache with SSL/TLS 3.
**Authentication**: Add authentication layer via reverse proxy 4.
**Private Network**: Run on private network only

File Access
~~~~~~~~~~~

-  Server runs with limited user permissions
-  Only configured repository paths are accessible
-  Write operations exist only under ``/v1/`` and require
   authentication when it is enabled

--------------

Performance
-----------

Response Times
~~~~~~~~~~~~~~

Typical response times on local network:

=========== ============= ===============================
Endpoint    Response Time Notes
=========== ============= ===============================
``/status`` < 1ms         Cached information
``/repos``  < 5ms         Directory metadata
``/search`` 10-100ms      Depends on directory size
``/list``   5-50ms        Depends on directory size
``/file``   10-500ms      Depends on file size
PDF convert 1-30s         Depends on file size/complexity
=========== ============= ===============================

Caching
~~~~~~~

Three disk caches are maintained under ``/tmp/``, all keyed by an MD5
hash of the file path and modification time. Entries expire after 7
days and are cleaned on server start.

``/tmp/paperman-thumbnails/``
   JPEG thumbnails generated by ``pdftocairo``.

``/tmp/paperman-pages/``
   Single-page PDFs extracted from multi-page documents via
   ``page=N``.

``/tmp/paperman-converted/``
   Full-document PDFs converted from non-PDF formats (e.g. ``.max``)
   via ``type=pdf``. Conversion uses the File class directly, so no
   external binary is needed. Page images are extracted sequentially,
   then compressed in parallel across all available CPU cores using
   ``QtConcurrent``, then merged into the final PDF. If the
   requesting client disconnects mid-extraction the partial file is
   removed.

--------------

Troubleshooting
---------------

PDF Conversion Issues
~~~~~~~~~~~~~~~~~~~~~

**Problem**: Conversion returns error - **Solution**: Check journalctl
logs for detailed error messages:
``sudo journalctl -u paperman-server -f``

**Problem**: Conversion is slow for large files - **Solution**: The
first request converts and caches the result; subsequent requests are
served from ``/tmp/paperman-converted/``. Compression runs in
parallel across all CPU cores. If the client disconnects before
conversion finishes, the server aborts and cleans up.

File Access Issues
~~~~~~~~~~~~~~~~~~

**Problem**: “File not found” but file exists - **Solution**: Check file
path is relative to repository root, not absolute

**Problem**: “Invalid file path” error - **Solution**: Path contains
``..`` or starts with ``/``. Use relative paths only.

--------------

Changelog
---------

Version 1.3 (Current)
~~~~~~~~~~~~~~~~~~~~~

-  Parallel PDF compression using ``QtConcurrent`` across all CPU cores
-  Streamed file responses (512 KB chunks with flow control)
-  Conversion progress reporting via ``progress=true``
-  Optional local URL for fast LAN downloads (app)

Version 1.2
~~~~~~~~~~~~

-  PDF conversion uses the File class directly instead of spawning
   a ``paperman`` subprocess
-  Conversion cache (``/tmp/paperman-converted/``) with 7-day expiry
-  Server aborts conversion when the client disconnects
-  Return 500 error instead of raw file on conversion failure

Version 1.1
~~~~~~~~~~~~

-  Single-page PDF extraction via ``page=N`` parameter
-  Page count query via ``pages=true`` parameter
-  Page cache with 7-day expiry (``/tmp/paperman-pages/``)

Version 1.0
~~~~~~~~~~~

-  Initial release
-  Basic search, list, and file retrieval
-  Multi-repository support
-  On-the-fly PDF conversion
-  Binary file download support
-  Security: Path traversal prevention
-  CORS enabled for web applications

--------------

Support
-------

For issues, feature requests, or contributions: - **GitHub**:
https://github.com/sjg20/paperman - **Email**: sjg@chromium.org

--------------

License
-------

GPL-2 - See LICENSE file for details
