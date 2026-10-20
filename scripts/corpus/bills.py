# SPDX-License-Identifier: GPL-2.0-or-later

"""Bills, statements and tax papers for the demo corpus

Each function lays out one document as a clean PDF. The numbers agree
with each other where they meet: the bank statement pays the bills, and
the tax summary counts the gift to the food pantry.
"""

# pylint: disable=too-many-arguments,too-many-locals,too-many-statements

import datetime

from reportlab.lib.pagesizes import letter

from corpus import household as hh
from corpus.layout import Doc, money, us_date, long_date

MONTHS = ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep',
          'Oct', 'Nov', 'Dec')

# kWh used each month, from October 2025
USAGE = (690, 820, 1010, 1080, 1020, 912, 760, 655, 640, 870, 1104, 1060,
         720)


def _usage_months(bill_date):
    """Get the last 13 months of usage up to a bill, oldest first"""
    end = (bill_date.year - 2025) * 12 + bill_date.month - 10
    labels, values = [], []
    for num in range(end - 12, end + 1):
        month = (9 + num) % 12
        labels.append(MONTHS[month])
        values.append(USAGE[num] if 0 <= num < len(USAGE) else 700)
    return labels, values


def power_bill(path, bill_date):
    """A two-page electricity bill, with a chart of the year's usage"""
    name, colour = hh.POWER
    labels, values = _usage_months(bill_date)
    kwh = values[-1]
    start = bill_date - datetime.timedelta(days=33)
    end = bill_date - datetime.timedelta(days=3)
    due = bill_date + datetime.timedelta(days=21)
    rate = 0.1121
    energy = kwh * rate
    basic = 11.50
    transmission = kwh * 0.0109
    public = (basic + energy + transmission) * 0.03
    total = basic + energy + transmission + public

    doc = Doc(path, title=f'{name} bill {us_date(bill_date)}', author=name)
    y = doc.letterhead(name, colour, 'bolt', (
        'PO Box 4410', f'{hh.TOWN}, OR 97355',
        'Customer service (541) 555-0110', 'junipervalley.example.com'),
        'Your local, member-owned utility', band=True)
    doc.address_block(y + 10, (hh.ADULTS[1].upper(), hh.STREET.upper(),
                               f'{hh.TOWN.upper()} OR {hh.ZIP}'))
    box_x = doc.width - doc.margin - 220
    doc.box(box_x, y, 220, 96, fill='#fff4e0', stroke=colour)
    doc.lines(box_x + 10, y + 18, ('Account number', 'Bill date',
                                   'Amount due', 'Due date'), 'Sans', 9)
    doc.lines(box_x + 210, y + 18, (hh.POWER_ACCOUNT, us_date(bill_date),
                                    money(total), us_date(due)),
              'Sans-Bold', 9, align='right')
    y += 120

    y = doc.text(doc.margin, y, 'Your electricity use', 'Sans-Bold', 12,
                 colour) + 4
    y = doc.bar_chart(doc.margin, y + 10, 300, 90, labels, values, colour,
                      highlight=len(values) - 1, unit='kWh')
    side = doc.margin + 330
    doc.lines(side, y - 100, (
        f'Service period: {us_date(start)} to {us_date(end)}',
        f'Days in period: {(end - start).days}',
        'Meter number: 88-104-552',
        f'Previous reading: {48210 + sum(values[:-1]):,}',
        f'Current reading: {48210 + sum(values):,}',
        f'Energy used: {kwh:,} kWh',
        f'Average per day: {kwh / (end - start).days:.1f} kWh'), 'Sans', 9)

    y += 10
    y = doc.text(doc.margin, y, 'Charges for this period', 'Sans-Bold', 12,
                 colour) + 4
    rows = [['Description', 'Quantity', 'Rate', 'Amount'],
            ['Basic charge', '1 month', money(basic), money(basic)],
            ['Energy charge', f'{kwh:,} kWh', f'${rate:.4f}', money(energy)],
            ['Transmission and distribution', f'{kwh:,} kWh', '$0.0109',
             money(transmission)],
            ['Public purpose charge', '3%', '', money(public)],
            ['Total current charges', '', '', money(total)]]
    y = doc.table(doc.margin, y, (250, 90, 80, 84), rows, size=9,
                  header_fill='#fbe3bd', aligns='lrrr')
    y += 16
    y = doc.para(doc.margin, y, 'Payments received since your last bill are '
                 'shown on page 2. To pay by phone, call (541) 555-0110 and '
                 'have your account number ready. Payments made after the '
                 'due date may incur a late charge of 1.5% per month.',
                 face='Sans', size=9)

    # the slip to send back with a cheque
    y = doc.height - 150
    doc.c.setDash(4, 3)
    doc.rule(y, colour='#888888')
    doc.c.setDash()
    doc.text(doc.width / 2, y + 12, 'Please detach and return this portion '
             'with your payment', 'Sans-Italic', 7.5, '#555555', 'centre')
    doc.lines(doc.margin, y + 34, (name, 'PO Box 4410',
                                   f'{hh.TOWN}, OR 97355'), 'Sans', 9)
    doc.lines(doc.margin + 300, y + 34, (
        f'Account number   {hh.POWER_ACCOUNT}',
        f'Amount due       {money(total)}',
        f'Due date         {us_date(due)}', 'Amount enclosed  $__________'),
        'Mono', 9)
    doc.text(doc.margin, y + 100, f'||| {hh.POWER_ACCOUNT.replace("-", "")} '
             f'{int(total * 100):08d} 7 |||', 'Mono', 10)

    doc.new_page()
    y = doc.letterhead(name, colour, 'bolt', (), band=True)
    y = doc.text(doc.margin, y, 'Account activity', 'Sans-Bold', 12,
                 colour) + 4
    prev = total * 0.93
    rows = [['Date', 'Description', 'Amount'],
            [us_date(start), 'Previous balance', money(prev)],
            [us_date(start + datetime.timedelta(days=12)),
             'Payment received - thank you', money(-prev)],
            [us_date(bill_date), 'Current charges', money(total)],
            ['', 'Amount due', money(total)]]
    y = doc.table(doc.margin, y, (90, 320, 94), rows, aligns='llr',
                  header_fill='#fbe3bd') + 20
    for head, body in (
            ('Understanding your bill', 'The basic charge covers the cost of '
             'your meter, billing and customer service, whether or not you '
             'use any electricity. The energy charge is for the kilowatt '
             'hours (kWh) you used in the period. The transmission and '
             'distribution charge pays for the lines and substations which '
             'bring power to your home.'),
            ('Save with Time of Day', 'Members on the Time of Day plan pay '
             'less for electricity used between 9 pm and 7 am. Running a '
             'dishwasher or clothes dryer at night can lower your bill by '
             'up to 12%. Call us or visit our website to sign up.'),
            ('Outages', 'To report an outage, call (541) 555-0111 at any '
             'time. Stay clear of downed lines and assume they are live.'),
            ('Budget billing', 'Spread your costs evenly across the year. '
             'Budget billing averages your last twelve months of use, so '
             'winter heating and summer cooling do not bring large bills.'),
            ('Your rights', 'If you disagree with your bill, call us before '
             'the due date. If we cannot resolve it, you may contact the '
             'state utility commission at 1-800-555-0177.')):
        y = doc.text(doc.margin, y, head, 'Sans-Bold', 10) + 2
        y = doc.para(doc.margin, y, body, face='Sans', size=9, space=10)
    doc.footer(f'{name} • Account {hh.POWER_ACCOUNT}', 2)
    doc.save()


