# SPDX-License-Identifier: GPL-2.0-or-later

"""The household's other papers for the demo corpus

Insurance, the house, a manual and a warranty, the car, a blood test,
school, a letter, receipts, a recipe, an old photograph, a flight and
the unsorted post. Each function lays out one document as a clean PDF;
the *_marks() functions write on the pages once they are rendered.
"""

# pylint: disable=too-many-arguments,too-many-locals,too-many-statements

import datetime
import math
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter
from reportlab.lib.pagesizes import letter

from corpus import household as hh
from corpus.layout import Doc, us_date, long_date
from corpus.scanner import PENCIL, PEN_BLUE

INCH = 72

def kitchen_quote(path, when):
    """A builder's two-page quote for a kitchen remodel"""
    name, colour = hh.BUILDER
    doc = Doc(path, title='Estimate', author=name)
    y = doc.letterhead(name, colour, 'house', (
        'CCB #224018 • Licensed, bonded, insured', '77 Depot Road, '
        f'{hh.TOWN}, OR 97355', '(541) 555-0190'),
        'Remodeling • Additions • Repairs')
    y = doc.text(doc.margin, y + 4, 'ESTIMATE #2026-041', 'Sans-Bold', 14,
                 colour)
    y = doc.lines(doc.margin, y + 4, (
        f'Date: {us_date(when)}',
        f'Customer: {hh.ADULTS[1]}, {hh.STREET}, {hh.TOWN}',
        'Project: kitchen remodel - cabinets, counters, sink wall and '
        'lighting'), 'Sans', 10) + 10
    rows = [['Item', 'Description', 'Amount'],
            ['1', 'Demolition and haul-away of existing cabinets, counters',
             '$1,850.00'],
            ['2', 'Base and wall cabinets, shaker style, painted (18 lf)',
             '$11,400.00'],
            ['3', 'Laminate counters with undermount sink cut-out',
             '$2,960.00'],
            ['4', 'Relocate sink 30" toward window; new supply and drain',
             '$2,240.00'],
            ['5', 'Tile backsplash, 3x6 ceramic, 32 sq ft', '$1,420.00'],
            ['6', 'Recessed LED lighting (6) and under-cabinet strips',
             '$1,980.00'],
            ['7', 'Electrical permit and inspection', '$310.00'],
            ['8', 'Paint walls and ceiling, two coats', '$1,150.00'],
            ['', 'Subtotal', '$23,310.00'],
            ['', 'Contingency (10%)', '$2,331.00'],
            ['', 'Total estimate', '$25,641.00']]
    y = doc.table(doc.margin, y, (40, 370, 94), rows, aligns='llr',
                  header_fill='#ead8c5') + 18
    y = doc.text(doc.margin, y, 'Schedule', 'Sans-Bold', 11, colour) + 2
    y = doc.para(doc.margin, y, 'Work can start four to six weeks after '
                 'the deposit is received. Expected duration is three weeks, '
                 'during which the kitchen will not be usable. We will set up '
                 'a temporary sink in the laundry room.', face='Sans',
                 size=9.5)
    doc.footer(f'{name} • Estimate #2026-041', 2)
    doc.new_page()
    y = doc.letterhead(name, colour, 'house', ())
    y = doc.text(doc.margin, y, 'Terms', 'Sans-Bold', 11, colour) + 4
    for para in (
            'Payment: 30% deposit on signing, 40% when cabinets are '
            'installed, balance on completion.',
            'This estimate is valid for 60 days. Prices for materials may '
            'change after that time.',
            'Changes to the work must be agreed in writing and may change '
            'the price and schedule.',
            'Hidden conditions found during demolition, such as rot, '
            'mold, or wiring not to code, are not included and will be '
            'quoted separately before any work is done.',
            'All work is warranted for one year from completion. Cabinets '
            'and fixtures carry the manufacturers\' warranties.',
            'Oregon law requires us to give you the Consumer Protection '
            'Notice and the Notice of Procedure; both are attached.'):
        y = doc.para(doc.margin, y, '• ' + para, face='Sans', size=9.5,
                     space=8)
    y += 30
    doc.lines(doc.margin, y, ('Accepted by: ________________________________',
                              '', 'Date: _____________'), 'Sans', 10)
    doc.lines(doc.margin + 290, y, ('For Hartwell Builders:',
                                    '', 'Tom Hartwell, owner'), 'Sans', 10)
    doc.footer(f'{name} • Estimate #2026-041', 2)
    doc.save()


def kitchen_marks(num, hand):
    """Pencil notes made while thinking about the quote"""
    if num == 0:
        word = hand.find('Laminate', 'counters')
        hand.write(word.x1 + 200, word.y0 - 14, 'quartz instead?', 14,
                   PENCIL, angle=6)
        word = hand.find('Relocate', 'sink')
        hand.underline(word.x0 - 50, word.x0 + 230, word.y1 + 1.5)
        word = hand.find('$25,641.00')
        hand.circle(word.x0 - 8, word.y0 - 5, word.x1 - word.x0 + 16,
                    word.y1 - word.y0 + 10)
        word = hand.find('Schedule')
        hand.write(word.x0 + 300, word.y0 - 30, 'ask about May start\n'
                   '- before Sofia\'s birthday', 14, PENCIL, angle=-3)
    elif num == 1:
        word = hand.find('Payment:')
        hand.write(word.x0 + 320, word.y0 - 24, 'deposit = $7,692', 14,
                   PENCIL)


