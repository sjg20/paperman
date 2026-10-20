Troubleshooting
===============

Scanning
--------

**No scanner is listed.** Paperman uses the scanner drivers of the
platform it runs on, so the scanner must work there first:

- On Linux, check that ``scanimage -L`` lists it. The snap carries its
  own drivers, so use ``paperman.scanimage -L`` instead.
- On Windows, install the scanner's 64-bit TWAIN driver, then sign out
  and in again, as some drivers only make the scanner known at login.

See :doc:`install` and :doc:`scanning` for each platform, and for
giving a network scanner's address.

**The scanner stops answering after it has slept.** Use **Tools >
Reconnect Scanner**, which closes the connection to the scanner and
opens it again.

**"Sorry, you cannot do this while scanning".** While a scan is going
into a folder, the stacks there cannot be lined up with **Tools >
Arrange**. Wait for the scan to finish.

Finding
-------

**Searching for words finds nothing.** The text must be read from the
pages first (see :doc:`finding`):

- Check that ``tesseract`` is installed (see :doc:`install`).
- Check that **Read the text of scanned pages (OCR)** is ticked in
  **File > Options**.
- Stacks scanned before then need **Read text (OCR)**, in a stack's
  right-click menu.
- In a folder on a server, the server must be run with ``--index``, and
  with ``--read-pages`` to read the pages itself (see :doc:`server`).

**"Only Paperman's own (.max) stacks can keep the text read from their
pages".** A PDF or a JPEG cannot hold the text Paperman reads. Copy it
as a ``.max`` stack with **Duplicate... > as Max** (Ctrl+Shift+D), then
read that.

**A stack is missing.** It may be in another folder: search for its
name from the top of the repository (see :doc:`finding`). Also check
that the **Filter** box above the stack pane is empty.

Filing
------

**A stack was deleted by mistake.** Use **Edit > Undo** (Ctrl+Z). Deleted
stacks go to a folder named ``.maxview-trash`` at the top of the
repository, so one can be fetched back from there with your file manager
too.

**A folder cannot be deleted.** Paperman leaves deleting folders to your
file manager. Delete it there, then use **Refresh** in the folder's
right-click menu.

**Changes made outside Paperman do not show.** Use **Refresh**, or
**Refresh cache**, in the folder's right-click menu.

Servers
-------

**"Failed to connect to …" or "Some configured servers are
unavailable".** Paperman could not reach a server, which may be down,
or behind a firewall, or listed with the wrong address. Paperman carries
on without it. Use ``paperman-client probe`` with the server's host to
see where Paperman looks for it, and see the server's own
`Troubleshooting <server.html#troubleshooting>`_ section.

**The server status light is red.** The server has stopped answering, or
the news of changes made by others has stopped arriving. Click the light
for the details (see :doc:`network`).

Reporting a problem
-------------------

Report problems at https://github.com/sjg20/paperman/issues, saying
what you did, what you expected and what happened instead. A log helps:
start Paperman with ``--log`` and the name of a file, and it writes all
its messages there, with diagnostics which a normal run keeps quiet. For
a problem with a scanner, add ``--sane-debug 15`` to log every command
sent to the scanner and its answer:

.. code:: bash

   paperman --log paperman.log --sane-debug 15