def internet_bill(path, bill_date):
    """A broadband bill, sent by e-mail as a PDF, with a usage table"""
    name, colour = hh.INTERNET
    doc = Doc(path, title=f'{name} statement', author=name)
    y = doc.letterhead(name, colour, 'wave', (
        'brightwave.example.com', 'Support (541) 555-0120'),
        'Fibre to your front door', band=True)
    y = doc.text(doc.margin, y + 6, f'Statement for {long_date(bill_date)}',
                 'Sans-Bold', 14)
    y = doc.lines(doc.margin, y + 4, (
        f'{hh.ADULTS[0]} • {hh.STREET}, {hh.TOWN}, {hh.STATE} {hh.ZIP}',
        f'Account {hh.INTERNET_ACCOUNT} • Plan: Brightwave Fibre 500'),
        'Sans', 9.5) + 10
    due = bill_date + datetime.timedelta(days=15)
    doc.box(doc.margin, y, doc.text_width, 60, fill='#efe8fa', stroke=None,
            radius=6)
    doc.text(doc.margin + 16, y + 26, 'Total due', 'Sans', 11)
    doc.text(doc.margin + 16, y + 46, f'Paid automatically on {us_date(due)}',
             'Sans', 9, '#555555')
    doc.text(doc.right - 16, y + 38, '$79.99', 'Sans-Bold', 22, colour,
             'right')
    y += 84
    rows = [['Item', 'Period', 'Amount'],
            ['Brightwave Fibre 500 (500/500 Mbps)',
             f'{bill_date:%b} 1 - {bill_date:%b} 30', '$70.00'],
            ['Wi-Fi 7 router rental', '', '$8.00'],
            ['Paperless discount', '', '-$3.00'],
            ['Regulatory recovery fee', '', '$2.49'],
            ['State and local taxes', '', '$2.50'],
            ['Total', '', '$79.99']]
    y = doc.table(doc.margin, y, (260, 150, 94), rows, aligns='llr',
                  header_fill='#ddd0f2', size=9.5) + 24
    y = doc.text(doc.margin, y, 'Your data this month', 'Sans-Bold', 12,
                 colour) + 4
    rows = [['Week', 'Download (GB)', 'Upload (GB)', 'Peak speed (Mbps)']]
    for week, down, up, peak in ((1, 212.4, 31.0, 487), (2, 188.9, 27.6, 491),
                                 (3, 240.2, 44.8, 476), (4, 205.7, 29.3, 489)):
        rows.append([f'Week {week}', f'{down:.1f}', f'{up:.1f}', str(peak)])
    rows.append(['Total', '847.2', '132.7', ''])
    y = doc.table(doc.margin, y, (126, 126, 126, 126), rows, aligns='lrrr',
                  header_fill='#ddd0f2', zebra='#f7f3fd') + 20
    doc.para(doc.margin, y, 'Your plan has no data cap. Speeds are measured '
             'at the router and can be lower over Wi-Fi.', face='Sans',
             size=9)
    doc.footer(f'{name} • {hh.INTERNET_ACCOUNT}', 2)

    doc.new_page()
    y = doc.letterhead(name, colour, 'wave', (), band=True)
    for head, body in (
            ('Moving house?', 'Tell us 14 days before you move and we will '
             'set up service at your new address with no installation fee.'),
            ('Router rental', 'Your router remains the property of '
             'Brightwave. If you close your account, return it within 30 '
             'days to avoid a non-return charge of $150.'),
            ('Autopay', 'Your payment is taken from your account on file on '
             'the due date. To change your payment method, sign in to My '
             'Brightwave.'),
            ('Questions about this statement', 'Call (541) 555-0120, '
             '7 am to 10 pm, seven days a week.')):
        y = doc.text(doc.margin, y, head, 'Sans-Bold', 10) + 2
        y = doc.para(doc.margin, y, body, face='Sans', size=9, space=10)
    doc.footer(f'{name} • {hh.INTERNET_ACCOUNT}', 2)
    doc.save()


