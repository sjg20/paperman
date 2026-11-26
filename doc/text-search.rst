How Text Search Works
=====================

How Paperman finds stacks by the words on their pages: how a page is
read, where its text is kept, how the index is built and how it is
searched. This is for anyone changing that code. See :doc:`finding` for
how it is used, :doc:`server-internals` for the server's side in more
detail and :doc:`api` for the routes.

The path a word takes
---------------------

::

   page image
       |  OCR engine (tesseract)                 ocr.cpp, ocrtess.cpp
       v
   words, each with its box                      ocrpage.cpp
       |  kept with the page                     filemax.cpp, pdfio.cpp
       v
   the stack's own file (.max, or a PDF's text layer)
       |  read when the stack changes            searchindex.cpp
       v
   SQLite FTS5 index (.paperindex, or the server's own file)
       |  MATCH query                            searchindex.cpp
       v
   pages found  ->  one result for each stack, at its best page

Two things follow from this shape:

- The stack's file is the only place the text is kept. The index is a
  copy, built from the files, and can be deleted at any time: it is
  built again from what the stacks hold.
- Nothing is indexed which has not first been read and written to a
  stack. Reading and indexing are separate steps, done at different
  times, often by different programs.

Reading a page
--------------

The engine
~~~~~~~~~~

``Ocr`` (``ocr.h``) is the interface to an engine. ``Ocr::getOcr()``
makes one engine for the whole program and keeps it. Tesseract
(``Ocrtess``) is the only engine built by default; OmniPage (``Ocromni``)
is built only with ``CONFIG_use_omnipage``, which is off. There is no
setting which chooses an engine.

An engine has two calls:

``imageToPage()``
   Reads a page image into an ``OcrPage``: its words, each with where it
   is. This is the one used wherever the result is kept.

``imageToText()``
   Reads a page image into plain text.

An engine which cannot say where words are gets a default
``imageToPage()``, which makes one "word" of each line of text, with an
empty box. The text is then right for searching, but a PDF gets no text
layer from it.

Tesseract
~~~~~~~~~

``Ocrtess`` runs the ``tesseract`` program, found on the ``PATH``; it is
not linked as a library. For each page it:

#. saves the image, unchanged, as a PNG file in the temporary directory
#. runs ``tesseract <image> <base> tsv``
#. reads the words from ``<base>.tsv`` with ``OcrPage::fromTsv()``

No language, page segmentation mode or engine mode is given, so
tesseract's own defaults apply, and the image is not scaled, straightened
or made black and white first. There is no time limit: a read which
takes more than a minute is noted in the ``paperman.ocr`` log category
each minute, and waited for.

A reader working in the background (``setBackground()``) keeps out of
the way: tesseract is held to one thread with ``OMP_THREAD_LIMIT=1`` and
run through ``nice -n 15`` where there is a ``nice``. Several pages are
read at once by running several of these.

Words and where they are
~~~~~~~~~~~~~~~~~~~~~~~~

``OcrPage`` (``ocrpage.h``) holds the size of the image which was read,
in pixels, and its words in reading order. Each ``OcrWord`` has:

.. list-table::
   :widths: 15 85

   * - ``box``
     - Where the word is, in the pixels of the page image
   * - ``text``
     - The word, with no spaces in it
   * - ``conf``
     - How sure the engine is, from 0 to 100
   * - ``line``, ``para``
     - The line and the paragraph it is in, each numbered from 0 through
       the page

``fromTsv()`` takes the word rows of tesseract's output (level 5) which
have text, and numbers lines and paragraphs through the page from
tesseract's block, paragraph and line numbers.

``text()`` gives the page as plain text: words joined by a space, lines
by a newline and paragraphs by a blank line. This is what is indexed and
what the text pane shows.

``toBytes()`` and ``fromBytes()`` give the form in which a page's words
are kept, which is compact JSON in UTF-8::

   {"v":1,"w":2550,"h":3300,
    "words":[[left,top,width,height,conf,line,para,"text"],...]}

``v`` is the version of this form (``stored_version`` in
``ocrpage.cpp``). A page with no words is kept as nothing at all, and a
page whose version is not known reads back as empty.

When pages are read
-------------------

On the desktop
~~~~~~~~~~~~~~

``OcrReader`` (``ocrreader.cpp``) reads stacks in the background. There
is one, made by ``Mainwidget``. Stacks are given to it:

- when a scan is saved, or a camera capture is finished, if the option
  to read scanned pages is on (the ``ocr/background`` setting) and the
  stack can keep text
- by **Read text (OCR)** (``Desktopwidget::readText()``), for the stacks
  selected, whether the option is on or not
- shortly after start-up, from the list of stacks which were still
  waiting when Paperman last closed (the ``ocr/pending`` setting)

It keeps a queue of stacks by path, so a stack can be renamed or moved
while it waits (``stackMoved()``), and reads each page which has no
words yet. The page images of a ``.max`` stack are decoded on the worker
threads, not the display's. The number of pages read at once is the
``ocr/workers`` setting, or half the processors, up to 16, when that is
0. When a page has been read, the reader checks that the page is still
the one it started with (by its size, compressed size and timestamp),
writes the words with ``File::putPageOcr()`` and tells the page view, so
the text pane fills in.

A page is read when it is a ``.max`` page with no words kept. Only a
``.max`` stack on this computer can keep them (``Desktopmodel::keepsOcr()``).

Three other things read a page on the desktop:

- The **OCR** button in the text pane (``Pagewidget::ocrPage()``) reads
  the page shown at once, on the calling thread, and keeps the words if
  the stack can keep them.
- Naming a stack with the assistant reads the first few pages if too few
  have text. What is read then is used for the name and not kept.
- ``paperman --ocr <dir>`` reads every page with no words in every
  ``.max`` stack under a directory, then brings the index there up to
  date.

On a server
~~~~~~~~~~~

``paperman-server`` reads pages in two cases:

- With ``--read-pages``, each repository has a ``RepoReader``, which
  looks through the repository for ``.max`` stacks with pages not read,
  newest first, and reads them in the background. It writes to a copy of
  the stack and swaps it in, and keeps a record of the stacks it has
  dealt with in a small database of its own, so that a stack whose pages
  have no words is not read again each time. It looks through the
  repository again 15 minutes after it last finished.
- ``POST .../pages/{n}/ocr`` reads one page at once, whether or not the
  server was started with ``--read-pages``. This is what the **OCR**
  button does for a stack on a server.

``POST .../read`` puts a stack at the front of the reader's queue, which
is what **Read text (OCR)** does for a stack on a server.

There is no route which takes the words of a page from a client. They
reach a server inside a ``.max`` file which is uploaded with its pages
already read, or because the server read the pages itself.

Where the text is kept
----------------------

In a .max file
~~~~~~~~~~~~~~

A ``.max`` file can hold three kinds of text, and it matters which is
which:

.. list-table::
   :header-rows: 1
   :widths: 22 26 26 26

   * - Text
     - Where
     - Written by
     - Read with
   * - A page's words
     - An OCR chunk for the page
     - Paperman, when it reads the page
     - ``getPageOcr()``
   * - A page's plain text
     - A text chunk for the page
     - PaperPort; Paperman does not write it
     - ``getPageText()``
   * - The stack's OCR annotation
     - The annotation chunk
     - Typing in the text pane for a page with no words; a server
       reading a file which cannot keep words
     - ``getAnnot (Annot_ocr)``

The page's words are Paperman's own addition to the format. Each page
has a chunk of type ``CT_ocr`` holding the JSON above. The page's record
holds the chunk's position after a marker (``ROSWELL_OCR_MAGIC``, the
characters ``OCR1``), near the end of the record. A page written by
PaperPort has something else there, so the position is believed only
when the marker is present and the file's header carries Paperman's
version.

