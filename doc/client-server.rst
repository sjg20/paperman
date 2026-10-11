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
of the folder's ``.paperdesk``.

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
the first action is done, with the code part-way through it. Much of the
desktop still waits, and the work to stop it goes flow by flow.

Each blocking wait in ``RemoteBackend``, and ``ClientConfig::probe()``,
reports itself to ``BlockWatch``, naming the request. It notes only waits
on the main thread, and only while watching. Setting
``PAPERMAN_BLOCKWATCH`` in the environment makes the desktop log each one,
which shows what is left in use.

The ``async`` test suite (``test/test_async.cpp``) drives the desktop
against a server on a thread of its own, behind a proxy which holds each
reply back for half a second (``test/remoteharness.cpp``). A server on
the test's own thread would answer from inside any nested event loop, so
would hide a wait. Around each step an ``AsyncCheck`` collects the
requests the main thread waited for and the longest time it went without
handling events, and the step must have neither. A flow which still
waits is marked as an expected failure, and the test logs what it waited
for, so the suite is also the list of work left. Making a flow
asynchronous turns its expected failure into an unexpected pass, which
fails, so the mark has to come off with the change.

The proxy can also hold replies back for ever, as a hung server would,
which is for testing the desktop once it no longer waits.

What Remains
------------

- **Searching every repository at once**: a search covers one repository,
  local or remote.
- **The app** fetches each stack whole, as a PDF the server makes, does
  not follow other clients' changes, and makes no changes itself.
- **Changes made at the same time**: the last to arrive wins, with no
  warning.
- **Waiting for the server**: connecting, listing a folder, showing a
  stack and each change still wait for the server on the main thread; see
  Not Waiting for the Server above.
- **Conversions on the server**: converting a remote stack downloads it
  and uploads the result, which is slow over a slow link, where the server
  could do it in place.
