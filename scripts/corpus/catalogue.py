# SPDX-License-Identifier: GPL-2.0-or-later

"""The documents in the demo corpus: where each goes and how it is made

Paper which came through the letterbox is scanned, into a .max stack.
Things which arrived by e-mail or were downloaded are kept as PDFs, with
their text, apart from the blood test, which the clinic sent as a PDF of
scanned images, so only OCR can read it.
"""

import dataclasses
import datetime
import random
from typing import Callable, Optional

from corpus import bills, manual, papers, policy, tenancy
from corpus.scanner import CARD, CREAM, THERMAL, WHITE, at

D = datetime.date


@dataclasses.dataclass
class Spec:  # pylint: disable=too-many-instance-attributes
    """One document in the corpus

    Properties:
        folder (str): Folder in the repository
        name (str): Name of the stack or file, without its extension
        when (datetime.datetime): When it was scanned or saved
        kind (str): 'max' to scan it, 'pdf' to keep the laid-out PDF, or
            'image-pdf' for a PDF of scanned images
        build (callable): Lays out the clean PDF; called with its path
        marks (callable): Writes on page num of the rendered pages;
            called with (num, Hand), or None
        pages (callable): Makes the rendered pages itself, given the
            resolution, instead of build(), or None
        duplex (bool): True if the pages pair up as the front and back
            of each sheet; otherwise each page is a sheet of its own
        tint (tuple of int): Colour of the paper
        dpi (int): Resolution of the scan
        sideways (str): 'left' or 'right' if fed sideways, else None
        fade (float): How faded the print is, 0 to 1
        settings (tuple of str): Extra scanner options, as name=value
    """
    folder: str
    name: str
    when: datetime.datetime
    kind: str
    build: Optional[Callable] = None
    marks: Optional[Callable] = None
    pages: Optional[Callable] = None
    duplex: bool = False
    tint: tuple = WHITE
    dpi: int = 300
    sideways: Optional[str] = None
    fade: float = 0.0
    settings: tuple = ()


def _both(*funcs):
    """Combine several marks() functions into one"""
    def marks(num, hand):
        for func in funcs:
            func(num, hand)
    return marks


MONTHS = ('jan', 'feb', 'mar', 'apr', 'may', 'jun', 'jul', 'aug', 'sep',
          'oct', 'nov', 'dec')

# the bills cover these months, up to the one the screenshots are set in
FIRST_MONTH = D(2025, 10, 1)
LAST_MONTH = D(2026, 10, 1)

# the shop whose till receipt is kept each month, if any, by month
RECEIPTS = {10: 'ridgeline fuel', 11: 'green basket', 12: 'pine and nail',
            1: 'pharmacy', 3: 'oak and ember', 4: 'green basket',
            5: 'ridgeline fuel', 6: 'pine and nail', 7: 'oak and ember',
            8: 'pharmacy', 9: 'green basket'}


def month_folder(when):
    """Get the folder for a month's bills, as the scan panel names them

    Paperman looks for a year and a month such as 2026 and 10oct in a
    folder's path, to suggest the current month's folder to scan into
    and to offer to make it when last month's is all there is.
    """
    return f'bills/{when.year}/{when.month:02d}{MONTHS[when.month - 1]}'