``getPageOcr()`` returns an empty page, and no error, both for a page
which has not been read and for one which was read and has no words:
nothing in the file tells them apart. The desktop's reader therefore
reads a blank page again each time its stack is queued; the server's
reader avoids that with its own record.

``getPageText()`` reads only the text chunk. It does not return the
words Paperman read, so code which wants "the text of this page" asks
for the words first and falls back to the text chunk, as
``stackText()`` in ``searchindex.cpp`` and ``Desktopwidget::pageText()``
do.

What happens to a page's words when the page changes:

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Operation
     - The words
   * - Stack, unstack, duplicate, convert
     - Go with the page, where the file it goes to can keep them
   * - Reorder pages
     - Go with the page
   * - Remove a page
     - Go with it, and come back with it on undo
   * - Turn or flip a page
     - Are dropped, since the boxes no longer fit the image
   * - Crop a page
     - Are dropped for the new page; undo brings the old page back with
       its words

In a PDF
~~~~~~~~

A PDF's text is read with Poppler (``Pdfio::getPageText()``), whatever
program made the PDF. The words and their boxes cannot be read back
from a PDF: ``getPageOcr()`` is not implemented for it.

When Paperman makes a PDF from pages which have words, it writes them as
a layer of invisible text over the page image
(``Pdfio::addTextLayer()``), so that other programs can search the PDF
and copy text from it, and so that Paperman's own index finds it through
Poppler. Each word is drawn in text rendering mode 3 (invisible), at its
box scaled from image pixels to the page, stretched to the box's width,
in a font with no glyphs (``GlyphLessFont``, as tesseract's own PDFs
use) whose character codes are Unicode. A word with no box is left out,
as is the whole layer on a page which is rotated.

