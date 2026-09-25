Releasing
=========

Paperman uses a GitHub Actions workflow to automate releases.  Pushing a
version tag triggers three jobs: one builds a ``.deb`` package and
publishes a GitHub Release, one builds the Windows installer and adds it
to that release, and one signs and uploads source packages to the
Launchpad PPA.

Release Checklist
-----------------

1. Update the version and changelog entry at the top of
   ``debian/changelog.in``.  The first line sets the version used for
   tagging, packaging and uploading — everything else is derived from it.

2. Commit and push the changelog update.

3. Build ``.deb`` packages locally for all target distros::

      scripts/do-build

4. Create the tag, push it and upload the ``.deb`` files::

      make release

   This:

   - extracts the version from ``debian/changelog.in`` (e.g. ``1.3.3-1``
     → tag ``v1.3.3``)
   - checks that the tag does not already exist
   - checks that ``.deb`` files are present in ``../release/all/``
   - creates and pushes the tag, triggering the GitHub Actions workflow
   - waits for the workflow to create the GitHub Release
   - uploads all locally-built ``.deb`` packages to the release

   If you need to re-upload ``.deb`` files to an existing release (e.g.
   after rebuilding)::

      make release-upload

What the Workflow Does
----------------------

**build-deb** — Build ``.deb`` and create GitHub Release
   Installs Qt5/C++ build dependencies and Debian packaging tools, generates
   ``debian/changelog``, ``control`` and ``rules`` from the ``.in`` templates,
   builds a binary ``.deb`` with ``dpkg-buildpackage``, then creates a GitHub
   Release with the ``.deb`` attached.

**windows-installer** — Build the Windows installer
   Builds the app in an MSYS2 MinGW64 shell, runs
   ``scripts/win-installer.sh`` to gather it together with the libraries
   it needs and build ``packaging/windows/paperman.iss`` with Inno Setup,
   and adds the
   resulting ``paperman-setup-VERSION.exe`` to the release.  It waits for
   **build-deb**, which is what creates the release to add it to.

**ppa-upload** — Sign and upload to Launchpad PPA
   Imports the GPG signing key from repository secrets, configures
   ``gpg-agent`` for non-interactive signing and sets up ``dput``, then runs
   ``scripts/ppa-upload`` which handles building, signing and uploading
   source packages for all target distributions.

Required Secrets
----------------

The PPA job needs two secrets configured in the GitHub repository settings
(Settings > Secrets and variables > Actions):

``GPG_PRIVATE_KEY``
   The ASCII-armoured GPG private key.  Export it with:

   .. code:: bash

      gpg --armor --export-secret-keys <KEY_ID>

``GPG_PASSPHRASE``
   The passphrase for the GPG key.

Without these secrets the PPA job fails, but the ``.deb`` / GitHub Release
job still succeeds independently.

Building the Windows Installer by Hand
--------------------------------------

Every push builds the installer and keeps it as a CI artifact, so there is
usually no need to build one locally.  To do it anyway, in an MSYS2
MINGW64 shell with the build dependencies the ``windows`` CI job lists,
and with `Inno Setup <https://jrsoftware.org/isinfo.php>`_ installed
(``winget install JRSoftware.InnoSetup``)::

   qmake6 paperman.pro -o Makefile.win
   make -f Makefile.win -j$(nproc) installer

That leaves ``packaging/windows/paperman-setup-VERSION.exe``.  It must be
a build without ``CONFIG+=test``, which makes a console program for the
tests.  On the way it gathers the program and every library it needs in
``dist/paperman``, which can also be copied to another machine and run as
it is; ``scripts/win-stage.sh`` does just that part.

The installer asks for no more rights than the user has, so it installs
into the user's own programs folder without an administrator; there is a
choice at the start for installing it for everyone on the machine.
Scanners are reached through their TWAIN driver rather than through SANE,
so nothing scanner-related is bundled.

Manual PPA Upload
-----------------

To upload to the PPA without the CI workflow, see :doc:`ppa`.