def property_tax(path, when):
    """A county property tax statement, in black on white"""
    name = hh.COUNTY[0]
    doc = Doc(path, margin=46, title='Property tax statement', author=name)
    y = doc.lines(doc.margin, 46, (name.upper(), 'Linden County Courthouse, '
                                   'Room 110 • (541) 555-0194'), 'Sans-Bold',
                  10, leading=14)
    doc.text(doc.right, 50, f'{when.year}-{when.year + 1 - 2000} PROPERTY '
             'TAX STATEMENT', 'Sans-Bold', 13, align='right')
    doc.text(doc.right, 66, 'July 1 to June 30', 'Sans', 9, align='right')
    doc.rule(y + 4, weight=1.2, colour='#000000')
    y += 26
    doc.lines(doc.margin, y, ('Account', hh.COUNTY_ACCOUNT, '',
                              'Property location', f'{hh.STREET}, {hh.TOWN}',
                              '', 'Owner',
                              f'{hh.ADULTS[0]} & {hh.ADULTS[1]}'), 'Sans',
              9.5, leading=13)
    rows = [['Values', 'Last year', 'This year'],
            ['Real market value - land', '$148,000', '$151,000'],
            ['Real market value - structures', '$402,600', '$411,900'],
            ['Total real market value', '$550,600', '$562,900'],
            ['Maximum assessed value', '$318,442', '$327,995'],
            ['Assessed value', '$318,442', '$327,995']]
    y = doc.table(doc.margin + 200, y - 12, (170, 75, 75), rows, aligns='lrr',
                  size=9, header_fill='#e0e0e0') + 24
    rows = [['Tax by district', 'Rate per $1,000', 'Amount'],
            ['Linden County', '1.2793', '$419.60'],
            ['City of Cedar Ridge', '6.4100', '$2,102.45'],
            ['Cedar Ridge School District 31', '4.8672', '$1,596.42'],
            ['Linden Community College', '0.6158', '$201.98'],
            ['Education service district', '0.2964', '$97.22'],
            ['Fire district bond', '0.3802', '$124.70'],
            ['Library levy', '0.2900', '$95.12'],
            ['Total tax', '14.1389', '$4,637.49']]
    y = doc.table(doc.margin, y, (300, 110, 110), rows, aligns='lrr', size=9.5,
                  header_fill='#e0e0e0') + 24
    rows = [['Payment options', 'Discount', 'Pay by', 'Amount'],
            ['Full payment', '3%', '11/15', '$4,498.37'],
            ['Two-thirds payment', '2%', '11/15', '$3,029.83'],
            ['One-third payment', 'none', '11/15', '$1,545.83']]
    y = doc.table(doc.margin, y, (230, 90, 90, 110), rows, aligns='lrrr',
                  size=9.5, header_fill='#e0e0e0') + 20
    doc.para(doc.margin, y, 'Tax is due in three installments, on November '
             '15, February 15 and May 15. Interest accrues on late payments '
             'at 1.33% per month. Delinquent taxes may lead to foreclosure '
             'after three years.', face='Sans', size=9.5)
    y = doc.height - 160
    doc.c.setDash(4, 3)
    doc.rule(y, colour='#000000')
    doc.c.setDash()
    doc.lines(doc.margin, y + 24, (f'Return with payment to {name}',
                                   'PO Box 310, Linden, OR 97360'), 'Sans', 9)
    doc.lines(doc.margin + 300, y + 24, (
        f'Account     {hh.COUNTY_ACCOUNT}', 'Full payment $4,498.37',
        'Amount paid $__________'), 'Mono', 9.5)
    doc.save()


def auto_invoice(path, when):
    """A garage's invoice for a service and new brake pads"""
    name, colour = hh.GARAGE
    doc = Doc(path, title='Invoice', author=name)
    y = doc.letterhead(name, colour, 'wrench', (
        '1200 Main Street', f'{hh.TOWN}, OR 97355', '(541) 555-0196'),
        'Honest repairs since 1987')
    y = doc.text(doc.margin, y + 4, 'REPAIR INVOICE 48817', 'Sans-Bold', 14,
                 colour) + 4
    rows = [['Customer', f'{hh.ADULTS[0]} • {hh.PHONE}', 'Date',
             us_date(when)],
            ['Vehicle', hh.CAR, 'Plate', hh.PLATE],
            ['VIN', hh.VIN, 'Odometer', '48,213 mi']]
    y = doc.table(doc.margin, y, (70, 230, 70, 134), rows, header=False,
                  size=9.5) + 16
    rows = [['Op', 'Description', 'Hours', 'Parts', 'Amount'],
            ['1', 'Full synthetic oil change, 0W-20, filter', '0.5', '$42.18',
             '$94.68'],
            ['2', 'Replace front brake pads, resurface rotors', '1.6',
             '$88.40', '$256.40'],
            ['3', 'Rotate tires, check pressure', '0.3', '', '$31.50'],
            ['4', 'Multi-point inspection', '', '', 'No charge'],
            ['', 'Shop supplies', '', '', '$12.00'],
            ['', 'Discount (loyalty)', '', '', '-$81.74'],
            ['', 'Total', '', '', '$312.84']]
    y = doc.table(doc.margin, y, (30, 270, 50, 70, 84), rows, aligns='llrrr',
                  header_fill='#f5c9c9') + 18
    y = doc.text(doc.margin, y, 'Technician notes', 'Sans-Bold', 11,
                 colour) + 2
    for note in ('Rear pads at 5 mm, recheck at next service.',
                 'Wiper blades streaking, recommend replacement.',
                 'Next oil change due at 53,200 mi or 9/2026.'):
        y = doc.para(doc.margin, y, '• ' + note, face='Sans', size=10,
                     space=4)
    y += 20
    doc.para(doc.margin, y, 'Parts and labor warranted for 24 months or '
             '24,000 miles, whichever comes first.', face='Sans-Italic',
             size=9)
    doc.save()


