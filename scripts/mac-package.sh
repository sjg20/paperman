#!/bin/bash
# Build a Mac package, dist/Paperman-VERSION-ARCH.dmg, holding a
# Paperman.app which needs nothing installed on the machine
#
# Run it once paperman.app has been built without CONFIG+=test:
#
#    SANE_PREFIX=~/sane scripts/mac-package.sh
#
# Qt's own frameworks and plugins come from macdeployqt. The scanner back
# ends are loaded by libsane as the program runs, so nothing finds them
# from the program: those in SANE_PREFIX, where SANE was installed, are
# copied into Contents/PlugIns/sane with their configuration in
# Contents/Resources/sane.d, which paperman points libsane at when it
# runs from a bundle. Then every library anything in the bundle asks for,
# from outside macOS itself, is copied into Contents/Frameworks and asked
# for there instead, and the whole is signed ad hoc, which is all Apple
# Silicon needs to run it.
#
# To sign it for others to open without a warning, set MACOS_SIGN_IDENTITY
# to a Developer ID Application identity in the keychain. Then to have
# Apple notarise it, set NOTARY_PROFILE to a profile stored with
# 'xcrun notarytool store-credentials', or NOTARY_KEY_FILE, NOTARY_KEY_ID
# and NOTARY_ISSUER_ID to an App Store Connect API key.

set -e

sane=${SANE_PREFIX:?set SANE_PREFIX to where SANE is installed}
backends=${SANE_BACKENDS:-fujitsu finet}
version=$(sed -n 's/.*CONFIG_version_str "\(.*\)".*/\1/p' config.h)
arch=$(uname -m)
dist=dist
app=$dist/Paperman.app
fw=$app/Contents/Frameworks

if [ ! -d paperman.app ]; then
   echo "no paperman.app: build it first, without CONFIG+=test" >&2
   exit 1
fi

qmake=${QMAKE:-$(command -v qmake6 || command -v qmake)}
bins=$("$qmake" -query QT_INSTALL_BINS)
plugins=$("$qmake" -query QT_INSTALL_PLUGINS)

rm -rf "$dist"
mkdir -p "$dist"
cp -R paperman.app "$app"

# the name and version macOS shows, which qmake leaves out
plist=$app/Contents/Info.plist
for key in CFBundleShortVersionString CFBundleVersion; do
   /usr/libexec/PlistBuddy -c "Delete :$key" "$plist" 2>/dev/null || true
   /usr/libexec/PlistBuddy -c "Add :$key string $version" "$plist"
done
for key in CFBundleName CFBundleDisplayName; do
   /usr/libexec/PlistBuddy -c "Delete :$key" "$plist" 2>/dev/null || true
   /usr/libexec/PlistBuddy -c "Add :$key string Paperman" "$plist"
done

# where libraries named by @rpath may be found: Homebrew's lib holds a
# link to everything it has installed
libdirs=("$sane/lib" $(pkg-config --variable=libdir poppler-qt6 libpodofo \
                       libtiff-4 libjpeg 2>/dev/null | sort -u))
if command -v brew >/dev/null; then
   libdirs+=("$(brew --prefix)/lib")
fi

args=()
for d in "${libdirs[@]}"; do
   args+=("-libpath=$d")
done
"$bins/macdeployqt" "$app" "${args[@]}"

# paperman keeps its search index in SQLite, and the other drivers ask
# for client libraries which are not there
find "$app/Contents/PlugIns/sqldrivers" -type f ! -name 'libqsqlite*' \
   -delete

# the platform plugin which draws nothing, which --scan, -o, -q and the
# other modes without a window run on
cp "$plugins/platforms/libqoffscreen.dylib" "$app/Contents/PlugIns/platforms/"

# the scanner back ends and their configuration, listing only these
mkdir -p "$app/Contents/PlugIns/sane" "$app/Contents/Resources/sane.d"
: > "$app/Contents/Resources/sane.d/dll.conf"
for be in $backends; do
   cp "$sane/lib/sane/libsane-$be.1.so" "$app/Contents/PlugIns/sane/"
   [ -f "$sane/etc/sane.d/$be.conf" ] &&
      cp "$sane/etc/sane.d/$be.conf" "$app/Contents/Resources/sane.d/"
   echo "$be" >> "$app/Contents/Resources/sane.d/dll.conf"
done

# every Mach-O file in the bundle
machos () {
   find "$app/Contents" -type f | while read -r f; do
      file -b "$f" | grep -q Mach-O && echo "$f"
   done
}

# the libraries a file asks for, leaving out its own name
deps () {
   otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -vxF "$(otool -D "$1" | tail -n +2)"
}

# find a library named by @rpath in the places it may be
find_lib () {
   for d in "${libdirs[@]}"; do
      [ -f "$d/$1" ] && { echo "$d/$1"; return; }
   done
   return 0    # not found: the caller says so
}

