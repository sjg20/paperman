# SPDX-License-Identifier: GPL-2.0-or-later

"""The household whose papers make up the demo corpus

Everyone and everything here is made up. Telephone numbers are in the
555-0100 to 555-0199 range kept for fiction, e-mail addresses are at
example.com and the account numbers follow no real scheme, so nothing in
the corpus can be mistaken for a real record.
"""

FAMILY = 'Reyes'
ADULTS = ('Daniel Reyes', 'Laura Reyes')
CHILDREN = ('Sofia Reyes', 'Theo Reyes')
GRANDMOTHER = 'June Whitlock'

STREET = '418 Juniper Lane'
TOWN = 'Cedar Ridge'
STATE = 'OR'
ZIP = '97355'
ADDRESS = (STREET, f'{TOWN}, {STATE} {ZIP}')
PHONE = '(541) 555-0142'
EMAIL = 'laura.reyes@example.com'

# Where the family rented while working in England, before moving back
UK_ADDRESS = ('Flat 3, 27 Alder Road', 'Reading', 'RG1 9ZZ')
UK_LANDLORD = 'Margaret Ellison'
UK_LANDLORD_ADDRESS = ('14 Kennet Walk', 'Reading', 'RG1 9ZY')
UK_LANDLORD_EMAIL = 'm.ellison@example.com'
UK_LANDLORD_PHONE = '0118 496 0731'

# Organisations, with the colour each prints in
POWER = ('Juniper Valley Electric', '#e08a00')
POWER_ACCOUNT = '4471-2290-18'
INTERNET = ('Brightwave Broadband', '#6a3fb5')
INTERNET_ACCOUNT = 'BW-2086-5513'
WATER = (f'City of {TOWN} Water & Sewer', '#1f6fb2')
WATER_ACCOUNT = '00-31877-02'
GAS = ('Riverbend Natural Gas', '#c0392b')
GAS_ACCOUNT = '7720 418 3305'
BANK = ('Redwood Community Bank', '#8c2d19')
BANK_ACCOUNT = '0091-5520-3381'
CARD = ('Meridian Card Services', '#0f5c5c')
CARD_NUMBER = '5299 •••• •••• 7714'
CPA = ('Holloway & Park, CPAs', '#2b3a55')
PANTRY = (f'{TOWN} Food Pantry', '#3d7a2a')
INSURER = ('Northstar Mutual Insurance', '#14407a')
HOME_POLICY = 'HO-3 7710-4492'
AUTO_POLICY = 'PA 7710-5038'
BUILDER = ('Hartwell Builders', '#7a4b1f')
COUNTY = ('Linden County Tax Collector', '#333333')
COUNTY_ACCOUNT = 'R118204'
APPLIANCE = ('Aquaclean', '#00838f')
DISHWASHER = 'DW-450'
HEAT_PUMP = ('Northwind Comfort', '#c62828')
HEAT_PUMP_SERIAL = 'NW24-0381-7752'
GARAGE = ('Main Street Auto Care', '#b71c1c')
CAR = '2019 Kestrel Tourer 2.5'
PLATE = '8TRK214'
VIN = 'KT9WG2580KX200417'
LAB = ('Valley Diagnostic Laboratories', '#00695c')
SCHOOL = (f'{TOWN} Elementary School', '#1b5e20')
GROCER = 'Green Basket Market'
HARDWARE = 'Pine & Nail Hardware'
AIRLINE = ('Skyline Air', '#0d47a1')
DENTIST = ('Bright Smile Family Dental', '#00897b')
LIBRARY = (f'{TOWN} Public Library', '#5d4037')
CITY = (f'City of {TOWN}', '#2e7d32')
