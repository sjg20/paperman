#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Put right MSYS2 packages which Paperman cannot be built or run with
#
# MSYS2 updates its packages one at a time, and now and then one which
# Paperman needs does not work with it for a while. This puts such a
# package back to one which does, from MSYS2's repository, with a warning,
# and otherwise does nothing:
#
# - Qt modules at different versions. Qt's own modules use parts of Qt
#   which are the same only within one version: QtStateMachine (in
#   qt6-scxml) built for one version will not load with another, and
#   Paperman then does not start ('entry point not found'). When qt6-scxml
#   is behind qt6-base, qt6-base and qt6-svg go back to its version.
#
# - Builds which are known not to work, listed in 'known_bad' below, each
#   with the build to use instead. A newer build than the one listed is
#   left alone.
#
# Usage, in an MSYS2 shell: scripts/msys2_fix_packages.sh
set -e

case "$MSYSTEM" in
UCRT64) pkg=mingw-w64-ucrt-x86_64 ;;
CLANGARM64) pkg=mingw-w64-clang-aarch64 ;;
CLANG64) pkg=mingw-w64-clang-x86_64 ;;
MINGW64) pkg=mingw-w64-x86_64 ;;
*) echo "msys2_fix_packages: unknown MSYSTEM '$MSYSTEM'" >&2; exit 1 ;;
esac
system=$(echo "$MSYSTEM" | tr A-Z a-z)
repo=https://repo.msys2.org/mingw/$system

# Builds which are known not to work: the system, the package, the build
# which does not work and the one to use instead.
#
# PoDoFo 1.1.3 is built with link-time optimisation, and Paperman built
# with GCC against it does not start: the runtime cannot reach one of the
# library's variables ('32 bit pseudo relocation out of range').
known_bad="
ucrt64 podofo 1.1.3-1 1.1.2-2
"

# the version of an installed package, with its build number
installed() {
	pacman -Q "$pkg-$1" 2>/dev/null | awk '{print $2}'
}

# the same without the build number, such as 6.12.0
upstream() {
	installed "$1" | sed 's/-[0-9]*$//'
}

files=()

want=$(upstream qt6-scxml)
have=$(upstream qt6-base)
if [ -n "$want" ] && [ "$want" != "$have" ]; then
	echo "::warning::MSYS2's qt6-scxml is at $want but qt6-base at $have:" \
		"putting qt6-base and qt6-svg back to $want"

	# the newest build of each at that version, from the repository
	index=$(curl -fsSL "$repo/")
	want_re=$(echo "$want" | sed 's/\./\\./g')
	for name in qt6-base qt6-svg; do
		file=$(echo "$index" |
			grep -o "$pkg-$name-$want_re-[0-9]*-any\.pkg\.tar\.zst\"" |
			tr -d '"' | sort -u |
			sed -E "s/.*-([0-9]+)-any\.pkg\.tar\.zst$/\1 &/" |
			sort -n | tail -1 | cut -d' ' -f2)
		if [ -z "$file" ]; then
			echo "msys2_fix_packages: no $name $want in $repo" >&2
			exit 1
		fi
		files+=("$repo/$file")
	done
fi

while read -r where name bad good; do
	[ -z "$where" ] || [ "$where" != "$system" ] && continue
	if [ "$(installed "$name")" = "$bad" ]; then
		echo "::warning::MSYS2's $name $bad does not work with Paperman:" \
			"using $good instead"
		files+=("$repo/$pkg-$name-$good-any.pkg.tar.zst")
	fi
done <<< "$known_bad"

if [ ${#files[@]} -eq 0 ]; then
	echo "msys2_fix_packages: nothing to put right"
	exit 0
fi
pacman -U --noconfirm "${files[@]}"
