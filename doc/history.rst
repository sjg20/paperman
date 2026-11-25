Release History
===============

What has changed in each release of Paperman, newest first.

1.5.1
-----

- a scan after changing the scan mode no longer comes out blank: the
  change set the brightness to its limit
- adding stacks after searching more than once no longer crashes
- the scanner's Scan button keeps working after a jam
- a large page can be scrolled to its edges in the page view
- pages of a PDF stack can be deleted with the tick beside each, with undo
- a JPEG stack keeps all its pages when it is copied, moved, deleted or
  brought back, and a copy made as JPEG can be deleted
- 'Reverse pages' in a stack's menu puts its pages in the opposite order
- --log also logs each page OCR reads, each step done, undone and redone,
  and the rows added to and removed from the desktop

1.5.0
-----

- the text of scanned pages is read in the background (tesseract), and each
  page keeps its words, with where each is, in its .max file, so they go
  with the page when it is stacked, unstacked, deleted or copied
- a 'Read text (OCR)' action for reading stacks scanned before
- the OCR pane in the page view shows the text of the page being shown
- searching for stacks by the text on their pages (Edit > Search), each
  found stack turned to the page which matches best
- the PDFs paperman makes, and those the server makes, have an invisible
  layer of text, so they can be searched and their text copied
- a stack's pages can be dragged into a new order in the page list
- opening a folder of long stacks reads far less of each file, which is most
  noticeable over a network
- each view mode of the page list keeps its own layout, and the one last
  chosen is used at the next start
- a server's users can be limited to a part of a repository
- File > Add server attaches a server from the desktop program and
  remembers it
- a check once a day for a newer version, which can be turned off in the
  options
- a page, or every page of a stack, can be cropped, with undo
- a page of a stack can be marked as its cover, which is what the desk
  then shows for the stack
- files dropped from another program go into the folder as stacks, or
  into the stack they are dropped on; stacks and files can be dropped
  among a stack's pages
- stacks can be moved or copied to a folder found by typing part of its
  name (Ctrl+M), and Ctrl+F goes to a folder in the same way
- stacks can be sent by email with the system's mail program or Gmail
- pages can be scanned with a document camera, which finds each page in
  the picture and cuts it out, including the two pages of an open book;
  this is new and on Linux only
- the resolution each page is scanned at is kept with it
- new icons, with a choice of a light or dark look, or the desktop's
- the main toolbar has one scan button; Scan now is in the File menu (F5)
- working with a server no longer holds up the window: folders, stacks
  and pages appear as they arrive, with the state of the connection in
  the status bar
- more can be done in a repository on a server: scanning into it, moving,
  copying and converting stacks, making folders, reading and searching the
  text of its pages, and undoing these
- a server keeps each folder's layout, so every desktop shows the same
- a server refuses a desktop program too old or too new for it, and the
  desktop program says why
- the server can serve a demo, in which each login has its own copy of a
  repository for a while
- the phone app logs in to a server which has users, searches the text on
  the pages and scans pages with the phone's camera
- a user guide, with pictures, which is also built as a PDF manual
- a PDF page which draws a form, as tax forms often do, is shown, rather
  than as a blank or red page

1.4.0
-----

- runs on Windows (10 and 11, x64 and Arm, with an installer) and macOS
  (Apple Silicon and Intel, with a disk image)
- an AppImage for Linux distributions other than Ubuntu (x86_64 and
  aarch64), which uses the machine's SANE
- a snap, in the Snap Store (amd64 and arm64), which carries its own SANE
  with the back ends for the newest Ricoh fi-series scanners
- a TWAIN back end for scanners on Windows
- scanning: auto size and straightening by the scanner, auto colour, blank-
  page detection in colour, sideways feeding, a Long size for sheets longer
  than Legal, recovery from misfeeds and jams, Stop keeping the sheets
  already fed, and much faster transfer from Fujitsu scanners
- scanning from the command line, with ``--log`` and ``--sane-debug`` for
  finding out what went wrong
- unfolding scanned booklets into their pages
- working with repositories on a paperman server: browsing, opening,
  changing and scanning into them, with changes shown as they happen
- scan modes Mono, Dither, Grey, Colour and Auto, and presets which keep the
  paper size, feed, auto size, straightening, brightness and contrast too,
  and which can be updated and renamed
- dark icons and colours following the desktop's dark theme
- a first run which offers somewhere to keep papers and lays the window out
  to suit the screen
- a fake Fujitsu scanner for testing without paper (Linux)

1.3.1
-----

- support Ubuntu oracular
- update build scripts to work on 24.04 host
- more tweaks to the findFolders() feature

1.3
---

- Fix problems with dir cache being out of date
- Add more tests for Dirmodel
- Drop the 'recently used' folder
- Set permissions on created files and directories

1.2.1
-----

- Expand tests to include common stacking/unstacking operations
- Fix various bugs in recent new features, particularly files not being
  opened before use

1.2
---

- Import files from outside directories
- Avoid keeping all touched-files open
- Bring back command-line operation
- Presets (Ctrl-1 to Ctrl-6) for basic scanning operation
- Faster folder-selection within the scanning window
- Set the group for files

1.1
---

- presets for scanning, use Ctrl-1 to Ctrl-6 to quickly select
- fast folder finding in the scanning dialogue

  - type the partial match in the 'Folder' field (press F6 to get there)
  - it expects directories to be called yyyy and mmMMM, e.g. 2024/03mar/...
  - press <enter> to scan, or click on the match to just create the dir

- filtering out of directories which don't match the current year / month
- keyboard shortcuts for A4 (Alt-A) and US letter/legal toggle (Alt-L)
- Fix most warnings

1.0.1
-----

- builds with QT5, including on Ubuntu 20.04 Focal

1.0
---

- fully ported to QT4
- should build on very modern distributions that don't have QT3

0.8
---

- first github release
- new icon
- cleaned up README

0.7.1
-----

- a few bug fixes

0.7
---

- better multithreaded scanning
- undo/redo
- better PDF support
- preview panes
- OCR engine
- zooming in and out in preview windows (hold down control and use mouse
  wheel)
- rewritten for QT4
- many many other changes and improvements

0.5
---

- better scanning, should now work properly with SANE and most scanners
- stacking/unstacking, moving files between directories, etc.
- colour previews are created (greyscale still not sorry)
- PDF, JPEG and TIFF conversion supported. Results may vary
- View quality menu to trade speed for quality

0.4
---

- basic scanning
- partial creation of .max files (but monochrome images are uncompressed)
  (also it doesn't create greyscale/colour previews)
