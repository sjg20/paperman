#!/bin/sh
# Measure test coverage of the paperman C++ code.
#
# Builds the test binary with gcov instrumentation in build-coverage/,
# runs the full test suite and writes a report:
#
#   build-coverage/coverage.html   - annotated per-file report
#   build-coverage/coverage.txt    - plain-text summary (also printed)
#
# Environment:
#   QMAKE     qmake to use (default: qmake from PATH)
#   JOBS      parallel build jobs (default: nproc)
#   SUITE     test suite(s) to run (default: all)
#
# Requires gcovr (apt install gcovr).

set -e

src="$(cd "$(dirname "$0")/.." && pwd)"
build="$src/build-coverage"
qmake="${QMAKE:-qmake}"
jobs="${JOBS:-$(nproc)}"

mkdir -p "$build"
cd "$build"
"$qmake" "$src/paperman.pro" CONFIG+=test CONFIG+=coverage
make -j"$jobs"

# Old counts would understate coverage of a re-run
find "$build" -name '*.gcda' -delete

# Isolate settings and use the offscreen platform so the GUI tests run
# headless; fixtures load relative to the source directory
scratch="$(mktemp -d)"
cd "$src"
QT_QPA_PLATFORM=offscreen HOME="$scratch/home" XDG_CONFIG_HOME="$scratch/config" \
    "$build/paperman" -t $SUITE || true
rm -rf "$scratch"

# Report on the application code: leave out the tests themselves,
# generated moc/qrc/ui files and the bundled QuiteInsane scanner code
cd "$build"
# --gcov-ignore-parse-errors: newer gcc emits per-block lines that
# some gcovr versions do not understand; they carry no line counts
gcovr --root "$src" --object-directory "$build" \
    --gcov-ignore-parse-errors=all \
    --exclude "$src/test/" \
    --exclude "$src/qi/" \
    --exclude '.*/moc_.*' --exclude '.*/qrc_.*' --exclude '.*/ui_.*' \
    --exclude-unreachable-branches \
    --html-details coverage.html \
    --txt coverage.txt \
    --print-summary

echo
echo "Full report: $build/coverage.html"
