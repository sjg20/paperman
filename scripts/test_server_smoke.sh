#!/bin/bash
# Start the real server on a max stack and a PDF and use the routes which
# read them, checking that each answers and that the server is still up.
#
# The C++ tests run the server inside the test program, which has a
# QApplication; the server itself has only a QCoreApplication, so anything
# which needs a GUI application (such as a QPixmap with a size) passes the
# tests and aborts the real server.  This catches that.
#
# The server also reads the stack's pages with tesseract (--read-pages),
# on worker threads, where such a thing crashes rather than aborts.
#
# Requires: paperman-server binary, curl, test/files (scripts/make_test_files.py)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVER="${SERVER:-$ROOT_DIR/paperman-server}"

die() { echo "FAIL: $*" >&2; exit 1; }

[ -x "$SERVER" ] || die "paperman-server not found (run 'make paperman-server' first)"
command -v curl >/dev/null || die "curl not found"
for f in testfile.max testpdf.pdf; do
    [ -f "$ROOT_DIR/test/files/$f" ] || die "test/files/$f not found"
done

PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("",0)); print(s.getsockname()[1]); s.close()')
TMPDIR=$(mktemp -d)
REPO=$(basename "$TMPDIR")
trap 'kill $SERVER_PID 2>/dev/null; rm -rf "$TMPDIR" "$TMPDIR.data"' EXIT
cp "$ROOT_DIR/test/files/testfile.max" "$ROOT_DIR/test/files/testpdf.pdf" "$TMPDIR/"

# the record of the pages read goes here, not in the user's own
XDG_DATA_HOME="$TMPDIR.data" "$SERVER" -p "$PORT" -r 1 "$TMPDIR" \
    > "$TMPDIR.log" 2>&1 &
SERVER_PID=$!
for i in $(seq 1 50); do
    curl -sf "http://localhost:$PORT/status" >/dev/null && break
    sleep 0.2
done
curl -sf "http://localhost:$PORT/status" >/dev/null || die "server did not start"

check() {
    local label="$1" path="$2"

    if ! curl -sf -o /dev/null "http://localhost:$PORT$path"; then
        kill -0 $SERVER_PID 2>/dev/null || {
            tail -5 "$TMPDIR.log" >&2
            die "server died on $label ($path)"
        }
        die "$label failed ($path)"
    fi
    echo "ok: $label"
}

check "list"           "/list"
check "browse"         "/browse?repo=$REPO"
check "max thumbnail"  "/thumbnail?repo=$REPO&path=testfile.max&page=1&size=small"
check "max page 2"     "/thumbnail?repo=$REPO&path=testfile.max&page=2&size=medium"
check "pdf thumbnail"  "/thumbnail?repo=$REPO&path=testpdf.pdf&page=1&size=small"
check "max as pdf"     "/file?repo=$REPO&path=testfile.max&type=pdf"

# the stack's pages are read and the words put into it
if command -v tesseract >/dev/null; then
    for i in $(seq 1 120); do
        curl -sf "http://localhost:$PORT/v1/status" | grep -q '"written":1' \
            && break
        kill -0 $SERVER_PID 2>/dev/null || {
            tail -5 "$TMPDIR.log" >&2
            die "server died reading the pages"
        }
        sleep 0.5
    done
    curl -sf "http://localhost:$PORT/v1/status" | grep -q '"written":1' \
        || die "the pages were not read"
    echo "ok: read pages"
else
    echo "skip: read pages (no tesseract)"
fi

kill -0 $SERVER_PID 2>/dev/null || die "server is no longer running"
rm -f "$TMPDIR.log"
echo "PASS"