This happens wherever a ``.max`` stack becomes a PDF: **Duplicate... > as
PDF**, emailing as PDF, converting from the command line, and the PDFs a
server makes of a stack or of one page.

In other files
~~~~~~~~~~~~~~

A JPEG keeps no words. Its OCR annotation is kept in the picture's EXIF
``UserComment``. Other types of file have no text. Neither is indexed.

The index
---------

``SearchIndex`` (``searchindex.cpp``) is the index, for the desktop, the
command line and the server alike. It is a SQLite database, used through
Qt's ``QSQLITE`` driver, with SQLite's FTS5 full-text search.

Where it is
~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Used by
     - The database
   * - The desktop, for a repository on this computer
     - ``.paperindex`` at the top of the repository
   * - ``paperman --ocr`` and ``--search``
     - ``.paperindex`` in the directory given
   * - ``paperman-server --index``
     - A file for each repository in the server's own data directory,
       named for the repository and a hash of its path (see
       :doc:`server-internals`)

The server keeps its index out of the repository so that it can be on
the server's own disk, opened with a write-ahead log and a busy timeout:
one connection searches while another, on a thread of its own, writes.
A ``.paperindex`` in a repository may be on a network filesystem, where
a write-ahead log is not safe, so it uses SQLite's default journal.

What is in it
~~~~~~~~~~~~~

.. code:: sql

   CREATE VIRTUAL TABLE ocr_index USING fts5(
     filepath UNINDEXED, filename, pagenum UNINDEXED, text,
     tokenize='porter unicode61 remove_diacritics 1');

   CREATE TABLE files (
     filepath TEXT PRIMARY KEY, mtime INTEGER, size INTEGER);

   CREATE TABLE facts (
     filepath TEXT PRIMARY KEY, type TEXT, sender TEXT, date TEXT,
     period TEXT, amount TEXT, currency TEXT, summary TEXT);

``ocr_index``
   One row for each page which has text: the stack's full path, its
   filename, the page number from 0 and the page's text. The tokeniser
   splits text into words by Unicode's rules, folds accents away and
   stems each word with the Porter algorithm, so that *invoices* finds
   *invoice*. The stemming is for English.

``files``
   One row for each stack looked at, with its modification time and
   size when it was indexed. This is how a later pass tells that a stack
   has changed.

``facts``
   A copy of the facts each stack keeps about its document (see
   :doc:`filing`), for finding stacks by them (``findFacts()``). It is
   filled in the same pass, but full-text search does not look in it.