def water_bill(path, bill_date):
    """A one-page quarterly water and sewer bill, in black only"""
    name = hh.WATER[0]
    doc = Doc(path, title='Utility bill', author=name)
    y = doc.lines(doc.width / 2, 50, (name.upper(),
                                      'Utility Billing • 200 Main Street',
                                      f'{hh.TOWN}, OR 97355 • (541) 555-0130'),
                  'Sans-Bold', 10, align='centre', leading=14) + 10
    doc.rule(y)
    y = doc.address_block(y + 24, (hh.ADULTS[0].upper(),
                                   hh.STREET.upper(),
                                   f'{hh.TOWN.upper()} OR {hh.ZIP}'))
    rows = [['Account', 'Service address', 'Bill date', 'Due date'],
            [hh.WATER_ACCOUNT, hh.STREET, us_date(bill_date),
             us_date(bill_date + datetime.timedelta(days=20))]]
    y = doc.table(doc.margin, y + 10, (110, 170, 110, 114), rows,
                  header_fill='#e6e6e6') + 20
    rows = [['Service', 'Previous', 'Current', 'Used (gal)', 'Amount'],
            ['Water - residential 5/8"', '1,204,300', '1,221,900', '17,600',
             '$58.21'],
            ['Sewer', '', '', '', '$71.40'],
            ['Storm drainage', '', '', '', '$12.75'],
            ['Street maintenance fee', '', '', '', '$6.00'],
            ['Total due', '', '', '', '$148.36']]
    y = doc.table(doc.margin, y, (170, 80, 80, 80, 94), rows,
                  header_fill='#e6e6e6', aligns='lrrrr') + 24
    for para in (
            'Water use is billed every three months, from meter readings '
            'taken in the last week of the quarter. Your use this quarter '
            'averaged 191 gallons a day.',
            'Watering restrictions apply from June 1 to September 30: lawns '
            'may be watered on odd days at odd-numbered addresses and on '
            'even days at even-numbered addresses, before 10 am or after '
            '6 pm.',
            'Pay online at cedarridge.example.gov/pay, by phone, or at City '
            'Hall. A late fee of $10 applies to payments received after the '
            'due date.'):
        y = doc.para(doc.margin, y, para, face='Sans', size=9.5, space=8)
    doc.save()


