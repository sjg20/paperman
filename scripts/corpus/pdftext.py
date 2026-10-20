# SPDX-License-Identifier: GPL-2.0-or-later

"""Find where words are on the pages of a PDF

Handwriting is placed by the printed words it goes with, rather than at
fixed positions, so that it stays in place if a layout changes.
"""

import collections
import html
import re
import subprocess

Word = collections.namedtuple('Word', 'x0 y0 x1 y1 text')


def words(pdf):
    """Get the words on each page of a PDF, with where they are

    Args:
        pdf (str): Path to the PDF

    Returns:
        dict: page number (from 1) -> list of Word, positions in points
            from the top left
    """
    out = subprocess.run(['pdftotext', '-bbox', pdf, '-'], check=True,
                         capture_output=True, text=True).stdout
    pages = {}
    for num, page in enumerate(out.split('<page ')[1:], 1):
        pages[num] = [
            Word(float(x0), float(y0), float(x1), float(y1),
                 html.unescape(text))
            for x0, y0, x1, y1, text in re.findall(
                r'xMin="([\d.]+)" yMin="([\d.]+)" xMax="([\d.]+)" '
                r'yMax="([\d.]+)">([^<]*)<', page)]
    return pages


def find(page_words, phrase, nth=0):
    """Find a run of words on a page

    Args:
        page_words (list of Word): Words on the page, in reading order
        phrase (tuple of str): Words to find, in order
        nth (int): Which match to return, from 0

    Returns:
        Word: The last word of the match

    Raises:
        ValueError: There are not that many matches
    """
    count = 0
    for start in range(len(page_words) - len(phrase) + 1):
        if all(page_words[start + i].text == part
               for i, part in enumerate(phrase)):
            if count == nth:
                return page_words[start + len(phrase) - 1]
            count += 1
    raise ValueError(f'cannot find {" ".join(phrase)!r} (match {nth})')
