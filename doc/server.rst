Paperman Server
===============

``paperman-server`` serves paper repositories over the network: to the
desktop program on other computers, which can show, change and scan into
them, and to the Android and iPhone app. It can also read the text of
the pages (OCR) and let clients search it.

This page covers running a server and connecting to it. How it works
inside, and its API, are under Development (see :doc:`server-internals`
and :doc:`api`).

Building
--------

The server is not yet in the packages, so build it from the source with
Qt 5 or Qt 6:

.. code:: bash

   qmake6 paperman-server.pro -o server.mk
   make -f server.mk

This makes ``paperman-server``. To read pages it needs tesseract, as the
desktop program does (see :doc:`install`).

Running the Server
------------------

Give it the repository to serve:

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

For example, on port 9000, reading and indexing the text:

.. code:: bash

   ./paperman-server -p 9000 -i -r 4 /home/user/Documents/papers

To run it as a service, see :doc:`systemd`; to reach it from outside
over HTTPS, see :doc:`deployment`.

Reading Pages
~~~~~~~~~~~~~

With ``--read-pages`` the server reads the pages of every stack with
tesseract, as it has time, and keeps the words with the pages, so that
stacks can be found by what is written on them. New scans, stacks which
change and stacks which the desktop's "Read text (OCR)" asks for are
read first; otherwise the newest stacks are read first. The trash is
left alone, and pages which have been read are not read again, even
after a restart.

Reading takes a second or two a page for each CPU core it uses, so a
large repository takes some hours at first. ``n`` can be about the
number of cores the server can spare. With systemd, add the option to
the service:

.. code:: ini

   ExecStart=/opt/paperman/paperman-server -p 8081 -i -r 8 /srv/papers

Searching Text
~~~~~~~~~~~~~~

With ``--index`` the server keeps an index of the text on the pages, so
that the desktop's Edit > Search, "Text on the pages", and the app can
find a stack in its repositories by what is written on it. The text is
what was read from each page, by the desktop or the server, or a PDF's
own text. Building the index the first time takes a while for a large
repository, though it can be searched meanwhile; after that it keeps up
as stacks change.

Together with ``--read-pages``, this finds stacks which nobody has
opened.

Users
-----

A server without users lets anyone who can reach it read and change its
repository. Set up users before letting anything but the local machine
reach it, and run it behind a reverse proxy (nginx or Apache) with HTTPS
for use from outside (see :doc:`deployment`).

A demo for anyone to try
------------------------

With ``--demo``, the server lets anyone try Paperman on some papers of
their own. Logging in with a name and a password of your choosing starts
a session: a copy of the repository given, for that name and password.
Logging in with the same name and password again, as from a phone beside
the desktop program, joins the session, and each sees what the other
does. Anyone else has a session of their own, and can file, rename,
delete and scan as they like, without seeing anything of anyone else's.
The repository given is never changed. A session ends once nothing has
been connected to it for a while, and its copy is deleted.

.. code:: bash

   ./paperman-server -p 8095 --demo /srv/pmdemo/sessions /srv/pmdemo/Paperman

The folder given to ``--demo`` holds the sessions' copies: everything in
it is deleted when the server starts, so give it a folder of its own.
These set the limits:

-  ``--demo-password <pw>`` - let only this password log in (default:
   any)
-  ``--demo-sessions <n>`` - sessions at once (default: 100); a login
   beyond them is told to try again later
-  ``--demo-idle <min>`` - how long a session may be quiet (default: 60)
-  ``--demo-space <mb>`` - space for each session (default: 200); an
   upload beyond it is refused, and a session which grows to half as much
   again is ended

A demo listens on the loopback address only, and has no users of its own.
Put it behind a web server with HTTPS, which also limits how much each
client may send and how often (see :doc:`deployment`), and run it as a
user of its own, which can write only to the sessions' folder. It needs
Linux or another Unix.

The desktop program and the app reach it as they do any server, and ask
for a user and password the first time. Give each the same name and
password to work on the same papers from both.

Connecting Clients
------------------

Both the desktop program (``paperman``) and the command-line client
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

The name is a convenient label: it is shown in the desktop's directory
tree and can be given to ``--server`` in place of the full URL. When no
name is supplied the server's host is used.

Set ``PAPERMAN_CONFIG`` to use a different file, which is handy for
trying a throwaway configuration.

Adding a server
~~~~~~~~~~~~~~~

Edit the file by hand, or let the client add to it:

.. code:: bash

   paperman-client add-server http://localhost:8080
   paperman-client add-server https://paper.office.example.com Office

A server which is already listed is not added twice.

Listing servers
~~~~~~~~~~~~~~~

.. code:: bash

   paperman-client servers

The one the next command would talk to is marked with ``*``.

Choosing a server
~~~~~~~~~~~~~~~~~

``--server`` accepts a URL, the name of a configured server, or just the
host the server runs on:

.. code:: bash

   paperman-client --server Office ls repo
   paperman --server Office
   paperman --server ohau
   paperman --server ohau:9000

Given only a host, Paperman looks for a server there on port 8080 (the
default), then on port 8081 (as in the systemd example), then over
https, and uses the first it finds. With a port, it tries just that
port, over http and then https. If it finds no server it says where it
looked. To see which URL a host leads to:

.. code:: bash

   paperman-client probe ohau

``add-server`` takes a host in the same way and stores the URL found.

Without ``--server``, ``paperman-client`` uses ``$PAPERMAN_SERVER`` if
it is set, otherwise the first server in ``client.conf``, otherwise
``http://localhost:8080``.

The desktop program attaches *every* server listed in ``client.conf``
when it starts, so their repositories appear in the directory tree beside
the local ones. A server which cannot be reached is reported once and
the program carries on without it.

Logging in
~~~~~~~~~~

Log in once to each server which has users, and the login is kept for
both the desktop program and ``paperman-client``:

.. code:: bash

   paperman-client --server Office login simon

The desktop program also asks for a user and password itself when it
attaches to a server which needs them. ``client.conf`` holds no
passwords.

Troubleshooting
---------------

**The server won't start:** check whether the port is already in use
(``ss -ltn | grep 8080``), and that the repository exists and the
server can read it.

**A client cannot connect:** check that the server is running, that a
firewall is not in the way, and that the client has the right host and
port.

**The desktop says the server is too old or too new:** the desktop
needs paperman-server 1.4.0 or later, and a server newer than the
desktop may need the desktop updating.

**Text search finds nothing:** the server must be run with ``--index``,
and the pages must have been read, by the desktop or by a server run
with ``--read-pages``.

License
-------

GPL-2 (same as Paperman)

Copyright (C) 2009 Simon Glass
