The Activity Log
================

Paperman keeps a record of what is done to the papers in a repository:
each scan, rename, move, stack and split, each page turned, cropped or
removed, each time text is read, and each undo. This page describes the
record for anyone changing the code which writes or reads it.

It is being built in steps. The desktop writes the record, for a
repository on this computer, and shows it in **View > Activity** (see
:doc:`desktop`). ``paperman-server`` writes it for the repositories it
holds, and gives it to clients; the desktop shows that in the same
window.

Where it is kept
----------------

The record is the file ``.paperlog`` at the top of the repository, so it
goes where the papers go and is the same from whichever computer they
are used. Each entry is one line of JSON, added at the end. Nothing in
the file is ever changed, so two programs can add to it, and it is safe
on a network filesystem, where a database is not.

It records what Paperman does, and is not what makes anything happen.
Deleting it loses the history and nothing else.

The desktop writes nothing while the ``activity/record`` setting is off,
which is the box in the options. Reading is not affected.

``ActivityLog`` (``activitylog.cpp``) reads and writes it, and has no
need of the rest of the desktop.

An entry
--------

.. code:: json

   {"id":1795732200123000,"t":"2026-11-26T22:30:00.123Z","who":"user",
    "do":"rename","path":"bills/power bill.max","from":"bills/scan1.max"}

.. list-table::
   :header-rows: 1
   :widths: 12 88

   * - Key
     - Meaning
   * - ``id``
     - The entry's number: the time it was added, in microseconds, made
       larger where needed so that no two are the same. Entries sort by
       it as they happened
   * - ``t``
     - When it was done, in UTC
   * - ``who``
     - ``user``; ``assistant``, for what the AI assistant suggested and
       the user accepted; or ``reader``, for Paperman reading text in the
       background
   * - ``do``
     - What was done: see below
   * - ``path``
     - The stack, as a path below the top of the repository, as it is
       once the thing is done
   * - ``from``
     - Where the stack was before, for a rename or a move; or what it
       was made from, for a copy or a stack taken out of another
   * - ``page``
     - The page it was done to, from 0, if it was done to one page
   * - ``n``
     - How many pages or stacks it covers
   * - ``note``
     - Anything more, in words: a page's new name, or the name of a group
   * - ``in``
     - The ``id`` of the entry this one belongs under
   * - ``ref``
     - For an undo or a redo, the ``id`` of the entry it is about; for a
       ``restore``, of the entry which deleted the stack
   * - ``kept``
     - For a ``trash``, the name the stack has in the repository's trash

A key with nothing to say is left out. A line which cannot be read, or
whose ``do`` is not known, is passed over, so a newer Paperman can add
kinds of entry which an older one ignores.

The values of ``do``, which must never change once used:

.. list-table::
   :widths: 22 78

   * - ``scan``, ``scan-page``
     - A stack was scanned or photographed; each of its pages is an entry
       under it
   * - ``import``
     - Files were brought in as stacks
   * - ``rename``, ``move``
     - A stack was given a new name, or moved to another folder
   * - ``trash``
     - A stack was deleted; ``path`` is where it was, and ``kept`` its
       name in the trash
   * - ``restore``
     - A deleted stack was put back from the trash; ``path`` is where
       it now is
   * - ``duplicate``
     - A copy of a stack was made, here or in another folder
   * - ``stack``, ``unstack``, ``split``
     - Stacks were put together, pages were taken out as a stack, or a
       stack was divided into several
   * - ``remove-pages``, ``reorder``
     - Pages were removed, or put in another order
   * - ``turn``, ``crop``, ``cover``, ``rename-page``
     - A page was turned or flipped, cropped, made the cover, or named
   * - ``facts``, ``details``
     - The facts about the document, or the stack's title, author,
       keywords or notes, were changed
   * - ``read``
     - Text was read (OCR): from one page, or by the background reader
       from ``n`` pages of a stack
   * - ``unfold``
     - A booklet was unfolded
   * - ``folder-new``, ``folder-move``
     - A folder was made, or was renamed or moved into another. Here
       ``path``, and ``from`` for a move, are folders
   * - ``group``
     - Several things were done as one step: they are the entries under
       it
   * - ``undo``, ``redo``
     - The entry named by ``ref`` was undone, or done again

Following a stack
-----------------

