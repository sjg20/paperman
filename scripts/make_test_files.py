#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create test files for exercising paperman functionality"""

import argparse
import os
import random
import subprocess

import numpy as np
from PIL import Image
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.pdfgen import canvas

# All generated files go here
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'test', 'files')
PAPERMAN = os.path.join(os.path.dirname(__file__), '..', 'paperman')


def _plasma_layer(rng, xgrid, ygrid, channels):
    """Add one layer of sine-based plasma noise to the RGB channels

    Args:
        rng (random.Random): Random number generator
        xgrid (np.ndarray): 2D array of x coordinates
        ygrid (np.ndarray): 2D array of y coordinates
        channels (list of np.ndarray): Three arrays [R, G, B] to accumulate into
    """
    xf = rng.uniform(1, 8)
    yf = rng.uniform(1, 8)
    xp = rng.uniform(0, 2 * np.pi)
    yp = rng.uniform(0, 2 * np.pi)

    val = np.sin(xgrid * xf + xp)
    val += np.sin(ygrid * yf + yp)
    val += np.sin((xgrid + ygrid) * (xf + yf) / 2)

    for ch in channels:
        cf = rng.uniform(0.5, 3)
        cp = rng.uniform(0, 2 * np.pi)
        ch += np.sin(val * cf + cp)


def _to_uint8(arr, num_layers):
    """Normalise a summed-sine array to 0-255

    Args:
        arr (np.ndarray): Array with values in [-num_layers, num_layers]
        num_layers (int): Number of layers that were summed

    Returns:
        np.ndarray: Array of uint8 values in [0, 255]
    """
    return np.clip((arr / num_layers + 1) / 2 * 255, 0, 255).astype(np.uint8)


def make_plasma_jpeg(fname, width=2400, height=3300, quality=90):
    """Create a colour JPEG with plasma-style fractal noise

    Generates a deterministic image by summing six layers of sine-based
    noise with random frequencies and phase offsets.

    Args:
        fname (str): Output file path
        width (int): Image width in pixels
        height (int): Image height in pixels
        quality (int): JPEG quality 1-100
    """
    rng = random.Random(42)
    num_layers = 6

    # Coordinate grids scaled to [0, pi]
    xgrid, ygrid = np.meshgrid(
        np.linspace(0, np.pi, width, dtype=np.float32),
        np.linspace(0, np.pi, height, dtype=np.float32))

    channels = [np.zeros((height, width), dtype=np.float32) for _ in range(3)]
    for _ in range(num_layers):
        _plasma_layer(rng, xgrid, ygrid, channels)

    rgb = np.stack([_to_uint8(ch, num_layers) for ch in channels], axis=2)
    img = Image.fromarray(rgb, 'RGB')

    img.save(fname, 'JPEG', quality=quality)
    print(f'Wrote {fname} ({width}x{height}, quality {quality})')


# Words for generating random text (a mix of common English words)
WORDS = (
    'the be to of and a in that have I it for not on with he as you do at '
    'this but his by from they we say her she or an will my one all would '
    'there their what so up out if about who get which go me when make can '
    'like time no just him know take people into year your good some could '
    'them see other than then now look only come its over think also back '
    'after use two how our work first well way even new want because any '
    'these give day most us great small large report system process data '
    'program information method function result value number type set point '
    'table line part group order change place state form each begin between '
    'both under last right move try again still found hand high keep left '
    'change end near old long show here next below few along every further'
).split()


def _random_text(rng, min_words=30, max_words=80):
    """Generate a random paragraph with sentence breaks

    Args:
        rng (random.Random): Random number generator
        min_words (int): Minimum number of words
        max_words (int): Maximum number of words

    Returns:
        str: A paragraph of random text
    """
    n = rng.randint(min_words, max_words)
    words = [rng.choice(WORDS) for _ in range(n)]
    words[0] = words[0].capitalize()

    # Add some sentence breaks
    i = rng.randint(8, 15)
    while i < len(words):
        words[i - 1] += '.'
        words[i] = words[i].capitalize()
        i += rng.randint(8, 15)
    words[-1] += '.'
    return ' '.join(words)


def _random_heading(rng):
    """Generate a random section heading

    Args:
        rng (random.Random): Random number generator

    Returns:
        str: A title-cased heading of 2-5 words
    """
    n = rng.randint(2, 5)
    words = [rng.choice(WORDS).capitalize() for _ in range(n)]
    return ' '.join(words)


_HEADING_FONT = 'Helvetica-Bold'
_BODY_FONT = 'Times-Roman'
_HEADING_SIZE = 16
_BODY_SIZE = 11
_LINE_SPACING = _BODY_SIZE * 1.4
_MARGIN = inch


def _draw_wrapped(cvs, text, y, text_width):
    """Draw word-wrapped body text, returning the updated y position

    Args:
        cvs (canvas.Canvas): Reportlab canvas to draw on
        text (str): Paragraph text to wrap and draw
        y (float): Starting y coordinate
        text_width (float): Available width for text

    Returns:
        float: The y position after drawing
    """
    words = text.split()
    line = ''
    for word in words:
        test = f'{line} {word}'.strip()
        if cvs.stringWidth(test, _BODY_FONT, _BODY_SIZE) > text_width:
            cvs.drawString(_MARGIN, y, line)
            y -= _LINE_SPACING
            line = word
            if y <= _MARGIN + 30:
                break
        else:
            line = test
    if line and y > _MARGIN + 30:
        cvs.drawString(_MARGIN, y, line)
        y -= _LINE_SPACING
    return y


