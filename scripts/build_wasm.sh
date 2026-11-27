#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Build the desktop to run in a web browser (WebAssembly)
#
# See doc/web.rst. This needs the Emscripten SDK set up in the shell
# (source emsdk_env.sh), at the version which the Qt in use asks for, and
# Qt for WebAssembly, multi-threaded, with its host tools.
#
# Usage: scripts/build_wasm.sh -q qtdir [-s sysroot] [-t tiff-source] outdir
#   -q qtdir        Qt for WebAssembly, such as .../6.10.3/wasm_multithread
#   -s sysroot      where libtiff for the browser is, or is put if it is
#                   not there (default <outdir>/sysroot)
#   -t tiff-source  libtiff's source, needed only to build it
#   outdir          where to build; the files to serve end up in
#                   <outdir>/web
set -e

qt=
sysroot=
tiff=

while getopts "q:s:t:h" opt; do
	case $opt in
	q) qt=$OPTARG ;;
	s) sysroot=$OPTARG ;;
	t) tiff=$OPTARG ;;
	*) sed -n '3,17s/^# \?//p' "$0"; exit 1 ;;
	esac
done
shift $((OPTIND - 1))
out=$1

if [ -z "$qt" ] || [ -z "$out" ]; then
	sed -n '3,17s/^# \?//p' "$0"
	exit 1
fi
if ! command -v emcc >/dev/null; then
	echo "emcc is not on the PATH: source emsdk_env.sh first" >&2
	exit 1
fi

src=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$out"
out=$(cd "$out" && pwd)
sysroot=${sysroot:-$out/sysroot}

# .max pages are kept with libtiff's fax coding, so it is needed, with
# only the codecs it has built in
if [ ! -f "$sysroot/lib/libtiff.a" ]; then
	if [ -z "$tiff" ] || [ ! -f "$tiff/CMakeLists.txt" ]; then
		echo "libtiff is not in $sysroot: give its source with -t" >&2
		exit 1
	fi
	mkdir -p "$out/tiff"
	(cd "$out/tiff" && emcmake cmake -G Ninja "$tiff" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$sysroot" \
		-DBUILD_SHARED_LIBS=OFF -DCMAKE_C_FLAGS=-pthread \
		-Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF \
		-Dtiff-docs=OFF -Djpeg=OFF -Dzlib=OFF -Dlzma=OFF -Dzstd=OFF \
		-Dwebp=OFF -Djbig=OFF -Dlerc=OFF -Dlibdeflate=OFF -Dcxx=OFF &&
		ninja install)
fi

mkdir -p "$out/build"
(cd "$out/build" && WASM_SYSROOT=$sysroot "$qt/bin/qmake" -o Makefile \
	"$src/paperman.pro" && make -j"$(nproc)")

# the files to serve, with compressed copies of the large ones, which
# paperman-server --web sends to a browser which takes them
mkdir -p "$out/web"
cp "$out/build/paperman.html" "$out/build/paperman.js" \
	"$out/build/paperman.wasm" "$out/build/qtloader.js" \
	"$out/build/qtlogo.svg" "$out/web/"
gzip -9 -k -f "$out/web/paperman.wasm" "$out/web/paperman.js"
echo "Built: serve it with paperman-server --web $out/web <repository>"