def blood_test(path, when):
    """A two-page lab report, sent by the clinic as a scanned PDF"""
    name, colour = hh.LAB
    doc = Doc(path, title='Laboratory report', author=name)
    footer = f'{name} • CLIA 38D5550147 • Report for {hh.ADULTS[1]}'

    def head():
        y = doc.letterhead(name, colour, 'cross', (
            '900 Medical Parkway, Eugene, OR 97401', '(541) 555-0198'),
            'Accredited clinical laboratory')
        rows = [['Patient', hh.ADULTS[1], 'DOB', '04/22/1986'],
                ['Ordered by', 'Dr. Priya Nandakumar', 'Collected',
                 f'{us_date(when)} 08:10'],
                ['Accession', 'VD-26-0412233', 'Reported',
                 us_date(when + datetime.timedelta(days=1))]]
        return doc.table(doc.margin, y, (80, 200, 70, 154), rows,
                         header=False, size=9) + 16

    def results(y, title, rows):
        y = doc.text(doc.margin, y, title, 'Sans-Bold', 11, colour) + 4
        rows = [['Test', 'Result', 'Flag', 'Reference range', 'Units']] + rows
        return doc.table(doc.margin, y, (170, 70, 40, 130, 94), rows,
                         aligns='lrcll', size=9, header_fill='#e0e0e0',
                         zebra='#f6f6f6') + 16

    y = head()
    y = results(y, 'Complete blood count', [
        ['White blood cells', '6.2', '', '4.0 - 10.5', 'K/uL'],
        ['Red blood cells', '4.48', '', '3.90 - 5.20', 'M/uL'],
        ['Hemoglobin', '12.9', '', '12.0 - 15.5', 'g/dL'],
        ['Hematocrit', '38.7', '', '35.0 - 46.0', '%'],
        ['Platelets', '262', '', '150 - 400', 'K/uL']])
    y = results(y, 'Metabolic panel', [
        ['Glucose, fasting', '104', 'H', '70 - 99', 'mg/dL'],
        ['Sodium', '139', '', '135 - 145', 'mmol/L'],
        ['Potassium', '4.2', '', '3.5 - 5.1', 'mmol/L'],
        ['Creatinine', '0.78', '', '0.57 - 1.00', 'mg/dL'],
        ['Calcium', '9.4', '', '8.6 - 10.3', 'mg/dL']])
    doc.footer(footer, 2)
    doc.new_page()
    y = head()
    y = results(y, 'Lipid panel', [
        ['Total cholesterol', '212', 'H', '< 200', 'mg/dL'],
        ['HDL cholesterol', '58', '', '> 45', 'mg/dL'],
        ['LDL cholesterol (calc)', '131', 'H', '< 100', 'mg/dL'],
        ['Triglycerides', '115', '', '< 150', 'mg/dL']])
    y = results(y, 'Other', [
        ['Hemoglobin A1c', '5.6', '', '4.0 - 5.6', '%'],
        ['TSH', '2.10', '', '0.45 - 4.50', 'uIU/mL'],
        ['Vitamin D, 25-hydroxy', '27', 'L', '30 - 100', 'ng/mL']])
    doc.para(doc.margin, y, 'H = above reference range, L = below reference '
             'range. Results should be interpreted by your health care '
             'provider together with your history and examination.',
             face='Sans-Italic', size=9)
    doc.footer(footer, 2)
    doc.save()


