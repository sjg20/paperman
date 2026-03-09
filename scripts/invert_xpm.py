#!/usr/bin/env python3
"""Invert XPM icon colours for dark mode.

Reads each XPM file, inverts all colour values (RGB), and writes to
images/dark/. Transparent ("None") colours are preserved.
"""

import re
import os

# X11 named colours used in XPM files (lowercase -> (R, G, B))
NAMED_COLOURS = {
    'black': (0, 0, 0),
    'white': (255, 255, 255),
    'dimgray': (105, 105, 105),
    'gray': (190, 190, 190),  # X11 "gray" is #BEBEBE
    'lightgray': (211, 211, 211),
    'gainsboro': (220, 220, 220),
}
# Add gray0..gray100 (X11 grayN = round(N * 255 / 100))
for i in range(101):
    v = round(i * 255 / 100)
    NAMED_COLOURS[f'gray{i}'] = (v, v, v)


def invert_rgb(r, g, b):
    return (255 - r, 255 - g, 255 - b)


def invert_hex_colour(match):
    """Invert a hex colour like #RRRRGGGGBBBB or #RRGGBB."""
    hexstr = match.group(0)
    digits = hexstr[1:]

    if len(digits) == 12:
        r = 0xFFFF - int(digits[0:4], 16)
        g = 0xFFFF - int(digits[4:8], 16)
        b = 0xFFFF - int(digits[8:12], 16)
        return f'#{r:04X}{g:04X}{b:04X}'
    elif len(digits) == 6:
        r = 0xFF - int(digits[0:2], 16)
        g = 0xFF - int(digits[2:4], 16)
        b = 0xFF - int(digits[4:6], 16)
        return f'#{r:02X}{g:02X}{b:02X}'
    return hexstr


def invert_colour_line(line):
    """Invert the colour in an XPM colour definition line.

    Handles both hex (#RRGGBB) and X11 named colours (gray54, LightGray,
    etc.).  Lines containing "None"/"none" (transparent) are left unchanged.
    """
    if 'None' in line or 'none' in line:
        return line

    # Try hex colours first
    result = re.sub(r'#[0-9A-Fa-f]{6,12}', invert_hex_colour, line)
    if result != line:
        return result

    # Try named colours: match "c <name>" in colour definition lines
    m = re.match(r'^(.*\bc\s+)([a-zA-Z]\w*)(",.*)$', line)
    if m:
        prefix, name, suffix = m.groups()
        key = name.lower()
        if key in NAMED_COLOURS:
            r, g, b = invert_rgb(*NAMED_COLOURS[key])
            return f'{prefix}#{r:02X}{g:02X}{b:02X}{suffix}'

    return line


def invert_xpm(src, dst):
    with open(src) as f:
        content = f.read()

    lines = content.split('\n')
    out = [invert_colour_line(line) for line in lines]

    with open(dst, 'w') as f:
        f.write('\n'.join(out))


if __name__ == '__main__':
    srcdir = 'images'
    dstdir = 'images/dark'

    os.makedirs(dstdir, exist_ok=True)

    # Only invert the icons used in toolbars
    icons = [
        'print.xpm', 'swap.xpm', 'prev.xpm', 'next.xpm',
        'pprev.xpm', 'pnext.xpm', 'options.xpm', 'scan-go.xpm',
        'scan.xpm', 'rleft.xpm', 'rright.xpm', 'hflip.xpm', 'vflip.xpm',
        'pointer.xpm', 'hand.xpm', 'scanmode.xpm', 'info.xpm',
        'document-save.xpm', 'document-revert.xpm',
        'zoom-best-fit.xpm', 'zoom-original.xpm',
        'zoom-out.xpm', 'zoom-in.xpm',
        'locate.xpm', 'unknown.xpm', 'no_access.xpm',
        'left.xpm', 'right.xpm', 'pages.xpm',
        'pageblank.xpm', 'pagekeep.xpm', 'pageremove.xpm',
    ]

    for icon in icons:
        src = os.path.join(srcdir, icon)
        dst = os.path.join(dstdir, icon)
        if os.path.exists(src):
            invert_xpm(src, dst)
            print(f'  {icon}')
        else:
            print(f'  SKIP {icon} (not found)')
