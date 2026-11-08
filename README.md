[![CI](https://github.com/sjg20/paperman/actions/workflows/ci.yml/badge.svg)](https://github.com/sjg20/paperman/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fsjg20.github.io%2Fpaperman%2Fcoverage%2Fsummary.json&query=%24.line_percent&suffix=%25&label=coverage)](https://sjg20.github.io/paperman/coverage/)

# Paper Manager

Visit **[getpaperman.com](https://getpaperman.com)** to
[download Paperman](https://getpaperman.com/download/),
[see what it does](https://getpaperman.com/features/) or
[try the demo](https://getpaperman.com/server-and-app/#try-it).

Paperman is an electronic filing cabinet: it scans paper into stacks of
pages and lets you view, arrange, annotate, print, search and send them.
It reads and writes PDF, JPEG and its own variant of PaperPort's .max file
format.

It has three parts: the desktop program, for Linux, Windows and macOS; a
search server, `paperman-server`, which serves repositories over the
network to the desktop program on other machines and to the app; and an
app for Android and iPhone, for finding and reading papers from a phone
or tablet. See the
[introduction](https://paperman.readthedocs.io/en/latest/introduction.html).

On Ubuntu, install it from the PPA (`ppa:sjg1/ppa`). On most Linux
distributions it is also a [snap](https://snapcraft.io/paperman)
(`sudo snap install paperman`). Windows installers, macOS disk images
and a Linux AppImage, for other distributions, come with each
[release](https://github.com/sjg20/paperman/releases/latest), and to try
something not yet released, from the artifacts of the latest
[CI run](https://github.com/sjg20/paperman/actions/workflows/ci.yml) on
master. See the
[installation guide](https://paperman.readthedocs.io/en/latest/install.html).

Full documentation is available at https://paperman.readthedocs.io

## Screenshots

![The desktop, with folders, stacks, a stack's pages and the page's text](doc/images/desktop.png)

A long stack, with each page's text | Finding stacks by the words on their pages
----------------------------------- | -----------------------------------------
![A 63-page stack open at a signed page](doc/images/page-view.png) | ![Stacks found by a word on their pages](doc/images/search-results.png)

Following the desktop's dark theme | Scanning, with presets
---------------------------------- | ----------------------
![The desktop in a dark theme](doc/images/desktop-dark.png) | ![The scanning panel](doc/images/scan-panel.png)

## Features

- simple GUI based around stacks and pages
- view previews and browse through pages
- move pages in and out of stacks, and drag them into a new order
- navigate through directories
- move stacks between directories
- double click to view full size page image (also on right pane)
- print stacks and pages, including page annotations
- email files as PDF with your mail program, or Gmail (Ctrl+Shift+E)
- copy files as PDF to the clipboard (Ctrl+C, paste anywhere)
- full undo/redo
- fast scanning from sheet-fed scanners, through SANE on Linux and macOS
  and TWAIN on Windows, with presets; pages cut to the sheet and
  straightened, colourless pages stored as grey or mono, blank pages left
  out, sheets fed sideways turned upright, and a misfeed or jam waiting to
  be cleared without losing pages (see the
  [scanning guide](https://paperman.readthedocs.io/en/latest/scanning.html))
- scanning from the command line (`--scan`)
- OCR with full-text search: scanned pages are read in the background
  with tesseract, each page's text is shown beside it, stacks can be found
  by the words on their pages, and the PDFs paperman makes can be searched
  and their text copied
- a search server whose repositories the desktop can show and change over
  the network, with user accounts which can be limited to some repositories
- an app for Android and iPhone, for browsing, searching and reading the
  server's repositories, and scanning into them with the phone's camera


See the [release history](https://paperman.readthedocs.io/en/latest/history.html)
for what has changed.

## Command line

Paperman can also be used without its window, for example:

    paperman -p "power bill.max"                 # convert to PDF
    paperman --scan --repo ~/paper --dir inbox   # scan into a folder
    paperman --ocr ~/paper                       # read the text of each stack
    paperman --search invoice ~/paper            # find it again

`paperman --help` lists every option and the
[command-line guide](https://paperman.readthedocs.io/en/latest/cli.html)
goes through them.

## Windows

There is an installer for x64 PCs (`paperman-setup-VERSION.exe`) and one
for Windows on Arm (`paperman-setup-VERSION-arm64.exe`), with each release
from 1.4.0 on, and built by every CI run; it needs
Windows 10 version 1809 or later, and no administrator. To scan, install
the scanner's 64-bit TWAIN driver: for the Ricoh fi-series that is
PaperStream IP (TWAIN x64), a separate download from the 32-bit one.
Sign out and back in after installing it, since PaperStream only sets the
scanner up for TWAIN when the user logs in.

To build it, use the MSYS2 MinGW64 toolchain, which is also what the CI
job uses. In a MINGW64 shell:

```
pacman -S make mingw-w64-x86_64-gcc mingw-w64-x86_64-pkgconf \
    mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-scxml mingw-w64-x86_64-qt6-svg \
    mingw-w64-x86_64-poppler-qt6 mingw-w64-x86_64-podofo \
    mingw-w64-x86_64-libtiff mingw-w64-x86_64-libjpeg-turbo \
    mingw-w64-x86_64-tesseract-ocr mingw-w64-x86_64-tesseract-data-eng
qmake6 paperman.pro -o Makefile.win
make -f Makefile.win
```

`make -f Makefile.win installer` builds the installer too, with Inno
Setup installed.

Scanners are driven through TWAIN rather than SANE: every TWAIN data
source installed on the machine appears in the device list. The TWAIN
back end lives in win32/twainsane.cpp and presents the same options as
SANE's fujitsu backend.

When bringing up a new scanner, run paperman from a shell with
`PAPERMAN_TWAIN_DEBUG=1` set: it logs every TWAIN operation and the
capabilities the driver reports to stderr. If colours come out with red and
blue swapped, set `PAPERMAN_TWAIN_RGB=1`. `PAPERMAN_TWAIN_DUMP=<file>` writes
the first scanned image as a PBM/PGM/PPM, exactly as the back end hands it to
paperman, which separates driver problems from filing problems. Only the TWAIN 2 data source
manager (TWAINDSM.dll, installed with any current driver) is used; set
`PAPERMAN_TWAIN_LEGACY=1` to fall back to the twain_32.dll shipped with
Windows.

## macOS

There is a disk image for Apple Silicon (`Paperman-VERSION-arm64.dmg`) and
one for Intel Macs (`Paperman-VERSION-x86_64.dmg`), with each release from
1.4.0 on, and built by every CI run. It carries its own
SANE, with the fujitsu and finet back ends from the fork which knows the
fi-8950; `scripts/mac-package.sh` makes it from a built `paperman.app`.

Paperman builds on macOS with Qt 6 and Homebrew. Homebrew's Poppler has
no Qt bindings, so Poppler is built separately:

```
brew install pkgconf podofo libtiff jpeg-turbo sane-backends cmake ninja \
    openjpeg little-cms2 fontconfig
# Qt 6 from qt.io, or with aqtinstall:
#    aqt install-qt mac desktop 6.11.3 clang_64 -m qtscxml -O ~/Qt
curl -LO https://poppler.freedesktop.org/poppler-26.09.0.tar.xz
tar xf poppler-26.09.0.tar.xz
cmake -S poppler-26.09.0 -B pbuild -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/poppler \
    -DCMAKE_PREFIX_PATH="$HOME/Qt/6.11.3/macos;$(brew --prefix)" \
    -DENABLE_QT5=OFF -DENABLE_GLIB=OFF -DENABLE_NSS3=OFF \
    -DENABLE_GPGME=OFF -DENABLE_LIBCURL=OFF -DENABLE_BOOST=OFF \
    -DENABLE_HARFBUZZ=OFF
ninja -C pbuild install
export PKG_CONFIG_PATH=$HOME/poppler/lib/pkgconfig:$(brew --prefix)/lib/pkgconfig
~/Qt/6.11.3/macos/bin/qmake6 paperman.pro -o Makefile
make -f Makefile
```

That makes `paperman.app`; a build with `CONFIG+=test` makes a plain
`paperman` instead, for the tests. Use `make -f Makefile`, since the
GNUmakefile in the repository builds the other programs too. Scanners
are reached through SANE, as on Linux, from Homebrew's sane-backends;
a scanner too recent for that release, such as the fi-8950, needs a
newer SANE built from source.
On an Intel Mac, Homebrew has no ready-built packages for the latest
macOS, so `brew install` needs `--build-from-source`.

## Icons

The icons are from [Tabler Icons](https://tabler.io/icons) (MIT licence,
see images/icons/LICENSE.tabler), with the scanner icons drawn to match.
They are SVGs, in images/icons, drawn in the theme's text colour, so the
same icon suits a light or a dark theme.

## Code signing policy

Free code signing provided by [SignPath.io](https://about.signpath.io),
certificate by [SignPath Foundation](https://signpath.org).

Only the Windows program and installer which this project builds are
signed, by its release workflow, once an approver approves each
release. Committers, reviewers and approvers: Simon Glass
([@sjg20](https://github.com/sjg20)).

Once a day the desktop program asks getpaperman.com whether there is a
newer version, sending its version, the operating system, the processor
and the kind of package; this can be turned off in the options. Otherwise
this program will not transfer any information to other networked systems
unless specifically requested by the user or the person installing or
operating it. See the [privacy policy](https://getpaperman.com/privacy-policy/).

See [the full policy](doc/code-signing.rst) for how releases are signed.

## Building

The [build guide](https://paperman.readthedocs.io/en/latest/build.html)
covers each platform and component.

## Testing

Paperman includes a comprehensive test suite to verify functionality. Tests are built
using the Qt Test framework.

### Building with Tests

To build paperman with test support enabled:

    qmake6 paperman.pro CONFIG+=test -o Makefile
    make -f Makefile paperman

### Running Tests

Run all tests:

    env QT_QPA_PLATFORM=offscreen ./paperman -t

This will run all test suites including:
- **utils** - Utility function tests
- **ops** - File operation tests (duplicate, stack, unstack, rename, etc.)
- **file** - Reading and writing each file type
- **searchserver** - HTTP search server tests
- **ocrsearch** - OCR and the search index

Run one suite, or one test, by naming its class:

    env QT_QPA_PLATFORM=offscreen ./paperman -t TestOcrSearch
    env QT_QPA_PLATFORM=offscreen ./paperman -t TestOcrSearch::testOcrSearch

The tests which run tesseract are skipped if it is not installed. All tests use
temporary directories and clean up after themselves.

`make coverage` measures how much of the C++ code the tests run; master's
report is [published](https://sjg20.github.io/paperman/coverage/).


## Future Features

Here's what I'd like it to support:

- more operations on images

If you have other ideas or find a problem, please open an
[issue](https://github.com/sjg20/paperman/issues).