TRANSACTIONS = (
    # day, description, amount (negative for money out)
    (1, 'DIRECT DEP CEDAR RIDGE SCH DIST PAYROLL', 2841.66),
    (2, 'CHECK 1184', -1650.00),
    (3, 'DEBIT GREEN BASKET MARKET #12', -84.31),
    (5, 'ACH JUNIPER VALLEY ELEC AUTOPAY', -101.27),
    (6, 'DEBIT PINE & NAIL HARDWARE', -46.90),
    (8, 'ACH BRIGHTWAVE BROADBAND', -79.99),
    (9, 'DEBIT CEDAR RIDGE PUBLIC LIBRARY', -4.50),
    (11, 'DEBIT GREEN BASKET MARKET #12', -112.08),
    (12, 'ACH NORTHSTAR MUTUAL INS PREM', -142.10),
    (14, 'ATM WITHDRAWAL 3RD & OAK', -60.00),
    (15, 'DIRECT DEP JUNIPER VALLEY CLINIC PAYROLL', 2215.40),
    (16, 'ACH MERIDIAN CARD PAYMENT', -618.42),
    (18, 'DEBIT MAIN STREET AUTO CARE', -312.84),
    (19, 'DEBIT BRIGHT SMILE FAMILY DENTAL', -45.00),
    (21, 'DEBIT GREEN BASKET MARKET #12', -96.55),
    (22, 'TRANSFER TO SAVINGS ****4402', -400.00),
    (23, 'CHECK 1185 HARTWELL BUILDERS', -1500.00),
    (25, 'DEBIT CEDAR RIDGE FOOD PANTRY DONATION', -50.00),
    (27, 'DEBIT GREEN BASKET MARKET #12', -71.22),
    (28, 'ACH CITY OF CEDAR RIDGE UTIL', -148.36),
    (29, 'INTEREST PAYMENT', 1.84),
    (30, 'DEBIT SKYLINE AIR TICKETS', -684.60),
)


