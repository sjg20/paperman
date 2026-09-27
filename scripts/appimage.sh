#!/bin/bash
# Build a Linux AppImage, dist/Paperman-VERSION-ARCH.AppImage, holding
# paperman and the libraries it needs, Qt among them
#
#    make appimage
#
# or scripts/appimage.sh, which is the same. It builds paperman with Qt 6
# in build-appimage, without the tests which a development build has, and
# then packs it. Set QMAKE to the qmake to build with if that is not
# qmake6, and JOBS to how many jobs make is to run, or set PAPERMAN to a
# paperman already built, without CONFIG+=test, to pack that instead.
#
# An AppImage runs on a Linux with the same C library as the one it was
# built on, or a later one, so build it on the oldest Linux it is to run
# on: the release builds it on Debian 12.
#
# SANE is the one library left out. The scanner back ends, their
# configuration and the rules which let a user open a scanner belong to
# the SANE installed on the machine, so paperman uses that SANE, as the
# .deb does. The same goes for tesseract, which paperman runs to read the
# text of a page.
#
# The tools are fetched into APPIMAGE_TOOLS (by default in ~/.cache) the
# first time.

set -e

cd "$(dirname "$0")/.."
version=$(sed -n 's/.*CONFIG_version_str "\(.*\)".*/\1/p' config.h)
arch=$(uname -m)
id=io.github.sjg20.paperman
dist=dist
appdir=$dist/AppDir
tools=${APPIMAGE_TOOLS:-${XDG_CACHE_HOME:-$HOME/.cache}/paperman-appimage}
export QMAKE=${QMAKE:-$(command -v qmake6 || command -v qmake)}

if [ -n "$PAPERMAN" ]; then
   bin=$PAPERMAN
else
   echo "building paperman in build-appimage"
   mkdir -p build-appimage
   (cd build-appimage && "$QMAKE" ../paperman.pro -o Makefile &&
    make -f Makefile -j"${JOBS:-$(nproc)}")
   bin=build-appimage/paperman
fi
if [ ! -x "$bin" ]; then
   echo "no $bin" >&2
   exit 1
fi

# the tests would be carried along, with QtTest
if grep -q TestQscanner "$bin"; then
   echo "$bin is built with CONFIG+=test: build it without" >&2
   exit 1
fi

# linuxdeploy copies in the libraries, and its Qt plugin Qt's plugins
mkdir -p "$tools"
fetch () {
   local file=$tools/$1-$arch.AppImage

   if [ ! -x "$file" ]; then
      echo "fetching $1"
      curl -fsSL -o "$file.part" \
         "https://github.com/linuxdeploy/$1/releases/download/continuous/$1-$arch.AppImage"
      chmod +x "$file.part"
      mv "$file.part" "$file"
   fi
}
fetch linuxdeploy
fetch linuxdeploy-plugin-qt

rm -rf "$appdir"
mkdir -p "$appdir"
install -D -m 755 "$bin" "$appdir/usr/bin/paperman"
install -D -m 644 packaging/linux/$id.desktop \
   "$appdir/usr/share/applications/$id.desktop"
install -D -m 644 packaging/linux/$id.metainfo.xml \
   "$appdir/usr/share/metainfo/$id.metainfo.xml"
install -D -m 644 \
   app/macos/Runner/Assets.xcassets/AppIcon.appiconset/app_icon_256.png \
   "$appdir/usr/share/icons/hicolor/256x256/apps/$id.png"

# the platform plugin which draws nothing, which --scan, -o, -q and the
# other modes without a window run on. QMAKE says which Qt to take the
# plugins from
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so

# and the GTK platform theme, which gives the colours of the desktop's
# theme, dark ones among them, on GNOME and the other desktops which use
# GTK: without it, paperman is light whatever the desktop, with the light
# icons. It brings GTK along
export DEPLOY_PLATFORM_THEMES=1
if [ ! -f "$("$QMAKE" -query QT_INSTALL_PLUGINS)/platformthemes/libqgtk3.so" ]; then
   echo "no GTK platform theme for Qt: install qt6-gtk-platformtheme" >&2
   exit 1
