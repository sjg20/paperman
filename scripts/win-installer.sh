#!/bin/bash
# Build the Windows installer, packaging/windows/paperman-setup-VERSION.exe
#
# Run it in an MSYS2 MINGW64 shell once paperman.exe has been built, or
# have make do both:
#
#    make -f Makefile.win installer
#
# It gathers what paperman needs with win-stage.sh and hands that to Inno
# Setup, which must be installed: 'winget install JRSoftware.InnoSetup'
# or 'choco install innosetup'. Set ISCC to its compiler if it is
# somewhere else.

set -e

stage=dist/paperman
version=$(sed -n 's/.*CONFIG_version_str "\(.*\)".*/\1/p' config.h)

# where Inno Setup goes, for everyone or for just the one user, with its
# major version in the name of the directory
iscc=${ISCC:-$(command -v iscc || true)}
if [ -z "$iscc" ]; then
   for try in "$(cygpath -F 42)"/Inno\ Setup\ */ISCC.exe \
              "$(cygpath -F 38)"/Inno\ Setup\ */ISCC.exe \
              "$(cygpath -u "$LOCALAPPDATA")"/Programs/Inno\ Setup\ */ISCC.exe; do
      if [ -f "$try" ]; then
         iscc=$try
         break
      fi
   done
fi
if [ -z "$iscc" ]; then
   echo "no Inno Setup: install it with 'winget install JRSoftware.InnoSetup'" >&2
   echo "or set ISCC to its ISCC.exe" >&2
   exit 1
fi

# the tests are built as a console program, so that their output is not
# lost, and that is not what anyone wants to install
if objdump -p paperman.exe | grep -q "Subsystem.*CUI"; then
   echo "paperman.exe is built for the tests: build it again without" >&2
   echo "CONFIG+=test" >&2
   exit 1
fi

scripts/win-stage.sh "$stage"

# Inno Setup is a Windows program, so give it a Windows path, and stop
# MSYS2 taking its /D switches for paths and turning them into others
MSYS2_ARG_CONV_EXCL='*' "$iscc" "/DStageDir=$(cygpath -w "$PWD/$stage")" "/DAppVersion=$version" \
   packaging/windows/paperman.iss

echo "built packaging/windows/paperman-setup-$version.exe"
