Code Signing Policy
===================

Free code signing provided by `SignPath.io <https://about.signpath.io>`_,
certificate by `SignPath Foundation <https://signpath.org>`_.

The Windows releases of paperman are signed, so that Windows can tell
that they come from this project and have not been changed since.  What
is signed is what this project builds from its own source: the program,
``paperman.exe``, and the installer which carries it,
``paperman-setup-VERSION.exe`` (and ``paperman-setup-VERSION-arm64.exe``
for Windows on Arm).  The libraries the installer also carries, such as
Qt, Poppler and PoDoFo, come from other projects through MSYS2 and are
not signed by this one.

How a Release is Signed
-----------------------

Nothing is signed by hand.  Pushing a version tag runs the release
workflow (``.github/workflows/release.yml``) on GitHub's own runners,
which builds paperman from the source at that tag and then:

1. sends ``paperman.exe`` to SignPath to be signed
2. builds the installer around the signed program
3. sends the installer to SignPath to be signed
4. adds the signed installer to the GitHub release

SignPath signs only a program and an installer which say they are
Paperman, of the version being released, and only when one of the
approvers below has approved that request.  A build from any other
branch or workflow, including the one on every push, is not signed.

Roles
-----

Committers and reviewers
   `Simon Glass <https://github.com/sjg20>`_

Approvers
   `Simon Glass <https://github.com/sjg20>`_

Changes from anyone else come as pull requests, which a committer
reviews before they are merged.  Everyone in these roles uses
multi-factor authentication for GitHub and for SignPath.

Privacy
-------

This program will not transfer any information to other networked
systems unless specifically requested by the user or the person
installing or operating it.  Paperman only connects to a paperman
server which its user attaches, and only sends mail or opens a web page
when its user asks it to.

Setting it Up
-------------

The release workflow signs only once the repository has, under
Settings > Secrets and variables > Actions:

``SIGNPATH_API_TOKEN`` (secret)
   an API token for a SignPath user who may submit signing requests

``SIGNPATH_ORGANIZATION_ID`` (variable)
   the SignPath organisation's ID

and, if they differ from ``paperman`` and ``release-signing``, the
variables ``SIGNPATH_PROJECT_SLUG`` and ``SIGNPATH_SIGNING_POLICY``.
Until then the installer is built and released unsigned.

The SignPath project needs the GitHub.com trusted build system and two
artifact configurations, whose XML is kept in
``packaging/windows/signpath``: ``paperman-exe`` for the program and
``installer`` for the installer.
