#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""Draw the diagram of how Paperman's parts work together

The desktop program at the scanner puts the papers on a server, which
the desktop program on other computers and the phone app reach. Each
part is shown by a small picture of it, taken from the screenshots, with
a few words, and arrows to the server.

Usage:
    scripts/draw_diagram.py doc/images
writes doc/images/parts.png, using desktop.png from the same directory
"""

import argparse
import os

from PIL import Image, ImageDraw, ImageFont

from annotate_screenshot import FONT, INK, PAPER, arrow, wrap

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
ICON = os.path.join(ROOT, 'app', 'assets', 'icon.png')

WIDTH, HEIGHT = 1500, 1030
TOP = 200                   # where the row of desktops and the server is
SIZE = 26
TITLE = 30
FRAME = (120, 120, 120)


def screen(desktop, width):
    """Make a small monitor showing the desktop's window

    Args:
        desktop (Image): Screenshot of the window
        width (int): Width of the monitor

    Returns:
        Image: The monitor, on a stand
    """
    inner = desktop.resize((width - 24, (width - 24) * desktop.height //
                            desktop.width))
    height = inner.height + 24
    out = Image.new('RGBA', (width, height + 40), (0, 0, 0, 0))
    draw = ImageDraw.Draw(out)
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=10,
                           fill=(50, 50, 55))
    out.paste(inner, (12, 12))
    draw.rectangle((width // 2 - 15, height, width // 2 + 15, height + 25),
                   fill=(80, 80, 85))
    draw.rounded_rectangle((width // 2 - 60, height + 25, width // 2 + 60,
                            height + 38), radius=5, fill=(80, 80, 85))
    return out


def server(width, height):
    """Draw a server: a tower with drive bays and a light"""
    out = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(out)
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=12,
                           fill=(60, 64, 72))
    for num in range(4):
        top = 25 + num * 38
        draw.rounded_rectangle((18, top, width - 18, top + 26), radius=5,
                               fill=(90, 96, 106))
        draw.ellipse((width - 40, top + 9, width - 30, top + 19),
                     fill=(10, 170, 58))
    return out


def phone(icon, width):
    """Draw a phone with the app's icon on its screen"""
    height = width * 2
    out = Image.new('RGBA', (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(out)
    draw.rounded_rectangle((0, 0, width - 1, height - 1), radius=22,
                           fill=(40, 40, 45))
    draw.rounded_rectangle((8, 30, width - 9, height - 30), radius=6,
                           fill=(245, 245, 245))
    size = width // 2
    small = icon.resize((size, size))
    out.paste(small, ((width - size) // 2, (height - size) // 2), small)
    return out


def caption(draw, fonts, pos, words, width=320):
    """Write a part's name and a few words under it, centred

    Args:
        draw (ImageDraw): Where to write
        fonts (tuple): Font for the words, then the bold one for the name
        pos (tuple): Centre and top of the caption
        words (tuple): The part's name, then what it does
        width (int): Width to wrap the words to
    """
    font, bold = fonts
    centre, top = pos
    title, text = words
    draw.text((centre, top), title, fill=INK, font=bold, anchor='ma')
    y = top + TITLE + 10
    for line in wrap(draw, text, font, width):
        draw.text((centre, y), line, fill=INK, font=font, anchor='ma')
        y += int(SIZE * 1.3)


def edge(box, towards):
    """Find the point on a box's edge which faces a point"""
    left, top, right, bottom = box
    cx, cy = (left + right) / 2, (top + bottom) / 2
    dx, dy = towards[0] - cx, towards[1] - cy
    scale = min((right - left) / 2 / abs(dx) if dx else 1e9,
                (bottom - top) / 2 / abs(dy) if dy else 1e9)
    return (cx + dx * scale, cy + dy * scale)


def both_ways(draw, box_a, box_b, gap=14):
    """Draw arrows both ways between two boxes, side by side"""
    a = edge(box_a, ((box_b[0] + box_b[2]) / 2, (box_b[1] + box_b[3]) / 2))
    b = edge(box_b, ((box_a[0] + box_a[2]) / 2, (box_a[1] + box_a[3]) / 2))
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = (dx * dx + dy * dy) ** 0.5
    ox, oy = -dy / length * gap / 2, dx / length * gap / 2
    shrink = 18 / length
    a2 = (a[0] + dx * shrink, a[1] + dy * shrink)
    b2 = (b[0] - dx * shrink, b[1] - dy * shrink)
    arrow(draw, (a2[0] + ox, a2[1] + oy), (b2[0] + ox, b2[1] + oy), width=5)
    arrow(draw, (b2[0] - ox, b2[1] - oy), (a2[0] - ox, a2[1] - oy), width=5)


def draw_parts(images, out_path):
    """Draw the diagram

    Args:
        images (str): Directory holding desktop.png
        out_path (str): Where to write the diagram
    """
    fonts = (ImageFont.truetype(FONT, SIZE),
             ImageFont.truetype(FONT.replace('Regular', 'Bold'), TITLE))
    sheet = Image.new('RGB', (WIDTH, HEIGHT), PAPER)
    draw = ImageDraw.Draw(sheet)
    desktop = Image.open(os.path.join(images, 'desktop.png')).convert('RGB')

    parts = {
        'scan': screen(desktop, 330),
        'server': server(170, 200),
        'other': screen(desktop, 330),
        'phone': phone(Image.open(ICON).convert('RGBA'), 120),
    }
    tops = {'scan': TOP, 'server': TOP + 20, 'other': TOP, 'phone': TOP + 440}
    lefts = {'scan': 60, 'other': WIDTH - 60 - parts['other'].width}
    boxes = {}
    for name, image in parts.items():
        left = lefts.get(name, (WIDTH - image.width) // 2)
        boxes[name] = (left, tops[name], left + image.width,
                       tops[name] + image.height)
        sheet.paste(image, boxes[name][:2], image)

    for name in ('scan', 'other', 'phone'):
        both_ways(draw, boxes[name], boxes['server'])

    caption(draw, fonts, ((boxes['scan'][0] + boxes['scan'][2]) // 2,
                          boxes['scan'][3] + 20),
            ('At the scanner',
             'The desktop program scans papers, files them and finds them'))
    # above the server, as the arrows to the phone run below it
    caption(draw, fonts, (WIDTH // 2, 30),
            ('Server', 'Keeps the papers in its repositories, reads their '
             'text and searches it'), width=300)
    caption(draw, fonts, ((boxes['other'][0] + boxes['other'][2]) // 2,
                          boxes['other'][3] + 20),
            ('On other computers',
             "The desktop program shows the server's repositories beside "
             'its own, to file and find papers there too'), width=340)
    caption(draw, fonts, (WIDTH // 2 + 250, boxes['phone'][1] + 60),
            ('On a phone or tablet',
             'The app finds and reads papers, and scans new ones with the '
             'camera, wherever you are'))
    draw.rectangle((0, 0, WIDTH - 1, HEIGHT - 1), outline=FRAME)
    sheet.save(out_path, optimize=True)


def main():
    """Draw the diagrams asked for"""
    parser = argparse.ArgumentParser(
        description=__doc__.split('\n', maxsplit=1)[0])
    parser.add_argument('dir', help='directory holding the screenshots')
    args = parser.parse_args()
    out = os.path.join(args.dir, 'parts.png')
    draw_parts(args.dir, out)
    print(f'saved {out}')


if __name__ == '__main__':
    main()
