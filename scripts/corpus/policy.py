# SPDX-License-Identifier: GPL-2.0-or-later

"""Insurance papers for the demo corpus: a homeowners policy and a renewal

The policy is long enough to page through, with clauses worth searching
for, such as water damage or the deductible.
"""

# pylint: disable=too-many-arguments,too-many-locals,too-many-statements

import datetime

from corpus import household as hh
from corpus.layout import Doc, us_date, long_date

# Clauses for the long insurance policy, as (heading, paragraph)
POLICY_SECTIONS = (
    ('Definitions', (
        ('"You" and "your"', 'mean the named insured shown in the '
         'Declarations and the spouse of the named insured if a resident of '
         'the same household. "We", "us" and "our" mean the company '
         'providing this insurance.'),
        ('"Residence premises"', 'means the one-family dwelling where you '
         'reside, including the other structures and grounds, shown as the '
         'residence premises in the Declarations.'),
        ('"Occurrence"', 'means an accident, including continuous or '
         'repeated exposure to substantially the same general harmful '
         'conditions, which results during the policy period in bodily '
         'injury or property damage.'),
        ('"Actual cash value"', 'means the cost to repair or replace damaged '
         'property with property of like kind and quality, less a deduction '
         'for depreciation.'),
    )),
    ('Section I - Property coverages', (
        ('Coverage A - Dwelling', 'We cover the dwelling on the residence '
         'premises, including structures attached to the dwelling, and '
         'materials and supplies on or next to the residence premises used '
         'to construct, alter or repair the dwelling.'),
        ('Coverage B - Other structures', 'We cover other structures on the '
         'residence premises set apart from the dwelling by clear space, '
         'such as a detached garage, shed or fence.'),
        ('Coverage C - Personal property', 'We cover personal property '
         'owned or used by an insured while it is anywhere in the world. '
         'Limits apply to money, jewelry, firearms, silverware and business '
         'property, as shown in the special limits of liability.'),
        ('Coverage D - Loss of use', 'If a loss covered under this section '
         'makes the residence premises not fit to live in, we cover any '
         'necessary increase in living expenses incurred by you so that '
         'your household can maintain its normal standard of living.'),
    )),
    ('Section I - Perils insured against', (
        ('Fire or lightning', 'We insure for direct physical loss to '
         'property caused by fire or lightning.'),
        ('Windstorm or hail', 'This peril does not include loss to the '
         'inside of a building caused by rain, snow, sleet, sand or dust '
         'unless the wind or hail first damages the roof or walls and the '
         'wind forces rain, snow, sleet, sand or dust through the opening.'),
        ('Water damage', 'We cover sudden and accidental discharge or '
         'overflow of water or steam from within a plumbing, heating, air '
         'conditioning or automatic fire protective sprinkler system or from '
         'within a household appliance. We do not cover loss caused by '
         'continuous or repeated seepage or leakage of water over a period '
         'of weeks, months or years.'),
        ('Freezing', 'We cover freezing of a plumbing, heating, air '
         'conditioning or sprinkler system or of a household appliance, but '
         'only if you have used reasonable care to maintain heat in the '
         'building or shut off the water supply and drain all systems.'),
        ('Theft', 'We cover theft, including attempted theft and loss of '
         'property from a known place when it is likely that the property '
         'has been stolen.'),
    )),
    ('Section I - Exclusions', (
        ('Earth movement', 'We do not insure for loss caused by earthquake, '
         'landslide, mudflow, sinkhole or the sinking, rising or shifting '
         'of the earth, unless direct loss by fire or explosion ensues.'),
        ('Flood', 'We do not insure for loss caused by flood, surface water, '
         'waves, tidal water, overflow of a body of water, or water below '
         'the surface of the ground. Flood insurance is available '
         'separately through the National Flood Insurance Program.'),
        ('Neglect', 'We do not insure for loss caused by neglect, meaning '
         'neglect of an insured to use all reasonable means to save and '
         'preserve property at and after the time of a loss.'),
        ('Wear and tear', 'We do not insure for wear and tear, marring, '
         'deterioration, mechanical breakdown, latent defect, rust, mold or '
         'wet or dry rot.'),
    )),
    ('Section I - Conditions', (
        ('Deductible', 'Unless otherwise noted in this policy, the following '
         'deductible provision applies: subject to the policy limits, we '
         'will pay only that part of the total of all loss payable that '
         'exceeds the deductible amount shown in the Declarations.'),
        ('Duties after loss', 'In case of a loss to covered property, you '
         'must give prompt notice to us or our agent, protect the property '
         'from further damage, make reasonable and necessary repairs, keep '
         'an accurate record of repair expenses, and prepare an inventory '
         'of damaged personal property showing the quantity, description, '
         'actual cash value and amount of loss.'),
        ('Loss settlement', 'Buildings are settled at replacement cost '
         'without deduction for depreciation, up to the limit of liability '
         'that applies, provided the building is repaired or replaced. '
         'Personal property is settled at actual cash value unless the '
         'replacement cost endorsement is shown in the Declarations.'),
        ('Appraisal', 'If you and we fail to agree on the amount of loss, '
         'either may demand an appraisal of the loss. Each party will choose '
         'a competent and impartial appraiser within 20 days after '
         'receiving a written request from the other.'),
    )),
    ('Section II - Liability coverages', (
        ('Coverage E - Personal liability', 'If a claim is made or a suit '
         'is brought against an insured for damages because of bodily '
         'injury or property damage caused by an occurrence to which this '
         'coverage applies, we will pay up to our limit of liability for '
         'the damages for which an insured is legally liable, and provide '
         'a defense at our expense by counsel of our choice.'),
        ('Coverage F - Medical payments to others', 'We will pay the '
         'necessary medical expenses that are incurred or medically '
         'ascertained within three years from the date of an accident '
         'causing bodily injury to a person on the insured location with '
         'the permission of an insured.'),
        ('Damage to property of others', 'We will pay, at replacement cost, '
         'up to $1,000 per occurrence for property damage to property of '
         'others caused by an insured.'),
    )),
    ('Section II - Exclusions', (
        ('Motor vehicle liability', 'Coverages E and F do not apply to any '
         'motor vehicle liability if, at the time and place of an '
         'occurrence, the involved motor vehicle is registered for use on '
         'public roads or property.'),
        ('Expected or intended injury', 'Coverages E and F do not apply to '
         'bodily injury or property damage which is expected or intended by '
         'an insured, even if the resulting bodily injury or property '
         'damage is of a different kind or degree than initially '
         'expected.'),
        ('Business', 'Coverages E and F do not apply to bodily injury or '
         'property damage arising out of or in connection with a business '
         'conducted from an insured location.'),
    )),
    ('Sections I and II - Conditions', (
        ('Policy period', 'This policy applies only to loss in Section I or '
         'bodily injury or property damage in Section II which occurs '
         'during the policy period.'),
        ('Concealment or fraud', 'We provide coverage to no insureds under '
         'this policy if, whether before or after a loss, an insured has '
         'intentionally concealed or misrepresented any material fact or '
         'circumstance, engaged in fraudulent conduct, or made false '
         'statements relating to this insurance.'),
        ('Cancellation', 'You may cancel this policy at any time by '
         'returning it to us or by letting us know in writing of the date '
         'cancellation is to take effect. When this policy has been in '
         'effect for less than 60 days and is not a renewal with us, we may '
         'cancel for any reason by letting you know at least 10 days before '
         'the date cancellation takes effect.'),
        ('Nonrenewal', 'We may elect not to renew this policy. We may do so '
         'by delivering to you, or mailing to you at your mailing address '
         'shown in the Declarations, written notice at least 30 days before '
         'the expiration date of this policy.'),
        ('Assignment', 'Assignment of this policy will not be valid unless '
         'we give our written consent.'),
    )),
    ('Endorsements', (
        ('HO 04 90 - Personal property replacement cost', 'Covered losses '
         'to personal property are settled at replacement cost, meaning the '
         'cost of repair or replacement without deduction for '
         'depreciation, but not more than the amount actually and '
         'necessarily spent to repair or replace the property.'),
        ('HO 04 95 - Water back-up and sump discharge', 'We cover direct '
         'physical loss caused by water which backs up through sewers or '
         'drains, or which overflows or is discharged from a sump, sump pump '
         'or related equipment, up to $10,000 per loss.'),
        ('Identity fraud expense', 'We will pay up to $25,000 for expenses '
         'incurred by an insured as the direct result of any one identity '
         'fraud first discovered during the policy period.'),
    )),
)


