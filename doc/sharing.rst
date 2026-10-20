Sharing
=======

Papers kept in Paperman can be printed, emailed, copied to other programs
and saved as PDFs or pictures.

Printing
--------

Press **Ctrl+P**, or the print button in the main toolbar. In the
desktop view this prints the stacks selected; in the page view it
prints the page shown. A stack in a folder on a server is fetched first.

The print dialog is your system's own, with a tab of Paperman's options
added:

.. image:: images/print-options.png
   :alt: Paperman's options for printing

.. list-table::
   :widths: 35 65

   * - **Odd Pages**, **Even Pages**
     - Which pages to print, for printing both sides by hand: print the
       odd pages, put the paper back in, then the even ones
   * - **Each stack on separate sheet**
     - When printing on both sides, start each stack on a new sheet
   * - **Shrink to fit**, **Expand to fit**
     - Shrink a page which is too big for the paper, or enlarge one
       which is too small
   * - **Page numbers**
     - Print the page number at the bottom right of each page
   * - **Stack and page names**
     - Print the names of the stack and the page
   * - **Sequence information**
     - Print a sequence number for the print job on every page
   * - **Image information**
     - Print the size, resolution and depth of each page's picture
   * - **Image timestamp**
     - Print when each page was scanned
   * - **Text Font**, **Text Colour**, **Text Background**
     - How the printed names and numbers look

Press **Save** in the print dialog to keep these settings for next time.

Emailing
--------

The **Email...** submenu, in a stack's right-click menu, sends the stacks
selected:

.. image:: images/email-menu.png
   :alt: The Email submenu

.. list-table::
   :widths: 35 65

   * - **Files** (Ctrl+E)
     - Send the stacks as they are kept
   * - **as Max** (Ctrl+Alt+E)
     - Send them as ``.max`` stacks
   * - **as PDF** (Ctrl+Shift+E)
     - Send them as PDFs, which anyone can open
   * - **Copy as PDF** (Ctrl+C)
     - Put them on the clipboard as PDFs, to paste anywhere (see below)

Paperman makes the file, puts it on the clipboard and opens a new
message in Gmail in your web browser. Click in the message and press
**Ctrl+V** to attach the file. More than one stack is packed into a
single zip file first, since a message takes one file pasted at a time.

The clipboard is the only way to attach a file to a message opened from
another program, so the same works with other webmail which takes a
pasted file, such as Outlook on the web or Fastmail.

Copying
-------

**Copy as PDF** (Ctrl+C) turns the stacks selected into PDFs and puts
them on the clipboard, as emailing does but without opening a message.
Paste them wherever a file is wanted: a message, a file manager or a
chat. More than one stack is packed into a single zip file.

Saving as a PDF or pictures
---------------------------

To keep a stack as a PDF, or its pages as JPEG pictures, beside it in the
same folder, use **Duplicate... > as PDF** (Ctrl+Shift+P) or **as JPEG**
(Ctrl+Shift+J) in the stack's right-click menu (see :doc:`filing`). A
PDF made by Paperman keeps the text read from its pages, so it can be
searched and its text copied.

From the command line, ``paperman -p`` turns a stack into a PDF and
``paperman -j`` into JPEGs (see :doc:`cli`).
