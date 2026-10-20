# SPDX-License-Identifier: GPL-2.0-or-later

"""A dishwasher's manual and a heat pump's warranty card for the corpus"""

# pylint: disable=too-many-arguments,too-many-locals,too-many-statements

from corpus import household as hh
from corpus.layout import Doc, us_date
from corpus.scanner import PEN_BLUE

INCH = 72


def dishwasher_manual(path):
    """A twelve-page owner's manual, as the maker publishes it"""
    name, colour = hh.APPLIANCE
    model = hh.DISHWASHER
    doc = Doc(path, title=f'{name} {model} Use & Care Guide', author=name)
    footer = f'{name} {model} Use & Care Guide'

    def page(title):
        doc.box(0, 0, doc.width, 40, fill=colour, stroke=None)
        doc.text(doc.margin, 26, f'{name} {model}', 'Sans-Bold', 12, '#ffffff')
        doc.text(doc.right, 26, title, 'Sans', 10, '#ffffff', 'right')
        return 76

    def warn(y, kind, body):
        doc.box(doc.margin, y, doc.text_width, 54, stroke='#000000',
                weight=1)
        doc.text(doc.margin + 10, y + 18, f'⚠ {kind}', 'Sans-Bold', 11)
        doc.para(doc.margin + 10, y + 32, body, doc.text_width - 20, 'Sans',
                 9)
        return y + 66

    def diagram(y, h, parts):
        """Draw a line drawing of the dishwasher with labelled parts"""
        x = doc.margin + 40
        doc.box(x, y, 180, h, stroke='#333333', weight=1.4, radius=6)
        doc.box(x + 10, y + 10, 160, 22, stroke='#333333')
        for rack in (0.35, 0.65):
            yy = y + h * rack
            doc.rule(yy, x + 14, x + 166, '#555555', 1)
            for tine in range(14):
                doc.c.line(x + 20 + tine * 10, doc.fy(yy),
                           x + 20 + tine * 10, doc.fy(yy - 12))
        doc.c.circle(x + 90, doc.fy(y + h * 0.82), 14)
        for num, (label, py) in enumerate(parts, 1):
            yy = y + h * py
            doc.rule(yy, x + 150, x + 260, '#888888', 0.5)
            doc.c.circle(x + 270, doc.fy(yy), 8)
            doc.text(x + 270, yy + 3, str(num), 'Sans-Bold', 8,
                     align='centre')
            doc.text(x + 284, yy + 3, label, 'Sans', 9)
        return y + h + 20

    # cover
    doc.box(0, 0, doc.width, doc.height, fill=colour, stroke=None)
    doc.text(doc.width / 2, 260, name.upper(), 'Sans-Bold', 40, '#ffffff',
             'centre')
    doc.text(doc.width / 2, 300, f'Dishwasher {model}', 'Sans', 22,
             '#ffffff', 'centre')
    doc.text(doc.width / 2, 340, 'Use & Care Guide', 'Sans-Italic', 18,
             '#ffffff', 'centre')
    doc.logo(doc.width / 2 - 50, 420, 100, 'drop', '#ffffff')
    doc.text(doc.width / 2, 700, 'Read and save these instructions',
             'Sans-Bold', 11, '#ffffff', 'centre')

    doc.new_page()
    y = page('Contents')
    y = doc.text(doc.margin, y, 'Contents', 'Sans-Bold', 18, colour) + 10
    for title, num in (('Important safety instructions', 3),
                       ('Parts and features', 4), ('Loading the racks', 5),
                       ('Detergent and rinse aid', 6),
                       ('Wash cycles and options', 7),
                       ('Cleaning the filter', 8), ('Care and cleaning', 9),
                       ('Troubleshooting', 10), ('Warranty', 11),
                       ('Specifications', 12)):
        doc.text(doc.margin, y, title, 'Sans', 11)
        doc.text(doc.right, y, str(num), 'Sans', 11, align='right')
        doc.c.setDash(1, 3)
        doc.rule(y + 1, doc.margin + 200, doc.right - 20, '#999999')
        doc.c.setDash()
        y += 22
    y += 20
    doc.para(doc.margin, y, 'Thank you for choosing Aquaclean. Register '
             'your dishwasher at aquaclean.example.com/register to receive '
             'safety notices and to make warranty service quicker. You will '
             'need the model and serial numbers, found on the label on the '
             'right edge of the door.', face='Sans', size=10)
    doc.footer(footer)

    doc.new_page()
    y = page('Safety')
    y = doc.text(doc.margin, y, 'Important safety instructions', 'Sans-Bold',
                 16, colour) + 10
    y = warn(y, 'WARNING', 'Risk of electric shock. Connect the dishwasher '
             'to a grounded, metal, permanent wiring system. Do not use an '
             'extension cord or adapter plug.')
    y = warn(y, 'WARNING', 'Risk of fire. Do not store or use gasoline or '
             'other flammable vapors and liquids near this or any other '
             'appliance.')
    y = warn(y, 'CAUTION', 'Hot water and steam. Do not open the door while '
             'the dishwasher is running. Let dishes cool before unloading.')
    y = warn(y, 'CAUTION', 'Sharp items. Load knives and other sharp '
             'utensils with the handles up, to reduce the risk of cuts.')
    for para in (
            'Do not let children play in or on the dishwasher. Keep '
            'detergent out of the reach of children.',
            'Do not wash plastic items unless they are marked dishwasher '
            'safe. Place them in the upper rack.',
            'Do not tamper with controls. Do not abuse, sit on or stand on '
            'the door or dish racks.',
            'Under certain conditions hydrogen gas may be produced in a hot '
            'water system that has not been used for two weeks or more. '
            'Before using the dishwasher, turn on all hot water faucets and '
            'let the water flow for several minutes.'):
        y = doc.para(doc.margin, y, '• ' + para, face='Sans', size=10,
                     space=8)
    doc.footer(footer)

    doc.new_page()
    y = page('Parts and features')
    y = doc.text(doc.margin, y, 'Parts and features', 'Sans-Bold', 16,
                 colour) + 10
    y = diagram(y, 300, (('Control panel', 0.06), ('Upper rack', 0.35),
                         ('Upper spray arm', 0.45), ('Lower rack', 0.65),
                         ('Lower spray arm', 0.75), ('Filter assembly', 0.82),
                         ('Detergent dispenser', 0.92)))
    doc.para(doc.margin, y, 'The serial number label is on the right edge '
             'of the door. Write the number here for reference: '
             '______________________', face='Sans', size=10)
    doc.footer(footer)

    for title, sections in (
            ('Loading the racks', (
                ('Upper rack', 'Load cups, glasses and small bowls in the '
                 'upper rack, facing down and angled towards the center. '
                 'Long utensils can lie flat across the third-level rack.'),
                ('Lower rack', 'Load plates, pots and pans in the lower '
                 'rack, with the dirty surfaces facing the spray arm. Do '
                 'not block the spray arms or the detergent dispenser.'),
                ('Silverware basket', 'Mix spoons, forks and knives so they '
                 'do not nest together. Load sharp knives with the handles '
                 'up.'),
                ('What not to wash', 'Cast iron, wooden boards and utensils, '
                 'pewter, gold-leaf china and insulated travel mugs should be '
                 'washed by hand.'))),
            ('Detergent and rinse aid', (
                ('Detergent', 'Use only detergent made for automatic '
                 'dishwashers. Pods go in the main wash compartment; close '
                 'the lid until it latches.'),
                ('Rinse aid', 'Rinse aid helps water run off dishes so that '
                 'they dry without spots. Fill the dispenser until the '
                 'indicator turns dark. One fill lasts about a month.'),
                ('Hard water', 'If your water is hard, set the water '
                 'softener level in the settings menu. See Specifications '
                 'for the hardness levels.'))),
            ('Wash cycles and options', (
                ('Auto', 'Senses how dirty the dishes are and adjusts water '
                 'and time. Recommended for everyday loads. 105-150 '
                 'minutes.'),
                ('Heavy', 'For pots, pans and baked-on food. 165 minutes.'),
                ('Eco', 'Uses the least water and energy. 195 minutes.'),
                ('Quick', 'For lightly soiled dishes which do not need '
                 'drying. 60 minutes.'),
                ('Sanitize option', 'Raises the final rinse to 155 °F to '
                 'remove 99.999% of bacteria.'),
                ('Delay start', 'Starts the cycle 1 to 24 hours later, to '
                 'run at night when electricity costs less.'))),
            ('Cleaning the filter', (
                ('Why clean the filter', 'The filter stops food from '
                 'clogging the pump. A dirty filter leads to poor washing '
                 'and bad smells.'),
                ('How often', 'Clean the filter once a month, or when you '
                 'see food on the screen.'),
                ('Steps', '1. Turn the upper filter a quarter turn '
                 'counterclockwise and lift it out. 2. Pull the lower '
                 'filter forward. 3. Rinse both under running water and '
                 'scrub gently with a soft brush. 4. Refit the lower filter, '
                 'then the upper filter, turning it clockwise until it '
                 'locks.'))),
            ('Care and cleaning', (
                ('Outside', 'Wipe the control panel with a damp cloth. Do '
                 'not use abrasive cleaners on stainless steel.'),
                ('Inside', 'Run an empty Heavy cycle with a cup of white '
                 'vinegar in the upper rack every few months to remove '
                 'mineral deposits.'),
                ('Spray arms', 'If the holes become blocked, remove the arm '
                 'and clear the holes with a toothpick.'),
                ('Vacations', 'Before leaving for a long time, turn off the '
                 'water supply and the power to the dishwasher.'))),
    ):
        doc.new_page()
        y = page(title)
        y = doc.text(doc.margin, y, title, 'Sans-Bold', 16, colour) + 10
        for head, body in sections:
            y = doc.text(doc.margin, y, head, 'Sans-Bold', 11.5) + 3
            y = doc.para(doc.margin, y, body, face='Sans', size=10.5,
                         space=12)
        if title == 'Cleaning the filter':
            diagram(y + 10, 200, (('Upper filter', 0.75),
                                  ('Lower filter', 0.85)))
        doc.footer(footer)

    doc.new_page()
    y = page('Troubleshooting')
    y = doc.text(doc.margin, y, 'Troubleshooting', 'Sans-Bold', 16,
                 colour) + 10
    rows = [['Problem', 'Possible cause', 'What to do']]
    for row in (
            ('Will not start', 'Door not latched', 'Close the door firmly'),
            ('', 'Delay start set', 'Cancel delay start'),
            ('Dishes not clean', 'Filter dirty', 'Clean the filter'),
            ('', 'Spray arm blocked', 'Check that nothing stops the arms'),
            ('Spots on glasses', 'Rinse aid empty', 'Fill the rinse aid'),
            ('', 'Hard water', 'Raise the softener level'),
            ('Water left in tub', 'Drain hose kinked', 'Straighten the hose'),
            ('Error E4', 'Water leak detected', 'Turn off and call service'),
            ('Error E9', 'Water not heating', 'Call service'),
            ('Noisy', 'Dishes touching arm', 'Reload the racks')):
        rows.append(list(row))
    doc.table(doc.margin, y, (130, 160, 214), rows, size=9.5,
              header_fill='#b2dfe2', zebra='#eef8f9')
    doc.footer(footer)

    doc.new_page()
    y = page('Warranty')
    y = doc.text(doc.margin, y, 'Limited warranty', 'Sans-Bold', 16,
                 colour) + 10
    for head, body in (
            ('What is covered', 'For one year from the date of purchase, '
             'Aquaclean will pay for factory replacement parts and repair '
             'labor to correct defects in materials or workmanship. For '
             'five years, Aquaclean will pay for replacement racks and the '
             'electronic control board. The stainless steel tub is covered '
             'for the life of the dishwasher against leaks.'),
            ('What is not covered', 'Service calls to correct the '
             'installation, to instruct you how to use the dishwasher, or '
             'to replace house fuses; damage caused by accident, misuse, '
             'fire, floods or acts of God; and products used commercially.'),
            ('How to get service', 'Keep your receipt as proof of purchase. '
             'Call 1-800-555-0102 with your model and serial numbers and the '
             'date of purchase. Service must be done by an authorized '
             'Aquaclean servicer.'),
            ('Your rights', 'This warranty gives you specific legal rights, '
             'and you may also have other rights which vary from state to '
             'state.')):
        y = doc.text(doc.margin, y, head, 'Sans-Bold', 11.5) + 3
        y = doc.para(doc.margin, y, body, face='Sans', size=10.5, space=12)
    doc.footer(footer)

    doc.new_page()
    y = page('Specifications')
    y = doc.text(doc.margin, y, 'Specifications', 'Sans-Bold', 16,
                 colour) + 10
    rows = [['Item', model]]
    for row in (('Capacity', '16 place settings'),
                ('Noise level', '42 dBA'),
                ('Dimensions (W x H x D)', '23 7/8 x 33 7/8 x 24 5/8 in'),
                ('Electrical', '120 V, 60 Hz, 15 A'),
                ('Water pressure', '20-120 psi'),
                ('Water use (Eco)', '3.2 gallons'),
                ('Energy use', '269 kWh/year'),
                ('Water hardness levels', 'H1 (soft) to H6 (very hard)')):
        rows.append(list(row))
    doc.table(doc.margin, y, (220, 284), rows, size=10,
              header_fill='#b2dfe2')
    doc.footer(footer)
    doc.save()


