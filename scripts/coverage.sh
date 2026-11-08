#!/bin/sh
# Measure test coverage of the paperman C++ code.
#
# Builds the test binary with gcov instrumentation in build-coverage/,
# runs the full test suite and writes a report:
#
#   build-coverage/report/         - annotated per-file report, index.html,
#                                    with its figures in summary.json
#   build-coverage/coverage.txt    - plain-text summary (also printed)
#   build-coverage/coverage.json   - the count for each line
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

# The tests need these, and this build is the one which is here to make them
PAPERMAN="$build/paperman" python3 "$src/scripts/make_test_files.py"

# Old counts would understate coverage of a re-run
find "$build" -name '*.gcda' -delete

# Isolate settings and use the offscreen platform so the GUI tests run
# headless; fixtures load relative to the source directory
scratch="$(mktemp -d)"
status=0
cd "$src"
QT_QPA_PLATFORM=offscreen HOME="$scratch/home" XDG_CONFIG_HOME="$scratch/config" \
    "$build/paperman" -t $SUITE || status=$?
rm -rf "$scratch"

# Coverage is still worth having when a test fails, but a crash loses
# the counts of everything which ran before it
if [ "$status" -ne 0 ]; then
    echo "Warning: the tests exit with status $status, so coverage may read low"
fi

# Report on the application code: leave out the tests themselves,
# generated moc/qrc/ui files and the bundled QuiteInsane scanner code
cd "$build"
rm -rf report
mkdir report
# --gcov-ignore-parse-errors: newer gcc emits per-block lines that
# some gcovr versions do not understand; they carry no line counts
gcovr --root "$src" --object-directory "$build" \
    --gcov-ignore-parse-errors=all \
    --exclude "$src/test/" \
    --exclude "$src/qi/" \
    --exclude '.*/moc_.*' --exclude '.*/qrc_.*' --exclude '.*/ui_.*' \
    --exclude-unreachable-branches \
    --html-title 'Paperman C++ test coverage' \
    --html-details report/index.html \
    --json-summary report/summary.json \
    --json coverage.json \
    --txt coverage.txt \
    --print-summary

echo
echo "Full report: $build/report/index.html"
