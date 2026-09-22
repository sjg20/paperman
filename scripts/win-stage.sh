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

set -e

dir=${1:-dist/paperman}
prefix=${MINGW_PREFIX:-/mingw64}

if [ ! -f paperman.exe ]; then
   echo "no paperman.exe: build it first with" >&2
   echo "   qmake6 paperman.pro -o Makefile.win && make -f Makefile.win" >&2
   exit 1
fi

rm -rf "$dir"
mkdir -p "$dir"
cp paperman.exe "$dir"/

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
               cp -u "$prefix/bin/$name" "$dir"/
               next+=("$prefix/bin/$name")
            fi
         done
      done
      todo=("${next[@]}")
   done
}

# Qt's libraries, plugins and compiler runtime
deploy=$(command -v windeployqt6 || command -v windeployqt || true)
if [ -z "$deploy" ]; then
   echo "no windeployqt: install mingw-w64-x86_64-qt6-base" >&2
   exit 1
fi
"$deploy" --release --no-translations --no-system-d3d-compiler \
   --no-opengl-sw --compiler-runtime "$dir/paperman.exe"

# everything else, including what the plugins themselves need
copy_deps "$dir"/paperman.exe $(find "$dir" -name '*.dll')

cat > "$dir"/README.txt <<'EOF'
Paperman - an electronic filing cabinet: scan, print, stack, arrange

Run paperman.exe to start.

Scanners are reached through their TWAIN driver, so install the driver
which came with the scanner and paperman will offer it.

Two things are used if they are on the PATH and simply not offered if
they are not: tesseract, to read the text of a page, and exiftool, to
keep the details a JPEG carries when a page is changed.
EOF

echo "staged in $dir:"
du -sh "$dir"
ls "$dir" | head -20
