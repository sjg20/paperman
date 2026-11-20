# Paperman client/server implementation plan

## 1. Goals and scope

Make the `paperman` GUI runnable as a thin client whose authoritative store of
papers lives in a `paperman-server` instance. Multiple clients can point at
one server. Local-only mode continues to work via a separate code path
(`LocalBackend`); we are *not* tunnelling local mode through an in-process
server.

The existing server (`searchserver.cpp`) already exposes a read-only HTTP API
(`/status`, `/repos`, `/search`, `/list`, `/browse`, `/file`, `/thumbnail`),
documented in `doc/api.rst`. The new work is:

- a mutating API,
- a notification/change channel (SSE),
- per-user authentication,
- an abstraction inside the GUI that hides "local disk vs network" from
  `Desktopmodel` / `Pagemodel` / `Dirmodel`.

### Key design decisions (locked in)

| # | Decision |
|---|---|
| 1 | **Thumbnails are server-rendered JPEG; full pages are raw bytes.** The client downloads the whole file and parses it with its existing `Filemax`/`Filepdf`/`Filejpeg` code. Keeps the server simple. |
| 2 | JPEG (PNG for 1bpp mono pages) is the only thumbnail format. |
| 3 | Per-user auth from v1 — bearer tokens, identity recorded in scan attribution. |
| 4 | Two parallel backends: `LocalBackend` keeps current direct-to-disk behaviour; `RemoteBackend` is the HTTP path. No in-process-server unification. |
| 5 | Server owns `.papertree`. Clients never write it. |
| 6 | Last-write-wins concurrency. No `If-Match` / 409. |
| 7 | OCR runs server-side only. Clients read results via annotations. |
| 8 | Single shared trash per repo (status quo). |
| 9 | SSE behind nginx requires `proxy_buffering off`; documented. |
| 10 | Long-running server ops report progress via SSE (consistent with notifications). |
| 11 | Server holds the search index and the rendered-page cache. |
| 12 | `delInfo` blobs from `removePages` are stored server-side; the wire format is an opaque `undoId`. |
| 13 | A `Dirmodel` can mix local and remote repos in one tree. |
| 14 | Full-text-search-in-desk-view is out of scope for v1. |

## 2. Current architecture summary

- `Dirmodel` (`dirmodel.cpp`) owns a list of `Diritem` repositories. Each
  `Diritem` has a `_dir_cache` (a `TreeItem*`) backed by
  `<repo>/.papertree[.<user>]`. Calls like `utilReadTree`, `utilWriteTree`,
  `utilScanDir` touch disk directly.
- `Desktopmodel` (`desktopmodel.cpp`, ~2k lines) owns a list of `Desk*`
  objects, one per currently-visible directory. Mutating ops
  (`opTrashStacks`, `opDuplicateStacks`, `opStackStacks`, `opUnstackPage`,
  `opRenameStack`, `opRenamePage`, `opDeletePages`, `opUpdateAnnot`,
  `opChangeDir`, `opMoveStacks`, `opUpdateRepositoryList`, `opEmailFiles`,
  scan ops) all run client-side on local files. Each is individually wrapped
  in an undo command.
- `Desk` (`desk.h`, `desk.cpp`) is per-directory: scans the dir with `QDir`,
  writes `maxdesk.ini`, holds `QList<File*>`. Most filesystem assumptions
  live here.
- `File` (`file.h`) is the abstract base for `Filemax`, `Filepdf`,
  `Filejpeg`, `Fileother`. Each subclass opens a real file from disk and
  implements `load/create/flush/remove`, `getImage`, `getPreviewPixmap`,
  `getPageText`, `addPage`, `removePages`, `stackStack`, `unstackPages`,
  `duplicate`, `getAnnot/putAnnot`, etc.
- `Pagemodel` (`pagemodel.h`) holds `Pageinfo` for the currently-open stack;
  per-page pixmaps are produced from `File::getImage()`.