def bank_statement(path, month):
    """A three-page checking account statement, downloaded as a PDF"""
    name, colour = hh.BANK
    doc = Doc(path, title=f'Statement {month:%B %Y}', author=name)
    first = datetime.date(month.year, month.month, 1)
    last = datetime.date(month.year, month.month, 30)
    opening = 3874.12
    rows = [['Date', 'Description', 'Withdrawals', 'Deposits', 'Balance']]
    balance = opening
    for day, desc, amount in TRANSACTIONS:
        balance += amount
        rows.append([f'{month.month:02d}/{day:02d}', desc,
                     money(-amount) if amount < 0 else '',
                     money(amount) if amount > 0 else '', money(balance)])
    ins = sum(a for _, _, a in TRANSACTIONS if a > 0)
    outs = -sum(a for _, _, a in TRANSACTIONS if a < 0)

    y = doc.letterhead(name, colour, 'tree', (
        'Member FDIC', 'redwoodbank.example.com', '(541) 555-0150'),
        'Banking rooted in the valley since 1921')
    y = doc.text(doc.margin, y, 'Everyday Checking statement', 'Serif-Bold',
                 16, colour)
    y = doc.lines(doc.margin, y + 4, (
        f'{hh.ADULTS[0]} and {hh.ADULTS[1]}', hh.STREET,
        f'{hh.TOWN}, {hh.STATE} {hh.ZIP}'), 'Sans', 10) + 8
    doc.lines(doc.right, y - 60, (
        f'Account number {hh.BANK_ACCOUNT}',
        f'Statement period {us_date(first)} to {us_date(last)}'), 'Sans', 9,
        align='right')
    num_in = sum(a > 0 for *_, a in TRANSACTIONS)
    num_out = sum(a < 0 for *_, a in TRANSACTIONS)
    rows_sum = [['Summary', ''],
                ['Opening balance', money(opening)],
                [f'Deposits and credits ({num_in})', money(ins)],
                [f'Withdrawals and debits ({num_out})', money(-outs)],
                ['Closing balance', money(balance)]]
    y = doc.table(doc.margin, y, (300, 120), rows_sum, aligns='lr',
                  header_fill='#f3dcd5') + 20
    y = doc.text(doc.margin, y, 'Transactions', 'Serif-Bold', 13,
                 colour) + 4
    widths = (44, 250, 72, 66, 72)
    y = doc.table(doc.margin, y, widths, rows[:12], size=8.5, aligns='llrrr',
                  header_fill='#f3dcd5', zebra='#faf3f1')
    doc.footer(f'{name} • Everyday Checking {hh.BANK_ACCOUNT}', 3)
    doc.new_page()
    y = doc.letterhead(name, colour, 'tree', ())
    y = doc.text(doc.margin, y, 'Transactions (continued)', 'Serif-Bold', 13,
                 colour) + 4
    y = doc.table(doc.margin, y, widths, [rows[0]] + rows[12:], size=8.5,
                  aligns='llrrr', header_fill='#f3dcd5', zebra='#faf3f1') + 20
    y = doc.text(doc.margin, y, 'Checks paid', 'Serif-Bold', 13, colour) + 4
    doc.table(doc.margin, y, (100, 100, 100), [
        ['Check', 'Date paid', 'Amount'],
        ['1184', f'{month.month:02d}/02', '$1,650.00'],
        ['1185', f'{month.month:02d}/23', '$1,500.00']], aligns='lll',
        header_fill='#f3dcd5')
    doc.footer(f'{name} • Everyday Checking {hh.BANK_ACCOUNT}', 3)
    doc.new_page()
    y = doc.letterhead(name, colour, 'tree', ())
    for head, body in (
            ('In case of errors or questions about your electronic '
             'transfers', 'Call us at (541) 555-0150 or write to us at '
             'Redwood Community Bank, PO Box 900, Cedar Ridge, OR 97355, as '
             'soon as you can, if you think your statement or receipt is '
             'wrong or if you need more information about a transfer listed '
             'on the statement or receipt. We must hear from you no later '
             'than 60 days after we sent the first statement on which the '
             'problem or error appeared. Tell us your name and account '
             'number, describe the error or the transfer you are unsure '
             'about, and explain as clearly as you can why you believe it '
             'is an error, and tell us the dollar amount of the suspected '
             'error. We will investigate your complaint and will correct '
             'any error promptly.'),
            ('Overdraft protection', 'Link your savings account to your '
             'checking account and we will move money across to cover an '
             'overdraft, for a fee of $5 per transfer rather than $32 per '
             'item.'),
            ('Balancing your account', 'Write down any checks, withdrawals '
             'or deposits not shown on this statement. Add deposits not '
             'shown to the closing balance and subtract withdrawals not '
             'shown. The result should match the balance in your '
             'register.')):
        y = doc.text(doc.margin, y, head, 'Sans-Bold', 10) + 2
        y = doc.para(doc.margin, y, body, face='Serif', size=9.5, space=12)
    doc.footer(f'{name} • Everyday Checking {hh.BANK_ACCOUNT}', 3)
    doc.save()


