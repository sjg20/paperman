#!/usr/bin/env python3
"""Say in Markdown what the C++ tests cover, for the summary of a CI run

Reads what scripts/coverage.sh writes, and optionally the figures for
master and the changes being tested, to show how the coverage moves and
which of the changed lines the tests do not run. Only the standard
library is needed.
"""

import argparse
import json
import os
import re
import sys

HUNK = re.compile(r'@@ -\d+(?:,(\d+))? \+(\d+)(?:,(\d+))? @@')


def changed_lines(diff):
    """Find the lines added or changed by a diff made with 'git diff -U0'

    Returns:
        dict: path of each file -> set of line numbers in its new version
    """
    changed = {}
    path = None
    body = 0
    for line in diff.splitlines():
        if line.startswith('\\'):
            continue

        # The lines of a hunk can look like anything, even '+++ b/file'
        if body:
            body -= 1
        elif line.startswith('+++ '):
            name = line[4:]
            path = name[2:] if name.startswith('b/') else None
        else:
            match = HUNK.match(line)
            if match:
                removed, start, added = match.groups()
                removed = 1 if removed is None else int(removed)
                added = 1 if added is None else int(added)
                body = removed + added
                if path:
                    changed.setdefault(path, set()).update(
                        range(int(start), int(start) + added))
    return changed


def changed_coverage(report, changed):
    """Work out which of the changed lines the tests run

    Lines which are not code, such as comments, are not in the report and
    so do not count.

    Args:
        report (dict): the count for each line, from coverage.json
        changed (dict): the result of changed_lines()

    Returns:
        dict: path of each file with changed code -> (lines run, list of
            the line numbers not run)
    """
    result = {}
    for entry in report['files']:
        wanted = changed.get(entry['file'])
        if not wanted:
            continue

        # A line can appear more than once, e.g. for each template instance
        counts = {}
        for line in entry['lines']:
            num = line['line_number']
            if num in wanted and not line.get('gcovr/excluded'):
                counts[num] = max(counts.get(num, 0), line['count'])
        if counts:
            result[entry['file']] = (
                sum(1 for count in counts.values() if count),
                sorted(num for num, count in counts.items() if not count))
    return result


def ranges(numbers):
    """Write some sorted line numbers briefly, e.g. '1-3, 7'"""
    spans = []
    for num in numbers:
        if spans and spans[-1][1] == num - 1:
            spans[-1][1] = num
        else:
            spans.append([num, num])
    return ', '.join(str(a) if a == b else f'{a}-{b}' for a, b in spans)


def lines_text(run, total):
    """Give a coverage figure, e.g. '73.2% (24,123 of 32,967)'"""
    percent = 100 * run / total if total else 100
    return f'{percent:.1f}% ({run:,} of {total:,})'


def change_text(summary, base):
    """Say how far the overall coverage has moved from master's"""
    if not base:
        return 'no figures for master yet'
    move = round(summary['line_percent'] - base['line_percent'], 1)
    return f'{move:+.1f} points' if move else 'no change'


def format_summary(summary, base=None, changed=None):
    """Produce the Markdown for a run

    Args:
        summary (dict): the figures for this run, from summary.json
        base (dict): the figures for master, or None if not known
        changed (dict): the result of changed_coverage(), or None if this
            run is not testing a set of changes

    Returns:
        str: the text to show
    """
    out = ['## C++ test coverage', '', '| | Lines | Against master |',
           '|---|---|---|',
           f"| All code | {lines_text(summary['line_covered'], summary['line_total'])}"
           f' | {change_text(summary, base)} |']

    if changed is not None:
        run = sum(item[0] for item in changed.values())
        total = run + sum(len(item[1]) for item in changed.values())
        text = lines_text(run, total) if total else 'no code changed'
        out.append(f'| Lines changed here | {text} | |')

    missing = sorted((path, item[1]) for path, item in (changed or {}).items()
                     if item[1])
    if missing:
        out += ['', 'Changed lines which the tests do not run:', '',
                '| File | Lines |', '|---|---|']
        out += [f'| `{path}` | {ranges(nums)} |' for path, nums in missing]

    out += ['', 'The annotated report is in the coverage-report artifact.', '']
    return '\n'.join(out)


def read_json(path):
    """Read a JSON file, returning None if there is none"""
    if not path or not os.path.exists(path):
        return None
    with open(path, encoding='utf-8') as inf:
        return json.load(inf)


def main(argv=None):
    """Write the summary to stdout"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('summary', help="this run's summary.json")
    parser.add_argument('--base', help="master's summary.json, if known")
    parser.add_argument('--full', help="this run's coverage.json")
    parser.add_argument('--diff', help="the changes, from 'git diff -U0'")
    args = parser.parse_args(argv)

    changed = None
    if args.diff and args.full and os.path.exists(args.diff):
        with open(args.diff, encoding='utf-8', errors='replace') as inf:
            changed = changed_coverage(read_json(args.full),
                                       changed_lines(inf.read()))
    print(format_summary(read_json(args.summary), read_json(args.base),
                         changed))
    return 0


if __name__ == '__main__':
    sys.exit(main())