- `SearchServer` (`searchserver.cpp`) already spawns Ghostscript /
  pdftocairo / its own `File` instances to render previews, has per-page
  caches under `/tmp/paperman-*`, and watches `.papertree` files via
  `QFileSystemWatcher`. It links `file.cpp`, `filemax.cpp`, `filepdf.cpp`,
  `filejpeg.cpp` (see `paperman-server.pro`), so it can call into them
  server-side.
- Scanning (`qscanner.cpp`, `qscandialog.cpp`, `qi/`) is SANE-based and
  lives entirely in the client. Server has no SANE link.
- OCR (`ocr.cpp`, `ocrtess.cpp`) runs in the client today; results stored
  as annotations on the file. Will move to server (decision 7).
- Tests: `test/test_searchserver.cpp` speaks raw HTTP to an in-process
  server; `test/test_ops.cpp` exercises `Desktopmodel` ops on local
  directories; `test/test_dirmodel.cpp` exercises `Dirmodel`.

## 3. API surface

All new endpoints share a versioned prefix `/v1/`. Legacy unversioned
endpoints (`/file`, `/search`, etc.) stay as aliases until clients move
over.

### 3.1 Reads

Because full pages are served as **raw file bytes** (decision 1), the API is
small — most per-page intelligence stays on the client.

| Endpoint | Status | Notes |
|---|---|---|
| `GET /v1/status` | extend `/status` | Add `apiVersion`, `features[]` (`["mutate","scan","ocr","events"]`), `serverId` (stable UUID, used for client cache keying). |
| `GET /v1/repos` | extend `/repos` | Keep. Add `id` (stable) and `writable` flag per repo. |
| `GET /v1/repos/{repo}/browse?path=…` | rewrap `/browse` | Hierarchical dir + stack listing. Return `dirs[]` and `stacks[]` (where a stack carries `path`, `type`, `pagecount`, `size`, `mtime`, `etag`). |
| `GET /v1/repos/{repo}/stacks/{path}` | new | Per-stack metadata in one round-trip: type, pagecount, annotations, per-page summaries (name, bpp, w, h). Avoids N follow-ups. |
| `GET /v1/repos/{repo}/stacks/{path}/thumbnail?size=…` | existing `/thumbnail` | Server-rendered JPEG (or PNG for 1bpp). Used for the desk view. |
| `GET /v1/repos/{repo}/stacks/{path}/raw` | rewrap `/file` | **Whole-file download**. This is what the client uses for full pages — it pulls the raw `.max`/`.pdf`/`.jpg` and parses it locally. Supports `If-None-Match`/`ETag` so re-opening a stack is cheap. |
| `GET /v1/repos/{repo}/stacks/{path}/annotations` | new | Annotation block (author/title/keywords/notes plus per-page OCR text). |
| `GET /v1/search?q=…&repo=…&path=…&recursive=…` | existing `/search` | Keep. Search runs server-side against the FTS5 index in `searchindex.cpp`. |

Implication of decision 1: no server-side raster page endpoint. The client's
existing `Filemax::getImage` etc. do the rendering after a single whole-file
download. Files are LRU-cached on the client at
`~/.cache/paperman/<serverId>/<repo>/<path>` keyed by ETag.

### 3.2 Mutations