def home_policy(path, start):
    """A ten-page homeowners policy: declarations and the policy form"""
    name, colour = hh.INSURER
    end = start.replace(year=start.year + 1)
    doc = Doc(path, title='Homeowners policy', author=name)
    footer = f'{name} • Policy {hh.HOME_POLICY} • Form HO-3 (Ed. 05-24)'
    y = doc.letterhead(name, colour, 'shield', (
        '1 Northstar Plaza, Salem, OR 97301', 'Claims 1-800-555-0180',
        'northstar.example.com'), 'Protecting Northwest families since 1952',
        band=True)
    y = doc.text(doc.width / 2, y, 'HOMEOWNERS POLICY DECLARATIONS',
                 'Sans-Bold', 14, colour, 'centre') + 8
    rows = [['Policy number', hh.HOME_POLICY],
            ['Policy period', f'{us_date(start)} to {us_date(end)}, '
             '12:01 am standard time'],
            ['Named insured', f'{hh.ADULTS[0]} and {hh.ADULTS[1]}'],
            ['Residence premises', f'{hh.STREET}, {hh.TOWN}, {hh.STATE} '
             f'{hh.ZIP}'],
            ['Agent', 'Dana Whitfield, Whitfield Insurance Agency, '
             '(541) 555-0181']]
    y = doc.table(doc.margin, y, (130, 374), rows, header=False,
                  aligns='ll', size=9.5) + 18
    y = doc.text(doc.margin, y, 'Coverages and limits of liability',
                 'Sans-Bold', 12, colour) + 4
    rows = [['Coverage', 'Limit', 'Premium'],
            ['A - Dwelling', '$485,000', '$1,104.00'],
            ['B - Other structures', '$48,500', 'Included'],
            ['C - Personal property', '$242,500', 'Included'],
            ['D - Loss of use', '$97,000', 'Included'],
            ['E - Personal liability (each occurrence)', '$500,000',
             '$38.00'],
            ['F - Medical payments to others (each person)', '$5,000',
             '$12.00'],
            ['Endorsements HO 04 90, HO 04 95, identity fraud', '', '$151.20'],
            ['Multi-policy discount (auto)', '', '-$100.00'],
            ['Total annual premium', '', '$1,205.20']]
    y = doc.table(doc.margin, y, (300, 100, 104), rows, aligns='lrr',
                  header_fill='#d5e0f0', zebra='#f2f6fb') + 18
    y = doc.lines(doc.margin, y, (
        'Deductible: $1,000 all perils; wind and hail 1% of Coverage A',
        'Mortgagee: Redwood Community Bank, ISAOA, PO Box 900, '
        'Cedar Ridge, OR 97355',
        'Construction: frame, built 1998, composition shingle roof '
        'replaced 2021',
        'Protection class 4 • 2.1 miles to fire station • monitored smoke '
        'alarms'), 'Sans', 9.5, leading=14)
    doc.footer(footer, 11)

    num = 1
    for head, clauses in POLICY_SECTIONS:
        doc.new_page()
        y = doc.letterhead(name, colour, 'shield', ('Form HO-3',), band=True)
        y = doc.text(doc.margin, y, head.upper(), 'Sans-Bold', 12,
                     colour) + 6
        for sub, body in clauses:
            y = doc.text(doc.margin, y, f'{num}. {sub}', 'Serif-Bold',
                         10.5) + 2
            y = doc.para(doc.margin + 14, y, body, doc.text_width - 14,
                         size=10, space=10)
            num += 1
        if head == 'Section I - Conditions':
            y = doc.para(doc.margin, y + 6, 'In the event of a claim, call '
                         '1-800-555-0180 at any time. Take photographs of '
                         'the damage before you begin repairs, and keep '
                         'damaged items until our adjuster has seen them.',
                         face='Sans-Italic', size=9.5)
        doc.footer(footer, 11)
    doc.new_page()
    y = doc.letterhead(name, colour, 'shield', ('Form HO-3',), band=True)
    y = doc.text(doc.margin, y, 'NOTICE OF PRIVACY PRACTICES', 'Sans-Bold',
                 12, colour) + 6
    for para in (
            'We collect information about you from your application, from '
            'your transactions with us, and from consumer reporting '
            'agencies. We use it to underwrite your policy, to settle '
            'claims, and to offer you other products.',
            'We do not disclose information about you to anyone except as '
            'permitted by law, such as to our agents, to adjusters and '
            'repair firms handling your claim, and to regulators.',
            'You may ask to see the information we hold about you and ask '
            'us to correct it. Write to the Privacy Officer, Northstar '
            'Mutual Insurance, 1 Northstar Plaza, Salem, OR 97301.'):
        y = doc.para(doc.margin, y, para, size=10, space=10)
    doc.footer(footer, 11)
    doc.save()