The version of what the index holds is kept as the database's
``user_version`` (``INDEX_VERSION`` in ``searchindex.cpp``). When an
index with a lower version is opened, all three tables are emptied, so
that the next pass reads every stack again.

What is indexed for a stack
~~~~~~~~~~~~~~~~~~~~~~~~~~~

``stackText()`` opens the stack and takes, for each page, its words as
plain text, or if it has none, the page's own text: the text chunk of a
``.max`` page, or a PDF page's text through Poppler. The stack's OCR
annotation, if it has one which the first page's text does not already
hold, is added to the first page, since it does not say which page it
came from.

Only ``.max`` and PDF files are indexed. Anything in a directory whose
name starts with a dot is left out, which covers the trash.

Keeping it up to date
~~~~~~~~~~~~~~~~~~~~~

``sync()`` brings the index up to date with a directory and everything
below it:

#. It reads the ``files`` rows for stacks under the directory.
#. It walks the directory for ``.max`` and PDF files.
#. A stack which is new, or whose modification time or size differs
   from its row, is indexed again: its rows are removed, each page with
   text is added, its facts are stored and its ``files`` row is written.
   A stack which cannot be read gets a ``files`` row all the same, so
   that it is not tried again until it changes.
#. It commits every 200 stacks, so that a first pass over a large
   repository can be searched while it runs and loses little if it is
   stopped.
#. A stack with a row but no file is removed, but only if the walk ran
   to the end.

The caller's progress function is called for each stack, and stops the
pass by returning false. ``syncStack()`` does the same for one stack.

Once a repository has been indexed, a pass reads only the stacks which
have changed, so it costs a walk of the directory tree and a look at
each file's time and size.

Searching
---------

The query
~~~~~~~~~

``matchQuery()`` turns what the user typed into an FTS5 query. The text
is split at spaces; each word is put in double quotes, with any double
quote in it doubled, so that nothing typed is taken as query syntax; and
the last word is followed by ``*``, since it may still be being typed.
The words are joined with spaces, which FTS5 reads as "all of these".
So ``power bil`` becomes ``"power" "bil"*``: pages with a word which
stems as *power* does and a word starting *bil*.

``paperman --search`` does not use ``matchQuery()``: it passes its
argument to FTS5 as it is, so FTS5's own syntax (``OR``, ``NEAR``,
phrases in quotes) works there and nowhere else.

The search
~~~~~~~~~~

.. code:: sql

   SELECT filepath, filename, pagenum,
          snippet(ocr_index, 3, '<b>', '</b>', '...', 20) AS snippet,
          rank
   FROM ocr_index
   WHERE text MATCH ?
     AND substr(filepath, 1, length(?)) = ?
   ORDER BY rank
   LIMIT ?

- Only the ``text`` column is matched. A stack's name is in the index
  but is not searched.
- ``rank`` is FTS5's own measure (BM25), best first.
- The snippet is up to 20 words of the page's text around a match, with
  the words matched between ``<b>`` and ``</b>``.
- The second condition keeps results to a directory and what is below
  it.
- A result is a page. The callers below keep the best page of each
  stack and drop the rest.

Who searches, and when the index is brought up to date
------------------------------------------------------

The desktop, for a repository on this computer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Edit > Search**, with **Text on the pages** chosen, comes to
``Desktopmodel::findLocalText()`` (``dmop.cpp``), which:

#. opens ``.paperindex`` at the top of the repository
#. calls ``sync()`` for the folder being searched, so the index is
   brought up to date before every search, and only for that folder
#. searches for up to 1000 pages under that folder
#. keeps the first, and so the best, page of each stack
#. shows the stacks in the desk of search results, each turned to its
   page

This runs on the display's thread. The window stays alive because the
progress function lets events through for each stack, and the pass is
shown as **Indexing text** in the status bar and in **View > Background
work**, where it can be stopped: the search then looks through what has
been indexed so far. The first search of a large folder is the slow one.