| Client op | Endpoint |
|---|---|
| `opRenameStack` | `POST /v1/repos/{repo}/stacks/{path}/rename` body `{"newName":…,"autoRename":true}` → new path. |
| `opRenamePage` | `POST /v1/repos/{repo}/stacks/{path}/pages/{n}/rename` body `{"newName":…}`. |
| `opTrashStacks` / `opUntrashStacks` | `POST /v1/repos/{repo}/stacks/{path}/trash`, `POST …/untrash` `{"destDir":…,"copy":bool}`. Single shared trash (decision 8). |
| `opDeleteStack` | `DELETE /v1/repos/{repo}/stacks/{path}` (skips trash). |
| `opMoveStacks` (position only) | `POST /v1/repos/{repo}/desks/{dir}/positions` `{"items":[{"path":…,"pos":[x,y]}]}`. |
| `moveToDir` / `opMoveFiles` | `POST /v1/repos/{repo}/stacks/{path}/move` `{"destDir":…,"copy":bool}`. |
| `opDuplicateStacks` | `POST /v1/repos/{repo}/stacks/{path}/duplicate` `{"type":"max|pdf|jpeg","oddEven":3}`. |
| `opStackStacks` | `POST /v1/repos/{repo}/stacks/{dest}/stack` `{"sources":[…],"insertPage":n}`. |
| `opUnstackStacks` / `opUnstackPage` | `POST /v1/repos/{repo}/stacks/{path}/unstack` `{"pagenum":n,"pagecount":k,"remove":bool}`. |
| `opDeletePages` | `POST /v1/repos/{repo}/stacks/{path}/pages/delete` `{"pages":[…]}` → returns `{"undoId":"…"}` (decision 12 — opaque token, server holds the delInfo). |
| `opUndeletePages` | `POST /v1/repos/{repo}/stacks/{path}/pages/undelete` `{"undoId":…}`. Server may garbage-collect un-redeemed undoIds after N hours; clients should treat them as best-effort. |
| `opUpdateAnnot` | `PATCH /v1/repos/{repo}/stacks/{path}/annotations` `{"author":…,…}`. |
| Add page (scan or external upload) | `POST /v1/repos/{repo}/stacks/{path}/pages` with JPEG body and query `name=…&colour=…&width=…&height=…`. Reuses `File::addPageJpeg()`. Auth user goes into the `Env_from` envelope field automatically. |
| Begin / commit / cancel scan stack | `POST /v1/repos/{repo}/desks/{dir}/scans` (begin: returns `scanId` + new stack path); `POST …/scans/{id}/commit`; `DELETE …/scans/{id}`. Pages added via the per-page endpoint with `?scanId=…` so they buffer before commit. |
| Server-side OCR | `POST /v1/repos/{repo}/stacks/{path}/pages/{n}/ocr` — server runs tesseract, stores text into annotations, returns text. Decision 7: this is the only OCR path. |
| `opChangeDir` | client-side only; reflected by `GET /browse` + per-user "last directory" stored locally in `~/.config/paperman/`. |
| `addRepository` / `removeRepository` | `PUT /v1/repos/{name}` `{"path":…}`, `DELETE /v1/repos/{name}` — requires admin token; not all servers will allow. |

Last-write-wins (decision 6): no `If-Match`. Two clients renaming the same
stack at the same time → the second wins silently. Acceptable for v1.

### 3.3 Notifications (SSE)

`GET /v1/repos/{repo}/events` keeps a long-lived HTTP response. Server writes
`event: stackChanged\ndata: {…}\n\n` frames on:

- `QFileSystemWatcher` changes (catches edits the server made outside the
  API, e.g. someone dropping a file into a watched directory),
- every successful mutation,
- every scan commit,
- per-job progress updates for long-running mutations (decision 10): server
  emits `event: progress\ndata: {"jobId":…,"percent":42}` until the job
  emits its terminal event (`stackChanged`/`pageDeleted`/etc).

Each mutation request carries a client-generated `requestId` header. The
event payload echoes the originating `requestId` so the originator can skip
its own echo and avoid a redundant re-fetch.

Reverse-proxy note (decision 9): nginx config must include
`proxy_buffering off` and a `proxy_read_timeout` of at least 1 hour for the
events path. Document in `doc/apache-to-nginx.rst`.

### 3.4 Wire format details

- **Thumbnails**: JPEG (or PNG when source is 1bpp mono). Content-type tells
  the client.
- **Full pages**: raw file bytes from `GET …/raw`. Client parses with its
  own `File` subclass.
- **Annotations**: JSON. Per-page OCR text lives under
  `{"pages":[{"n":1,"ocr":"…"}, …]}`.

## 4. Client-side abstraction

