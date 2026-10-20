#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""Label the parts of a screenshot, with an arrow to each

The screenshot tests (test/test_screenshots.cpp) save, beside some
pictures, where each part of the window is, as name.json. This puts the
picture on a wider sheet and writes a label for each part in the margin,
with an arrow from the label to the part, for the documentation.

Usage:
    scripts/annotate_screenshot.py doc/images desktop
writes doc/images/desktop-labelled.png from desktop.png and desktop.json
"""

import argparse
import json
import math
import os

from PIL import Image, ImageDraw, ImageFont

FONT = '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf'

ACCENT = (214, 72, 15)
INK = (35, 35, 35)
PAPER = (255, 255, 255)
SIZE = 21                   # text size, in pixels
MARGIN = 330                # width of the margins at the sides
GAP = 14                    # space between labels
PAD = 10                    # space inside a label
EDGE = 4                    # space between an arrow's tip and its part
LINE_HEIGHT = int(SIZE * 1.3)

# What to say about each picture: (part, label, side, where to point
# within the part as fractions of its width and height, and whether to
# outline the part). The side is the margin the label goes in.
LABELS = {
    'desktop': [
        ('menu', 'Menu bar', 'top', (0.1, 0.5), False),
        ('toolbar', 'Main toolbar: change the view, print and scan', 'left',
         (0.5, 0.5), False),
        ('folders', 'Folder pane: each repository and the folders in it, '
         'here a folder for each month', 'left', (0.55, 0.15), True),
        ('status', 'Status bar: about the stack and page selected', 'left',
         (0.15, 0.5), False),
        ('stack-tools', 'Stack toolbar: move between stacks, turn and '
         'flip their pages', 'top', (0.3, 0.5), False),
        ('filter', 'Filter: show only the stacks whose names match', 'top',
         (0.5, 0.5), False),
        ('page-tools', 'Page toolbar: select, reorder, turn and zoom '
         'pages', 'top', (0.5, 0.5), False),
        ('stack', 'A stack, selected: its name, the name of the page '
         'shown and the page, with arrows to turn it', 'bottom',
         (0.5, 0.75), True),
        ('stacks', 'Stack pane: the stacks in the folder', 'bottom',
         (0.75, 0.6), False),
        ('thumbnails', 'Page list: the pages of the stack selected',
         'bottom', (0.5, 0.45), True),
        ('page', 'Page viewer: the page, as it was scanned, with any notes '
         'written on it', 'right', (0.75, 0.3), True),
        ('text', 'Text pane: the text read from the page (OCR), for '
         'finding it by what it says', 'right', (0.8, 0.4), True),
    ],
    'drag-to-folder': [
        ('stack', 'Press on a stack, or on any of the stacks selected, '
         'and drag...', 'bottom', (0.85, 0.75), True),
        ('folder', '...then let go over the folder to move them to', 'left',
         (0.3, 0.5), True),
    ],
    'server': [
        ('repository', 'A repository on a server: its name, then the '
         "server's address", 'left', (0.3, 0.5), True),
        ('connection', 'Server status (see below)', 'bottom', (0.8, 0.5), True),
    ],
    'page-remove': [
        ('kept', 'Kept: the green tick at the foot of the page; click it '
         'to mark the page to be removed', 'left', (0.5, 0.5), True),
        ('removed', 'Marked to be removed: the red mark; click it again to '
         'keep the page after all', 'left', (0.5, 0.5), True),
        ('save', 'Save: take out the pages marked', 'top', (0.5, 0.5),
         True),
        ('revert', 'Revert: forget the marks, and any other changes not '
         'saved', 'top', (0.5, 0.5), True),
    ],
}


# Drags to draw on a picture: (part dragged, part dropped on, where the
# arrow bends towards, as fractions of the picture's width and height),
# shown by a broad arrow curving from one to the other, round anything in
# the way.
DRAGS = {
    'drag-to-folder': [('stack', 'folder', (0.30, 0.64))],
}


def wrap(draw, text, font, width):
    """Split text into lines which fit in a width

    Args:
        draw (ImageDraw.Draw): Something to measure with
        text (str): The text
        font (ImageFont): Its font
        width (int): Width in pixels

    Returns:
        list of str: The lines
    """
    lines, line = [], ''
    for word in text.split():
        trial = f'{line} {word}'.strip()
        if line and draw.textlength(trial, font=font) > width:
            lines.append(line)
            line = word
        else:
            line = trial
    lines.append(line)
    return lines


class Label:  # pylint: disable=R0902,R0903
    """A label, where it goes and what it points at

    Properties:
        lines (list of str): Its text, wrapped
        width (int): Width of its box
        height (int): Height of its box
        target (tuple of int): The point it points at, on the sheet
        side (str): Which margin it is in
        outline (bool): True to outline the part it points at
        part (list of int): The part, as x, y, width and height
        x (int): Left of its box, once placed
        y (int): Top of its box, once placed
    """

    def __init__(self, lines, size, target, side, part, outline):  # pylint: disable=R0913
        self.lines = lines
        self.width, self.height = size
        self.target = target
        self.side = side
        self.part = part
        self.outline = outline
        self.x = self.y = 0

    def attach(self):
        """Get the point on the box's edge the arrow leaves from"""
        if self.side == 'left':
            return (self.x + self.width, self.y + self.height // 2)
        if self.side == 'right':
            return (self.x, self.y + self.height // 2)
        if self.side == 'top':
            return (self.x + self.width // 2, self.y + self.height)
        return (self.x + self.width // 2, self.y)


def spread(labels, key, lo, hi, size):
    """Place labels along a margin near what they point at, not overlapping

    Each goes as near as it can to its target, in order, pushed along to
    leave a gap after the one before, then pulled back if the last runs
    past the end.

    Args:
        labels (list of Label): Labels in one margin
        key (callable): The target's position along the margin
        lo (int): Start of the margin
        hi (int): End of the margin
        size (callable): A label's extent along the margin
    """
    labels.sort(key=key)
    pos = []
    at = lo
    for label in labels:
        want = max(at, key(label) - size(label) // 2)
        pos.append(want)
        at = want + size(label) + GAP
    over = at - GAP - hi
    if over > 0:
        for i in range(len(pos) - 1, -1, -1):
            limit = hi if i == len(pos) - 1 else pos[i + 1] - GAP
            pos[i] = min(pos[i], limit - size(labels[i]))
    return pos


def clip(start, end, rect):  # pylint: disable=R0914
    """Find where a line from start towards end first meets a rectangle

    The arrow stops there, at the edge of the part it points at, so that
    it does not cover the part itself.

    Args:
        start (tuple of int): Where the line starts, outside the rectangle
        end (tuple of int): Where it heads, inside the rectangle
        rect (tuple of int): Left, top, right and bottom of the rectangle

    Returns:
        tuple of float: The point on the rectangle's edge, or end if the
            line does not cross it
    """
    (x0, y0), (x1, y1) = start, end
    dx, dy = x1 - x0, y1 - y0
    best = 1.0
    left, top, right, bottom = rect
    for t_edge, lo, hi, var in ((left, top, bottom, 'x'),
                                (right, top, bottom, 'x'),
                                (top, left, right, 'y'),
                                (bottom, left, right, 'y')):
        delta = dx if var == 'x' else dy
        if not delta:
            continue
        t = (t_edge - (x0 if var == 'x' else y0)) / delta
        other = y0 + t * dy if var == 'x' else x0 + t * dx
        if 0 <= t < best and lo <= other <= hi:
            best = t
    return (x0 + best * dx, y0 + best * dy)


def arrow(draw, start, end, width=3):
    """Draw a line from start to end with an arrowhead at the end"""
    draw.line([start, end], fill=ACCENT, width=width)
    ang = math.atan2(end[1] - start[1], end[0] - start[0])
    head = 16
    left = (end[0] - head * math.cos(ang - 0.4),
            end[1] - head * math.sin(ang - 0.4))
    right = (end[0] - head * math.cos(ang + 0.4),
             end[1] - head * math.sin(ang + 0.4))
    draw.polygon([end, left, right], fill=ACCENT)


def make_labels(specs, parts, font):  # pylint: disable=R0914
    """Work out each label's text and size, and what it points at

    Args:
        specs (list of tuple): Labels, as in LABELS
        parts (dict): Where each part is, as saved by the tests
        font (ImageFont): Font for the labels

    Returns:
        list of Label: The labels, not yet placed
    """
    measure = ImageDraw.Draw(Image.new('RGB', (1, 1)))
    labels = []
    for name, text, side, (fx, fy), outline in specs:
        width = MARGIN - 60 if side in ('left', 'right') else 260
        lines = wrap(measure, text, font, width - 2 * PAD)
        size = (max(int(measure.textlength(line, font=font))
                    for line in lines) + 2 * PAD,
                len(lines) * LINE_HEIGHT + 2 * PAD)
        x, y, w, h = parts[name]
        labels.append(Label(lines, size, (x + int(w * fx), y + int(h * fy)),
                            side, parts[name], outline))
    return labels


def margins(labels):
    """Work out how wide each margin must be for its labels

    Returns:
        dict: side -> width in pixels
    """
    room = {'left': 40, 'right': 40, 'top': 40, 'bottom': 40}
    for label in labels:
        if label.side in ('left', 'right'):
            room[label.side] = MARGIN
        else:
            room[label.side] = max(room[label.side], label.height + 90)
    return room


def place(labels, room, shot_size):
    """Put each label in its margin, near what it points at

    Args:
        labels (list of Label): The labels, with targets on the sheet
        room (dict): Width of each margin
        shot_size (tuple of int): Width and height of the screenshot
    """
    width, height = shot_size
    for side in room:
        group = [label for label in labels if label.side == side]
        if not group:
            continue
        if side in ('left', 'right'):
            ys = spread(group, lambda l: l.target[1], 20,
                        height + room['top'] + room['bottom'] - 20,
                        lambda l: l.height)
            for label, y in zip(group, ys):
                label.y = y
                label.x = (room['left'] - 40 - label.width if side == 'left'
                           else room['left'] + width + 40)
        else:
            xs = spread(group, lambda l: l.target[0], room['left'],
                        room['left'] + width, lambda l: l.width)
            for label, x in zip(group, xs):
                label.x = x
                label.y = (room['top'] - 60 - label.height if side == 'top'
                           else room['top'] + height + 60)


def draw_labels(draw, labels, room, font):
    """Draw the outlines, the arrows and the labels, in that order"""
    for label in labels:
        if label.outline:
            x, y, w, h = label.part
            x += room['left']
            y += room['top']
            draw.rounded_rectangle((x, y, x + w, y + h), radius=6,
                                   outline=ACCENT, width=3)
    for label in labels:
        x, y, w, h = label.part
        x += room['left']
        y += room['top']
        edge = clip(label.attach(), label.target,
                    (x - EDGE, y - EDGE, x + w + EDGE, y + h + EDGE))
        arrow(draw, label.attach(), edge)
    for label in labels:
        draw.rounded_rectangle((label.x, label.y, label.x + label.width,
                                label.y + label.height), radius=8,
                               fill=PAPER, outline=ACCENT, width=2)
        for num, line in enumerate(label.lines):
            draw.text((label.x + PAD, label.y + PAD + num * LINE_HEIGHT),
                      line, fill=INK, font=font)


def draw_drags(draw, drags, parts, room, size):  # pylint: disable=R0914
    """Draw each drag, as a broad arrow curving from the foot of what is
    dragged to the side of where it is dropped

    Args:
        draw (ImageDraw.Draw): Something to draw on the sheet with
        drags (list of tuple): (part dragged, part dropped on, bend)
        parts (dict): Where each part is
        room (dict): Width of each margin
        size (tuple of int): Width and height of the screenshot
    """
    for src, dst, (bx, by) in drags:
        x, y, w, h = parts[src]
        fx, fy, fw, fh = parts[dst]
        start = (room['left'] + x + w / 2, room['top'] + y + h + EDGE)
        end = (room['left'] + fx + fw + EDGE, room['top'] + fy + fh / 2)
        bend = (room['left'] + size[0] * bx, room['top'] + size[1] * by)
        points = []
        for step in range(41):
            t = step / 40
            points.append(tuple((1 - t) ** 2 * s + 2 * (1 - t) * t * b
                                + t ** 2 * e
                                for s, b, e in zip(start, bend, end)))
        draw.line(points[:-1], fill=ACCENT, width=7, joint='curve')
        arrow(draw, points[-3], end, width=7)


def annotate(image_path, parts_path, specs, out_path, drags=()):  # pylint: disable=R0914
    """Write a labelled copy of a screenshot

    Args:
        image_path (str): The screenshot
        parts_path (str): Where its parts are, as saved by the tests
        specs (list of tuple): Labels, as in LABELS
        out_path (str): Where to write the labelled picture
        drags (list of tuple): Drags to draw, as in DRAGS
    """
    shot = Image.open(image_path).convert('RGB')
    with open(parts_path, encoding='utf-8') as inf:
        parts = json.load(inf)
    font = ImageFont.truetype(FONT, SIZE)
    labels = make_labels(specs, parts, font)
    room = margins(labels)

    sheet = Image.new('RGB', (shot.width + room['left'] + room['right'],
                              shot.height + room['top'] + room['bottom']),
                      PAPER)
    left, top = room['left'], room['top']
    sheet.paste(Image.new('RGB', shot.size, (200, 200, 200)),
                (left + 6, top + 6))
    sheet.paste(shot, (left, top))
    draw = ImageDraw.Draw(sheet)
    draw.rectangle((left - 1, top - 1, left + shot.width, top + shot.height),
                   outline=(150, 150, 150))
    for label in labels:
        label.target = (label.target[0] + left, label.target[1] + top)
    place(labels, room, shot.size)
    draw_drags(draw, drags, parts, room, shot.size)
    draw_labels(draw, labels, room, font)
    sheet.save(out_path, optimize=True)


def main():
    """Label the screenshots asked for"""
    parser = argparse.ArgumentParser(
        description=__doc__.split('\n', maxsplit=1)[0])
    parser.add_argument('dir', help='directory holding the screenshots')
    parser.add_argument('names', nargs='*', help='pictures to label '
                        f'(default: {", ".join(LABELS)})')
    args = parser.parse_args()
    for name in args.names or LABELS:
        base = os.path.join(args.dir, name)
        out = f'{base}-labelled.png'
        annotate(f'{base}.png', f'{base}.json', LABELS[name], out,
                 DRAGS.get(name, ()))
        print(f'saved {out}')


if __name__ == '__main__':
    main()