def card_statement(path, close):
    """A two-page credit card statement, posted on paper"""
    name, colour = hh.CARD
    doc = Doc(path, title='Statement', author=name)
    y = doc.letterhead(name, colour, 'star', (
        'Customer service 1-800-555-0160', 'Lost or stolen 1-800-555-0161'),
        'Meridian Rewards Visa')
    doc.address_block(y + 6, (hh.ADULTS[1].upper(), hh.STREET.upper(),
                              f'{hh.TOWN.upper()} OR {hh.ZIP}'))
    bx = doc.right - 230
    doc.box(bx, y, 230, 120, stroke=colour, weight=1.2)
    doc.lines(bx + 10, y + 18, ('Account ending', 'Closing date',
                                'New balance', 'Minimum payment due',
                                'Payment due date', 'Credit limit'), 'Sans', 9)
    doc.lines(bx + 220, y + 18, ('7714', us_date(close), '$618.42', '$35.00',
                                 us_date(close + datetime.timedelta(days=25)),
                                 '$12,000.00'), 'Sans-Bold', 9, align='right')
    y += 140
    y = doc.text(doc.margin, y, 'Transactions', 'Sans-Bold', 12, colour) + 4
    rows = [['Trans date', 'Post date', 'Description', 'Amount'],
            ['03/14', '03/15', 'GREEN BASKET MARKET CEDAR RIDGE OR', '$64.12'],
            ['03/16', '03/17', 'RIVERBEND BOOKS EUGENE OR', '$38.95'],
            ['03/19', '03/20', 'PINE & NAIL HARDWARE CEDAR RIDGE OR',
             '$129.40'],
            ['03/22', '03/23', 'CASCADE CINEMAS 8 EUGENE OR', '$46.00'],
            ['03/27', '03/28', 'SUMMIT OUTDOOR SUPPLY BEND OR', '$187.36'],
            ['03/30', '03/31', 'GREEN BASKET MARKET CEDAR RIDGE OR', '$58.71'],
            ['04/02', '04/03', 'STREAMLINE MUSIC MONTHLY', '$10.99'],
            ['04/05', '04/06', 'OAK & EMBER PIZZA CEDAR RIDGE OR', '$41.80'],
            ['04/08', '04/09', 'PAYMENT - THANK YOU', '-$402.66'],
            ['04/09', '04/10', 'SUMMIT OUTDOOR SUPPLY BEND OR', '$41.09']]
    y = doc.table(doc.margin, y, (66, 66, 278, 94), rows, aligns='lllr',
                  header_fill='#d6ebeb', zebra='#f2f9f9') + 20
    rows = [['Rewards summary', 'Points'],
            ['Points earned this period', '618'],
            ['Bonus points at grocery stores', '245'],
            ['Points available', '14,882']]
    doc.table(doc.margin, y, (200, 80), rows, aligns='lr',
              header_fill='#d6ebeb')
    doc.footer(f'{name} • Account ending 7714', 2)
    doc.new_page()
    y = doc.letterhead(name, colour, 'star', ())
    y = doc.text(doc.margin, y, 'Interest charge calculation', 'Sans-Bold',
                 12, colour) + 4
    rows = [['Type of balance', 'APR', 'Balance subject to interest',
             'Interest charge'],
            ['Purchases', '21.49% (v)', '$0.00', '$0.00'],
            ['Cash advances', '28.24% (v)', '$0.00', '$0.00']]
    y = doc.table(doc.margin, y, (150, 90, 160, 104), rows, aligns='lrrr',
                  header_fill='#d6ebeb') + 20
    for head, body in (
            ('Minimum payment warning', 'If you make only the minimum '
             'payment each period, you will pay more in interest and it '
             'will take you longer to pay off your balance.'),
            ('Billing rights summary', 'If you think there is an error on '
             'your statement, write to us at Meridian Card Services, PO Box '
             '7700, Wilmington, DE 19850. In your letter, give us your name, '
             'account number, the dollar amount of the suspected error and '
             'why you believe there is an error. You must contact us within '
             '60 days after the error appeared on your statement. While we '
             'investigate, you do not have to pay the amount in question.'),
            ('Disputed purchases', 'If you have a problem with the quality '
             'of goods or services you bought with your card, and you have '
             'tried in good faith to correct the problem with the merchant, '
             'you may have the right not to pay the remaining amount due.')):
        y = doc.text(doc.margin, y, head, 'Sans-Bold', 10) + 2
        y = doc.para(doc.margin, y, body, face='Sans', size=9, space=12)
    doc.footer(f'{name} • Account ending 7714', 2)
    doc.save()