def heat_pump_card(path, when):
    """A small warranty registration card, 7 by 5 inches"""
    name, colour = hh.HEAT_PUMP
    size = (7 * INCH, 5 * INCH)
    doc = Doc(path, size=size, margin=24, title='Warranty card', author=name)
    doc.box(0, 0, doc.width, 44, fill=colour, stroke=None)
    doc.text(doc.margin, 28, f'{name.upper()} • LIMITED WARRANTY', 'Sans-Bold',
             13, '#ffffff')
    y = doc.lines(doc.margin, 70, (
        'Model: HP-24 ductless heat pump, 24,000 BTU',
        f'Serial number: {hh.HEAT_PUMP_SERIAL}',
        f'Installed: {us_date(when)} by Valley Air Heating & Cooling',
        f'Owner: {hh.ADULTS[0]}, {hh.STREET}, {hh.TOWN}, {hh.STATE}'),
        'Sans', 10, leading=15) + 8
    doc.para(doc.margin, y, '10-year parts warranty and 12-year compressor '
             'warranty when registered within 60 days of installation. '
             'Labor is covered for one year by the installer. Keep this card '
             'with your proof of purchase. Annual maintenance by a licensed '
             'technician is required to keep the warranty in force.',
             doc.text_width, 'Sans', 9)
    doc.text(doc.margin, doc.height - 20, 'northwind.example.com/warranty • '
             '1-800-555-0103', 'Sans', 8, '#555555')
    doc.save()


def heat_pump_marks(num, hand):
    """The installer fills in the date"""
    if num == 0:
        word = hand.find('Installed:')
        hand.write(word.x0 + 250, word.y0 - 34, 'Registered online 11/8', 13,
                   PEN_BLUE, angle=3)
