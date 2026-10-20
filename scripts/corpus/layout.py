# SPDX-License-Identifier: GPL-2.0-or-later

"""Helpers for laying out the corpus's documents with reportlab

Coordinates are in points from the top left of the page, as a reader
sees it, rather than reportlab's bottom left, which keeps the document
code readable. Each helper returns the y position below what it drew.
"""

# pylint: disable=too-many-arguments,too-many-locals,too-many-statements

import os

from reportlab.lib.colors import HexColor, black, white
from reportlab.lib.pagesizes import letter
from reportlab.lib.utils import simpleSplit
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas

LIBERATION = '/usr/share/fonts/truetype/liberation'

# name: file within LIBERATION, with the built-in font to fall back to
FONTS = {
    'Sans': ('LiberationSans-Regular.ttf', 'Helvetica'),
    'Sans-Bold': ('LiberationSans-Bold.ttf', 'Helvetica-Bold'),
    'Sans-Italic': ('LiberationSans-Italic.ttf', 'Helvetica-Oblique'),
    'Serif': ('LiberationSerif-Regular.ttf', 'Times-Roman'),
    'Serif-Bold': ('LiberationSerif-Bold.ttf', 'Times-Bold'),
    'Serif-Italic': ('LiberationSerif-Italic.ttf', 'Times-Italic'),
    'Mono': ('LiberationMono-Regular.ttf', 'Courier'),
    'Mono-Bold': ('LiberationMono-Bold.ttf', 'Courier-Bold'),
}

_registered = {}


def font(name):
    """Get the reportlab name of one of our fonts, registering it first

    The Liberation fonts are embedded, so that the PDFs look the same
    everywhere; without them, reportlab's built-in fonts stand in.

    Args:
        name (str): Name from FONTS, e.g. 'Sans-Bold'

    Returns:
        str: Font name to give reportlab
    """
    if name not in _registered:
        fname, builtin = FONTS[name]
        path = os.path.join(LIBERATION, fname)
        if os.path.exists(path):
            pdfmetrics.registerFont(TTFont(name, path))
            _registered[name] = name
        else:
            _registered[name] = builtin
    return _registered[name]