def report_card(path):
    """A three-page school report, with a coloured crest and grades"""
    name, colour = hh.SCHOOL
    child = hh.CHILDREN[0]
    doc = Doc(path, title='Progress report', author=name)

    def head(sub):
        doc.box(0, 0, doc.width, 90, fill='#e8f5e9', stroke=None)
        doc.logo(doc.margin, 18, 56, 'shield', colour)
        doc.text(doc.margin + 70, 42, name, 'Serif-Bold', 18, colour)
        doc.text(doc.margin + 70, 62, f'Grade 4 Progress Report • {sub}',
                 'Sans', 10)
        doc.text(doc.right, 42, child, 'Sans-Bold', 12, align='right')
        doc.text(doc.right, 60, 'Teacher: Mr. Okafor • Room 12', 'Sans', 9,
                 align='right')
        return 116

    y = head('Trimester 1')
    doc.text(doc.margin, y, '4 = exceeds standard  3 = meets standard  '
             '2 = approaching  1 = not yet', 'Sans-Italic', 9)
    y += 16
    rows = [['Subject', 'Standard', 'T1', 'T2', 'T3']]
    for subject, standards in (
            ('Reading', ('Reads grade-level text fluently',
                         'Finds the main idea and details',
                         'Compares stories and poems')),
            ('Writing', ('Writes opinion pieces with reasons',
                         'Uses paragraphs and transitions',
                         'Spells grade-level words correctly')),
            ('Mathematics', ('Multiplies multi-digit numbers',
                             'Understands equivalent fractions',
                             'Solves measurement problems')),
            ('Science', ('Plans and carries out investigations',
                         'Explains energy transfer')),
            ('Social studies', ('Understands Oregon geography',
                                'Describes Native nations of Oregon'))):
        for num, standard in enumerate(standards):
            grade = str(random.Random(standard).choice((3, 3, 4, 4, 2)))
            rows.append([subject if num == 0 else '', standard, grade, '',
                         ''])
    y = doc.table(doc.margin, y, (100, 284, 40, 40, 40), rows, size=9.5,
                  aligns='llccc', header_fill='#c8e6c9',
                  header_colour=colour) + 16
    doc.footer(f'{name} • 2026-27 school year', 3)

    doc.new_page()
    y = head('Trimester 1')
    rows = [['Learning habits', 'T1', 'T2', 'T3'],
            ['Follows directions', '4', '', ''],
            ['Works well with others', '4', '', ''],
            ['Completes homework on time', '3', '', ''],
            ['Organizes materials', '2', '', ''],
            ['Participates in discussions', '4', '', '']]
    y = doc.table(doc.margin, y, (300, 60, 60, 60), rows, size=10,
                  aligns='lccc', header_fill='#c8e6c9',
                  header_colour=colour) + 20
    y = doc.text(doc.margin, y, 'Attendance', 'Sans-Bold', 11, colour) + 2
    y = doc.lines(doc.margin, y, ('Days present: 58 of 60', 'Days tardy: 1'),
                  'Sans', 10) + 16
    y = doc.text(doc.margin, y, 'Teacher comments', 'Sans-Bold', 11,
                 colour) + 4
    doc.para(doc.margin, y, 'Sofia has had a wonderful start to fourth '
             'grade. She is an enthusiastic reader and her book report on '
             'the Oregon Trail was one of the best in the class. In math, '
             'she is confident with multiplication and is now working on '
             'fractions. Sofia sometimes loses track of her homework folder; '
             'a checklist at home may help. She is kind to her classmates '
             'and always ready to help.', face='Serif', size=11)
    doc.footer(f'{name} • 2026-27 school year', 3)

    doc.new_page()
    y = head('Parent page')
    y = doc.text(doc.margin, y, 'Upcoming dates', 'Sans-Bold', 12,
                 colour) + 4
    rows = [['Date', 'Event'],
            ['November 11', 'Veterans Day - no school'],
            ['November 20-21', 'Parent-teacher conferences, early release'],
            ['November 26-28', 'Thanksgiving break'],
            ['December 12', 'Winter concert, 6:30 pm, gym'],
            ['December 22 - January 2', 'Winter break']]
    y = doc.table(doc.margin, y, (170, 334), rows, size=10,
                  header_fill='#c8e6c9', header_colour=colour) + 24
    y = doc.text(doc.margin, y, 'Please sign and return this page',
                 'Sans-Bold', 12, colour) + 8
    doc.lines(doc.margin, y + 10, (
        'I have read my child\'s progress report.', '',
        'Parent or guardian signature: ________________________________',
        '', 'Comments:', '', '_' * 80, '', '_' * 80), 'Sans', 10)
    doc.footer(f'{name} • 2026-27 school year', 3)
    doc.save()


def report_marks(num, hand):
    """A parent signs the last page"""
    if num == 2:
        word = hand.find('guardian', 'signature:')
        hand.signature(word.x1 + 14, word.y0 - 4, hh.ADULTS[1], 20)
        word = hand.find('Comments:')
        hand.write(word.x0 + 6, word.y1 + 6, 'Thank you! We will try the '
                   'homework checklist.', 15, PEN_BLUE)


def drawing(path):
    """A blank sheet for a child's crayon drawing, drawn in drawing_marks"""
    doc = Doc(path, title='Drawing', author=hh.CHILDREN[1])
    doc.text(doc.width / 2, 40, ' ', 'Sans', 8)
    doc.save()


def drawing_marks(num, hand):
    """Theo draws his family and the house in crayon"""
    if num:
        return
    draw = hand.draw
    s = hand.scale
    rng = random.Random(7)

    def crayon(points, colour, width=7):
        for _ in range(3):
            jitter = [(x * s + rng.uniform(-3, 3), y * s + rng.uniform(-3, 3))
                      for x, y in points]
            draw.line(jitter, fill=colour, width=int(width * s / 2.5),
                      joint='curve')

    # sun and sky
    for ang in range(0, 360, 30):
        x0 = 500 + 40 * math.cos(math.radians(ang))
        y0 = 110 + 40 * math.sin(math.radians(ang))
        crayon([(x0, y0), (500 + 70 * math.cos(math.radians(ang)),
                           110 + 70 * math.sin(math.radians(ang)))],
               (240, 180, 0))
    draw.ellipse(((470 * s), (80 * s), (530 * s), (140 * s)),
                 fill=(250, 200, 30))
    for x in range(80, 400, 90):
        crayon([(x, 90), (x + 30, 75), (x + 60, 90), (x + 30, 100), (x, 90)],
               (90, 160, 230))
    # house
    crayon([(150, 500), (150, 340), (330, 340), (330, 500), (150, 500)],
           (200, 40, 40), 9)
    crayon([(130, 345), (240, 250), (350, 345)], (120, 60, 30), 9)
    crayon([(215, 500), (215, 430), (265, 430), (265, 500)], (90, 50, 20))
    crayon([(170, 370), (210, 370), (210, 405), (170, 405), (170, 370)],
           (40, 120, 200))
    # family
    for x, height, colour in ((390, 150, (60, 60, 180)),
                              (440, 140, (200, 60, 150)),
                              (485, 100, (230, 120, 20)),
                              (520, 80, (40, 150, 60))):
        top = 520 - height
        draw.ellipse(((x - 14) * s, top * s, (x + 14) * s, (top + 28) * s),
                     outline=colour, width=int(5 * s / 2.5))
        crayon([(x, top + 28), (x, top + height * 0.65)], colour)
        crayon([(x - 22, top + 45), (x + 22, top + 45)], colour)
        crayon([(x, top + height * 0.65), (x - 14, 520)], colour)
        crayon([(x, top + height * 0.65), (x + 14, 520)], colour)
    # grass
    crayon([(40, 525), (570, 525)], (50, 170, 50), 12)
    hand.write(60, 600, 'MY FAMLY by Theo', 34, (220, 30, 30))
    hand.write(380, 545, 'Dad  Mom  Sofia  me', 16, (40, 40, 40))


