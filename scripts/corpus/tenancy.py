# SPDX-License-Identifier: GPL-2.0-or-later

"""Fill in the government's model tenancy agreement

The agreement is the Model Agreement for an Assured Shorthold Tenancy
which the UK government published on GOV.UK (January 2020 edition). It
is Crown copyright, reused under the Open Government Licence v3.0:

    Contains public sector information licensed under the Open
    Government Licence v3.0.
    https://www.nationalarchives.gov.uk/doc/open-government-licence/

The family rented a flat in England while working there. The pages are
rendered as a mono laser printer would print them and the blanks are
filled in by hand: each is found by the words printed around it, so the
fill does not depend on exact positions.
"""

import os

from PIL import ImageOps

from corpus import household as hh
from corpus.pdftext import find, words
from corpus.scanner import PEN_BLACK, PEN_BLUE, Hand, render

SOURCE = os.path.join(os.path.dirname(__file__), 'sources',
                      'model-tenancy-agreement.pdf')

TENANTS = hh.ADULTS
SIGNED = '20 August 2020'
START = '1 September 2020'
END = '31 August 2021'
WEEKLY = '311.54'
MONTHLY = '1,350.00'
DEPOSIT = '1,557.69'

# (page, words to find, which match, text to write, dx, dy)
# The text goes dx points to the right of the last word found and dy
# points below its top. A text of 'X' crosses a box instead.
FILLS = (
    (12, ('Landlord', '(insert', 'name)'), 0, hh.UK_LANDLORD, 14, -6),
    (12, ('Tenant', '1', '(insert', 'name)'), 0, TENANTS[0], 14, -6),
    (12, ('Tenant', '2', '(insert', 'name)'), 0, TENANTS[1], 14, -6),
    (13, ('Adult', '1'), 0, 'None', 90, -6),
    (13, ('(insert', 'number)'), 0, '4', 4, -14),
    (14, ('Property:[',), 0, ', '.join(hh.UK_ADDRESS) + ', 2 bed 1st floor '
     'flat', -395, 12),
    (14, ('Part', 'furnished', '['), 0, 'X', 0.5, 1),
    (15, ('Private', 'garden', '['), 0, 'X', 0.5, 1),
    (17, ('begins', 'on:', '(insert', 'date)'), 0, START, 8, -6),
    (17, ('ends', 'on:', '(insert', 'date)'), 0, END, 8, -6),
    (20, ('rent', 'is', '£'), 0, WEEKLY, 2, -6),
    (20, ('which', 'is', '£'), 0, MONTHLY, 2, -6),
    (22, ('made', 'on', '['), 0, START, 4, -6),
    (22, ('made', 'on', '['), 1, '1st of each month', 4, -6),
    (22, ('beginning', 'on', '['), 0, '1 October 2020', 4, -6),
    (24, ('deposit', 'of', '£'), 0, DEPOSIT, 2, -6),
    (50, ('Landlord', 'is:'), 0, ', '.join(hh.UK_LANDLORD_ADDRESS), -120,
     14),
    (51, ('purposes', 'is:'), 0, hh.UK_LANDLORD_EMAIL, -150, 14),
    (51, ('purposes', 'is:'), 1, 'daniel.reyes@example.com', -150, 14),
    (52, ('emergencies', 'is:'), 0, f'{hh.UK_LANDLORD_PHONE} (landlord)',
     6, -6),
)


def pages(dpi=300):
    """Render the agreement as printed, filled in and signed

    Args:
        dpi (int): Resolution

    Returns:
        list of Image: One page per sheet, in order
    """
    layout = words(SOURCE)
    imgs = [ImageOps.grayscale(img).convert('RGB')
            for img in render(SOURCE, dpi)]
    for page, phrase, nth, txt, dx, dy in FILLS:
        word = find(layout[page], phrase, nth)
        hand = Hand(imgs[page - 1], dpi)
        if txt == 'X':
            hand.cross(word.x0 + dx, word.y0 + dy, 7)
        else:
            hand.write(word.x1 + dx, word.y0 + dy, txt, 13, PEN_BLACK)

    _sign(imgs, layout, dpi)

    # the parties initial each page of the agreement itself
    for num in range(11, len(imgs) + 1):
        hand = Hand(imgs[num - 1], dpi)
        hand.write(440 + (num % 3) * 4, 800 - (num % 2) * 3, 'DR  LR  ME', 12,
                   PEN_BLACK, angle=(num % 5) - 2)
    return imgs


def _sign(imgs, layout, dpi):
    """Sign the agreement: the tenants on page 53, the landlord on 54"""
    page = layout[53]
    hand = Hand(imgs[52], dpi)
    for num, name in enumerate(TENANTS):
        # each tenant's block comes before its witness's, in reading order
        sig = find(page, ('Signature:',), num * 2)
        hand.signature(sig.x1 + 10, sig.y0 - 6, name, 20, PEN_BLUE)
        cap = find(page, ('capitals):',), num * 2)
        hand.write(cap.x1 + 6, cap.y0 - 6, name.upper(), 12, PEN_BLACK)
        addr = find(page, ('Address:',), num * 2)
        hand.write(addr.x1 + 6, addr.y0 - 6, ', '.join(hh.UK_ADDRESS[:2]),
                   11, PEN_BLACK)
        date = find(page, ('Date:',), num * 2)
        hand.write(date.x1 + 6, date.y0 - 6, SIGNED, 12, PEN_BLACK)

    page = layout[54]
    hand = Hand(imgs[53], dpi)
    sig = find(page, ('Signature:',), 0)
    hand.signature(sig.x1 + 10, sig.y0 - 6, hh.UK_LANDLORD, 20, PEN_BLUE)
    cap = find(page, ('capitals):',), 0)
    hand.write(cap.x1 + 6, cap.y0 - 6, hh.UK_LANDLORD.upper(), 12, PEN_BLACK)
    addr = find(page, ('Address:',), 0)
    hand.write(addr.x1 + 6, addr.y0 - 6, ', '.join(hh.UK_LANDLORD_ADDRESS[:2]),
               11, PEN_BLACK)
    date = find(page, ('Date:',), 0)
    hand.write(date.x1 + 6, date.y0 - 6, SIGNED, 12, PEN_BLACK)
