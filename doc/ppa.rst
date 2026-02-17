PPA (Personal Package Archive)
==============================

Paperman can be published to a `Launchpad PPA
<https://help.launchpad.net/Packaging/PPA>`_ so that users can install it
directly with ``apt``.

One-time Setup
--------------

These steps only need to be done once per Launchpad account.

1. **Register your GPG key** with the Ubuntu keyserver:

   .. code:: bash

      gpg --keyserver keyserver.ubuntu.com --send-keys <YOUR_KEY_FINGERPRINT>

2. **Import the key on Launchpad**.  Go to
   https://launchpad.net/~/+editpgpkeys, paste your key fingerprint, click
   "Import Key" and follow the email-verification steps.

3. **Create the PPA**.  Go to
   ``https://launchpad.net/~<USER>/+activate-ppa`` and fill in a name (e.g.
   ``ppa``), display name and description.  Click "Activate".

4. **Configure dput**.  Create or update ``~/.dput.cf``:

   .. code:: ini

      [ppa]
      fqdn = ppa.launchpad.net
      method = ftp
      incoming = ~<USER>/ubuntu/ppa/
      login = anonymous
      allow_unsigned_uploads = 0

5. **Set the GPG key** in ``scripts/ppa-upload``.  Edit the ``GPG_KEY``
   variable near the top of the script to match your key fingerprint.

Uploading
---------

Run from the project source directory (where ``debian/changelog.in`` lives):

.. code:: bash

   # Dry run — build source packages without uploading
   scripts/ppa-upload --dry-run

   # Upload for a single distro
   scripts/ppa-upload noble

   # Upload for all configured distros
   scripts/ppa-upload

The script:

- auto-detects the project name and version from ``debian/changelog.in`` and
  ``debian/control.in``
- creates a clean source tarball using ``git archive``
- builds source packages for each target distro in parallel
- signs each package sequentially (GPG agent cannot handle parallel signing)
- uploads to the PPA with ``dput``

Use ``--dry-run`` to test the build without uploading.  Individual distro
names can be passed as arguments to upload a subset.

Checking Build Status
---------------------

After uploading, check the build status on Launchpad at
``https://launchpad.net/~<USER>/+archive/ubuntu/ppa``.

Users install with:

.. code:: bash

   sudo add-apt-repository ppa:<USER>/ppa
   sudo apt update
   sudo apt install paperman

Target Distributions
--------------------

The default target distributions are configured in ``scripts/ppa-upload``
(the ``ALL_DISTROS`` variable).  Distro-specific ``debian/control`` and
``debian/rules`` files are selected automatically if present (e.g.
``debian/control.bionic``), otherwise the ``.in`` templates are used.
