# SPDX-License-Identifier: GPL-2.0-or-later

"""Turn laid-out pages into scanned stacks

A page is rendered from its PDF, made to look like paper (a tint, some
grain, a few specks) and can have handwriting added. Pages are then
either fed through paperman's fake scanner, which gives a .max stack
made exactly as a real scan makes one, or gathered into a PDF of
images, as another scanner would have made.
"""

# pylint: disable=too-many-arguments,too-many-locals,too-many-statements

import datetime
import glob
import os
import shutil
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

from corpus.pdftext import find

FONT_DIR = os.path.join(os.path.dirname(__file__), 'fonts')
HAND_FONT = os.path.join(FONT_DIR, 'Caveat.ttf')

# The fake scanner, as paperman names it
FAKE_DEVICE = 'fakefujitsu:fi-8170:00001'

# Ink colours for handwriting
PEN_BLUE = (28, 42, 110)
PEN_BLACK = (25, 25, 30)
PENCIL = (105, 105, 105)

# Paper colours
WHITE = (250, 249, 246)
CREAM = (246, 238, 214)
THERMAL = (244, 244, 240)
CARD = (247, 245, 238)


def render(pdf, dpi, first=None, last=None):
    """Render the pages of a PDF to images

    Args:
        pdf (str): Path to the PDF
        dpi (int): Resolution to render at
        first (int): First page to render, from 1, or None for the first
        last (int): Last page to render, or None for the last

    Returns:
        list of Image: One RGB image per page
    """
    with tempfile.TemporaryDirectory() as tmp:
        cmd = ['pdftoppm', '-r', str(dpi), '-png']
        if first:
            cmd += ['-f', str(first)]
        if last:
            cmd += ['-l', str(last)]
        subprocess.run(cmd + [pdf, os.path.join(tmp, 'p')], check=True)
        pages = []
        for fname in sorted(glob.glob(os.path.join(tmp, 'p-*.png'))):
            with Image.open(fname) as img:
                pages.append(img.convert('RGB'))
    return pages


def paper(img, rng, tint=WHITE, grain=2.5, specks=6, fade=0.0):
    """Make a clean rendering look like a sheet of paper

    The white of the page takes the paper's tint and the ink is darkened
    to match, as a scanner sees it. A little grain and a few specks of
    dust go on top.

    Args:
        img (Image): Clean page, RGB
        rng (random.Random): Source of randomness
        tint (tuple of int): Colour of the paper
        grain (float): Standard deviation of the grain, in levels
        specks (int): Number of specks of dust
        fade (float): How far the ink is faded towards the paper, 0 to 1,
            as on a thermal receipt

    Returns:
        Image: The page as paper
    """
    arr = np.asarray(img, dtype=np.float32) / 255
    if fade:
        arr = 1 - (1 - arr) * (1 - fade)
    arr = arr * (np.array(tint, dtype=np.float32) / 255)
    arr *= 255
    if grain:
        nprng = np.random.default_rng(rng.randrange(1 << 30))
        arr += nprng.normal(0, grain, arr.shape[:2])[..., None]
    out = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    draw = ImageDraw.Draw(out)
    for _ in range(specks):
        x = rng.randrange(out.width)
        y = rng.randrange(out.height)
        r = rng.uniform(0.6, 2.2) * out.width / 2550
        shade = rng.randrange(60, 140)
        draw.ellipse((x - r, y - r, x + r, y + r), fill=(shade,) * 3)
    return out.filter(ImageFilter.GaussianBlur(0.4 * out.width / 2550))