The snippets are not shown on the desktop.

The server
~~~~~~~~~~

With ``--index``, each repository has a ``RepoIndex`` (``repoindex.cpp``),
which holds two connections to its index. One, on a low-priority thread,
writes: a whole ``sync()`` when the server starts and again 15 minutes
after each pass finishes, and ``syncStack()`` for a stack as soon as it
is changed through the server (uploaded, renamed, stacked, read by the
reader and so on). A stack changed behind the server's back, by another
program writing to the repository, is found by the next whole pass. The
other connection searches.

``GET /v1/repos/{repo}/search`` takes ``text``, an optional ``path`` to
search below and ``max`` (100 if not given, 1000 at most). It applies
``matchQuery()``, keeps the best page of each stack and replies with
each stack's path, its page from 1 and the snippet, and ``complete``,
which is false until the first whole pass has finished. A server without
``--index`` replies 404. ``GET /v1/status`` lists ``textSearch`` among
the server's features and reports each index's progress.

The desktop, for a repository on a server
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``Desktopmodel::findTextLater()`` sends what was typed to the server's
search route and shows what comes back. Nothing is indexed on the
desktop. A server which does not list ``textSearch`` gives a message
saying that it must be run with ``--index``, and a reply which is not
``complete`` adds a note that some stacks may be missing.

The app and the command line
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The phone app uses the same route, and shows the snippets, with the
words matched in bold. ``paperman --search`` opens ``.paperindex`` in the
directory given and searches it as it stands, without bringing it up to
date first.

What text search does not do
----------------------------

Searching by stack name is separate, and uses no index. The box above
the stacks looks through the names in the folder shown. **Search** with
**Stack names** chosen looks through the names of files in the folder tree,
which the desktop holds in memory, or asks the server to look through
its own list. Both match any part of a name, in any type of file, with
no stemming.

Text search does not look at:

- stack names, page names, or the author, title, keywords and notes of a
  stack
- JPEG stacks and other types of file
- stacks in the trash
- pages which have not been read

Things worth knowing before relying on it:

- **Pages turned or cropped lose their words** and are not queued to be
  read again on the desktop. They are read again by **Read text (OCR)**,
  by the **OCR** button, by ``paperman --ocr``, or by a server's reader.
- **A stack on a server is never read by the desktop**. It needs the
  server run with ``--read-pages``, or its pages read one at a time.
- **One language.** Tesseract reads with its default language, and the
  index stems words as English.
- **The desktop indexes as it searches**, on the display's thread. A
  folder which has never been searched has nothing in the index.
- **Paths in the index are absolute.** If a repository is reached by
  another path, its stacks are indexed again under the new one, and the
  rows under the old path stay until the index is deleted.
- **A change is seen by time and size alone.** A stack written with the
  same size and the same modification time is not read again.
- **The server's** ``info?text=1`` gives each page's text chunk, or a
  PDF's text, not the words Paperman read, so it is empty for a ``.max``
  page read by Paperman.

Changing it
-----------

- If ``stackText()`` comes to find text which it did not before, raise
  ``INDEX_VERSION``, so that existing indexes are built again. Without
  that, only stacks which change are looked at.
- If the form of a page's words changes, raise ``stored_version`` in
  ``ocrpage.cpp`` and keep reading the old form: a page whose version is
  not known reads as empty, and is then read again and overwritten by
  whichever Paperman opens it.
- A new type of file which holds text needs ``getPageText()`` or
  ``getPageOcr()`` for it, and its extension added to the walks in
  ``SearchIndex::sync()`` and ``RepoIndex``.
- The index is never the only copy of anything. Keep it that way, so
  that deleting it stays safe.

The tests are in ``TestOcrSearch`` (the index, the form of a page's
words, the desktop's reader and the server's reader and index),
``TestFile`` (words in ``.max`` files and the PDF text layer),
``TestSearchServer`` (the routes, and the desktop searching a server)
and ``TestDesktopUi::testTextSearch`` (the search dialog). See
:doc:`testing`.
