Scanning
========

Paperman scans straight into a stack: pages appear in the window as the
scanner delivers them, and the stack is saved in the folder chosen for
it. It is written for sheet-fed document scanners, such as the Ricoh
(formerly Fujitsu) fi-series, but any scanner the platform can drive will
do.

Scanners on each platform
-------------------------

Linux
   Scanners are reached through `SANE <http://www.sane-project.org>`_,
   using its back end for the scanner. Install ``sane-utils`` and check
   that ``scanimage -L`` lists the scanner.

Windows
   Scanners are reached through their TWAIN driver: every TWAIN data
   source installed on the machine appears in the scanner list. Paperman
   is a 64-bit program, so it needs the 64-bit driver; for the fi-series
   that is **PaperStream IP (TWAIN x64)**, a separate download from the
   32-bit PaperStream IP (TWAIN) on Ricoh's site. After installing it,
   sign out and in again (or restart) before looking for the scanner:
   PaperStream sets the scanner up for TWAIN when the user logs in, and
   until then no scanner is listed.

macOS
   Scanners are reached through SANE, as on Linux, from Homebrew's
   ``sane-backends``. Some recent scanners are not yet known to the SANE
   release Homebrew has, in which case a newer SANE, built from source,
   is needed.

The first time Scan is pressed, Paperman asks which scanner to use, and
remembers it after that. **Select scanner...** in the scan panel picks
another.

The scan panel
--------------

Pressing the scan button in the toolbar opens the scan panel beside the
window.

Stack name, Page name and Folder
   What to call the new stack and its pages, and where to put it. Type in
   the Folder field (Ctrl-F or F4 reaches it) to find or create a folder:
   folders named by year and month, such as ``2026/09sep``, are matched as
   you type, and Enter scans straight into the match.

Presets
   Keep a set of scan settings under a name. Ctrl-1 to Ctrl-6 select the
   first six.

Mono, Dither, Grey and Colour
   The kind of image to scan.

Auto colour
   When scanning in colour, store each page with no colour on it as grey,
   and one with no shading either as mono, so that a stack of letters
   takes little space while a page with a coloured logo, a photo or a note
   in coloured pen stays in colour.

ADF and Duplex
   Feed sheets from the document feeder, and scan both sides of each.

Auto size
   Let the scanner find the size of each sheet and end the page at its
   foot. With this on, the size chosen is the most a page can be, and is
   shown as **Max size**.

Straighten
   Have the scanner straighten each sheet and cut the page down to it,
   corners and all. Sheets go through more slowly, since the scanner
   reads each one in full before passing it on.

DPI
   300 gives a good scan; 200 is fax quality.

Size
   The paper size. Alt-A selects A4 and Alt-L toggles US Letter and
   Legal. While the scanner is cutting pages to the sheet (Auto size or
   Straighten) and sheets are fed upright, there is also a **Long** size,
   as wide and as long as the scanner will take, for sheets longer than
   any paper size, such as till receipts.

Feed
   Sheets fed sideways (**Top at left** or **Top at right**) are scanned
   the long way across the scanner and turned upright as they are stored.
   The size chosen describes the page, and the whole page is scanned.

While a scan runs
-----------------

The panel shows each page's coverage, how many pages have come and how
fast (pages per second over the last ten seconds).

- **Stop** stops after the page being scanned. Sheets which the scanner
  has already taken in are still scanned and kept.
- **Cancel** abandons the scan.
- If the feeder misfeeds or jams, Paperman says so and waits: clear it,
  put the sheets back and press **Scan** to carry on into the same stack.
  A page cut short by a jam is kept at the length that was scanned.

Blank pages
   The Options dialog says what to do with a blank page: record it,
   leave it out if it is the back of a sheet, or leave it out wherever it
   is.

Scanning without the window
---------------------------

``paperman --scan`` scans from the command line with the same settings;
see :doc:`cli`.

When something goes wrong
-------------------------

``--log FILE`` writes everything Paperman would say on the terminal to a
file, including the dialogs it shows, and turns on diagnostics which a
normal run keeps quiet. With ``--sane-debug LEVEL`` the scanner back end
logs its commands too (15 shows every command and what the scanner
answered). A log from a scan which went wrong is the best thing to send
with a report.

Some environment variables help too:

``PAPERMAN_SCAN_STATS=1``
   Print statistics at the end of each scan: time per side, and how far
   the display fell behind the scanner. The first thing to look at if
   scanning seems slow.

``PAPERMAN_TWAIN_DEBUG=1`` (Windows)
   Log every TWAIN operation and the capabilities the driver reports.

``PAPERMAN_TWAIN_RGB=1`` (Windows)
   For a driver which swaps red and blue.

A fake scanner
--------------

On Linux, ``paperman --fake-scanner DIR`` offers a fake Fujitsu
scanner beside the real ones, fed from pictures dropped into
``DIR/hopper``. It is used by the tests and is handy for trying things
without paper; see :doc:`testing`.
