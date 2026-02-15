#!/bin/bash
# Integration test: parallel PDF-to-max conversion
# Requires: paperman binary, pdfinfo (poppler-utils)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PAPERMAN="$ROOT_DIR/paperman"
TEST_PDF="$ROOT_DIR/test/files/100pp.pdf"

export QT_QPA_PLATFORM=offscreen

die() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "  PASS: $*"; }

# Convert .max to PDF and return the page count
max_pages() {
    local maxfile="$1"
    local dir base pdf

    dir="$(dirname "$maxfile")"
    base="$(basename "$maxfile" .max)"
    pdf="$dir/$base.pdf"
    (cd "$dir" && "$PAPERMAN" -p "$maxfile") 2>/dev/null
    pdfinfo "$pdf" | awk '/^Pages:/{print $2}'
}

# Check prerequisites
[ -x "$PAPERMAN" ] || die "paperman not found (run 'make -f Makefile' first)"
[ -f "$TEST_PDF" ] || die "test PDF not found: $TEST_PDF"
command -v pdfinfo >/dev/null || die "pdfinfo not found (install poppler-utils)"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "=== Parallel PDF-to-max conversion tests ==="
echo ""

# Test 1: Page range conversion
echo "Test 1: Page range (pages 1-10)"
"$PAPERMAN" -m "$TEST_PDF" --page-range 1:10 --output "$TMPDIR/range10.max"
[ -f "$TMPDIR/range10.max" ] || die "output file not created"
PAGES=$(max_pages "$TMPDIR/range10.max")
[ "$PAGES" = "10" ] || die "expected 10 pages, got $PAGES"
pass "page-range 1:10 produced $PAGES pages"

# Test 2: Parallel matches sequential page count
echo "Test 2: Parallel vs sequential page count"
"$PAPERMAN" -m "$TEST_PDF" --jobs 1 --output "$TMPDIR/seq.max"
(cd "$TMPDIR" && "$PAPERMAN" -m "$TEST_PDF")
[ -f "$TMPDIR/100pp.max" ] || die "parallel output not created"
SEQ_PAGES=$(max_pages "$TMPDIR/seq.max")
PAR_PAGES=$(max_pages "$TMPDIR/100pp.max")
[ "$SEQ_PAGES" = "$PAR_PAGES" ] || die "page count mismatch: seq=$SEQ_PAGES par=$PAR_PAGES"
pass "sequential ($SEQ_PAGES) and parallel ($PAR_PAGES) page counts match"

# Test 3: Explicit --jobs values
echo "Test 3: Explicit --jobs 2 and --jobs 4"
for J in 2 4; do
    "$PAPERMAN" -m "$TEST_PDF" --jobs "$J" --output "$TMPDIR/jobs${J}.max"
    [ -f "$TMPDIR/jobs${J}.max" ] || die "--jobs $J output not created"
    JP=$(max_pages "$TMPDIR/jobs${J}.max")
    [ "$JP" = "100" ] || die "--jobs $J: expected 100 pages, got $JP"
    pass "--jobs $J produced $JP pages"
done

# Test 4: Small file fallback (below threshold, no parallel)
echo "Test 4: Small file fallback (2 pages)"
"$PAPERMAN" -m "$TEST_PDF" --page-range 1:2 --output "$TMPDIR/small.max"
SMALL_PAGES=$(max_pages "$TMPDIR/small.max")
[ "$SMALL_PAGES" = "2" ] || die "expected 2 pages, got $SMALL_PAGES"
pass "small file ($SMALL_PAGES pages) handled correctly"

echo ""
echo "=== All tests passed ==="
