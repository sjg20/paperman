Paper Manager
=============

This program provides a viewer for image files such as pdf, jpeg and
its own variant of PaperPort's .max file format.

See the INSTALL file for installation instructions.

Desktop view | Select scanner | Dated files | Open file | Preview | Preview2
------------ | -------------- | ----------- | --------- | ------- | --------
![1](doc/1.jpeg) | ![2](doc/2.jpeg) | ![3](doc/3.jpeg) | ![4](doc/4.jpeg) | ![5](doc/5.png) | ![6](doc/all.png)

Features
--------

- simple GUI based around stacks and pages
- view previews and browse through pages
- move pages in and out of stacks
- navigate through directories
- move stacks between directories
- double click to view full size page image (also on right pane)
- print stacks and pages, including page annotations
- full undo/redo


new in 0.4:
- basic scanning
- partial creation of .max files (but monochrome images are uncompressed)
       (also it doesn't create greyscale/colour previews)


new in 0.5:
- better scanning, should now work properly with SANE and most scanners
- stacking/unstacking, moving files between directories, etc.
- colour previews are created (greyscale still not sorry)
- PDF, JPEG and TIFF conversion supported. Results may vary
- View quality menu to trade speed for quality


new in 0.7:
- better multithreaded scanning
- undo/redo
- better PDF support
- preview panes
- OCR engine
- zooming in and out in preview windows (hold down control and use
     mouse wheel)
- rewritten for QT4
- many many other changes and improvements


new in 0.7.1:
- a few bug fixes


new in 0.8
- first github release
- new icon
- cleaned up README


new in 1.0
- fully ported to QT4
- should build on very modern distributions that don't have QT3


new in 1.0.1
- builds with QT5, including on Ubuntu 20.04 Focal

new in 1.1
- presets for scanning, use Ctrl-1 to Ctrl-6 to quickly select
- fast folder finding in the scanning dialogue
   - type the partial match in the 'Folder' field (press F6 to get there)
   - it expects directories to be called yyyy and mmMMM, e.g. 2024/03mar/...
   - press <enter> to scan, or click on the match to just create the dir
- filtering out of directories which don't match the current year / month
- keyboard shortcuts for A4 (Alt-A) and US letter/legal toggle (Alt-L)
- Fix most warnings

Future Features
---------------

Here's what I'd like it to support:

- more operations on images
- support for more image types (at the moment only JPEG is supported)

If you have other ideas then feel free to let me know on the mailing list.



Simon Glass
sjg@chromium.org
Aug 2020
