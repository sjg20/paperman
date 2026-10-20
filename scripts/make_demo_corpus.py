#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""Make a repository of papers for demonstrating paperman

The papers belong to a made-up household and cover about a year: bills,
statements, insurance, a manual, school reports, a handwritten letter,
receipts, a long tenancy agreement and so on, in a folder for each kind
of paper. Paper documents are scanned into .max stacks through
paperman's fake scanner, so they are stored just as a real scan stores
them; the rest are PDFs, as they would arrive by e-mail. See
doc/corpus.rst for the list.

Needs reportlab, Pillow, numpy and pdftoppm (poppler-utils), plus a
paperman built with its fake scanner, which is Linux only. With
faketime installed the pages carry the dates of the documents.

Usage:
    scripts/make_demo_corpus.py ~/paper-demo
    scripts/make_demo_corpus.py --only tenancy ~/paper-demo
"""

import argparse
import os
import random
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# pylint: disable=wrong-import-position
from corpus import catalogue
from corpus.pdftext import words
from corpus.scanner import (Hand, image_pdf, paper, render, scan_stack,
                            stamp)

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def make(spec, out, work, paperman, seed, verbose):  # pylint: disable=too-many-arguments
    """Make one document of the corpus

    Args:
        spec (catalogue.Spec): Document to make
        out (str): Repository directory
        work (str): Scratch directory
        paperman (str): Path to the paperman program
        seed (int): Seed for the randomness, so a run can be repeated
        verbose (bool): True to show paperman's output
    """
    rng = random.Random(f'{seed}:{spec.folder}:{spec.name}:{spec.when}')
    folder = os.path.join(out, spec.folder)
    os.makedirs(folder, exist_ok=True)
    clean = os.path.join(work, 'clean.pdf')

    if spec.kind == 'pdf':
        path = os.path.join(folder, f'{spec.name}.pdf')
        spec.build(path)
        stamp(path, spec.when)
        return

    layout = {}
    if spec.pages:
        pages = spec.pages(spec.dpi)
    else:
        spec.build(clean)
        pages = render(clean, spec.dpi)
        layout = words(clean)
    if spec.marks:
        for num, img in enumerate(pages):
            spec.marks(num, Hand(img, spec.dpi, layout.get(num + 1)))
    pages = [paper(img, rng, spec.tint, fade=spec.fade) for img in pages]

    if spec.kind == 'image-pdf':
        image_pdf(os.path.join(folder, f'{spec.name}.pdf'), pages, spec.dpi,
                  spec.when, spec.name)
        return

    if spec.duplex:
        sheets = [(pages[i], pages[i + 1] if i + 1 < len(pages) else None)
                  for i in range(0, len(pages), 2)]
    else:
        sheets = [(page, None) for page in pages]
    scan_stack(paperman, out, spec.folder, spec.name, sheets, spec.when,
               work, spec.dpi, spec.sideways, spec.settings, verbose)


def main():
    """Make the corpus, as the arguments ask"""
    parser = argparse.ArgumentParser(
        description=__doc__.split('\n', maxsplit=1)[0])
    parser.add_argument('outdir', help='directory to make the repository in')
    parser.add_argument('--only', action='append', default=[],
                        help='make only documents whose folder or name '
                        'contains this (may be repeated)')
    parser.add_argument('--paperman', default=os.path.join(ROOT, 'paperman'),
                        help='paperman program to scan with')
    parser.add_argument('--seed', type=int, default=1, help='random seed')
    parser.add_argument('--keep-work', action='store_true',
                        help='keep the scratch directory, and say where')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help="show paperman's output")
    args = parser.parse_args()

    specs = catalogue.documents()
    if args.only:
        specs = [s for s in specs
                 if any(o in f'{s.folder}/{s.name}' for o in args.only)]
        if not specs:
            parser.error('no documents match --only')
    if any(s.kind == 'max' for s in specs) and not os.access(args.paperman,
                                                             os.X_OK):
        parser.error(f'cannot run {args.paperman}: build paperman first, or '
                     'use --paperman')
    if not shutil.which('faketime'):
        print('faketime is not installed, so scanned pages carry today\'s '
              'date', file=sys.stderr)

    out = os.path.abspath(args.outdir)
    os.makedirs(out, exist_ok=True)
    work = tempfile.mkdtemp(prefix='corpus-')
    try:
        for num, spec in enumerate(specs, 1):
            print(f'[{num}/{len(specs)}] {spec.folder}/'
                  f'{spec.name or "(scanned name)"} ({spec.kind})',
                  flush=True)
            make(spec, out, work, os.path.abspath(args.paperman), args.seed,
                 args.verbose)
    finally:
        if args.keep_work:
            print(f'Scratch files are in {work}')
        else:
            shutil.rmtree(work)


if __name__ == '__main__':
    main()
