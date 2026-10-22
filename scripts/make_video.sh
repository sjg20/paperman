#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Record a short video of Paperman being used
#
# The TestVideo suite plays Paperman being used on the demo corpus (see
# doc/corpus.rst), at a person's pace: browsing, scanning with the fake
# scanner, searching and filing. This runs it on a display of its own,
# records the display, then puts each scene's caption on the video.
# paperman must be built with its tests (qmake CONFIG+=test), and Xvfb,
# metacity, ffmpeg and pdftoppm installed.
#
# Usage: scripts/make_video.sh [-c corpus] [-p paperman] [video.mp4]
#   -c corpus    the demo corpus (default /tmp/Paperman), made by
#                make_screenshots.sh or make_demo_corpus.py
#   -p paperman  the program (default ./paperman)
#   video.mp4    where the video goes (default paperman-demo.mp4)

set -e

corpus=/tmp/Paperman
paperman=./paperman
size=1440x900
display=:97

while getopts "c:p:h" opt; do
	case $opt in
	c) corpus=$OPTARG ;;
	p) paperman=$OPTARG ;;
	*) sed -n '3,17s/^# \?//p' "$0"; exit 1 ;;
	esac
done
shift $((OPTIND - 1))
out=${1:-paperman-demo.mp4}

for tool in Xvfb metacity ffmpeg pdftoppm; do
	command -v $tool >/dev/null || { echo "$tool is needed" >&2; exit 1; }
done
[ -d "$corpus" ] || { echo "no corpus in $corpus" >&2; exit 1; }

# a fixed place, so that the paths the window shows are tidy
work=/tmp/paperman-video
rm -rf "$work"
mkdir -p "$work"
cleanup() {
	[ -n "$rec" ] && kill -INT $rec 2>/dev/null && wait $rec 2>/dev/null
	[ -n "$wm" ] && kill $wm 2>/dev/null
	[ -n "$xvfb" ] && kill $xvfb 2>/dev/null
	rm -rf "$work"
}
trap cleanup EXIT

# the video scans into the papers and moves a stack, so use a copy
cp -a "$corpus" "$work/Paperman"
mkdir -p "$work/home" "$work/sheets"

# the sheets to scan: the pages of an itinerary
pdftoppm -r 150 -png "$corpus/travel/skyline itinerary.pdf" \
	"$work/sheets/sheet"

Xvfb $display -screen 0 ${size}x24 -nolisten tcp >/dev/null 2>&1 &
xvfb=$!
sleep 1

# a window manager, so that windows become active, as shortcuts and the
# scan panel's list of folders need, and have title bars, as on a desktop
DISPLAY=$display metacity --sm-disable >/dev/null 2>&1 &
wm=$!
sleep 2

# record the display, pointer and all, from now
ffmpeg -loglevel error -y -f x11grab -draw_mouse 1 -framerate 30 \
	-video_size $size -i $display -c:v libx264 -preset veryfast -crf 18 \
	-pix_fmt yuv420p "$work/raw.mp4" &
rec=$!
start=$(date +%s%3N)
sleep 1

HOME=$work/home DISPLAY=$display QT_QPA_PLATFORM=xcb \
	PAPERMAN_VIDEO_REPO=$work/Paperman \
	PAPERMAN_VIDEO_SHEETS=$work/sheets \
	PAPERMAN_VIDEO_DIR=$work \
	"$paperman" -t TestVideo 2>&1 | grep -E "^(PASS|FAIL|Totals|   Loc|   Actual|   Expected)"

kill -INT $rec
wait $rec || true
rec=
[ -s "$work/cues.json" ] || { echo "the video did not play" >&2; exit 1; }

# put each caption along the foot, from its cue until the next
python3 - "$work" "$start" > "$work/filter.txt" <<'EOF'
import json, sys
work, start = sys.argv[1], int(sys.argv[2])
cues = json.load(open(work + '/cues.json'))
parts = []
for i, cue in enumerate(cues):
    begin = (cue['ms'] - start) / 1000
    end = (cues[i + 1]['ms'] - start) / 1000 if i + 1 < len(cues) else 1e6
    path = '%s/caption%d.txt' % (work, i)
    open(path, 'w').write(cue['caption'])
    parts.append(
        "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf:"
        "textfile=%s:fontsize=34:fontcolor=white:box=1:"
        "boxcolor=0x202020@0.8:boxborderw=18:x=(w-text_w)/2:y=h-90:"
        "enable='between(t,%.2f,%.2f)'" % (path, begin, end))
print(','.join(parts))
EOF
ffmpeg -loglevel error -y -i "$work/raw.mp4" \
	-filter_script:v "$work/filter.txt" -c:v libx264 -preset slow -crf 20 \
	-pix_fmt yuv420p -movflags +faststart "$out"
echo "saved $out ($(ffprobe -v error -show_entries format=duration \
	-of csv=p=0 "$out" | cut -d. -f1) seconds)"
