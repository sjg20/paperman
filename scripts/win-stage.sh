#!/bin/bash
# Gather everything paperman needs to run on Windows into one directory,
# ready to be handed to the installer or copied to a machine as it is.
#
# Run it in an MSYS2 MINGW64 shell once paperman.exe has been built:
#
#    scripts/win-stage.sh [DIR]
#
# Qt's own files come from windeployqt, which reads the executable and
# brings in the libraries, plugins and compiler runtime it uses. Nothing
# brings in the rest - poppler, podofo, tiff and the others - so the
# libraries each binary asks for are followed here, and any that live
# with the toolchain are copied in beside it. A library from outside the
# toolchain is part of Windows and is already on the machine.
#
# The directory is laid out as the toolchain is, with the program and its
# libraries in bin, since some of them find their own files from where
# they are: OpenSSL looks for its modules in ../lib/ossl-modules

set -e

dir=${1:-dist/paperman}
prefix=${MINGW_PREFIX:-/mingw64}

if [ ! -f paperman.exe ]; then
   echo "no paperman.exe: build it first with" >&2
   echo "   qmake6 paperman.pro -o Makefile.win && make -f Makefile.win" >&2
   exit 1
fi

bin=$dir/bin

rm -rf "$dir"
mkdir -p "$bin"

# The build keeps its debug information, for reading a crash, which is
# almost all of the program: 121MB of 124MB. Ship it without; the one
# in the build directory keeps it
strip -o "$bin"/paperman.exe paperman.exe

# the libraries a binary asks for, by name
dll_names () {
   objdump -p "$1" 2>/dev/null | sed -n 's/^\s*DLL Name:\s*//p'
}

# follow what each binary asks for and copy in whatever the toolchain has
copy_deps () {
   local todo=("$@") seen=" " name path

   while [ ${#todo[@]} -gt 0 ]; do
      local next=()

      for path in "${todo[@]}"; do
         for name in $(dll_names "$path"); do
            case "$seen" in
               *" $name "*) continue ;;
            esac
            seen="$seen$name "
            if [ -f "$prefix/bin/$name" ]; then
               cp -u "$prefix/bin/$name" "$bin"/
               next+=("$prefix/bin/$name")
            fi
         done
      done
      todo=("${next[@]}")
   done
}

# Qt's libraries, plugins and compiler runtime, and the platform plugin
# which draws nothing, which --scan, -o, -q and the other modes without
# a window run on
deploy=$(command -v windeployqt6 || command -v windeployqt || true)
if [ -z "$deploy" ]; then
   echo "no windeployqt: install mingw-w64-x86_64-qt6-base" >&2
   exit 1
fi
"$deploy" --release --no-translations --no-system-d3d-compiler \
   --no-opengl-sw --compiler-runtime --include-plugins qoffscreen \
   "$bin/paperman.exe"

# PoDoFo loads OpenSSL's legacy provider as soon as it is itself loaded,
# and without it paperman does not start at all: Windows says that the
# application was unable to start correctly (0xc0000142)
if [ -d "$prefix/lib/ossl-modules" ]; then
   mkdir -p "$dir/lib/ossl-modules"
   cp "$prefix"/lib/ossl-modules/*.dll "$dir/lib/ossl-modules"/
fi

# everything else, including what the plugins themselves need
copy_deps "$bin"/paperman.exe $(find "$dir" -name '*.dll')

cat > "$dir"/README.txt <<'EOF'
Paperman - an electronic filing cabinet: scan, print, stack, arrange

Run bin\paperman.exe to start.

Scanners are reached through their TWAIN driver, so install the driver
which came with the scanner and paperman will offer it.

Two things are used if they are on the PATH and simply not offered if
they are not: tesseract, to read the text of a page, and exiftool, to
keep the details a JPEG carries when a page is changed.
EOF

# Check that it starts as it will on a machine without MSYS2, with only
# Windows to find libraries in, since a library missing from here, or one
# which cannot find its own files, is found in the toolchain otherwise
windir=$(cygpath -u "$SYSTEMROOT")
help=$(cd "$bin" && PATH="$windir/System32:$windir" ./paperman.exe -h 2>&1 \
       || true)
if ! grep -q -- "--server" <<< "$help"; then
   echo "the staged paperman.exe does not start:" >&2
   echo "$help" >&2
   exit 1
fi

# and that the modes without a window start, on the platform plugin which
# draws nothing: search an empty directory, which has no index. Without
# the plugin paperman shows an error box and waits, hence the timeout
empty=$(mktemp -d)
wait=$(command -v timeout)   # not the one in System32, which only waits
search=$(cd "$bin" && PATH="$windir/System32:$windir" "$wait" 60 \
         ./paperman.exe -q nothing "$(cygpath -w "$empty")" 2>&1 || true)
rmdir "$empty"
if ! grep -q "No search index" <<< "$search"; then
   echo "the staged paperman.exe does not start without a window:" >&2
   echo "$search" >&2
   exit 1
fi

echo "staged in $dir:"
du -sh "$dir"
ls "$dir" | head -20
