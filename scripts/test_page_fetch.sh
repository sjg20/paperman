#!/bin/bash
# Performance test: compare full-file vs single-page PDF download
# Requires: paperman-server binary, curl, pdfinfo (poppler-utils)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVER="$ROOT_DIR/paperman-server"
TEST_PDF="$ROOT_DIR/test/files/testpdf.pdf"
ITERATIONS=3

die() { echo "FAIL: $*" >&2; exit 1; }

# Check prerequisites
[ -x "$SERVER" ] || die "paperman-server not found (run 'make paperman-server' first)"
[ -f "$TEST_PDF" ] || die "test PDF not found: $TEST_PDF"
command -v curl >/dev/null || die "curl not found"

# Find a free port
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("",0)); print(s.getsockname()[1]); s.close()')

# Create a temporary repo directory with the test PDF
TMPDIR=$(mktemp -d)
trap 'kill $SERVER_PID 2>/dev/null; rm -rf "$TMPDIR"' EXIT
cp "$TEST_PDF" "$TMPDIR/"

# Start server
"$SERVER" -p "$PORT" "$TMPDIR" &
SERVER_PID=$!
sleep 1

# Verify server is up
curl -sf "http://localhost:$PORT/status" >/dev/null || die "server did not start"

echo "Server running on port $PORT (PID $SERVER_PID)"
echo "Test PDF: $(du -h "$TEST_PDF" | cut -f1) ($(pdfinfo "$TEST_PDF" 2>/dev/null | grep Pages: | awk '{print $2}') pages)"
echo ""

# Helper: download N times, collect sizes and times
bench() {
    local label="$1" url="$2"
    local times=() sizes=()

    for i in $(seq 1 $ITERATIONS); do
        local out="$TMPDIR/bench_$$_$i"
        local t0 t1 elapsed sz
        t0=$(date +%s%N)
        curl -sf -o "$out" "$url" || die "download failed: $url"
        t1=$(date +%s%N)
        elapsed=$(( (t1 - t0) / 1000000 ))  # ms
        sz=$(stat -c%s "$out")
        times+=($elapsed)
        sizes+=($sz)
        rm -f "$out"
    done

    # Sort for median
    IFS=$'\n' sorted_t=($(sort -n <<<"${times[*]}")); unset IFS
    IFS=$'\n' sorted_s=($(sort -n <<<"${sizes[*]}")); unset IFS
    local mid=$(( ITERATIONS / 2 ))

    echo "$label:"
    echo "  median time : ${sorted_t[$mid]} ms"
    echo "  median size : ${sorted_s[$mid]} bytes"
    # Return values via global vars
    _MEDIAN_TIME=${sorted_t[$mid]}
    _MEDIAN_SIZE=${sorted_s[$mid]}
}

bench "Full file" "http://localhost:$PORT/file?path=testpdf.pdf"
FULL_TIME=$_MEDIAN_TIME
FULL_SIZE=$_MEDIAN_SIZE

bench "Page 1 only" "http://localhost:$PORT/file?path=testpdf.pdf&page=1"
PAGE_TIME=$_MEDIAN_TIME
PAGE_SIZE=$_MEDIAN_SIZE

echo ""
if [ "$PAGE_TIME" -gt 0 ] && [ "$FULL_TIME" -gt 0 ]; then
    SPEEDUP=$(echo "scale=1; $FULL_TIME / $PAGE_TIME" | bc 2>/dev/null || echo "n/a")
    echo "Time speedup  : ${SPEEDUP}x"
fi
if [ "$PAGE_SIZE" -gt 0 ]; then
    REDUCTION=$(echo "scale=0; 100 - ($PAGE_SIZE * 100 / $FULL_SIZE)" | bc 2>/dev/null || echo "n/a")
    echo "Size reduction: ${REDUCTION}%"
fi

echo ""
echo "PASS"
