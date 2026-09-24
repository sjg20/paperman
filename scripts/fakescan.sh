#!/bin/bash
# Run a SANE front end with the fake Fujitsu scanner the tests use as
# its only scanner, driven by hand through a directory:
#
#    scripts/fakescan.sh DIR scanimage -d fakefujitsu:fi-8170:00001 --batch
#
# paperman does not need this: 'paperman --fake-scanner DIR' offers the
# fake scanner beside the real ones.
#
# Put pictures of sheets in DIR/hopper, and 'touch DIR/press-scan' to press
# the Scan button: see test/fakescan/fakescan.h. The scanner is built with
# paperman, on Linux, in test/fakescan under the build directory; set
# FAKESCAN_LIBDIR if that is not the source directory.

set -e

if [ $# -lt 2 ]; then
   echo "usage: $0 DIR COMMAND [ARGS...]" >&2
   exit 1
fi

dir=$(realpath "$1")
shift
libdir=$(realpath "${FAKESCAN_LIBDIR:-$(dirname "$0")/../test/fakescan}")

if [ ! -f "$libdir/libsane-fakefujitsu.so.1" ]; then
   echo "no fake scanner in $libdir: build paperman first" >&2
   exit 1
fi
mkdir -p "$dir/hopper"

# a configuration of libsane's own which lists only the fake scanner
conf=$(mktemp -d)
trap 'rm -rf "$conf"' EXIT
echo fakefujitsu > "$conf/dll.conf"

SANE_CONFIG_DIR=$conf \
LD_LIBRARY_PATH=$libdir${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH} \
FAKESCAN_DIR=$dir "$@"
