#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Take the screenshots for the documentation and the website
#
# They show the demo corpus (see doc/corpus.rst), which is made first if
# it is not there, and read by OCR so that its text can be shown and
# searched. The window is drawn off screen, so this needs no display.
# paperman must be built with its tests (qmake CONFIG+=test).
#
# Usage: scripts/make_screenshots.sh [-f] [-c corpus] [-p paperman] [outdir]
#   -f           make the corpus afresh
#   -c corpus    where the corpus is, or goes (default /tmp/Paperman,
#                which is the path the pictures show)
#   -p paperman  paperman program to use (default ./paperman)
#   outdir       where the pictures go (default doc/images)

set -e

corpus=/tmp/Paperman
paperman=./paperman
fresh=

while getopts "fc:p:h" opt; do
	case $opt in
	f) fresh=1 ;;
	c) corpus=$OPTARG ;;
	p) paperman=$OPTARG ;;
	*) sed -n '3,17s/^# \?//p' "$0"; exit 1 ;;
	esac
done
shift $((OPTIND - 1))
out=${1:-doc/images}

if ! QT_QPA_PLATFORM=offscreen "$paperman" -t list 2>/dev/null |
		grep -qw TestScreenshots; then
	echo "$paperman has no screenshot tests: build it with CONFIG+=test" >&2
	exit 1
fi

# paperman keeps its settings in the home directory, so give it one of its
# own, which also keeps the pictures the same whoever takes them
home=$(mktemp -d)
trap 'rm -rf "$home"' EXIT

if [ -n "$fresh" ] || [ ! -d "$corpus" ]; then
	rm -rf "$corpus"
	"$(dirname "$0")/make_demo_corpus.py" --paperman "$paperman" "$corpus"
	echo "Reading the text of the pages"
	HOME=$home QT_QPA_PLATFORM=offscreen "$paperman" --ocr "$corpus" \
		>/dev/null
fi

mkdir -p "$out"
HOME=$home QT_QPA_PLATFORM=offscreen QTEST_FUNCTION_TIMEOUT=120000 \
	PAPERMAN_SHOTS_REPO=$(realpath "$corpus") \
	PAPERMAN_SHOTS_DIR=$(realpath "$out") \
	"$paperman" -t TestScreenshots 2>&1 | grep -E '^(PASS|FAIL|Totals)|saved'
