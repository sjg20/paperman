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

## Building and testing

The [developer guide](https://paperman.readthedocs.io/en/latest/develop.html)
lists what is needed, and the
[build guide](https://paperman.readthedocs.io/en/latest/build.html) covers
each platform. To build on Linux with Qt 6 and run the tests:

    qmake6 paperman.pro CONFIG+=test -o Makefile
    make -f Makefile paperman
    env QT_QPA_PLATFORM=offscreen ./paperman -t

The [testing guide](https://paperman.readthedocs.io/en/latest/testing.html)
has more, and master's test
[coverage](https://sjg20.github.io/paperman/coverage/) is published.

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

## Future Features

Here's what I'd like it to support:

- more operations on images

If you have other ideas or find a problem, please open an
[issue](https://github.com/sjg20/paperman/issues).