class Hand:
    """Writes on a page in handwriting

    Positions are in points from the top left, as for the layout, and
    are converted to the page's pixels.

    Properties:
        img (Image): Page being written on
        scale (float): Pixels per point
        words (list of Word): Words printed on the page, for find()
    """

    def __init__(self, img, dpi, words=None):
        self.img = img
        self.scale = dpi / 72
        self.draw = ImageDraw.Draw(img)
        self.words = words or []

    def find(self, *phrase, nth=0):
        """Find where some words are printed on the page

        Args:
            phrase (str): The words, in order
            nth (int): Which match to use, from 0

        Returns:
            Word: The last of the words, with its position in points
        """
        return find(self.words, phrase, nth)

    def _font(self, size):
        return ImageFont.truetype(HAND_FONT, int(size * self.scale))

    def write(self, x, y, txt, size=16, ink=PEN_BLUE, angle=0):
        """Write some text with its top left near x, y

        Args:
            x (float): Left edge, in points
            y (float): Top edge, in points
            txt (str): Text, which may have several lines
            size (float): Size in points
            ink (tuple of int): Colour
            angle (float): Degrees anticlockwise to slope the text
        """
        fnt = self._font(size)
        if not angle:
            self.draw.multiline_text((x * self.scale, y * self.scale), txt,
                                     fill=ink, font=fnt,
                                     spacing=int(size * self.scale * 0.15))
            return
        box = self.draw.multiline_textbbox((0, 0), txt, font=fnt)
        layer = Image.new('L', (box[2] + 20, box[3] + 20), 0)
        ImageDraw.Draw(layer).multiline_text((10, 10), txt, fill=255,
                                             font=fnt)
        layer = layer.rotate(angle, expand=True,
                             resample=Image.Resampling.BICUBIC)
        ink_img = Image.new('RGB', layer.size, ink)
        self.img.paste(ink_img, (int(x * self.scale), int(y * self.scale)),
                       layer)

    def tick(self, x, y, size=12, ink=PEN_BLUE):
        """Draw a tick with its top left at x, y"""
        s = self.scale
        pts = [(x * s, (y + size * 0.55) * s),
               ((x + size * 0.35) * s, (y + size) * s),
               ((x + size) * s, y * s)]
        self.draw.line(pts, fill=ink, width=max(2, int(1.6 * s)),
                       joint='curve')

    def cross(self, x, y, size=8, ink=PEN_BLACK):
        """Draw a cross in a box whose top left is at x, y"""
        s = self.scale
        w = max(2, int(1.3 * s))
        self.draw.line([(x * s, y * s), ((x + size) * s, (y + size) * s)],
                       fill=ink, width=w)
        self.draw.line([((x + size) * s, y * s), (x * s, (y + size) * s)],
                       fill=ink, width=w)

    def circle(self, x, y, w, h, ink=PENCIL):
        """Draw a loose ring around the box with top left at x, y"""
        s = self.scale
        self.draw.ellipse((x * s, y * s, (x + w) * s, (y + h) * s),
                          outline=ink, width=max(2, int(1.4 * s)))

    def underline(self, x0, x1, y, ink=PENCIL):
        """Draw a line under some text"""
        s = self.scale
        self.draw.line([(x0 * s, y * s), (x1 * s, (y + 1.5) * s)], fill=ink,
                       width=max(2, int(1.4 * s)))

    def signature(self, x, y, name, size=22, ink=PEN_BLUE):
        """Sign a name, sloped a little, with a flourish under it"""
        self.write(x, y - size * 0.4, name, size, ink, angle=4)
        s = self.scale
        width = len(name) * size * 0.32
        self.draw.arc(((x - 4) * s, (y + size * 0.6) * s,
                       (x + width) * s, (y + size * 1.1) * s),
                      200, 340, fill=ink, width=max(2, int(1.2 * s)))


