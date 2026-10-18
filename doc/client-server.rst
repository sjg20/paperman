Client and Server
=================

This page describes how the desktop program works with repositories on a
``paperman-server``, for anyone changing that code. See :doc:`server` for
running a server and :doc:`api` for its routes.

The Shape of It
---------------

A repository is local or remote, and the desktop can show both side by
side in its directory tree. The server holds a remote repository's stacks;
the desktop shows them, opens them, changes them and scans into them over
HTTP, and other clients' changes appear as they happen. The Android and
iPhone app reads a server's repositories in the same way.

Each repository has a ``Backend``: ``LocalBackend`` reads the disk
directly, and ``RemoteBackend`` talks to a server. ``Dirmodel`` keeps one
backend per repository, and ``Desktopmodel`` gives each desk the backend of
its repository, so the rest of the code mostly need not care where a stack
lives.

Remote Stacks
-------------

A remote repository's folders and stacks are listed by the server
(``/browse``), and a stack's thumbnail is drawn by the server
(``/thumbnail``). A stack's ``File`` object points into the cache, at
``~/.cache/paperman/<serverId>/<repo>/<path>``, where its bytes are kept
once fetched, with the server's validator beside them in a ``.etag``
file, so opening it again costs a ``304``. A stack's path on the server is
worked out from its place in the cache (``Desktopmodel::remoteStackPath()``),
so a desk holding stacks from many folders, as a search's does, works too.

A server which lists ``pages`` in the features of ``/v1/status`` serves a
stack a page at a time: the desktop fetches its structure from ``/info``
and each page the first time it is shown, keeping them in
``<path>.d/`` beside where the whole file would go. Against an older
server the whole file is fetched.

The listing gives each stack's page count (``pages``), which a newer
server reads from the stack and keeps until the file changes. So a stack
shows its page controls, and the thumbnail of the page it is turned to,
before any of it is fetched; turning its page fetches the server's
thumbnail of the new one until the page itself is here.

Changes
-------

Each change the desktop makes to a remote stack is a ``POST`` to a route
under ``/v1/repos/{repo}/stacks/{path}``: ``rename``, ``move``,
``delete``, ``duplicate``, ``stack``, ``unstack``, ``transform``,
``annotations``, ``pages/delete`` and ``pages/undelete``, with the server
doing the work on its own copy. Deleting pages returns an ``undoId``,
which undoing the deletion hands back. The last change to arrive wins;
there is no check that a stack has not changed since it was read.

Some operations need the pages themselves: converting a stack to another
type, copying its odd or even pages, unfolding a booklet, and emailing or
copying it. The desktop does these from a copy of the whole stack,
downloaded outside the cache (``RemoteBackend::downloadFile()``), and
uploads a new stack it makes beside the original (``/upload``), taking the
name the server gives it if its own is taken. A scan into a remote folder
is uploaded the same way when it is confirmed.

The positions of the stacks on a desk are shared through the server's copy
of the folder's ``.paperdesk``. The server keeps it up to date as stacks
come and go: a stack which arrives is placed after the others, whichever
client put it there, and the stacks a desktop's upload of the layout does
not mention are kept.

A server which lists ``layout`` among its features gives each stack's
position in the folder's listing, so the desktop does not fetch the
layout itself. When stacks move, the desktop sends only those
(``POST /v1/repos/{repo}/layout``), one request at a time, and each other
desktop showing the folder moves them as the event comes. The desktop
keeps its own copy of the file in its cache, for what it notes of each
stack, such as the page it shows, which is its own. Against an older
server the desktop uploads the whole layout, as before.

Other clients' changes arrive on ``/v1/repos/{repo}/events``, a stream of
server-sent events which the desktop opens for each remote repository it
shows. Each event names the stack changed; the desktop drops its cached
copy and refreshes any desk showing that folder.

Accounts
--------

A server with users (``users.json``) only answers a client which has
logged in with ``POST /v1/auth/login``, giving a bearer token which lasts
30 days, or which gives an API key. A user may be limited to some of the
repositories. The desktop and ``paperman-client`` keep the token in
``~/.config/paperman/<serverId>.token``. The app logs in when the server
asks for credentials, and otherwise sends its username and password as
Basic auth, for a server behind a proxy which asks for them.