class Doc:
    """A PDF being laid out, page by page

    Properties:
        c (canvas.Canvas): reportlab canvas
        width (float): Page width in points
        height (float): Page height in points
        margin (float): Left and right margin in points
        page (int): Page number, from 1
    """

    def __init__(self, path, size=letter, margin=54, title=None,
                 author=None):
        self.c = canvas.Canvas(path, pagesize=size, pageCompression=1)
        self.width, self.height = size
        self.margin = margin
        self.page = 1
        if title:
            self.c.setTitle(title)
        if author:
            self.c.setAuthor(author)
            self.c.setCreator(author)

    @property
    def right(self):
        """x position of the right margin"""
        return self.width - self.margin

    @property
    def text_width(self):
        """Width between the margins"""
        return self.width - 2 * self.margin

    def new_page(self):
        """Finish this page and start another"""
        self.c.showPage()
        self.page += 1

    def save(self):
        """Finish the last page and write the file"""
        self.c.showPage()
        self.c.save()

    def fy(self, y):
        """Convert a y position from the top to reportlab's, from the foot"""
        return self.height - y

    def text(self, x, y, txt, face='Sans', size=10, colour=black,
             align='left'):
        """Draw one line of text with its baseline at y

        Args:
            x (float): x position: left, centre or right edge, per align
            y (float): Baseline, from the top
            txt (str): Text to draw
            face (str): Font, from FONTS
            size (float): Size in points
            colour (Color or str): Colour, or a hex string
            align (str): 'left', 'centre' or 'right'

        Returns:
            float: y position of the next line
        """
        c = self.c
        c.setFillColor(_colour(colour))
        c.setFont(font(face), size)
        if align == 'right':
            c.drawRightString(x, self.fy(y), txt)
        elif align == 'centre':
            c.drawCentredString(x, self.fy(y), txt)
        else:
            c.drawString(x, self.fy(y), txt)
        return y + size * 1.25

    def lines(self, x, y, lines, face='Sans', size=10, colour=black,
              leading=None, align='left'):
        """Draw lines of text, one below the other

        Returns:
            float: y position below the last line
        """
        leading = leading or size * 1.3
        for line in lines:
            self.text(x, y, line, face, size, colour, align)
            y += leading
        return y

    def para(self, x, y, txt, width=None, face='Serif', size=10,
             colour=black, leading=None, space=None):
        """Draw a paragraph, wrapped to a width

        Args:
            x (float): Left edge
            y (float): Baseline of the first line, from the top
            txt (str): Text of the paragraph
            width (float): Width to wrap to, or None for the text width
            face (str): Font, from FONTS
            size (float): Size in points
            colour (Color or str): Colour
            leading (float): Distance between lines, or None for 1.3 x size
            space (float): Space after the paragraph, or None for a line

        Returns:
            float: y position for the next paragraph
        """
        width = width or self.text_width
        leading = leading or size * 1.3
        wrapped = simpleSplit(txt, font(face), size, width)
        y = self.lines(x, y, wrapped, face, size, colour, leading)
        return y + (leading if space is None else space)

    def rule(self, y, x0=None, x1=None, colour='#999999', weight=0.6):
        """Draw a horizontal line across the text width"""
        c = self.c
        c.setStrokeColor(_colour(colour))
        c.setLineWidth(weight)
        c.line(self.margin if x0 is None else x0, self.fy(y),
               self.right if x1 is None else x1, self.fy(y))
        return y

    def box(self, x, y, w, h, fill=None, stroke='#000000', weight=0.6,
            radius=0):
        """Draw a rectangle with its top left at x, y"""
        c = self.c
        c.setLineWidth(weight)
        if stroke:
            c.setStrokeColor(_colour(stroke))
        if fill:
            c.setFillColor(_colour(fill))
        args = (x, self.fy(y + h), w, h)
        kw = {'stroke': 1 if stroke else 0, 'fill': 1 if fill else 0}
        if radius:
            c.roundRect(*args, radius, **kw)
        else:
            c.rect(*args, **kw)
        return y + h

    def table(self, x, y, widths, rows, size=9, header=True,
              header_fill='#dddddd', header_colour=black, zebra=None,
              aligns=None, face='Sans', leading=None, grid='#bbbbbb'):
        """Draw a table, one line per row

        Args:
            x (float): Left edge
            y (float): Top edge
            widths (list of float): Width of each column
            rows (list of list of str): Cells, the first row being the
                header if header is True
            size (float): Font size
            header (bool): True if the first row is a header
            header_fill (str): Background of the header row
            header_colour (str or Color): Text colour of the header row
            zebra (str): Background of every other row, or None
            aligns (str): One letter per column: l, r or c
            face (str): Font of the body
            leading (float): Row height, or None for 1.7 x size
            grid (str): Colour of the lines between rows, or None

        Returns:
            float: y position below the table
        """
        leading = leading or size * 1.7
        aligns = aligns or 'l' * len(widths)
        total = sum(widths)
        for num, row in enumerate(rows):
            is_header = header and num == 0
            fill = header_fill if is_header else (
                zebra if zebra and num % 2 == 0 else None)
            if fill:
                self.box(x, y, total, leading, fill=fill, stroke=None)
            base = y + leading * 0.68
            cx = x
            for width, cell, align in zip(widths, row, aligns):
                face_now = 'Sans-Bold' if is_header else face
                colour = header_colour if is_header else black
                if align == 'r':
                    self.text(cx + width - 4, base, cell, face_now, size,
                              colour, 'right')
                elif align == 'c':
                    self.text(cx + width / 2, base, cell, face_now, size,
                              colour, 'centre')
                else:
                    self.text(cx + 4, base, cell, face_now, size, colour)
                cx += width
            y += leading
            if grid:
                self.rule(y, x, x + total, grid, 0.4)
        return y

    def logo(self, x, y, size, shape, colour):  # pylint: disable=too-many-branches
        """Draw a simple made-up logo with its top left at x, y

        Args:
            x (float): Left edge
            y (float): Top edge
            size (float): Width and height
            shape (str): bolt, wave, drop, flame, tree, shield, house, leaf,
                wrench, plane, cross, book, tooth, sun or star
            colour (str): Main colour
        """
        c = self.c
        col = _colour(colour)
        c.saveState()
        c.translate(x, self.fy(y + size))
        c.scale(size / 100, size / 100)
        c.setFillColor(col)
        c.setStrokeColor(col)
        if shape in ('bolt', 'drop', 'wave', 'cross', 'tooth', 'book',
                     'wrench', 'plane'):
            c.circle(50, 50, 50, stroke=0, fill=1)
            c.setFillColor(white)
            c.setStrokeColor(white)
        path = c.beginPath()
        if shape == 'bolt':
            _polygon(c, ((58, 90), (28, 45), (50, 45), (40, 10), (74, 58),
                           (52, 58)))
        elif shape == 'drop':
            path.moveTo(50, 88)
            path.curveTo(30, 60, 22, 46, 26, 34)
            path.curveTo(32, 12, 68, 12, 74, 34)
            path.curveTo(78, 46, 70, 60, 50, 88)
            c.drawPath(path, stroke=0, fill=1)
        elif shape == 'wave':
            c.setLineWidth(8)
            for base in (35, 52, 69):
                path = c.beginPath()
                path.moveTo(18, base)
                path.curveTo(34, base + 12, 50, base - 12, 82, base)
                c.drawPath(path, stroke=1, fill=0)
        elif shape == 'cross':
            c.rect(40, 18, 20, 64, stroke=0, fill=1)
            c.rect(18, 40, 64, 20, stroke=0, fill=1)
        elif shape == 'tooth':
            path.moveTo(30, 75)
            path.curveTo(30, 90, 70, 90, 70, 75)
            path.curveTo(72, 55, 64, 20, 58, 20)
            path.curveTo(54, 20, 54, 45, 50, 45)
            path.curveTo(46, 45, 46, 20, 42, 20)
            path.curveTo(36, 20, 28, 55, 30, 75)
            c.drawPath(path, stroke=0, fill=1)
        elif shape == 'book':
            c.rect(22, 28, 26, 44, stroke=0, fill=1)
            c.rect(52, 28, 26, 44, stroke=0, fill=1)
        elif shape == 'wrench':
            c.setLineWidth(12)
            c.line(30, 30, 66, 66)
            c.circle(70, 70, 14, stroke=0, fill=1)
        elif shape == 'plane':
            _polygon(c, ((18, 46), (82, 64), (60, 40), (66, 18), (54, 18),
                           (46, 36)))
        elif shape == 'tree':
            _polygon(c, ((50, 98), (12, 30), (88, 30)))
            c.rect(42, 4, 16, 28, stroke=0, fill=1)
        elif shape == 'shield':
            path.moveTo(10, 92)
            path.lineTo(90, 92)
            path.lineTo(90, 50)
            path.curveTo(90, 25, 70, 12, 50, 2)
            path.curveTo(30, 12, 10, 25, 10, 50)
            path.close()
            c.drawPath(path, stroke=0, fill=1)
            c.setFillColor(white)
            c.setFont(font('Serif-Bold'), 40)
            c.drawCentredString(50, 38, '★')
        elif shape == 'house':
            _polygon(c, ((50, 96), (6, 56), (18, 56), (18, 6), (82, 6),
                           (82, 56), (94, 56)))
            c.setFillColor(white)
            c.rect(42, 6, 16, 30, stroke=0, fill=1)
        elif shape == 'leaf':
            path.moveTo(15, 15)
            path.curveTo(15, 70, 50, 92, 90, 90)
            path.curveTo(88, 45, 60, 12, 15, 15)
            c.drawPath(path, stroke=0, fill=1)
        elif shape == 'sun':
            c.circle(50, 50, 24, stroke=0, fill=1)
            c.setLineWidth(7)
            for dx, dy in ((0, 1), (1, 0), (0, -1), (-1, 0), (0.7, 0.7),
                           (-0.7, 0.7), (0.7, -0.7), (-0.7, -0.7)):
                c.line(50 + dx * 32, 50 + dy * 32, 50 + dx * 46, 50 + dy * 46)
        elif shape == 'flame':
            path.moveTo(50, 96)
            path.curveTo(30, 70, 14, 50, 22, 26)
            path.curveTo(30, 4, 70, 4, 78, 26)
            path.curveTo(86, 50, 70, 70, 50, 96)
            c.drawPath(path, stroke=0, fill=1)
            c.setFillColor(white)
            c.circle(50, 30, 12, stroke=0, fill=1)
        elif shape == 'star':
            c.setFont(font('Sans-Bold'), 110)
            c.drawCentredString(50, 12, '★')
        c.restoreState()

    def letterhead(self, name, colour, shape, lines=(), tagline=None,
                   y=36, band=False):
        """Draw a company's name and logo at the top of the page

        Args:
            name (str): Company name
            colour (str): Its colour
            shape (str): Logo shape, see logo()
            lines (list of str): Address and contact lines, shown at the
                right
            tagline (str): Slogan under the name, or None
            y (float): Top edge
            band (bool): True to draw a coloured band under the heading

        Returns:
            float: y position below the letterhead
        """
        self.logo(self.margin, y, 44, shape, colour)
        self.text(self.margin + 54, y + 22, name, 'Sans-Bold', 17, colour)
        if tagline:
            self.text(self.margin + 54, y + 38, tagline, 'Sans-Italic', 9,
                      '#555555')
        self.lines(self.right, y + 10, lines, 'Sans', 8, '#333333',
                   align='right')
        y += 56
        if band:
            self.box(0, y, self.width, 6, fill=colour, stroke=None)
            y += 6
        return y + 14

    def address_block(self, y, lines, x=None):
        """Draw the addressee, where an envelope's window shows it"""
        return self.lines(self.margin + 18 if x is None else x, y, lines,
                          'Sans', 10.5) + 6

    def footer(self, txt, total=None):
        """Draw a footer line with the page number"""
        y = self.height - 30
        self.rule(y - 12, colour='#cccccc')
        self.text(self.margin, y, txt, 'Sans', 7.5, '#555555')
        num = f'Page {self.page}' + (f' of {total}' if total else '')
        self.text(self.right, y, num, 'Sans', 7.5, '#555555', 'right')

    def bar_chart(self, x, y, w, h, labels, values, colour, highlight=None,
                  unit=''):
        """Draw a small bar chart, as bills show for past usage

        Args:
            x (float): Left edge
            y (float): Top edge
            w (float): Width
            h (float): Height of the bars' area
            labels (list of str): Label under each bar
            values (list of float): Height of each bar
            colour (str): Colour of the bars
            highlight (int): Index of a bar to draw darker, or None
            unit (str): Unit for the largest value, shown at the top

        Returns:
            float: y position below the chart
        """
        top = max(values)
        step = w / len(values)
        self.rule(y + h, x, x + w, '#666666', 0.6)
        for num, (label, value) in enumerate(zip(labels, values)):
            bh = h * value / top
            fill = colour if num == highlight else _lighter(colour)
            self.box(x + num * step + step * 0.15, y + h - bh, step * 0.7,
                     bh, fill=fill, stroke=None)
            self.text(x + num * step + step / 2, y + h + 10, label, 'Sans',
                      6.5, '#333333', 'centre')
        self.text(x, y - 4, f'{top:,.0f} {unit}', 'Sans', 6.5, '#555555')
        return y + h + 20


def _polygon(c, points):
    """Fill a closed polygon through the given points"""
    path = c.beginPath()
    path.moveTo(*points[0])
    for point in points[1:]:
        path.lineTo(*point)
    path.close()
    c.drawPath(path, stroke=0, fill=1)


def _colour(colour):
    """Convert a hex string to a reportlab colour, leaving others alone"""
    return HexColor(colour) if isinstance(colour, str) else colour


def _lighter(colour):
    """Get a paler version of a hex colour, for the bars not picked out"""
    col = HexColor(colour)
    mix = [0.55 + 0.45 * part for part in (col.red, col.green, col.blue)]
    return '#' + ''.join(f'{int(part * 255):02x}' for part in mix)


def money(amount):
    """Format an amount of dollars, e.g. 1234.5 -> '$1,234.50'"""
    sign = '-' if amount < 0 else ''
    return f'{sign}${abs(amount):,.2f}'


def us_date(when):
    """Format a date as US forms show it, e.g. 03/14/2026"""
    return when.strftime('%m/%d/%Y')


def long_date(when):
    """Format a date as US letters show it, e.g. March 14, 2026"""
    return f'{when:%B} {when.day}, {when.year}'