def letter_from_grandma(path):
    """Two blank sheets of note paper, written on in letter_marks"""
    size = letter
    doc = Doc(path, size=size, title='Letter', author=hh.GRANDMOTHER)
    for page in range(2):
        for line in range(30):
            yy = 110 + line * 22
            doc.rule(yy, 54, doc.width - 54, '#b8c8e0', 0.5)
        doc.c.setStrokeColorRGB(0.9, 0.6, 0.6)
        doc.c.line(80, 0, 80, doc.height)
        if page == 0:
            doc.new_page()
    doc.save()


LETTER_TEXT = (
    f'''                                  {long_date(datetime.date(2026, 5, 3))}
Dear Laura, Daniel, Sofia and Theo,

Thank you so much for the lovely photos from Theo's birthday.
I have put the one of him with the dinosaur cake on the fridge
so I can see it every morning. He looks so grown up now!

The garden is coming along. The tomatoes are in and the roses
along the fence are covered in buds. Your Uncle Ray came over
on Saturday and fixed the gate at last, so the deer should stay
out this year. We will see.

Sofia, I loved your story about the sea otter. You must keep
writing. I have enclosed the recipe for the apple cake you
asked for. It was my mother's, and the secret is to use tart
apples and not to skimp on the cinnamon.''',
    '''I am planning to come and visit in July, if that still suits
you. I can take the train to Eugene if someone could pick me up
from the station. I will bring my quilting so I won't be under
your feet.

Laura, please don't worry about the doctor's appointment.
They said it was nothing to be concerned about and I am to go
back in six months. I feel fine, just a bit slow in the
mornings, which is to be expected at my age!

Give my love to everyone and a big hug to the children.

                    With all my love,
                              Grandma June

P.S. Theo, the dinosaur book is on its way. Look out for it!''')


def letter_marks(num, hand):
    """Grandma's handwriting, one page of the letter on each sheet"""
    if num < len(LETTER_TEXT):
        lines = LETTER_TEXT[num].split('\n')
        for line_num, line in enumerate(lines):
            hand.write(90, 92 + line_num * 22, line, 15.5, PEN_BLUE)


def receipt(path, when, shop, lines, payment):
    """A long, narrow till receipt on thermal paper

    Args:
        path (str): PDF to write
        when (datetime.datetime): Time of the sale
        shop (tuple of str): Shop name and address lines
        lines (list of tuple): (item, price) for each line
        payment (str): How it was paid, e.g. 'VISA ****7714'
    """
    width = 3.15 * INCH
    height = (3.6 + 0.19 * len(lines)) * INCH
    doc = Doc(path, size=(width, height), margin=12, title='Receipt',
              author=shop[0])
    y = doc.lines(width / 2, 24, shop, 'Mono-Bold', 9, align='centre',
                  leading=11) + 6
    y = doc.lines(width / 2, y, (when.strftime('%m/%d/%Y %I:%M %p'),
                                 'Store 12  Reg 3  Trans 4471'), 'Mono', 7.5,
                  align='centre', leading=10) + 8
    total = 0
    for item, price in lines:
        doc.text(doc.margin, y, item[:24], 'Mono', 7.5)
        doc.text(doc.right, y, f'{price:.2f}', 'Mono', 7.5, align='right')
        total += price
        y += 10
    tax = 0.0
    y += 4
    for label, amount in (('SUBTOTAL', total), ('TAX', tax),
                          ('TOTAL', total + tax)):
        doc.text(doc.margin, y, label, 'Mono-Bold', 8.5)
        doc.text(doc.right, y, f'{amount:.2f}', 'Mono-Bold', 8.5,
                 align='right')
        y += 12
    y += 4
    y = doc.lines(doc.margin, y, (payment, 'APPROVED  AUTH 08812C',
                                  f'ITEMS {len(lines)}'), 'Mono', 7.5,
                  leading=10) + 10
    doc.lines(width / 2, y, ('THANK YOU FOR SHOPPING', 'Returns within 30',
                             'days with receipt'), 'Mono', 7.5,
              align='centre', leading=10)
    doc.save()


GROCERIES = (('ORGANIC BANANAS 2.3LB', 1.84), ('APPLES GRANNY SMITH 3LB', 4.47),
             ('CINNAMON GROUND 2OZ', 3.29), ('FLOUR ALL PURPOSE 5LB', 4.19),
             ('BUTTER UNSALTED 1LB', 5.49), ('EGGS LARGE DOZEN', 3.99),
             ('MILK 2% GALLON', 4.29), ('SOURDOUGH LOAF', 5.50),
             ('CHEDDAR SHARP 8OZ', 4.79), ('PASTA PENNE 16OZ', 1.89),
             ('TOMATO SAUCE 24OZ', 3.49), ('BROCCOLI CROWNS', 2.68),
             ('CHICKEN THIGHS 2.1LB', 9.43), ('COFFEE BEANS 12OZ', 11.99),
             ('YOGURT GREEK 32OZ', 5.99), ('DISH SOAP', 3.79))

