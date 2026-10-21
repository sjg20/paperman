#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""Say which pictures in the user guide may need taking again

Each picture is listed in doc/images/shots.yaml with the files whose look
it shows. One which was last changed before one of those files may be out
of date: this lists them, with the commits since, and how to take each
again. It also checks that every picture kept is listed.

A picture taken again which comes out the same is not committed, so it
would still be listed: --checked notes that the pictures named, or all
those made a given way, are up to date as of HEAD. make_screenshots.sh
does this for those it takes.

Usage:
    scripts/stale_shots.py                   # list what may be stale
    scripts/stale_shots.py --strict          # and fail if anything is
    scripts/stale_shots.py --checked --made test script
"""

import argparse
import json
import os
import subprocess
import sys

import yaml

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
IMAGES = 'doc/images'
MANIFEST = os.path.join(IMAGES, 'shots.yaml')
CHECKED = os.path.join(IMAGES, 'shots-checked.json')
HOW = {'test': 'make_screenshots.sh takes it',
       'script': 'make_screenshots.sh draws it',
       'hand': 'take it by hand'}


def git(*args):
    """Run git in the source tree and return what it printed"""
    return subprocess.run(['git', '-C', ROOT, *args], check=True,
                          capture_output=True, text=True).stdout.strip()


def is_ancestor(older, newer):
    """True if commit @older is @newer or comes before it"""
    return subprocess.run(['git', '-C', ROOT, 'merge-base', '--is-ancestor',
                           older, newer], check=False).returncode == 0


def load():
    """Read the manifest and the commits each picture was checked at

    Returns:
        tuple: (dict of shots, by name, each with its 'shows' expanded
            from the groups; dict of commits, by name)
    """
    with open(os.path.join(ROOT, MANIFEST), encoding='utf-8') as inf:
        manifest = yaml.safe_load(inf)
    groups = manifest.get('groups', {})
    shots = manifest['shots']
    for name, shot in shots.items():
        shows = []
        for item in shot.get('shows', []):
            if item.startswith('@'):
                shows += groups[item[1:]]
            else:
                shows.append(item)
        shot['shows'] = shows
        if shot.get('made') not in HOW:
            raise ValueError(f"{name}: 'made' must be one of "
                             f"{', '.join(HOW)}")
    checked = {}
    path = os.path.join(ROOT, CHECKED)
    if os.path.exists(path):
        with open(path, encoding='utf-8') as inf:
            checked = json.load(inf)
    return shots, checked


def problems(shots):
    """Find what is wrong with the manifest

    Returns:
        list of str: a line for each problem
    """
    out = []
    kept = git('ls-files', IMAGES).splitlines()
    pictures = [path[len(IMAGES) + 1:] for path in kept
                if path.endswith('.png')]
    for pic in pictures:
        if not any(pic == name or (name.endswith('/') and
                                   pic.startswith(name))
                   for name in shots):
            out.append(f'{pic}: not listed in {MANIFEST}')
    for name, shot in shots.items():
        if not os.path.exists(os.path.join(ROOT, IMAGES, name)):
            out.append(f'{name}: listed but not in {IMAGES}')
        for path in shot['shows']:
            if not os.path.exists(os.path.join(ROOT, path)):
                out.append(f'{name}: shows {path}, which does not exist')
    return out


def since(name, checked):
    """Find the commit a picture is up to date as of

    This is the later of the last commit to change it and the commit it
    was last checked at, if that is still in the history.

    Returns:
        str: the commit, or None if the picture has never been committed
    """
    last = git('log', '-1', '--format=%H', '--',
               os.path.join(IMAGES, name)) or None
    seen = checked.get(name)
    if seen and is_ancestor(seen, 'HEAD') and (not last or
                                                is_ancestor(last, seen)):
        return seen
    return last


def stale(shots, checked):
    """Find the pictures which may be out of date

    Returns:
        list of tuple: (name, shot, list of str: commits since, as
            'hash subject')
    """
    out = []
    for name, shot in shots.items():
        base = since(name, checked)
        if not base:
            continue
        log = git('log', '--format=%h %s', f'{base}..HEAD', '--',
                  *shot['shows'])
        if log:
            out.append((name, shot, log.splitlines()))
    return out


def show(found):
    """Print the pictures which may be out of date, and how to take each"""
    for made, how in HOW.items():
        some = [item for item in found if item[1]['made'] == made]
        if not some:
            continue
        print(f'May be out of date ({how}):')
        for name, shot, commits in some:
            where = shot.get('test') or shot.get('script') or shot.get(
                'platform', '')
            print(f'  {name} ({where}), since:')
            for line in commits[:5]:
                print(f'      {line}')
            if len(commits) > 5:
                print(f'      ...and {len(commits) - 5} more')
            if made == 'hand':
                print('    steps:')
                for step in shot.get('steps', []):
                    print(f'      - {step}')
        print()
    if any(item[1]['made'] != 'hand' for item in found):
        print('Run scripts/make_screenshots.sh to take those it takes.')


def mark(shots, checked, names, made):
    """Note that pictures are up to date as of HEAD"""
    head = git('rev-parse', 'HEAD')
    for name, shot in shots.items():
        if name in names or shot['made'] in made:
            checked[name] = head
    with open(os.path.join(ROOT, CHECKED), 'w', encoding='utf-8') as outf:
        json.dump(dict(sorted(checked.items())), outf, indent=2)
        outf.write('\n')


def main():
    """Check the pictures, or note them as checked"""
    parser = argparse.ArgumentParser(
        description=__doc__.split('\n', maxsplit=1)[0])
    parser.add_argument('--strict', action='store_true',
                        help='fail if any picture may be out of date')
    parser.add_argument('--checked', action='store_true',
                        help='note the pictures named, or those made as '
                        '--made says, as up to date')
    parser.add_argument('--made', nargs='+', default=[], choices=list(HOW),
                        help='with --checked, all the pictures made so')
    parser.add_argument('names', nargs='*', help='pictures, for --checked')
    args = parser.parse_args()

    shots, checked = load()
    if args.checked:
        mark(shots, checked, args.names, args.made)
        return 0
    bad = problems(shots)
    for line in bad:
        print(f'error: {line}', file=sys.stderr)
    found = stale(shots, checked)
    if found:
        show(found)
    else:
        print('All the pictures are up to date')
    if bad:
        return 2
    return 1 if args.strict and found else 0


if __name__ == '__main__':
    sys.exit(main())
