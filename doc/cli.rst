Command-line Interface
======================

Paperman provides command-line tools for converting between file formats and
performing batch operations without the GUI.

Basic Usage
-----------

.. code:: bash

   paperman [options] <file-or-directory>

Conversion Options
------------------

``-m, --max <file>``
   Convert the given file to .max format. For PDFs with 8 or more pages, the
   conversion runs in parallel using multiple processes for faster rendering.

``-p, --pdf <file>``
   Convert the given file to .pdf format.

``-j, --jpeg <file>``
   Convert the given file to .jpg format.

``--output <file>``
   Write the converted output to the specified path instead of the current
   directory. Works with ``-m``, ``-p`` and ``-j``.

``--page-range S:E``
   Convert only pages S to E (1-based, inclusive). For example,
   ``--page-range 1:10`` converts the first ten pages.

``--jobs N``
   Set the number of parallel worker processes for ``-m`` conversion. The
   default (0) auto-detects the CPU count, limited so that each worker gets
   at least 10 pages. Use ``--jobs 1`` to force single-process conversion.

Other Options
-------------

``-s, --sum <dir>``
   Compute MD5 checksums for all files in a directory.

``-o, --ocr <dir>``
   Run OCR on all .max files in a directory (recursive).

``-q, --search <query>``
   Search the OCR index for a query string.

``-t, --test``
   Run the built-in unit tests.

``-h, --help``
   Display usage information.

Parallel PDF-to-max Conversion
------------------------------

Converting large PDFs to .max format is CPU-intensive because each page must
be rendered via Poppler's ``renderToImage()``, which takes roughly 55 ms per
page. Since Poppler serialises rendering within a single process, threads do
not help.

Paperman works around this by spawning multiple child processes, each with its
own Poppler ``Document`` instance, so rendering happens truly in parallel.

How it works
~~~~~~~~~~~~

When ``paperman -m big.pdf`` detects 8 or more pages:

1. The parent determines the worker count:
   ``min(cpu_count, page_count / 10)``, overridable with ``--jobs``
2. Pages are split into roughly-equal ranges across N workers
3. Each worker runs as a separate process:
   ``paperman -m big.pdf --page-range S:E --output /tmp/.../partN.max``
4. Workers run with ``QT_QPA_PLATFORM=offscreen`` so no display is needed
5. Once all workers finish, the parent merges the partial .max files using
   ``stackStack()``, which copies compressed chunks directly — no
   decompression or re-encoding
6. Temporary files are cleaned up automatically

Each worker gets at least 10 pages to avoid process-spawn overhead dominating
on high-core machines. Each page renders in ~55 ms, so a 100-page PDF uses
up to 10 workers and completes in under a second of wall time rather than
~5.5 seconds sequentially.

Examples
~~~~~~~~

Convert a PDF using automatic parallelism:

.. code:: bash

   paperman -m document.pdf

Force sequential (single-process) conversion:

.. code:: bash

   paperman -m document.pdf --jobs 1

Convert with 4 workers, output to a specific path:

.. code:: bash

   paperman -m document.pdf --jobs 4 --output /path/to/output.max

Convert only pages 20-30:

.. code:: bash

   paperman -m document.pdf --page-range 20:30 --output pages20-30.max

Convert a .max file to PDF at a specific path:

.. code:: bash

   paperman -p document.max --output /path/to/output.pdf

Testing
~~~~~~~

.. code:: bash

   # Run integration tests
   make test-parallel

   # Run unit tests (includes testImageDepth)
   paperman -t

Remote Servers
--------------

``paperman-client`` is a small command-line client for a
``paperman-server``. It shares its configuration and cached login tokens
with the GUI, so a server configured for one is available to the other.
See :doc:`server` for the ``client.conf`` format.

``servers``
   List the servers configured in ``client.conf``, marking the one the
   next command would use.

``add-server <url> [name]``
   Add a server to ``client.conf``, optionally under a friendly name. A
   server that is already listed is left alone.

``login <user>`` / ``logout``
   Log in to the selected server (prompting for the password) and cache
   the bearer token, or forget the cached token.

``status`` / ``repos``
   Show the server's status, or list the repositories it serves.

``ls <repo> [path]``
   List the files in a repository directory.

``cat <repo> <path> [-o OUT]``
   Fetch a file, writing it to stdout or to ``OUT``.

``--server <url-or-name>`` picks the server; without it the client uses
``$PAPERMAN_SERVER``, else the first entry in ``client.conf``, else
``http://localhost:8080``.

.. code:: bash

   paperman-client add-server https://paper.office.example.com Office
   paperman-client --server Office login simon
   paperman-client --server Office ls docs Invoices
