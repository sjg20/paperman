# Progressive scan-preview regression investigation

## What we want
Paperman should display the scanned image progressively (line-by-line) in the
page-list thumbnail (`Pageview` widget on the right side of the main window)
while the scanner is actively scanning a page. The user reports it currently
shows only diagonal cross-hatching during the scan and the real image only
appears once the scan completes.

## Environment
- Scanner: Fujitsu fi-8170 (USB)
- SANE backend: `libsane-fujitsu` (sane-backends)
- Qt: Both Qt5 (`/usr/bin/qmake`) and Qt6 (`/usr/bin/qmake6`) available
- App branch: `adj` (in `/home/ubuntu/project`)
- Local sane-backends source: `~/dev/libsane/backends/` (older copy; latest in
  the upstream git tree)

## Architecture of the scan-progress pipeline

```
Scanner (USB)
  └─> SANE backend (fujitsu.c)
      └─> QScanner::read(buf, 256KB)              [qscanner.cpp]
          └─> Paperscan thread loop               [paperstack.cpp]
              └─> emit stackPageProgress(page)    [paperstack.cpp:789]
                  └─> Mainwidget::slotStackPageProgress  [mainwidget.cpp:573]
                      └─> Desktopmodel::pageProgress     [desktopmodel.cpp:906]
                          └─> Desktopmodel::getNewScaledImage [desktopmodel.cpp:933]
                              └─> emit newScaledImage(image, line) [desktopmodel.cpp:923]
                                  └─> Pagewidget::slotNewScaledImage [pagewidget.cpp:931]
                                      └─> Pagemodel::newScaledImage  [pagemodel.cpp:475]
                                          ├─> paints into _scan_image
                                          ├─> Pageinfo::updateScanImage (sets _pixmap)
                                          └─> emit pagePartChanged   [pagemodel.cpp:514]
                                              └─> Pageview::slotPagePartChanged [pageview.cpp:162]
                                                  └─> viewport()->update(part_rect)
                                                      └─> Pagedelegate::paint [pagedelegate.cpp:193]
```

The main GUI thread runs a tight `qApp->processEvents()` loop while
`_scanning == true` (Mainwidget::scanInto, mainwidget.cpp:344). Paint events
should be processed in that loop.

## What the diagnostics confirmed

WIP commit `28685b1` adds `printf()` instrumentation in:
- `Desktopmodel::pageProgress` — chunk arrivals
- `Pagewidget::slotNewScaledImage` — receipt
- `Pagemodel::newScaledImage` — paint into `_scan_image`, source/dest formats,
  and counts of non-white pixels in the source
- `Pageview::slotPagePartChanged` — repaint requests
- `Pagedelegate::paint` — pixmap state at paint time
- Wall-clock timestamps on the pageProgress events

The output (`/home/ubuntu/project/asc`, captured by user) showed:

1. The pipeline works end-to-end. Pagedelegate::paint is called repeatedly
   during the scan with `pm=600x850` (a non-null pixmap of the right size).
2. The source image arriving at `newScaledImage` is `Format_Mono` (fmt=1),
   which we now convert to `Format_RGB32` before drawing. The non-white pixel
   count is non-zero, confirming the source has real scan content (e.g.
   "nonwhite orig=153 conv=153" for the first chunk).
3. The destination strip in `_scan_image` also gets the non-white pixels
   (drawImage works).
4. **Timestamps reveal the real problem**: for a scan the user perceives as
   ~1 second long, all five chunks arrive in the last ~75 ms:

   ```
   [   0] pageProgress: getData=1 size=261950 ...
   [  15] pageProgress: getData=1 size=523900 ...
   [  32] pageProgress: getData=1 size=785850 ...
   [  48] pageProgress: getData=1 size=1047800 ...
   [  59] pageProgress: getData=1 size=1086860 ...
   ```

So during the first ~925 ms of the user-perceived scan, no data flows at all —
the SANE backend `sane_read` is blocked, and only releases data near the end of
the physical scan.

## Why the data is bunched at the end

`fujitsu.c::sane_read` calls `read_from_scanner` which calls
`scanner_control_ric` (RIC = "Read In Cancel"; sends a SCSI control command
with `set_SC_ric_len(cmd, bytes)`). RIC blocks until the scanner has `bytes`
worth of data ready. Then a SCSI READ pulls those bytes.

Two SANE options control how aggressively this batches:

1. `buffermode` (string: default/on/off) — when ON, instructs the scanner via
   `set_MSEL_buff_mode` to buffer pages internally before USB transfer. The
   description says "Request scanner to read pages quickly from ADF into
   internal memory."
2. `buffer-size` (integer; default 64 KB; min 4 KB) — set in
   `/etc/sane.d/fujitsu.conf`. Determines the chunk size for each
   RIC + READ round-trip. The user's diagnostic chunks are ~256 KB each, so
   their config has bumped `buffer-size` above the default.

