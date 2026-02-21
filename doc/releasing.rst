Releasing
=========

Paperman uses a GitHub Actions workflow to automate releases.  Pushing a
version tag triggers two parallel jobs: one builds a ``.deb`` package and
publishes a GitHub Release, the other signs and uploads source packages to
the Launchpad PPA.

Creating a Release
------------------

1. Update ``debian/changelog.in`` with the new version and changes.

2. Commit and push the changelog update.

3. Tag the release and push the tag:

   .. code:: bash

      git tag v1.3.3
      git push origin v1.3.3

The workflow (``.github/workflows/release.yml``) runs automatically on any
tag matching ``v*``.

What the Workflow Does
----------------------

**build-deb** — Build ``.deb`` and create GitHub Release
   Installs Qt5/C++ build dependencies and Debian packaging tools, generates
   ``debian/changelog``, ``control`` and ``rules`` from the ``.in`` templates,
   builds a binary ``.deb`` with ``dpkg-buildpackage``, then creates a GitHub
   Release with the ``.deb`` attached.

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

Manual PPA Upload
-----------------

To upload to the PPA without the CI workflow, see :doc:`ppa`.
