The Demo Corpus
===============

Screenshots, videos and documentation need papers to show, and they
cannot be anyone's real ones. ``scripts/make_demo_corpus.py`` makes a
repository of papers belonging to a made-up household, the Reyes family
of Cedar Ridge, Oregon, covering about a year:

.. code:: bash

   scripts/make_demo_corpus.py ~/paper-demo

It takes a few minutes. ``--only`` makes just the documents whose folder
or name contains a word, which is handy when changing one of them:

.. code:: bash

   scripts/make_demo_corpus.py --only tenancy ~/paper-demo

How it works
------------

Each document is laid out with reportlab. Paper which would have come
through the letterbox is then rendered, given the look of paper (a tint,
some grain, a few specks of dust), written on by hand where someone
would, and fed through paperman's fake scanner (see :doc:`testing`), so
the .max stack is made exactly as a real scan makes it: each side is cut
to the sheet, blank backs are left out and each page is stored in
colour, grey or mono as auto-colour decides. Papers which would have
arrived by e-mail or been downloaded are kept as the PDFs they are.

Paperman runs with a home directory of its own, so the scans neither use
nor change your settings. With ``faketime`` installed, each scan seems to
happen on the document's date, which is then what the pages are named
after; without it they carry today's date. Every file is given the
document's date as its modification time.

The script needs reportlab, Pillow, numpy and ``pdftoppm`` (from
poppler-utils), and a paperman built on Linux, where the fake scanner is
built. Runs with the same ``--seed`` give the same pages.

What is in it
-------------

.. list-table::
   :header-rows: 1
   :widths: 18 28 8 46

   * - Folder
     - Document
     - Kind
     - What it shows
   * - bills/power
     - Three electricity bills
     - max, pdf
     - Stacks named by date; searching for the account number finds all
       three, the third arriving by e-mail
   * - bills/internet
     - Broadband bill
     - pdf
     - Tables in a PDF with text
   * - bills/water
     - Water and sewer bill
     - max
     - A black-only page, stored as mono
   * - banks
     - Bank statement, credit card statement
     - pdf, max
     - Searching for a payee; a charge queried in pen
   * - taxes
     - Tax return summary, donation receipt
     - pdf, max
     - A form-like layout; a half-size slip fed sideways and turned
       upright
   * - insurance
     - Homeowners policy (11 pages), car insurance renewal
     - max
     - Paging through a long stack; searching for "water damage" or
       "deductible"
   * - house
     - Kitchen quote, property tax statement, tenancy agreement
     - max
     - Pencil notes; the 63-page tenancy agreement, filled in by hand,
       initialled on every page and signed
   * - manuals
     - Dishwasher manual (12 pages), heat pump warranty card
     - pdf, max
     - Searching for "warranty" or "filter"; a card smaller than the
       page
   * - car
     - Garage invoice
     - max
     - Searching by number plate
   * - medical
     - Blood test results
     - pdf
     - A PDF of scanned images, which only OCR can read
   * - school
     - Report card, child's crayon drawing
     - max
     - Colour, and a page with no text at all
   * - letters
     - Handwritten letter from Grandma
     - max
     - Handwriting on cream paper, where OCR does poorly
   * - receipts
     - Two till receipts
     - max
     - Long, narrow sheets of faded thermal paper
   * - recipes
     - Recipe card with a photo
     - max
     - A small card in colour
   * - photos
     - An old photograph
     - max
     - A photo with writing on its back
   * - travel
     - Flight itinerary
     - pdf
     - An e-mail attachment
   * - inbox
     - Three unsorted scans
     - max
     - Stacks still with the names the scanner gave them, for showing
       how to file them

Everyone in the corpus is made up. Telephone numbers are in the 555-01xx
range kept for fiction, e-mail addresses are at example.com and no
company or brand is real, so nothing in it can be taken for a real
record. The household lives in ``scripts/corpus/household.py``.

Sources
-------

The tenancy agreement is the government's Model Agreement for an Assured
Shorthold Tenancy (January 2020 edition), which the family signed while
renting in England. It contains public sector information licensed
under the `Open Government Licence v3.0
<https://www.nationalarchives.gov.uk/doc/open-government-licence/>`_.

The handwriting is in Caveat, by the Caveat Project Authors, under the
SIL Open Font License; see ``scripts/corpus/fonts/OFL.txt``.