def power_marks(num, hand):
    """Noting that the bill has been paid"""
    if num == 0:
        due = hand.find('Due', 'date')
        hand.write(due.x0 + 30, due.y1 + 22, 'paid 3/20\nconf #A4471', 14)


def card_marks(num, hand):
    """Laura queries a charge on the credit card statement"""
    if num == 0:
        word = hand.find('SUMMIT', 'OUTDOOR', nth=1)
        hand.circle(word.x0 - 70, word.y0 - 6, 330, word.y1 - word.y0 + 12,
                    ink=(30, 30, 30))
        hand.write(word.x0 + 160, word.y1 + 24, 'charged twice? called 4/18'
                   '\nrefund due in 7-10 days', 13, ink=(30, 30, 30))


def tax_summary(path, year):
    """A four-page summary of the year's tax return, from the accountant"""
    name, colour = hh.CPA
    doc = Doc(path, title=f'{year} Tax Return Summary', author=name)

    def heading(page_title):
        y = doc.letterhead(name, colour, 'shield', (
            '310 Willamette Street, Suite 4', 'Eugene, OR 97401',
            '(541) 555-0170'), 'Certified Public Accountants')
        y = doc.text(doc.margin, y, page_title, 'Serif-Bold', 15, colour)
        return doc.text(doc.margin, y + 2, f'{hh.ADULTS[0]} and '
                        f'{hh.ADULTS[1]} • Married filing jointly • Tax year '
                        f'{year}', 'Sans', 9.5, '#444444') + 10

    def boxes(y, rows):
        """Draw form-like rows of numbered boxes"""
        for num, (label, value) in enumerate(rows, 1):
            doc.box(doc.margin, y, 30, 22, fill='#eef1f6', stroke='#8090a8')
            doc.text(doc.margin + 15, y + 15, str(num), 'Sans-Bold', 9,
                     align='centre')
            doc.box(doc.margin + 30, y, 330, 22, stroke='#8090a8')
            doc.text(doc.margin + 38, y + 15, label, 'Sans', 9)
            doc.box(doc.margin + 360, y, 144, 22, stroke='#8090a8')
            doc.text(doc.margin + 498, y + 15, value, 'Mono', 9.5,
                     align='right')
            y += 22
        return y + 16

    y = heading(f'{year} Federal Return Summary')
    y = boxes(y, (
        ('Wages, salaries, tips (W-2)', '$124,186'),
        ('Taxable interest', '$212'),
        ('Ordinary dividends', '$1,046'),
        ('Capital gain or (loss)', '$880'),
        ('Total income', '$126,324'),
        ('Adjustments to income', '($3,000)'),
        ('Adjusted gross income', '$123,324'),
        ('Standard or itemized deduction', '($31,500)'),
        ('Taxable income', '$91,824'),
        ('Tax', '$10,577'),
        ('Child tax credit (2 children)', '($4,400)'),
        ('Total tax', '$6,177'),
        ('Federal income tax withheld', '$8,940'),
        ('Refund', '$2,763')))
    doc.para(doc.margin, y, 'Your refund will be deposited into your '
             f'account at {hh.BANK[0]} ending 3381, usually within 21 days '
             'of the return being accepted.', face='Sans', size=9.5)
    doc.footer(f'{name} • Prepared for the {hh.FAMILY} family', 4)

    doc.new_page()
    y = heading(f'{year} Oregon Return Summary')
    y = boxes(y, (
        ('Federal adjusted gross income', '$123,324'),
        ('Additions', '$0'),
        ('Subtractions (federal tax liability)', '($6,177)'),
        ('Oregon income after subtractions', '$117,147'),
        ('Deduction', '($5,495)'),
        ('Oregon taxable income', '$111,652'),
        ('Tax', '$9,384'),
        ('Personal exemption credits (4)', '($1,000)'),
        ('Working family household credit', '$0'),
        ('Net tax', '$8,384'),
        ('Oregon tax withheld', '$8,110'),
        ('Amount you owe', '$274')))
    doc.para(doc.margin, y, 'Pay by April 15 to avoid interest and '
             'penalties. A payment voucher is enclosed with your filing '
             'copy.', face='Sans', size=9.5)
    doc.footer(f'{name} • Prepared for the {hh.FAMILY} family', 4)

    doc.new_page()
    y = heading('Charitable contributions')
    rows = [['Organisation', 'Date', 'Type', 'Amount'],
            [hh.PANTRY[0], '01/18/' + str(year), 'Cash', '$150.00'],
            [hh.PANTRY[0], '06/25/' + str(year), 'Cash', '$50.00'],
            ['Cedar Ridge Elementary PTA', '09/09/' + str(year), 'Cash',
             '$75.00'],
            ['Willamette Wildlife Rescue', '11/30/' + str(year), 'Cash',
             '$120.00'],
            ['Second Chance Thrift (clothing)', '12/14/' + str(year),
             'Goods', '$210.00'],
            ['Total', '', '', '$605.00']]
    y = doc.table(doc.margin, y, (240, 90, 80, 94), rows, aligns='lllr',
                  header_fill='#dfe4ee') + 20
    y = doc.para(doc.margin, y, 'Your contributions are below the amount at '
                 'which itemizing would lower your tax, so the standard '
                 'deduction was used. Keep the receipts with this summary '
                 'for at least three years.', face='Sans', size=9.5)
    doc.footer(f'{name} • Prepared for the {hh.FAMILY} family', 4)

    doc.new_page()
    y = heading('Notes and planning for next year')
    for para in (
            'Your withholding produced a federal refund of $2,763. You may '
            'wish to reduce your federal withholding a little through a new '
            'Form W-4 with your employers, which would give you about $230 '
            'more each month instead of a refund.',
            'Oregon withholding was slightly low. Consider asking the '
            'school district to withhold an extra $25 per pay period.',
            'If you complete the kitchen remodel next year, keep the '
            'invoices: improvements add to the basis of your home and '
            'reduce any gain when you sell it.',
            'Contributions to a 529 college savings plan for Sofia and Theo '
            'can be subtracted on your Oregon return, up to $340 per year '
            'for a joint return.',
            'Please bring your Form 1099-INT from Redwood Community Bank, '
            'your W-2s, and your property tax statement to next year\'s '
            'appointment.'):
        y = doc.para(doc.margin, y, '• ' + para, face='Serif', size=10.5,
                     space=12)
    doc.footer(f'{name} • Prepared for the {hh.FAMILY} family', 4)
    doc.save()