Text
----

The server reads the pages of its stacks with tesseract when run with
``--read-pages``, keeping the words with each page as the desktop does.
A client can ask for a stack to be read before the rest
(``…/stacks/{path}/read``), as the desktop's "Read text (OCR)" does for a
remote stack, and can have one page read at once (``…/pages/{n}/ocr``).

When run with ``--index``, the server keeps an index of the text of its
stacks, and ``GET /v1/repos/{repo}/search`` searches it. The desktop's
search for text on the pages uses it for a remote repository, and its
own ``.paperindex`` for a local one; the app uses it too.

Versions
--------

``/v1/status`` gives the API's version and a list of features. Clients
check for features one by one (``pages``, ``textSearch``, ``readPages``),
so a newer server works with an older client and the other way about. The
desktop refuses a server from before the ``/v1`` API (before 1.4.0), and
one with a newer major version of it, saying which version is needed.

Not Waiting for the Server
--------------------------

The desktop should never wait for a server on its main thread. A wait
either freezes the window or, through the nested event loop which
``RemoteBackend``'s blocking calls spin, lets the user act again before
the first action is done, with the code part-way through it. Each flow
which talks to a server is built not to wait, as below.

Code which must not wait asks ``RemoteBackend`` with a context object and
a callback, such as ``fetchStatus(context, done)``, and carries on when
the callback comes with the ``Result``. The callback is dropped if the
context has gone by then, so a window closed with a request still out is
safe. The blocking calls remain, for ``paperman-client`` and for code
which cannot go on without the answer, such as the tests, and some are
built on the asynchronous ones. Attaching a server works this way:
``Desktopwidget::addRemoteServer()`` calls back once the server's
repositories are in the tree, and at startup the window opens at once,
with each server, including one named by its host with ``--server``,
appearing as it answers.

Folders work this way too. Opening a folder on a server puts its desk in
the model at once, empty, while ``Desktopmodel::listRemoteDesk()`` fetches
the listing and the desk file together; once both have come the files go
on the end of the desk and are announced as new rows, and ``deskListed()``
lets the window select a stack it was asked to locate. A change made by
another client lists the folder again the same way, swapping the rows only
once the new listing is in. A desk's layout goes back to the server in the
background, one upload at a time, so that an earlier layout cannot land
after a later one; only the final save as the window closes waits. Code
which needs a folder's files before it can go on, as many tests do, passes
``wait`` to ``showDir()``.

The page view does the same for a stack. Showing one asks for its
structure with ``requestContent()``, and showing a page which is not here
asks for it with ``requestPage()``, leaving the page blank meanwhile;
``Desktopmodel::pageHere()`` says whether a page can be read without the
server. When the structure arrives the model emits ``pageContentChanged()``
for the whole stack, so that the view lays it out again with its real page
count, and when a page arrives, for that page, so that it is drawn.

Changes are harder, since each is an undo command whose undo depends on
what the change did: undoing a move to the trash needs the name the stack
got there, which only the server's answer gives. So each undo command
queues its operation with ``Desktopundocmd::queue()``, and
``Desktopmodel::queueOp()`` runs the operations on a server one at a time,
each once the one before has finished. An operation finds what it acts on
by name when it starts, and anything its undo needs goes in a shared
object the operation fills in, so an undo pressed before the server has
answered waits its turn and then reverses what the change did. An
operation on a local desk runs at once, as before. Every change works
this way: renaming a stack or a page, rotating, changing annotations,
moving to the trash (or another folder), deleting and undeleting pages,
unstacking, stacking, duplicating, converting, unfolding and deleting
stacks. Each has a form which calls back once done and one which waits,
for code that cannot go on until it has. An operation needing several
requests, such as unstacking every page of a stack, makes them one after
another with ``Desktopmodel::runSteps()``. A stack the server changed is
fetched afresh, rather than having the change mirrored onto its cached
copy.