def _months():
    """Yield the first day of each month the bills cover"""
    when = FIRST_MONTH
    while when <= LAST_MONTH:
        yield when
        when = D(when.year + when.month // 12, when.month % 12 + 1, 1)


def monthly_bills():
    """Get a year of bills, filed by year and month

    Each month has the power bill and the broadband bill, the heating
    months a gas bill, every third month a water bill, and most months a
    receipt kept from a shop.
    """
    specs = []
    for month in _months():
        folder = month_folder(month)
        rng = random.Random(month.toordinal())
        bill = month.replace(day=6)
        specs.append(Spec(folder, 'power bill', at(bill.replace(day=9)),
                          'max', lambda p, d=bill: bills.power_bill(p, d),
                          marks=bills.power_marks if month.month == 3
                          else None))
        specs.append(Spec(folder, 'brightwave', at(month, 8, 30), 'pdf',
                          lambda p, d=month: bills.internet_bill(p, d)))
        if month.month in (11, 12, 1, 2, 3):
            gas = month.replace(day=11)
            specs.append(Spec(folder, 'gas bill', at(gas.replace(day=14)),
                              'max', lambda p, d=gas: bills.gas_bill(p, d)))
        if month.month in (1, 4, 7, 10):
            water = month.replace(day=12)
            specs.append(Spec(folder, 'water bill',
                              at(water.replace(day=15)), 'max',
                              lambda p, d=water: bills.water_bill(p, d)))
        shop = RECEIPTS.get(month.month)
        if shop:
            day = rng.randint(2, 19 if month == LAST_MONTH else 27)
            when = at(month.replace(day=day), rng.randint(9, 19),
                      rng.randint(0, 59))
            specs.append(Spec(
                folder, shop, when, 'max',
                lambda p, w=when, s=shop, r=random.Random(day):
                papers.shop_receipt(p, w, s, r),
                tint=THERMAL, fade=rng.uniform(0.15, 0.45)))
    return specs


def documents():
    """Get the documents in the corpus

    Returns:
        list of Spec: The documents, in no particular order
    """
    return monthly_bills() + [
        Spec('banks', 'redwood statement jun26', at(D(2026, 7, 2), 19, 20),
             'pdf', lambda p: bills.bank_statement(p, D(2026, 6, 1))),
        Spec('banks', 'meridian card apr26', at(D(2026, 4, 17)), 'max',
             lambda p: bills.card_statement(p, D(2026, 4, 10)),
             marks=bills.card_marks),
        Spec('taxes', 'tax summary 2025', at(D(2026, 3, 30), 14, 5), 'pdf',
             lambda p: bills.tax_summary(p, 2025)),
        Spec('taxes', 'food pantry receipt', at(D(2026, 6, 26)), 'max',
             lambda p: bills.pantry_receipt(p, D(2026, 6, 25)),
             marks=bills.pantry_marks, sideways='left'),
        Spec('insurance', 'home policy 2026', at(D(2026, 2, 3)), 'max',
             lambda p: policy.home_policy(p, D(2026, 2, 1))),
        Spec('insurance', 'car renewal 2026', at(D(2026, 8, 20)), 'max',
             lambda p: policy.car_renewal(p, D(2026, 8, 17)),
             marks=policy.car_renewal_marks),
        Spec('house', 'kitchen quote', at(D(2026, 3, 18)), 'max',
             lambda p: papers.kitchen_quote(p, D(2026, 3, 16)),
             marks=papers.kitchen_marks),
        Spec('house', 'property tax 2025-26', at(D(2025, 10, 28)), 'max',
             lambda p: papers.property_tax(p, D(2025, 10, 20))),
        Spec('house', 'tenancy agreement 2020', at(D(2026, 2, 21), 15, 0),
             'max', pages=tenancy.pages,
             settings=('page-width=210', 'page-height=297', 'br-x=210',
                       'br-y=297')),
        Spec('manuals', 'dishwasher manual', at(D(2025, 11, 22), 20, 41),
             'pdf', manual.dishwasher_manual),
        Spec('manuals', 'heat pump warranty', at(D(2025, 11, 9)), 'max',
             lambda p: manual.heat_pump_card(p, D(2025, 11, 3)),
             marks=manual.heat_pump_marks, tint=CARD),
        Spec('car', 'auto service mar26', at(D(2026, 3, 13)), 'max',
             lambda p: papers.auto_invoice(p, D(2026, 3, 12))),
        Spec('medical', 'blood test feb26', at(D(2026, 2, 13), 12, 30),
             'image-pdf', lambda p: papers.blood_test(p, D(2026, 2, 10))),
        Spec('school', 'sofia report t1', at(D(2026, 10, 9)), 'max',
             papers.report_card,
             marks=papers.report_marks),
        Spec('school', 'theo drawing', at(D(2026, 5, 30)), 'max',
             papers.drawing,
             marks=papers.drawing_marks, dpi=200),
        Spec('letters', 'letter from grandma', at(D(2026, 5, 9)), 'max',
             papers.letter_from_grandma,
             marks=papers.letter_marks, tint=CREAM),
        Spec('recipes', 'apple cake', at(D(2026, 5, 9), 10, 20), 'max',
             papers.recipe_card,
             marks=papers.recipe_marks, tint=CARD, dpi=200),
        Spec('photos', 'lake whitlock 1978', at(D(2026, 7, 26)), 'max',
             _two_pages(papers.old_photo),
             marks=_both(papers.photo_marks, papers.photo_back_marks),
             duplex=True, tint=CARD, dpi=200),
        Spec('travel', 'skyline itinerary', at(D(2026, 9, 30), 21, 15),
             'pdf', lambda p: papers.itinerary(p, D(2026, 9, 30))),
        # the post not yet filed keeps the names the scanner gave it
        Spec('inbox', None, at(D(2026, 10, 12), 9, 5), 'max',
             papers.dentist_card, tint=CARD),
        Spec('inbox', None, at(D(2026, 10, 15), 18, 40), 'max',
             lambda p: papers.library_notice(p, D(2026, 10, 14))),
        Spec('inbox', None, at(D(2026, 10, 18), 11, 12), 'max',
             papers.recycling_flyer),
    ]


def _two_pages(build):
    """Make a builder give a second, blank page, for the back of a sheet"""
    def two(path):
        build(path, pages=2)
    return two
