Releasing
=========

Paperman uses a GitHub Actions workflow to automate releases.  Pushing a
version tag triggers the jobs below: one builds a ``.deb`` package and
publishes a GitHub Release, others build the Windows installers, the
macOS disk images, the Linux AppImages and the snaps and add them to that
release, and one signs and uploads source packages to the Launchpad PPA.
The snaps also go to the Snap Store.

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
   Installs Qt 6 build dependencies and Debian packaging tools, generates
   ``debian/changelog``, ``control`` and ``rules`` from the ``.in`` templates,
   builds a binary ``.deb`` with ``dpkg-buildpackage``, then creates a GitHub
   Release with the ``.deb`` attached.

**windows-installer** — Build the Windows installers
   Builds the app in an MSYS2 shell, runs ``scripts/win-stage.sh`` to
   gather it together with the libraries it needs, builds
   ``packaging/windows/paperman.iss`` with Inno Setup and adds the
   resulting installer to the release.  It runs twice: in MinGW64 for
   ``paperman-setup-VERSION.exe``, and natively on a Windows on Arm
   runner in CLANGARM64 for ``paperman-setup-VERSION-arm64.exe``.  The
   x64 installer also installs on Windows on Arm, but runs more slowly
   there, under emulation.  Once SignPath is set up it signs the program
   and then the installer, each waiting for an approver; see
   :doc:`code-signing`.  It waits for **build-deb**, which is what
   creates the release to add it to.

**macos-package** — Build the macOS disk images
   Builds Poppler with its Qt bindings, SANE from the fork's
   ``all-work`` branch and the app, on a runner for each kind of Mac, and
   runs ``scripts/mac-package.sh`` to make ``Paperman-VERSION-arm64.dmg``
   and ``Paperman-VERSION-x86_64.dmg``, which it adds to the release.
   With the secrets below it signs them with the Developer ID and has
   Apple notarise them; otherwise they are signed ad hoc.

**appimage** — Build the Linux AppImages
   Builds the app with Qt 6 in a Debian 12 container, the oldest Linux the
   AppImage is to run on, and runs ``scripts/appimage.sh`` to make
   ``Paperman-VERSION-x86_64.AppImage`` and
   ``Paperman-VERSION-aarch64.AppImage``, on an x86 and an Arm runner,
   which it adds to the release.

**snap** — Build the snaps and publish them
   Builds the snap from ``snap/snapcraft.yaml`` with snapcraft, on an x86
   and an Arm runner, and adds ``paperman_VERSION_amd64.snap`` and
   ``paperman_VERSION_arm64.snap`` to the release. With the secret below
   it also releases them to the ``stable`` channel of the Snap Store.

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

The macOS job signs and notarises its disk images only when the
repository also has:

``MACOS_CERT_P12``
   A Developer ID Application certificate with its private key, exported
   from Keychain Access as a ``.p12`` and base64-encoded
   (``base64 -i cert.p12``)

``MACOS_CERT_PASSWORD``
   The password the ``.p12`` was exported with

``NOTARY_KEY``, ``NOTARY_KEY_ID``, ``NOTARY_ISSUER_ID``
   An App Store Connect API key with the Developer role: the ``.p8``'s
   contents, its key ID and the issuer ID

The CI workflow signs pushes to master the same way, but not pull
requests. To sign on a Mac by hand, set ``MACOS_SIGN_IDENTITY`` to the
identity and ``NOTARY_PROFILE`` to a profile stored with ``xcrun
notarytool store-credentials`` before running ``scripts/mac-package.sh``.

The snap job publishes to the Snap Store only when the repository has:

``SNAPCRAFT_STORE_CREDENTIALS``
   Credentials for the account which has registered the name
   ``paperman`` (``snapcraft register paperman``), made with:

   .. code:: bash

      snapcraft export-login --snaps=paperman \
         --acls package_access,package_push,package_update,package_release \
         credentials.txt

   and pasted in from ``credentials.txt``. Until the store has reviewed
   it, the snap's ``raw-usb`` connection must be made by hand; ask for it
   to be connected automatically on the Snapcraft forum, since it is how a
   scanning program reaches its scanner.

Building the Windows Installer by Hand
--------------------------------------

Every push builds the installer and keeps it as a CI artifact, so there is
usually no need to build one locally.  To do it anyway, in an MSYS2
MINGW64 shell with the build dependencies the ``windows`` CI job lists,
and with `Inno Setup <https://jrsoftware.org/isinfo.php>`_ installed
(``winget install JRSoftware.InnoSetup``)::

   qmake6 paperman.pro -o Makefile.win
   make -f Makefile.win -j$(nproc) installer

That leaves ``packaging/windows/paperman-setup-VERSION.exe``, or
``paperman-setup-VERSION-arm64.exe`` when built in a CLANGARM64 shell on
Windows on Arm.  It must be
a build without ``CONFIG+=test``, which makes a console program for the
tests.  On the way it gathers the program and every library it needs in
``dist/paperman``, which can also be copied to another machine and run as
it is, as ``bin\paperman.exe``; ``scripts/win-stage.sh`` does just that
part.  It is laid out as MSYS2 is, with the program and its libraries in
``bin``, since OpenSSL finds its modules from where it is, in
``lib/ossl-modules``; PoDoFo loads one of those as it starts, and
without it paperman does not start at all.

The installer asks for no more rights than the user has, so it installs
into the user's own programs folder without an administrator; there is a
choice at the start for installing it for everyone on the machine.
Scanners are reached through their TWAIN driver rather than through SANE,
so nothing scanner-related is bundled.

Manual PPA Upload
-----------------

To upload to the PPA without the CI workflow, see :doc:`ppa`.