The user already tried `scanimage --buffermode=off` at 400 dpi and reported
"almost saw some progress, right at the end" — so progressive transfer is
possible with buffermode=off, but the scanner still mostly produces data
at the end because:
- The fi-8170 is a high-speed sheet-fed scanner; the physical scan is fast
- Even with buffermode=off, the RIC mechanism waits for one full
  `buffer-size` chunk before unblocking
- If `buffer-size` is large compared to the per-second data rate, fewer chunks
  fit during the physical scan time

## What we want to try / decide

A: **Reduce buffer-size to 4096** in `/etc/sane.d/fujitsu.conf` and test with
   `buffermode=off`. Should produce many more chunks visible during the
   physical scan.

B: **Wire `buffermode=off` into QScanner** by default (or as a UI toggle), so
   paperman's scans are progressive without manual config tweaks.

C: **Accept** that fast sheet-fed scanners with hardware buffering largely
   complete the data transfer right at the end of the physical scan, and that
   meaningful progressive preview is only visible at high resolutions.

The user's gut feeling is that progressive preview used to work better. We've
confirmed it's not a code regression: the rendering pipeline functions
correctly. The constraint is at the SANE-driver / scanner layer.

## Outstanding questions

1. Does the fi-8170 firmware actually expose data line-by-line during physical
   scan, or only after each internal buffer fills? (The user's "almost saw
   some progress at the end" suggests it does line-by-line near the end of
   paper transit but not during.)
2. Is there a SANE backend tweak to push data more aggressively (smaller RIC
   request lengths in a tight loop)?
3. Is there an option (or upstream patch in newer sane-backends) that hasn't
   landed in the user's local copy?

## Key files / line numbers

| File | Where |
|---|---|
| `pagemodel.cpp:475` | `Pagemodel::newScaledImage` — paints chunk into `_scan_image`, calls `updateScanImage` |
| `pagemodel.cpp:715` | `Pageinfo::updateScanImage` — sets `_pixmap` |
| `pagemodel.cpp:668` | `Pageinfo::pixmap` — returns the pixmap to the delegate |
| `desktopmodel.cpp:906` | `Desktopmodel::pageProgress` — entry point on GUI thread |
| `desktopmodel.cpp:933` | `Desktopmodel::getNewScaledImage` — slices new lines |
| `pagewidget.cpp:931` | `Pagewidget::slotNewScaledImage` — only forwards if `_scanning` |
| `pageview.cpp:162` | `Pageview::slotPagePartChanged` — calls viewport update |
| `pagedelegate.cpp:193` | `Pagedelegate::paint` — draws pixmap or hatching |
| `paperstack.cpp:789` | `emit stackPageProgress` from scan thread |
| `qscanner.cpp:1045` | `QScanner::start` — sets non-blocking IO mode if supported |
| `qscanner.cpp:1062` | `QScanner::read` — wrapper around `do_sane_read` |
| `~/dev/libsane/backends/backend/fujitsu.c:8043` | `sane_read` |
| `~/dev/libsane/backends/backend/fujitsu.c:8656` | `read_from_scanner` |
| `~/dev/libsane/backends/backend/fujitsu.c:7415` | `scanner_control_ric` (the blocking call) |
| `~/dev/libsane/backends/backend/fujitsu.c:3903` | `OPT_BUFF_MODE` definition |
| `~/dev/libsane/backends/backend/fujitsu.c:868`  | `buffer-size` config-file parsing |

## Commit state

Branch `adj`, originally:
```
28685b1 WIP: Diagnostics for scan-preview regression
921d626 mainwidget: Auto-reconnect scanner on IO_ERROR
55ed59b qscanner: Add reconnect() to recover from sleep/IO errors
```

The WIP commit contains the printf instrumentation and the
`Format_RGB32`/`convertToFormat` changes in `Pagemodel::newScaledImage`. The
format-handling fix is worth keeping (makes rendering more robust regardless
of the timing issue); the printfs can be dropped before merging.

# Resolution

Confirmed by direct measurement (see `scan_chunks.c`) that the bottleneck is
the fujitsu backend's `buffer-size` config option: with the default
262 144-byte chunks the scanner internally buffers the whole page and dumps
it over USB in ~32 ms after the physical scan finishes. With 4 KB chunks the
scanner delivers data progressively at scanner-pace through `sane_read`.

A ~1 s pre-scan delay (paper feed + sensor warmup + motor startup before any
data exists) is unavoidable and lives entirely between `sane_start` returning
and the first `sane_read` unblocking — that's a hardware constraint, not the
backend.

Two fixes were applied in `~/dev/libsane/backends/`:

1. **Runtime `buffer-size` SANE option** (was config-file only). Frontends
   can now set it via `sane_control_option` before `sane_start`. This makes
   `--bsize` work with `scanimage -A` and lets paperman choose 4 KB at
   open-time without `/etc/sane.d` edits.

