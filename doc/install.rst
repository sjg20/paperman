Installation
============

Ubuntu PPA
----------

On Ubuntu, the easiest way to install Paperman is from the PPA:

.. code:: bash

   sudo add-apt-repository ppa:sjg1/ppa
   sudo apt update
   sudo apt install paperman

This provides pre-built packages for the following Ubuntu releases:

- Bionic (18.04 LTS)
- Focal (20.04 LTS)
- Jammy (22.04 LTS)
- Noble (24.04 LTS)
- Questing (25.10)

Pre-built .deb Packages
-----------------------

Pre-built ``.deb`` packages for a range of Debian and Ubuntu releases are
available from the `latest GitHub Release
<https://github.com/sjg20/paperman/releases/latest>`_.

Windows
-------

Paperman runs on Windows 10 (version 1809 or later) and Windows 11.
Releases from the next one on have two installers:

``paperman-setup-VERSION.exe``
   for ordinary (x64) PCs

``paperman-setup-VERSION-arm64.exe``
   for Windows on Arm; the x64 installer works there too, more slowly

The installer needs no administrator: it puts Paperman in your own
programs folder, with a Start menu entry and, if you like, a desktop
shortcut and ``.max`` files opening in it. It can be installed for
everyone on the machine instead from the choice at the start. Every
change to the code also has installers built for it, as artifacts of its
`CI run <https://github.com/sjg20/paperman/actions/workflows/ci.yml>`_ on
GitHub, which is where to get one until the next release, or to try
something before it is released.

Until the releases are signed, Windows warns that it protected the PC
from an unrecognised app: choose **More info** and then **Run anyway**.

To scan, install the scanner's 64-bit TWAIN driver; see :doc:`scanning`.

macOS
-----

There is no package for macOS yet. Paperman builds and runs there, on
Intel and Apple Silicon Macs, from source: see :doc:`build`.

Building from Source
--------------------

For other distributions or to get the latest development version, see
:doc:`build`.

The First Run
-------------

Paperman keeps papers in folders it calls repositories. On a first run
there are none, so it asks whether to keep them in ``Paperman`` in your
Documents folder, or another folder of your choosing. More can be added
at any time with **File > Add repository**.