An entry gives a stack's path at the time. To find where that stack is
now, ``ActivityLog::whereNow()`` follows the entries after it: a rename
or a move whose ``from`` is the stack's path takes it to its ``path``;
a ``folder-move`` takes everything under the folder it names, however
far down, to the same place under the folder's new path; an undo works
the entry it names backwards, and a redo forwards, with a group's entries
taken together; and a ``trash``, or a ``stack`` onto another, means that
it has gone, until a ``restore`` at its path brings it back. An entry
about a folder is followed in the same way, to
where that folder is now. This is how an entry from days ago
leads to the stack as it is today.

What writes it
--------------

Almost everything the desktop does to stacks is an undo command
(``desktopundo.cpp``), so the undo stack is where it is recorded:

- Each command says what it did through ``Desktopundocmd::activity()``,
  once it has been done, so that the names of stacks it made are known.
  A new command which changes papers should say so there; one which does
  not, such as a small move about the desk, gives nothing.
- ``Desktopundostack::push()`` writes the entries. Several from one
  command, or everything within ``beginMacro()`` and ``endMacro()``, go
  under a ``group`` entry.
- Undo and redo write an entry naming the one they are about.
- ``ActivityBy`` says who the commands done while it exists are done by,
  which is how what the assistant suggested is marked as its own.

Two things are not undo commands and are recorded where they happen,
through ``Desktopmodel::recordActivity()``: a scan, with its pages, when
it is saved; and the reading of text, by page for the **OCR** button and
by stack for the background reader, which says when it has finished with
a stack through ``OcrReader::stackRead()``.

Folders are not changed through undo commands either. ``Dirmodel``
records a folder being made, and one being moved by dragging it onto
another, through ``Dirmodel::recordFolder()``, which the desktop also
calls when a folder is renamed. Without these a stack could not be
followed once a folder above it had a new name.

On a server
~~~~~~~~~~~

``paperman-server`` writes the same file, at the top of each repository
it holds. Every change it makes to a stack passes through
``SearchServer::notifyStackEvent()``, which tells clients of it, so that
is where ``recordActivity()`` writes the entry. The route which made the
change adds what it knows which the event does not carry, such as the
page, through ``StackDetail``. What the server's own reader reads is
marked as the reader's. A folder which the server makes is recorded by
the route which makes it.

The server does not link an undo to the entry it undoes: a client which
undoes a rename asks for another rename, which is what is recorded.

``GET /v1/repos/{repo}/activity`` gives the newest entries (see
:doc:`api`).

What shows it
-------------

``Activitydialog`` (``activitydialog.cpp``) reads the newest entries and
lists them, with a group's entries under it and anything since undone
greyed. When a line is chosen it finds where the stack is now with
``whereNow()`` and, if the file is there, asks for it to be shown through
``Desktopwidget::showStack()``, which goes to the folder, selects the
stack and turns it to the page, leaving the keyboard with the window.
``Desktopmodel::activityRecorded()`` tells it when the record has grown.

For a folder on a server, ``Mainwindow::showActivityFor()`` gives the
window a function which fetches the entries instead
(``Activitydialog::setServer()``), which is
``RemoteBackend::fetchActivity()``. The answer comes later, and only the
answer to the latest request is shown. ``RemoteBackend::recordGrown()``
says when the server has announced a change, this client's own included,
by which time it is in the record. A stack on a server is not a file
here to look for, so the window trusts ``whereNow()`` and the folder
shown says if the stack is not there.

**Put back** works out, for the line chosen or each line under it which
renamed or moved a stack, where the stack is now (``whereNow()``) and
where it was: the entry's ``from``, in the folder as that is now
(``ActivityLog::folderNow()``). ``Desktopwidget::putBack()`` then renames
and moves each stack through the model, within one undo macro, so it is
recorded as a group like anything else and can be undone.

A line which deleted a stack is put back from the trash, which is the
folder ``.maxview-trash`` at the top of the repository, where the stack
has the name the entry kept. ``Desktopmodel::restoreStack()`` moves it
to the folder it was in, as that folder is now, with an undo command of
its own (``UCRestoreStack``) whose undo deletes it again. That is
recorded as a ``restore``, and the window shows the deletion as undone
while it stands. An entry written before ``kept`` existed is matched
with a stack of the same name in the trash only if it arrived there
within a few seconds of the entry, which Windows does not tell, so
there such an entry is not matched.