2. **`sane_read_dup` SANE extension** for progressive duplex. A new public
   entry point on libsane.so that returns front+back data in one call, so a
   frontend can update both side-pixmaps in lockstep across the page transit
   instead of seeing one side at a time. dll.c falls back to
   `SANE_STATUS_UNSUPPORTED` when the underlying backend doesn't implement
   it, so the addition is forward- and backward-compatible.

Measurement summary on fi-8170 / Lineart / 300 dpi:

| variant                          | sane_start | first chunk | chunks span | sides   |
|----------------------------------|-----------:|------------:|------------:|---------|
| original (262 144-byte chunks)   |   1180 ms  |    1013 ms  |       32 ms | sequential |
| `--bsize 4096`, `sane_read`      |    281 ms  |    1012 ms  |      637 ms | sequential |
| `--bsize 4096`, `sane_read_dup`  |    352 ms  |    1185 ms  |     1265 ms | **lockstep front+back** |

## Files changed

`~/dev/libsane/backends/backend/`
- `fujitsu.h` — `OPT_BUFFER_SIZE` enum value, `SANE_Range buffer_size_range`.
- `fujitsu.c` — option descriptor + GET + SET cases; new `sane_read_dup` body
  (~110 lines) implemented in terms of the existing `read_from_scanner` and
  `read_from_buffer` helpers, called once per side in lockstep.
- `dll.c` — `OP_READ_DUP` enum + dispatch table entry + public
  `sane_read_dup` wrapper that returns `SANE_STATUS_UNSUPPORTED` if the
  loaded backend doesn't export the symbol.
- `stubs.c` — public `sane_read_dup` stub forwarding to `ENTRY(read_dup)`.

`~/dev/libsane/backends/include/sane/`
- `sanei_backend.h` — `extern` declaration and macro redirect for the new
  entry point.

paperman tree:
- `qscanner.h` / `qscanner.cpp` — `mOptionBufferSize`, `setBufferSize(int)`,
  `hasReadDup()`, `readDup(...)`. `findOptions()` calls `setBufferSize(4096)`
  whenever the patched backend exposes the option. `dlsym(RTLD_DEFAULT,
  "sane_read_dup")` is cached at first call.
- `paperman.pro` — adds `-ldl`.
- `paperstack.h` / `paperstack.cpp` — parallel `_page_back` slot, with
  `addImageBack`, `addImageBytesBack`, `confirmImageBack`, `curPageBack`,
  `coverageStrBack`. The progressive-duplex branch in `Paperscan::scan()`
  fires when `numsides == 2 && !single && _scanner->hasReadDup()`; it does
  one `sane_start`, two `addImage`s, one `readDup` loop, two `confirmImage`s.
- `scan_chunks.c` — standalone diagnostic. `--bsize` sets the runtime
  backend option; `--duplex` exercises `sane_read_dup` via
  `dlsym(RTLD_DEFAULT)` so the binary still runs against an unpatched
  libsane.
- `GNUmakefile` — `scan_chunks` target.

## Runbook

Build:

```
# patched libsane (one-time after editing fujitsu/dll/stubs)
cd ~/dev/libsane/backends/backend
touch dll.c stubs.c fujitsu.c
make libsane.la libsane-fujitsu.la

# diagnostic
cd ~/dev/paperman && make -f GNUmakefile scan_chunks

# paperman (qmake project)
cd ~/dev/paperman && qmake paperman.pro && make -f Makefile -j4 paperman
```

Run paperman against the patched libsane:

```
LD_LIBRARY_PATH=$HOME/dev/libsane/backends/backend/.libs ./paperman
```

Without that env, paperman picks up the system libsane, which lacks
`sane_read_dup`; `hasReadDup()` returns false and the scan loop transparently
falls back to the legacy per-side path. The runtime `buffer-size` option
likewise becomes a no-op there, so the user sees the original
"all-data-at-end" behaviour. Both states are safe.

Diagnose chunk timing:

```
# simplex, default chunk size (originally 256 KB)
./scan_chunks --resolution 300 --buffer 4096

# simplex, runtime override to 4 KB
LD_LIBRARY_PATH=$HOME/dev/libsane/backends/backend/.libs \
  ./scan_chunks --resolution 300 --buffer 4096 --bsize 4096

# duplex, both sides progressive
LD_LIBRARY_PATH=$HOME/dev/libsane/backends/backend/.libs \
  ./scan_chunks --duplex --resolution 300 --buffer 4096 --bsize 4096
```

The summary line at the end reports per-side chunk count and the time
window over which they arrived; a healthy progressive duplex run has a
window of several hundred ms (matching the page transit time), not tens of
ms. The first chunk's `t_ms` is the unavoidable pre-scan delay.
