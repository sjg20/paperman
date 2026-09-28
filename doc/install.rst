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

AppImage
--------

For other Linux distributions, releases from the next one on have an
AppImage, which runs without being installed:

``Paperman-VERSION-x86_64.AppImage``
   for most PCs

``Paperman-VERSION-aarch64.AppImage``
   for 64-bit Arm machines

It needs a Linux at least as recent as Debian 12, Ubuntu 24.04 or
Fedora 37. Download it, make it executable and run it:

.. code:: bash

   chmod +x Paperman-*.AppImage
   ./Paperman-*.AppImage

The AppImage carries Qt and the other libraries Paperman uses, but not
SANE: it uses the scanner back ends installed on the machine, as other
scanning programs do, so install SANE from the distribution
(``libsane1`` on Debian and Ubuntu, ``sane-backends`` on Fedora and Arch)
and check that ``scanimage -L`` lists the scanner. For reading the text
of pages, install ``tesseract`` too. Until the next release, the
AppImages are artifacts of the latest `CI run
<https://github.com/sjg20/paperman/actions/workflows/ci.yml>`_ on master.

Snap
----

Paperman is also in the `Snap Store <https://snapcraft.io/paperman>`_,
from which it installs on most Linux distributions and keeps itself up
to date. Until a release puts it in the stable channel, take it from the
edge channel:

.. code:: bash

   sudo snap install paperman --edge
   sudo snap connect paperman:raw-usb
   sudo snap connect paperman:cups-control

Once it is in the stable channel it can also be installed from Ubuntu's
App Centre and the other software centres which offer snaps.

The second command lets it reach a USB scanner and the third lets it
print to the machine's printers: a snap is kept away from the machine's
devices until it is allowed to use them. Without the third, it can only
print to a PDF file.

A network scanner needs nothing more if it can be found by looking on
the network. If not, give its address to the scanner's back end in a
file of your own, which the snap reads before its own, in
``~/snap/paperman/common/sane.d``. For a Ricoh fi-series scanner, for
example, put its address in ``finet.conf`` there:

.. code:: bash

   mkdir -p ~/snap/paperman/common/sane.d
   echo 192.168.1.50 > ~/snap/paperman/common/sane.d/finet.conf

The snap carries its own SANE, with the scanner back ends from the same
fork as the macOS package, which know the newest Ricoh fi-series
scanners; it cannot use the SANE installed on the machine. To see what it
makes of the scanners, run ``paperman.scanimage -L``. The snap reaches
files in your home folder, and those on removable media once
``sudo snap connect paperman:removable-media`` is run.

Search server
-------------

``paperman-server`` serves repositories to the desktop program on other
machines and to the Android and iPhone app. It is built from source, on
the machine which holds the papers, alongside the desktop program; see
:doc:`server` to build and run it, :doc:`systemd` to run it as a service
and :doc:`deployment` to put it behind HTTPS so that it can be reached
from outside.

Android and iPhone app
----------------------

The app reaches papers through a search server, so set one up first.

Android
   The app for Android, ``app-release.apk``, is an artifact of each `CI
   run <https://github.com/sjg20/paperman/actions/workflows/ci.yml>`_
   (``paperman-apk``). Copy it to the phone and open it there, allowing
   it to be installed from that source when asked.

iPhone and iPad
   The app is built with Xcode on a Mac and installed on the device from
   there; see :doc:`app`.

In the app, enter the server's address, and a user and password if the
server asks for them. Without a server, the demo mode shows some sample
papers.

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

Releases from the next one on have a disk image for each kind of Mac:

``Paperman-VERSION-arm64.dmg``
   for Apple Silicon (M1 and later)

``Paperman-VERSION-x86_64.dmg``
   for Intel Macs

Open it and drag **Paperman** to **Applications**. Everything it needs is
inside, including the scanner back ends for the Ricoh fi-series, so there
is nothing else to install; other scanners SANE supports can be used by
building from source. Until the next release, the disk images are
artifacts of the latest `CI run
<https://github.com/sjg20/paperman/actions/workflows/ci.yml>`_ on master.

If macOS says it cannot verify the developer, open it with a right-click
and **Open** the first time, or allow it in **System Settings > Privacy &
Security**. Releases which are signed and notarised by Apple open without
this.

Paperman also builds from source on macOS: see :doc:`build`.

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