HARDWARE = (('WOOD SCREWS #8 1-1/4 100', 7.98), ('WOOD GLUE 8OZ', 5.49),
            ('SANDPAPER ASST 5PK', 6.99), ('HINGE CABINET 2PK', 8.47),
            ('PAINT ROLLER 9IN', 6.29), ('PAINTERS TAPE 1.88IN', 7.99),
            ('KEY CUT X2', 3.70))


PHARMACY = (('IBUPROFEN 200MG 100CT', 8.99), ('VITAMIN D3 2000IU', 11.49),
            ('BANDAGES ASST 30CT', 4.79), ('SUNSCREEN SPF50', 12.99),
            ('TOOTHPASTE 2PK', 6.49), ('RX COPAY', 15.00),
            ('ALLERGY RELIEF 24HR', 18.99), ('HAND CREAM', 5.29))

PIZZA = (('LG MARGHERITA', 21.00), ('MED PEPPERONI', 17.50),
         ('CAESAR SALAD', 9.00), ('GARLIC KNOTS', 6.50),
         ('LEMONADE X2', 7.00), ('KIDS CHEESE SLICE X2', 8.00),
         ('TIP', 9.00))

FUEL = (('UNLEADED 11.204 GAL', 46.38), ('CAR WASH DELUXE', 12.00),
        ('COFFEE LG', 2.79), ('WINDSHIELD FLUID', 4.49),
        ('TRAIL MIX', 3.99))

# shop: (name and address lines, things it sells, how it is paid)
SHOPS = {
    'green basket': ((hh.GROCER.upper(), '3RD & OAK',
                      f'{hh.TOWN.upper()} OR', '(541) 555-0108'),
                     GROCERIES, 'DEBIT ****3381'),
    'pine and nail': ((hh.HARDWARE.upper(), '640 DEPOT ROAD',
                       f'{hh.TOWN.upper()} OR', '(541) 555-0109'),
                      HARDWARE, 'VISA ****7714'),
    'pharmacy': (('CEDAR RIDGE PHARMACY', '12 MAIN STREET',
                  f'{hh.TOWN.upper()} OR', '(541) 555-0112'),
                 PHARMACY, 'VISA ****7714'),
    'oak and ember': (('OAK & EMBER PIZZA', '88 MILL STREET',
                       f'{hh.TOWN.upper()} OR', '(541) 555-0113'),
                      PIZZA, 'VISA ****7714'),
    'ridgeline fuel': (('RIDGELINE FUEL & MART', 'HWY 20 AT ELM',
                        f'{hh.TOWN.upper()} OR', '(541) 555-0114'),
                       FUEL, 'DEBIT ****3381'),
}


def shop_receipt(path, when, shop, rng):
    """A till receipt from one of the shops, with some of what it sells

    Args:
        path (str): PDF to write
        when (datetime.datetime): Time of the sale
        shop (str): Key in SHOPS
        rng (random.Random): Picks the things bought
    """
    lines, stock, payment = SHOPS[shop]
    count = min(len(stock), rng.randint(3, 9))
    receipt(path, when, lines, rng.sample(stock, count), payment)


def recipe_card(path):
    """A recipe card with a photograph of the cake, 6 by 4 inches"""
    size = (6 * INCH, 4 * INCH)
    doc = Doc(path, size=size, margin=14, title='Recipe', author=hh.GRANDMOTHER)
    doc.box(0, 0, doc.width, 26, fill='#c0504d', stroke=None)
    doc.text(doc.margin, 18, 'FROM THE KITCHEN OF ...', 'Sans-Bold', 10,
             '#ffffff')
    for line in range(9):
        doc.rule(56 + line * 22, 170, doc.width - 14, '#9fb7d4', 0.5)
    doc.save()


def recipe_marks(num, hand):
    """Grandma writes the recipe, and a photo of the cake is stuck on"""
    if num:
        return
    hand.write(162, 3, 'June Whitlock', 15, (255, 255, 255))
    hand.write(172, 34, "Mother's Apple Cake", 17, PEN_BLUE)
    hand.write(172, 58, '3 tart apples, peeled + sliced\n2 cups flour, '
               '1 cup sugar\n2 tsp cinnamon (be generous!)\n2 eggs, 1/2 cup '
               'butter, melted\n1 tsp baking soda, pinch salt\nBake 350° '
               'for 50 min', 12.5, PEN_BLUE)
    s = hand.scale
    photo = _cake_photo(int(150 * s), int(170 * s))
    hand.img.paste(photo, (int(10 * s), int(40 * s)))