Two backends, both behind a `Backend` interface (decision 4). The two paths
share interfaces but not implementation.

```
Backend (abstract)
  ├─ Repository listing                       (replaces Diritem disk scan)
  ├─ Directory browsing → Desk* / RemoteDesk* (replaces QDir in Desk)
  ├─ File factory       → File*  / RemoteFile*(per-type RemoteFilemax etc.)
  ├─ Mutating ops (rename/stack/unstack/delete/duplicate/move/trash)
  └─ Event subscription
```

### 4.1 New classes

- `class Backend` (abstract).
  - `QStringList listRepositories()`
  - `TreeItem *browse(repo, path)` (returns the same `TreeItem` structure
    `Dirmodel` already understands)
  - `Desk *openDesk(repo, path)`
  - `File *openFile(Desk*, name)` — picks the right subclass per file type
  - mutating ops mirror `Desktopmodel::opXxx`
  - `void subscribe(repo, std::function<void(Event)>)` — local backend is a
    no-op or a `QFileSystemWatcher` adapter; remote backend is SSE.
- `class LocalBackend : Backend` — wraps current `Desk` / `File` code,
  mostly unchanged.
- `class RemoteBackend : Backend` — owns `QNetworkAccessManager`, base URL,
  auth token, SSE subscription, LRU file cache on disk under
  `~/.cache/paperman/<serverId>/<repo>/<path>`.
- `class RemoteDesk : Desk` — overrides directory scan to populate `_files`
  from the `browse` response.
- `class RemoteFile : File` — abstract intermediate. Holds an ETag, a cache
  path, and a "dirty" flag. On `load()` it issues `GET …/raw` and writes the
  bytes to the cache file, then defers to the per-type subclass.
- `class RemoteFilemax : RemoteFile, Filemax` (or composition equivalent) —
  same for `RemoteFilepdf`, `RemoteFilejpeg`. The on-disk parsing code is
  *the existing code*; what changes is "where did the file come from" and
  "where do writes go". Page reads use the cached local file directly;
  mutating ops post to the server and invalidate the cache on the event
  echo.

Because of decision 1, `RemoteFile*` are *thin*. Mutating ops post; reads
use the cache. The bulk of `Filemax`/`Filepdf`/`Filejpeg` logic stays in
the base classes.

### 4.2 Refactors needed in existing code

- **`Desk`**: extract a small `DeskStorage` interface for "list files",
  "write `maxdesk.ini`", "create a new file of the right type". `Desk` keeps
  layout/positioning logic intact.
- **`Desktopmodel`**: the `opXxx` functions today call `Desk::xxx` or
  `File::xxx`. After the refactor they go via `_backend->xxx(file, …)`. The
  undo/redo classes stay the same — they call back into `opXxx`.
- **`Dirmodel`**: today reads `<dir>/.papertree[.user]` via
  `Diritem::readCache()` → `utilReadTree`. Replace with
  `_backend->browse(repo, "")`. The `TreeItem` data structure is unchanged.
  For remote mode, `searchserver.cpp` already understands `.papertree` (see
  `loadFromPapertree`). Client no longer writes `.papertree` itself
  (decision 5).
- **`Pagemodel`**: no change. It already goes through
  `Desktopmodel::getImage` / `getImagePreview` to reach `File`.
- **Scanner**: unchanged. In remote mode the scan stack's `File` is a
  `RemoteFile*`; `addPage` → `addPageJpeg` ships the page over HTTP.