# Copy in what is asked for from outside macOS until nothing more is, and
# ask for it in the bundle. Qt's frameworks are macdeployqt's already
changed=1
while [ $changed = 1 ]; do
   changed=0
   for f in $(machos); do
      for dep in $(deps "$f"); do
         case "$dep" in
            /System/*|/usr/lib/*|@executable_path/*|@loader_path/*) continue ;;
            *.framework/*) continue ;;
         esac
         name=$(basename "$dep")
         src=$dep
         case "$dep" in
            @rpath/*) src=$(find_lib "$name") ;;
         esac
         if [ ! -f "$fw/$name" ]; then
            if [ -z "$src" ] || [ ! -f "$src" ]; then
               echo "cannot find $dep, asked for by $f" >&2
               exit 1
            fi
            cp "$src" "$fw/$name"
            chmod u+w "$fw/$name"
            install_name_tool -id "@executable_path/../Frameworks/$name" \
               "$fw/$name" 2>/dev/null
            changed=1
         fi
         install_name_tool -change "$dep" \
            "@executable_path/../Frameworks/$name" "$f" 2>/dev/null
      done
   done
done

# Changing a library spoils its signature, so sign everything again: what
# is inside first, then the bundle, which signs the program. The program
# cannot be signed on its own while what it holds is not. A Developer ID
# signature is made with the hardened runtime, which notarising needs
sign=(-s -)
if [ -n "$MACOS_SIGN_IDENTITY" ]; then
   sign=(-s "$MACOS_SIGN_IDENTITY" --options runtime --timestamp)
fi
for f in $(machos); do
   case "$f" in
      "$app/Contents/MacOS/"*) ;;
      *) codesign --force "${sign[@]}" "$f" ;;
   esac
done
codesign --force "${sign[@]}" "$app"
codesign --verify --deep --strict "$app"

# Check that nothing is still asked for from outside the bundle and macOS,
# and that it starts without anything else to hand: a library missing
# from the bundle is found in Homebrew otherwise, which a user's Mac
# does not have
bad=0
for f in $(machos); do
   for dep in $(deps "$f"); do
      case "$dep" in
         /System/*|/usr/lib/*|@executable_path/*|@loader_path/*|@rpath/*) ;;
         *) echo "$f asks for $dep" >&2; bad=1 ;;
      esac
   done
done
[ $bad = 0 ] || exit 1

# run something for a minute at most: macOS has no timeout(1)
limit () {
   perl -e 'alarm shift; exec @ARGV or die "$ARGV[0]: $!"' 60 "$@"
}

exe=$app/Contents/MacOS/paperman
empty=$(mktemp -d)
echo "checking that the packaged paperman starts"
search=$(limit env -i HOME="$empty" PATH=/usr/bin:/bin \
         QT_QPA_PLATFORM=offscreen "$exe" -q nothing "$empty" 2>&1 || true)
if ! grep -q "No search index" <<< "$search"; then
   echo "the packaged paperman does not start:" >&2
   echo "$search" >&2
   exit 1
fi

# and that libsane loads the back ends from the bundle: asked to open a
# scanner on one, it loads that back end to look
mkdir -p "$empty/inbox"
for be in $backends; do
   echo "checking that it loads the $be back end from the bundle"
   scan=$(limit env -i HOME="$empty" PATH=/usr/bin:/bin \
          QT_QPA_PLATFORM=offscreen SANE_DEBUG_DLL=4 "$exe" --scan \
          --repo "$empty" --dir inbox --device "$be:nosuch" --pages 1 2>&1 \
          || true)
   if ! grep -q "backend \`$be' is version" <<< "$scan"; then
      echo "the packaged paperman does not load the $be back end:" >&2
      grep "\[dll\]" <<< "$scan" | tail -20 >&2
      exit 1
   fi
done
rm -rf "$empty"

# the disk image, with a link to Applications to drag it to
mkdir "$dist/dmg"
cp -R "$app" "$dist/dmg/"
ln -s /Applications "$dist/dmg/Applications"
dmg=$dist/Paperman-$version-$arch.dmg
hdiutil create -quiet -volname Paperman -srcfolder "$dist/dmg" -ov \
   -format UDZO "$dmg"
rm -rf "$dist/dmg"

# Have Apple notarise it, and staple the ticket to the image so that it
# opens without a warning even offline
notary=()
if [ -n "$NOTARY_PROFILE" ]; then
   notary=(--keychain-profile "$NOTARY_PROFILE")
elif [ -n "$NOTARY_KEY_FILE" ]; then
   notary=(--key "$NOTARY_KEY_FILE" --key-id "$NOTARY_KEY_ID" \
           --issuer "$NOTARY_ISSUER_ID")
fi
if [ -n "$MACOS_SIGN_IDENTITY" ]; then
   codesign --force -s "$MACOS_SIGN_IDENTITY" --timestamp "$dmg"
   if [ ${#notary[@]} -gt 0 ]; then
      out=$(xcrun notarytool submit "$dmg" "${notary[@]}" --wait 2>&1) || true
      echo "$out"
      # it can finish without failing when Apple has turned it down
      if ! grep -q "status: Accepted" <<< "$out"; then
         id=$(sed -n 's/^ *id: //p' <<< "$out" | head -1)
         [ -n "$id" ] && xcrun notarytool log "$id" "${notary[@]}" >&2
         echo "Apple did not notarise $dmg" >&2
         exit 1
      fi
      xcrun stapler staple "$dmg"
      spctl --assess --type open --context context:primary-signature -v "$dmg"
   fi
fi

echo "built $dmg"
du -sh "$app" "$dmg"