fi

# the tools are AppImages themselves, which need FUSE unless unpacked
# first, and a container has no FUSE
export APPIMAGE_EXTRACT_AND_RUN=1

# The libraries' licences go with them, in the copyright files of the
# packages they come from. linuxdeploy looks each library's package up
# with a dpkg-query of its own, which reads the whole package list every
# time and so takes minutes; one query for them all takes a second
export DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1

#
# \param $1  linuxdeploy's output, which names each library it copied
copyrights () {
   local paths=() pkg

   if ! command -v dpkg-query >/dev/null; then
      echo "no dpkg-query: leaving out the libraries' copyright files"
      return
   fi

   # a library found in /lib may be listed under /usr/lib, since one is a
   # link to the other, or be a link itself
   while read -r f; do
      paths+=("$f" "$(realpath "$f")")
      case "$f" in
         /lib/*) paths+=("/usr$f") ;;
         /usr/lib/*) paths+=("${f#/usr}") ;;
      esac
   done < <(sed -n 's|.*Deploying shared library \(/[^ ]*\).*|\1|p' "$1" |
            sort -u)

   # each line is 'pkg:arch, pkg:arch: /path', or a diversion
   dpkg-query -S "${paths[@]}" 2>/dev/null |
      sed -n 's|^\([^/]*\): /.*|\1|p' | tr ',' '\n' |
      sed 's/^ *//; s/:.*//' | sort -u |
      while read -r pkg; do
         if [ -f "/usr/share/doc/$pkg/copyright" ]; then
            install -D -m 644 "/usr/share/doc/$pkg/copyright" \
               "$appdir/usr/share/doc/$pkg/copyright"
         fi
      done
   echo "copied the copyright files of" \
      "$(ls "$appdir/usr/share/doc" | wc -l) packages"
}

deploy=("$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir"
   --desktop-file "$appdir/usr/share/applications/$id.desktop"
   --exclude-library 'libsane.so*')
log=$dist/linuxdeploy.log
set -o pipefail
"${deploy[@]}" --plugin qt 2>&1 | tee "$log"
set +o pipefail
copyrights "$log"

out=$dist/Paperman-$version-$arch.AppImage
export LDAI_OUTPUT=$out
rm -f "$out"
"${deploy[@]}" --output appimage

# Check that SANE is left to the machine, and that it starts without
# anything else to hand
if find "$appdir" -name 'libsane*' | grep -q .; then
   echo "SANE is in the AppImage, where it cannot reach the back ends" >&2
   exit 1
fi

run () {
   env -i HOME="$empty" PATH=/usr/bin:/bin APPIMAGE_EXTRACT_AND_RUN=1 \
      QT_QPA_PLATFORM=offscreen "$@"
}

empty=$(mktemp -d)
echo "checking that the AppImage starts"
search=$(run timeout 60 "$out" -q nothing "$empty" 2>&1 || true)
if ! grep -q "No search index" <<< "$search"; then
   echo "the AppImage does not start:" >&2
   echo "$search" >&2
   exit 1
fi

# and that the machine's SANE loads its back ends beside the libraries in
# the AppImage: asked to open a scanner on one, it loads that back end
echo "checking that it loads the machine's fujitsu back end"
mkdir -p "$empty/inbox"
scan=$(run SANE_DEBUG_DLL=4 timeout 60 "$out" --scan --repo "$empty" \
       --dir inbox --device fujitsu:nosuch --pages 1 2>&1 || true)
rm -rf "$empty"
if ! grep -q "backend \`fujitsu' is version" <<< "$scan"; then
   echo "the AppImage does not load the fujitsu back end:" >&2
   grep "\[dll\]" <<< "$scan" | tail -20 >&2
   exit 1
fi

echo "built $out"
du -sh "$out"