- **OCR**: removed from client (decision 7). `ocr.cpp`/`ocrtess.cpp` and the
  Ocrtess UI call paths become a `RemoteBackend::ocrPage(file, n)` call
  that hits the server. The server links tesseract (add `-ltesseract` to
  `paperman-server.pro`, or shell out, matching how it already calls
  `pdftocairo`). `LocalBackend::ocrPage` can either run tesseract locally
  (so local-only users don't lose OCR) or call back into the existing
  client-side `Ocrtess` code.
- **Operation (`op.h`)**: GUI-only progress dialog. Already excluded from
  server build via `QT_NO_WIDGETS`. Stays in the client. Progress for
  server-side mutations comes via SSE `progress` events.
- **`Dirmodel` mixed local+remote**: each `Diritem` records which `Backend`
  it belongs to. The tree renders them uniformly; mutations dispatch to the
  appropriate backend. No special UI required (decision 13).

### 4.3 Where the boundary is *not*

- File parsing: the client owns it. The server serves bytes. (Decision 1.)
- Undo/redo: lives entirely in the client. Undo records carry whatever
  opaque `undoId` the server needs.
- Image scaling for display: client decides.
- Local `~/.config/paperman/` settings, local LRU cache, the SANE scanner.

## 5. Authentication

Per-user from v1 (decision 3).

- **Login**: `POST /v1/auth/login` `{"user":…,"password":…}` → returns a
  bearer token (32 random bytes, hex-encoded). Token has a configurable TTL
  (default 30 days, refreshed on use).
- **Server-side storage**: hashed passwords in a simple JSON file
  `paperman-server.users` next to the server config. Argon2 (or scrypt) for
  hashing. An admin CLI subcommand `paperman-server useradd <name>` /
  `passwd <name>` / `userdel <name>` manages it. Per-user repo allowlist in
  the same file.
- **Client-side storage**: bearer token stored in `QKeychain` if available,
  otherwise in `~/.config/paperman/paperman.ini` with `0600` perms.
- **Attribution**: the authenticated user's name is recorded server-side on
  every mutation (already partly covered by `serverlog.cpp`) and inserted
  into the `Env_from` envelope on every scan-committed page.
- **Transport**: TLS via nginx reverse proxy. Server doesn't speak TLS
  itself. Document the recommended nginx config (already a starting point in
  `doc/apache-to-nginx.rst`).
- **Compatibility**: the existing `X-API-Key` env-var-based key continues to
  be accepted, mapped to a synthetic admin user. New deployments use bearer
  tokens; existing deployments don't break.

## 6. Migration path

Each step leaves a green CI build and a runnable GUI.

**Step 0 — preparation (no behaviour change)**
- Add `Backend` interface with a `LocalBackend` implementation that wraps
  the existing code, so `Desktopmodel`/`Dirmodel` are no longer talking to
  `Desk`/`File` constructors directly. Big mechanical change; everything
  else hinges on it.
- Land `TestBackendContract` — a Qt Test suite that exercises every
  `Backend` operation. `LocalBackend` is verified first.
- Add a stable per-server `serverId` (UUID).

**Step 1 — auth + read-only remote thumbnails**
- Add user-management commands to `paperman-server`. Add `POST
  /v1/auth/login`. Add bearer-token enforcement on the existing read
  endpoints (`X-API-Key` still works).
- Add `RemoteBackend` skeleton. Client gains a `--server URL` CLI flag and
  a setting; on first use it prompts for username/password and stores the
  token.
- In remote mode `Dirmodel`/`Desktopmodel` show repos/dirs/stacks from the
  server. Thumbnails arrive over the wire. All mutations greyed out.
- New `TestRemoteThumbnails` spawns `paperman-server` in `QProcess`.

**Step 2 — full file read via raw**
- `RemoteFile::load()` downloads `…/raw` to the LRU cache, then defers to
  the per-type subclass. `Pageview` works in remote mode for any file the
  client knows how to parse.
- Test: open a multi-page PDF and Max from a remote server, render every
  page, compare to the local rendering byte-for-byte.

**Step 3 — annotations and server-side OCR**
- `GET …/annotations` and `PATCH …/annotations`.
- Wire `Desktopmodel::updateAnnot`/`getAnnot` through the backend.
- Server links tesseract. `POST …/pages/{n}/ocr` runs it server-side and
  stores the result.
- Test: OCR a page on the server, fetch annotations, verify text. Then
  verify the search index sees the new text.

**Step 4 — simple mutations**
- Rename stack, rename page, move position, trash/untrash, delete. Each
  gets an endpoint, a `RemoteBackend` override, and a test through
  `Desktopmodel::opXxx`.
- **Run the existing `TestOps` suite twice in CI**: once against
  `LocalBackend`, once against `RemoteBackend` with an in-process server.
  This is the single highest-value test gain.

**Step 5 — structural mutations**
- Duplicate, stack, unstack, delete-pages/undelete-pages. Subtlest
  semantics (undo, server-side `undoId` lifecycle, page renumbering). Lock
  the undo round-trip with new tests.

**Step 6 — scan ingest**
- Begin/commit/cancel scan endpoints. `RemoteFile::addPageJpeg` posts the
  page. SANE still runs client-side; page bytes upload over HTTP.

**Step 7 — eventing (SSE)**
- `RemoteBackend` opens an SSE channel per remote repo and routes events to
  `Dirmodel`/`Desktopmodel` invalidations. Multi-client setups now stay
  consistent.

**Step 8 — local repos and remote repos coexisting in one tree**
- `Dirmodel` per-entry backend (decision 13). Final UX: the user can have
  `/home/me/papers` as a local repo and `https://server/papers` as a remote
  repo side by side in the dir tree.

Each step bumps `apiVersion` in `/v1/status`. Clients refuse too-old
servers with a clear error.

## 7. What stays local on the client

Independent of the server:

- All Qt settings under `~/.config/paperman/`: window geometry, recent
  repos, the server URL + bearer token, scanner preferences.
- The scanner. SANE devices are local to the machine. Scanning into a
  server-side repo uploads pages; the scanner must be attached to the
  GUI's host.
- Disk cache `~/.cache/paperman/<serverId>/…` for thumbnails and recently
  downloaded files.
- The undo/redo stack.

OCR moves *off* the client (decision 7). `Ocrtess` linkage stays for
`LocalBackend` users who want client-side OCR; `RemoteBackend` always uses
the server endpoint.

## 8. Testing strategy

Existing:

- `test/test_searchserver.cpp` — drives in-process `SearchServer` over real
  HTTP. Template for new server-side tests.
- `test/test_ops.cpp` — drives `Desktopmodel` ops on a real test tree.
  After Step 0 this becomes parametric on backend (the biggest test win).
- `test/test_dirmodel.cpp` — same shape, parametric.

New:

- `TestBackendContract` (Step 0): one test per `Backend` method against
  `LocalBackend`.
- `TestRemoteBackend`: same tests against `RemoteBackend` talking to an
  in-process server.
- `TestRoundTrip`: for each mutating op, spawn server, perform via
  `RemoteBackend`, verify the server's on-disk state and that a second
  `RemoteBackend` instance sees the result.
- `TestEvents`: open SSE channel; trigger a mutation from a second client;
  assert receipt within N ms.
- `TestAuth`: missing token → 401; valid token → 200; per-user repo
  restriction works; admin CLI add/del/passwd works.
- `TestUndoIdLifecycle`: deletePages → undeletePages → restored content.
  Also: deletePages → wait past TTL → undeletePages → 410 Gone.

The CI matrix (`qt`/`qt6`) already exists; new suites add maybe 30-60s.

## 9. Critical files

Primary:

- `searchserver.cpp` — new endpoints
- `desktopmodel.cpp` — route ops through backend
- `desk.cpp` — extract `DeskStorage`
- `file.cpp`/`file.h` — make `File` aware that a backend can give it bytes
- `dirmodel.cpp` — backend-aware `Diritem`s

Secondary:

- `filemax.cpp` (most complex backend; sets the bar for `RemoteFilemax`)
- `test/test_searchserver.cpp` (template for new server tests)
- `test/test_ops.cpp` (parametrise over backends)
- `paperman-server.pro` (add `-ltesseract`, link new auth code)
- `doc/api.rst` (extend with v1 endpoints)
- `doc/apache-to-nginx.rst` (SSE proxy config)