Reading a page's text on the server goes through the same queue, behind
any change to the stack, and the page view shows the text once it comes
if the page is still the one shown. Asking a server to read a stack's
pages next does not wait either. Searching a server's text shows the
stacks found once the answer comes; ``Desktopmodel::findTextLater()``
calls back with them, and a local repository, whose index is brought up
to date on the spot, calls back before it returns.

A stack scanned into a folder on a server is made in the local cache, as
before, and once confirmed is uploaded through the same queue, so the
next scan can start at once. Until then it is only here, so a listing of
its folder which replaces the rows waits behind any queued change, and
closing the window waits (for up to half a minute) for queued changes to
reach their server, rather than losing them. Printing first brings every
page of the stacks here with ``Desktopmodel::fetchStacksLater()``, which
asks for what each stack lacks and looks again each time something
arrives, then prints them as before.

Emailing a remote stack, or copying it to the clipboard, first fetches a
whole copy of it into the temporary folder with
``Desktopmodel::workCopiesLater()``, then packages it as before. A
server without the convert feature has a copy of the stack converted, or
unfolded, here: each stack is a step of the operation which downloads
the copy, converts it and uploads the result, whose reply names the new
stack just as the server's own conversion does.

Each blocking wait in ``RemoteBackend``, and ``ClientConfig::probe()``,
reports itself to ``BlockWatch``, naming the request. It notes only waits
on the main thread, and only while watching; watches nest, and each reads
the waits noted since its own ``mark()``. Setting ``PAPERMAN_BLOCKWATCH``
in the environment makes the desktop log each one, which shows any left
in use.

The ``async`` test suite (``test/test_async.cpp``) drives the desktop
against a server on a thread of its own, behind a proxy which holds each
reply back for half a second (``test/remoteharness.cpp``). A server on
the test's own thread would answer from inside any nested event loop, so
would hide a wait. Around each step an ``AsyncCheck`` collects the
requests the main thread waited for and the longest time it went without
handling events, and the step must have neither. Each test is also
watched from start to finish, and fails if the main thread waited for
the server anywhere in it: only closing the window may wait, for queued
changes and to save the folder's layout, and a test which closes it says
so with ``closeWindow()``. A flow being worked on can be marked as an
expected failure with ``CHECK_ASYNC()``, which logs what it waited for;
making it asynchronous turns that into an unexpected pass, which fails,
so the mark has to come off with the change.

The proxy can also hold replies back for ever, as a hung server would,
which is for testing the desktop once it no longer waits.

Traffic
-------

The proxy also records each request it passes on, with the bytes it
carried each way, headers included, and which requests opened a
connection: the server keeps a connection open for the next request, so
one may carry many. The ``traffic`` suite (``test/test_traffic.cpp``)
drives the desktop through things a user does, such as opening a folder
of stacks or turning through a stack's pages, prints the requests each
made, grouped by endpoint, and checks the totals against a budget of
requests, connections and bytes. A change which sends more fails the
budget; one which sends less tightens it.

Tracing the Connection
----------------------

The desktop logs each request to a server, and how it ends, when the
``paperman.net`` logging category is turned on:

.. code-block:: bash

   QT_LOGGING_RULES="paperman.net.debug=true" paperman --server ohau

This shows the addresses tried for a server given by name, and which
answered, then each request with a number, when the server answered it
and with what status, and how it ended::

   paperman.net: probe http://ohau:8081/status answers
   paperman.net: probe chose http://ohau:8081
   paperman.net: #1 GET ohau:8081/v1/status
   paperman.net: #1 answered 200 in 4 ms
   paperman.net: #1 done in 4 ms, ok

A warning from Qt itself says nothing of where it came from. Asking for a
backtrace with each warning helps, in a build with debug information:

.. code-block:: bash

   QT_MESSAGE_PATTERN='%{message}%{if-warning} %{backtrace depth=20}%{endif}' \
      paperman --server ohau

What Remains
------------

- **Searching every repository at once**: a search covers one repository,
  local or remote.
- **The app** fetches each stack whole, as a PDF the server makes, does
  not follow other clients' changes, and makes no changes itself.
- **Changes made at the same time**: the last to arrive wins, with no
  warning.
- **Conversions on the server**: converting a remote stack downloads it
  and uploads the result, which is slow over a slow link, where the server
  could do it in place.