def _cake_photo(width, height):
    """A small colour photograph of a cake on a plate"""
    img = Image.new('RGB', (width, height), (214, 196, 168))
    draw = ImageDraw.Draw(img)
    for y in range(height):
        shade = int(180 + 50 * y / height)
        draw.line([(0, y), (width, y)], fill=(shade, shade - 30, shade - 70))
    cx, cy = width / 2, height * 0.6
    draw.ellipse((cx - width * 0.45, cy - height * 0.12, cx + width * 0.45,
                  cy + height * 0.25), fill=(240, 240, 235))
    draw.ellipse((cx - width * 0.33, cy - height * 0.2, cx + width * 0.33,
                  cy + height * 0.12), fill=(176, 110, 48))
    draw.ellipse((cx - width * 0.3, cy - height * 0.24, cx + width * 0.3,
                  cy + height * 0.02), fill=(205, 145, 70))
    for i in range(9):
        x = cx + width * 0.2 * (i % 3 - 1)
        y = cy - height * 0.13 + 6 * (i // 3)
        draw.ellipse((x - 12, y - 4, x + 12, y + 4), fill=(235, 200, 120))
    img = img.filter(ImageFilter.GaussianBlur(1.2))
    ImageDraw.Draw(img).rectangle((0, 0, width - 1, height - 1),
                                  outline=(250, 250, 250), width=6)
    return img


def old_photo(path, pages=1):
    """A blank 5 x 3.5 inch print, the photograph being added in photo_marks

    Args:
        path (str): PDF to write
        pages (int): 2 to give the print a back as well
    """
    size = (5 * INCH, 3.5 * INCH)
    doc = Doc(path, size=size, title='Photograph')
    for _ in range(pages - 1):
        doc.new_page()
    doc.save()


def photo_marks(num, hand):
    """An old sepia print of a lake, with a white border"""
    if num:
        return
    img = hand.img
    s = hand.scale
    border = int(0.18 * 72 * s)
    w, h = img.width - 2 * border, img.height - 2 * border
    yy, xx = np.mgrid[0:h, 0:w]
    sky = 200 - 60 * yy / h
    lake_line = h * 0.55
    hills = lake_line - h * 0.12 * (1 + np.sin(xx / w * 7) * 0.5
                                    + np.sin(xx / w * 17) * 0.2)
    val = np.where(yy < hills, sky, 0)
    val = np.where((yy >= hills) & (yy < lake_line), 70 + 15 * np.sin(xx / 9),
                   val)
    water = 120 + 25 * np.sin(yy / 3.0) + 10 * np.sin(xx / 13.0)
    val = np.where(yy >= lake_line, water, val)
    rng = np.random.default_rng(3)
    val = val + rng.normal(0, 8, val.shape)
    val = np.clip(val, 0, 255) / 255
    sepia = np.stack([val * 240 + 10, val * 205 + 10, val * 160 + 5], -1)
    photo = Image.fromarray(np.clip(sepia, 0, 255).astype('uint8'))
    photo = photo.filter(ImageFilter.GaussianBlur(1.5))
    img.paste((247, 242, 230), (0, 0, img.width, img.height))
    img.paste(photo, (border, border))


def photo_back_marks(num, hand):
    """What Grandma wrote on the back of the photograph"""
    if num == 1:
        hand.write(40, 50, 'Lake Whitlock, summer 1978\nMom, Dad, Ray and me',
                   20, PEN_BLUE, angle=4)


def itinerary(path, when):
    """A flight itinerary, received by e-mail"""
    name, colour = hh.AIRLINE
    doc = Doc(path, title='Your trip confirmation', author=name)
    y = doc.letterhead(name, colour, 'plane', ('skyline.example.com',
                                               'Reservations 1-800-555-0104'),
                       'Fly further for less', band=True)
    y = doc.text(doc.margin, y + 6, 'Your trip is confirmed!', 'Sans-Bold', 20,
                 colour) + 6
    y = doc.lines(doc.margin, y, (
        'Confirmation code: QX7RLM',
        f'Booked {us_date(when)} by {hh.ADULTS[1]} • {hh.EMAIL}'), 'Sans',
        10.5) + 14

    def flight(y, title, date, rows):
        doc.box(doc.margin, y, doc.text_width, 24, fill=colour, stroke=None)
        doc.text(doc.margin + 10, y + 16, f'{title} • {date}', 'Sans-Bold', 11,
                 '#ffffff')
        y += 24
        return doc.table(doc.margin, y, (80, 140, 140, 144), rows,
                         size=9.5, header_fill='#dbe6f6') + 18

    out = datetime.date(2026, 12, 19)
    back = datetime.date(2027, 1, 2)
    y = flight(y, 'Departing', long_date(out), [
        ['Flight', 'Depart', 'Arrive', 'Aircraft'],
        ['SK 1182', 'Portland (PDX) 7:05 am', 'Denver (DEN) 10:39 am',
         'Airbus A321'],
        ['SK 640', 'Denver (DEN) 11:55 am', 'Boston (BOS) 5:48 pm',
         'Boeing 737-800']])
    y = flight(y, 'Returning', long_date(back), [
        ['Flight', 'Depart', 'Arrive', 'Aircraft'],
        ['SK 1907', 'Boston (BOS) 8:30 am', 'Portland (PDX) 12:02 pm',
         'Airbus A321neo']])
    y = doc.text(doc.margin, y, 'Passengers', 'Sans-Bold', 12, colour) + 4
    rows = [['Name', 'Ticket number', 'Seats (out / back)', 'Bags']]
    for num, person in enumerate(hh.ADULTS + hh.CHILDREN):
        rows.append([person.upper(), f'447 2{num}81 0566 1{num}',
                     f'{14 + num // 2}{"ABCD"[num]} / {22 + num // 2}'
                     f'{"ABCD"[num]}', '1 checked'])
    y = doc.table(doc.margin, y, (150, 130, 130, 94), rows, size=9.5,
                  header_fill='#dbe6f6') + 18
    rows = [['Payment', 'Amount'],
            ['Base fare (4 passengers)', '$1,068.00'],
            ['Taxes and fees', '$161.20'],
            ['Checked bags', '$140.00'],
            ['Total charged to VISA ****7714', '$1,369.20']]
    doc.table(doc.margin, y, (300, 100), rows, size=9.5, aligns='lr',
              header_fill='#dbe6f6')
    doc.footer(f'{name} • Confirmation QX7RLM', 2)
    doc.new_page()
    y = doc.letterhead(name, colour, 'plane', (), band=True)
    for head, body in (
            ('Check in', 'Check in online from 24 hours before departure. '
             'Bag drop closes 45 minutes before domestic flights.'),
            ('Security', 'Allow at least two hours at Portland in December. '
             'Children under 18 do not need ID for domestic flights.'),
            ('Changes', 'Main Cabin fares can be changed without a fee; you '
             'pay any difference in fare. Basic fares cannot be changed.'),
            ('Baggage', 'Each passenger may bring one carry-on bag and one '
             'personal item. The first checked bag is $35 each way.')):
        y = doc.text(doc.margin, y, head, 'Sans-Bold', 11) + 2
        y = doc.para(doc.margin, y, body, face='Sans', size=10, space=12)
    doc.footer(f'{name} • Confirmation QX7RLM', 2)
    doc.save()


def dentist_card(path):
    """An appointment reminder postcard, 6 by 4 inches"""
    name, colour = hh.DENTIST
    doc = Doc(path, size=(6 * INCH, 4 * INCH), margin=18,
              title='Appointment reminder', author=name)
    y = doc.letterhead(name, colour, 'tooth', ('(541) 555-0105',), y=14)
    y = doc.text(doc.margin, y, 'Time for your check-up!', 'Sans-Bold', 15,
                 colour) + 6
    doc.lines(doc.margin, y, (
        f'{hh.CHILDREN[0]} and {hh.CHILDREN[1]}',
        'Tuesday, October 13 at 3:30 pm and 4:00 pm',
        'Please call 24 hours ahead if you need to reschedule.'),
        'Sans', 10, leading=14)
    doc.save()


def library_notice(path, when):
    """An overdue notice from the library, in black only"""
    name = hh.LIBRARY[0]
    doc = Doc(path, title='Overdue notice', author=name)
    y = doc.lines(doc.margin, 60, (name.upper(), '350 Oak Street, '
                                   f'{hh.TOWN}, OR 97355 • (541) 555-0106'),
                  'Sans-Bold', 11, leading=15) + 20
    y = doc.text(doc.margin, y, 'OVERDUE NOTICE', 'Sans-Bold', 16) + 12
    y = doc.lines(doc.margin, y, (f'{hh.ADULTS[0]}', hh.STREET,
                                  f'{hh.TOWN}, {hh.STATE} {hh.ZIP}'),
                  'Sans', 11) + 16
    y = doc.para(doc.margin, y, 'Our records show the following items '
                 'checked out on your card are overdue. Please return or '
                 'renew them as soon as possible.', face='Sans', size=11)
    rows = [['Title', 'Due', 'Fine'],
            ['The Wild Robot / Peter Brown', us_date(when - datetime.timedelta(
                days=9)), '$0.90'],
            ['Dinosaurs of the Pacific Northwest', us_date(
                when - datetime.timedelta(days=9)), '$0.90'],
            ['Kitchen Remodeling Made Simple', us_date(
                when - datetime.timedelta(days=4)), '$0.40']]
    y = doc.table(doc.margin, y, (300, 110, 94), rows, size=10,
                  header_fill='#e0e0e0') + 20
    doc.para(doc.margin, y, 'Renew online at cedarridgelibrary.example.org '
             'or by phone. Fines stop at $5 per item.', face='Sans', size=10)
    doc.save()


def recycling_flyer(path):
    """A colourful flyer from the city about the new recycling rules"""
    name, colour = hh.CITY
    doc = Doc(path, title='Recycling changes', author=name)
    doc.box(0, 0, doc.width, 150, fill=colour, stroke=None)
    doc.logo(doc.margin, 30, 80, 'leaf', '#c8e6c9')
    doc.text(doc.margin + 100, 80, 'Recycling is changing!', 'Sans-Bold', 30,
             '#ffffff')
    doc.text(doc.margin + 100, 110, f'New rules for {hh.TOWN} from '
             'January 1', 'Sans', 15, '#ffffff')
    y = 190
    for title, fill, items in (
            ('YES - blue cart', '#1976d2', ('Paper and cardboard, flattened',
                                            'Plastic bottles and tubs #1, #2, '
                                            '#5', 'Metal cans, rinsed',
                                            'Glass now goes in the cart!')),
            ('NO - garbage', '#616161', ('Plastic bags and film',
                                         'Takeout containers with food',
                                         'Shredded paper', 'Batteries - '
                                         'take to the depot')),
            ('NEW - green cart', '#388e3c', ('Food scraps, including meat',
                                             'Yard debris', 'Pizza boxes',
                                             'Coffee grounds and filters'))):
        doc.box(doc.margin, y, doc.text_width, 30, fill=fill, stroke=None,
                radius=6)
        doc.text(doc.margin + 12, y + 21, title, 'Sans-Bold', 15, '#ffffff')
        y += 44
        for item in items:
            y = doc.text(doc.margin + 20, y, '• ' + item, 'Sans', 13) + 4
        y += 12
    doc.para(doc.margin, y + 10, 'Questions? Call the recycling hotline at '
             '(541) 555-0107 or visit cedarridge.example.gov/recycle.',
             face='Sans-Bold', size=12, colour=colour)
    doc.save()