def _draw_colour_bars(cvs, width, height):
    """Draw coloured rectangles across the page so it registers as colour

    Args:
        cvs (canvas.Canvas): Reportlab canvas to draw on
        width (float): Page width in points
        height (float): Page height in points
    """
    from reportlab.lib.colors import HexColor

    colours = ['#e63946', '#457b9d', '#2a9d8f', '#e9c46a', '#f4a261',
               '#264653', '#6a4c93']
    bar_h = (height - 2 * _MARGIN) / len(colours)
    y = _MARGIN
    for hex_col in colours:
        cvs.setFillColor(HexColor(hex_col))
        cvs.rect(_MARGIN, y, width - 2 * _MARGIN, bar_h, fill=1, stroke=0)
        y += bar_h


def _draw_grey_gradient(cvs, width, height):
    """Draw grey rectangles across the page so it registers as greyscale

    Args:
        cvs (canvas.Canvas): Reportlab canvas to draw on
        width (float): Page width in points
        height (float): Page height in points
    """
    steps = 16
    bar_h = (height - 2 * _MARGIN) / steps
    y = _MARGIN
    for i in range(steps):
        grey = i / (steps - 1)
        cvs.setFillGray(grey)
        cvs.rect(_MARGIN, y, width - 2 * _MARGIN, bar_h, fill=1, stroke=0)
        y += bar_h


def _draw_page(cvs, rng, page_num, width, height):
    """Draw a single page with heading, body paragraphs and page number

    Args:
        cvs (canvas.Canvas): Reportlab canvas to draw on
        rng (random.Random): Random number generator
        page_num (int): Page number to display
        width (float): Page width in points
        height (float): Page height in points
    """
    text_width = width - 2 * _MARGIN
    y = height - _MARGIN

    # Page heading
    cvs.setFont(_HEADING_FONT, _HEADING_SIZE)
    cvs.drawString(_MARGIN, y, _random_heading(rng))
    y -= _HEADING_SIZE * 2

    # Body paragraphs
    cvs.setFont(_BODY_FONT, _BODY_SIZE)
    while y > _MARGIN + 30:
        y = _draw_wrapped(cvs, _random_text(rng), y, text_width)
        y -= _LINE_SPACING * 0.5

        # Occasional sub-heading
        if y > _MARGIN + 60 and rng.random() < 0.3:
            y -= _LINE_SPACING * 0.5
            cvs.setFont(_HEADING_FONT, _HEADING_SIZE - 2)
            cvs.drawString(_MARGIN, y, _random_heading(rng))
            y -= _HEADING_SIZE * 1.5
            cvs.setFont(_BODY_FONT, _BODY_SIZE)

    # Page number centred at the bottom
    cvs.setFont(_BODY_FONT, 10)
    cvs.drawCentredString(width / 2, _MARGIN / 2, str(page_num))
    cvs.showPage()


def make_random_pdf(fname, pages=100, plasma_path=None):
    """Create a multi-page PDF with headings, body text and page numbers

    Page 1 has colour bars, page 2 has a greyscale gradient, and page 3
    embeds the plasma JPEG (if provided). The remaining pages are plain
    monochrome text. This gives a mix of depths for testing auto-detection.

    Args:
        fname (str): Output file path
        pages (int): Number of pages to generate
        plasma_path (str): Optional path to a JPEG to embed on page 3
    """
    rng = random.Random(42)
    width, height = letter
    cvs = canvas.Canvas(fname, pagesize=letter)

    for page_num in range(1, pages + 1):
        if page_num == 1:
            _draw_colour_bars(cvs, width, height)
        elif page_num == 2:
            _draw_grey_gradient(cvs, width, height)
        elif page_num == 3 and plasma_path:
            cvs.drawImage(plasma_path, 0, 0, width, height)

        _draw_page(cvs, rng, page_num, width, height)

    cvs.save()
    print(f'Wrote {fname} ({pages} pages)')


def pdf_to_max(pdf_path, max_name=None):
    """Convert a PDF file to .max format using paperman

    Args:
        pdf_path (str): Path to the input PDF file
        max_name (str): Optional output filename (without directory)
    """
    env = os.environ.copy()
    env['QT_QPA_PLATFORM'] = 'offscreen'
    cwd = os.path.dirname(os.path.abspath(pdf_path))
    subprocess.run([PAPERMAN, '-m', os.path.basename(pdf_path)],
                   check=True, env=env, cwd=cwd)
    default_max = os.path.splitext(pdf_path)[0] + '.max'
    if max_name:
        max_path = os.path.join(cwd, max_name)
        os.rename(default_max, max_path)
    else:
        max_path = default_max
    print(f'Wrote {max_path}')


def main():
    """Create all test files"""
    parser = argparse.ArgumentParser(description='Create test files')
    parser.add_argument('-o', '--output-dir', default=OUTPUT_DIR,
                        help='Output directory (default: test/files)')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    plasma_path = os.path.join(args.output_dir, 'colour_plasma.jpg')
    make_plasma_jpeg(plasma_path)

    pdf_path = os.path.join(args.output_dir, '100pp.pdf')
    make_random_pdf(pdf_path, plasma_path=plasma_path)
    pdf_to_max(pdf_path, '100pp_from_pdf.max')


if __name__ == '__main__':
    main()