def pantry_receipt(path, when):
    """A small donation receipt, half a letter sheet"""
    name, colour = hh.PANTRY
    size = (letter[0], letter[1] / 2)
    doc = Doc(path, size=size, margin=40, title='Donation receipt',
              author=name)
    y = doc.letterhead(name, colour, 'leaf', (
        '55 Mill Street', f'{hh.TOWN}, OR 97355', 'EIN 93-5550198'),
        'Neighbors feeding neighbors', y=24)
    y = doc.text(doc.width / 2, y, 'DONATION RECEIPT', 'Sans-Bold', 13,
                 colour, 'centre') + 8
    y = doc.lines(doc.margin, y, (
        f'Date received: {long_date(when)}',
        f'Received from: {hh.ADULTS[0]} and {hh.ADULTS[1]}',
        f'Address: {hh.STREET}, {hh.TOWN}, {hh.STATE} {hh.ZIP}',
        'Amount: $50.00 (debit card)'), 'Sans', 10, leading=15) + 8
    doc.para(doc.margin, y, f'Thank you for your gift. {name} is a '
             'tax-exempt organisation under section 501(c)(3) of the Internal '
             'Revenue Code. No goods or services were provided in exchange '
             'for this contribution. Please keep this receipt for your tax '
             'records.', face='Serif-Italic', size=9.5)
    doc.save()


def pantry_marks(num, hand):
    """Someone at the pantry signs the receipt"""
    if num == 0:
        word = hand.find('tax', 'records.')
        hand.signature(word.x1 - 140, word.y1 + 22, 'R. Delgado', 20,
                       ink=(30, 30, 30))