def car_renewal(path, when):
    """A one-page letter about renewing the car insurance"""
    name, colour = hh.INSURER
    doc = Doc(path, title='Auto renewal', author=name)
    y = doc.letterhead(name, colour, 'shield', (
        '1 Northstar Plaza, Salem, OR 97301', 'Claims 1-800-555-0180'))
    y = doc.text(doc.margin, y + 6, long_date(when), 'Serif', 11) + 16
    y = doc.lines(doc.margin, y, (f'{hh.ADULTS[0]} and {hh.ADULTS[1]}',
                                  hh.STREET, f'{hh.TOWN}, {hh.STATE} '
                                  f'{hh.ZIP}'), 'Serif', 11) + 16
    y = doc.text(doc.margin, y, f'Re: Renewal of auto policy {hh.AUTO_POLICY}',
                 'Serif-Bold', 11) + 12
    renewal = when + datetime.timedelta(days=30)
    for para in (
            f'Dear {hh.ADULTS[0].split()[0]} and {hh.ADULTS[1].split()[0]},',
            'Thank you for insuring your vehicle with Northstar Mutual. Your '
            f'auto policy renews on {long_date(renewal)}. Your new premium '
            'for six months is $612.40, a change of $18.60 from your current '
            'premium, which reflects higher repair costs across Oregon.',
            f'Your policy covers your {hh.CAR}, license plate {hh.PLATE}, '
            'with liability limits of $100,000/$300,000, comprehensive and '
            'collision coverage with a $500 deductible, and uninsured '
            'motorist coverage.',
            'Because you also insure your home with us, your multi-policy '
            'discount of 12% has been applied. You could save a further 5% '
            'by enrolling in paperless billing.',
            'There is nothing you need to do: your payment will continue to '
            'be taken from your account each month. If you would like to '
            'review your coverage, please call your agent, Dana Whitfield, '
            'at (541) 555-0181.',
            'Sincerely,'):
        y = doc.para(doc.margin, y, para, size=11, space=10)
    doc.lines(doc.margin, y + 40, ('Kenneth Orr', 'Vice President, Personal '
                                   'Lines'), 'Serif', 11)
    doc.save()


def car_renewal_marks(num, hand):
    """The insurer's signature, printed in blue"""
    if num == 0:
        name = hand.find('Kenneth', 'Orr')
        hand.signature(54, name.y0 - 34, 'Kenneth Orr', 22)