def scan_stack(paperman, repo, folder, name, sheets, when, work, dpi=300,
               sideways=None, settings=(), verbose=False):
    """Feed sheets through the fake scanner into a new .max stack

    Each sheet is a front with an optional back. The scan is duplex, with
    blank backs left out, so a sheet with nothing on its back gives one
    page, and each side is cut to the sheet, as the scanner does with
    auto-size on. Paperman runs in a home directory of its own, so that the
    user's settings are neither used nor changed, and under faketime when
    that is installed, so that the pages carry the date of the document
    rather than today's.

    Args:
        paperman (str): Path to the paperman program
        repo (str): Repository to scan into
        folder (str): Folder within it
        name (str): Name for the stack, without .max, or None to keep the
            name the scan gives it
        sheets (list of tuple): (front, back) images, back being None for
            a blank back
        when (datetime.datetime): When the scan is to seem to happen
        work (str): Directory for the scanner's hopper and home
        dpi (int): Resolution of the images, and of the scan
        sideways (str): 'left' or 'right' if the sheets go through
            sideways with the top of the page that way, else None
        settings (list of str): Extra scanner options, as name=value
        verbose (bool): True to show paperman's output

    Returns:
        str: Path to the new stack

    Raises:
        RuntimeError: The scan failed
    """
    fake = os.path.join(work, 'fakescan')
    hopper = os.path.join(fake, 'hopper')
    shutil.rmtree(fake, ignore_errors=True)
    os.makedirs(hopper)
    for num, (front, back) in enumerate(sheets):
        _save_sheet(front, os.path.join(hopper, f'{num:04d}.png'), dpi,
                    sideways)
        if back is not None:
            _save_sheet(back, os.path.join(hopper, f'{num:04d}.back.png'),
                        dpi, sideways)

    target = os.path.join(repo, folder)
    os.makedirs(target, exist_ok=True)
    before = set(glob.glob(os.path.join(target, '*.max')))

    home = os.path.join(work, 'home')
    os.makedirs(home, exist_ok=True)
    env = dict(os.environ, HOME=home, QT_QPA_PLATFORM='offscreen')
    env.pop('XDG_CONFIG_HOME', None)
    cmd = [paperman, '--fake-scanner', fake, '--scan', '--repo', repo,
           '--dir', folder, '--device', FAKE_DEVICE, '--auto-colour',
           '--blank', 'backs', '--set', 'source=ADF Duplex',
           '--set', 'mode=Color', '--set', 'hwdeskewcrop=yes',
           '--set', f'resolution={dpi}']
    for setting in settings:
        cmd += ['--set', setting]
    if sideways:
        cmd += ['--sideways', sideways]
    if shutil.which('faketime'):
        cmd = ['faketime', when.strftime('%Y-%m-%d %H:%M:%S')] + cmd
    res = subprocess.run(cmd, env=env, capture_output=not verbose,
                         text=True, check=False)
    if res.returncode:
        raise RuntimeError(f'scanning {folder}/{name} failed:\n'
                           f'{res.stderr or ""}{res.stdout or ""}')

    made = set(glob.glob(os.path.join(target, '*.max'))) - before
    if len(made) != 1:
        raise RuntimeError(f'scanning {folder}/{name} made {len(made)} '
                           'stacks')
    path = made.pop()
    if name:
        named = os.path.join(target, f'{name}.max')
        os.rename(path, named)
        path = named
    stamp(path, when)
    return path


def _save_sheet(img, path, dpi, sideways):
    """Save a sheet for the hopper, turned as it is to be fed"""
    if sideways == 'left':
        img = img.transpose(Image.Transpose.ROTATE_90)
    elif sideways == 'right':
        img = img.transpose(Image.Transpose.ROTATE_270)
    img.save(path, dpi=(dpi, dpi), compress_level=1)


def image_pdf(path, pages, dpi, when, title=None):
    """Gather page images into a PDF, as a scanner which makes PDFs would

    Args:
        path (str): PDF to write
        pages (list of Image): Pages, in order
        dpi (int): Their resolution
        when (datetime.datetime): Date for the file
        title (str): Title for the PDF, or None
    """
    first, rest = pages[0], pages[1:]
    first.save(path, 'PDF', resolution=dpi, save_all=True,
               append_images=rest, title=title or '')
    stamp(path, when)


def stamp(path, when):
    """Give a file the modification time of its document"""
    secs = when.timestamp()
    os.utime(path, (secs, secs))


def at(date, hour=10, minute=0):
    """Get a time on a date, for when something was scanned or made"""
    return datetime.datetime(date.year, date.month, date.day, hour, minute)
