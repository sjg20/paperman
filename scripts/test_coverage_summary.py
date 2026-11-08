#!/usr/bin/env python3
"""Tests for coverage_summary.py"""

import json
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from io import StringIO

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# pylint: disable=wrong-import-position
import coverage_summary as cs

DIFF = '''diff --git a/one.cpp b/one.cpp
--- a/one.cpp
+++ b/one.cpp
@@ -3,0 +4,2 @@ void func ()
+int a;
+++ b/not_a_file
@@ -10 +12 @@ void other ()
-old
+new
\\ No newline at end of file
diff --git a/gone.cpp b/gone.cpp
--- a/gone.cpp
+++ /dev/null
@@ -1,2 +0,0 @@
-x
-y
diff --git a/two.cpp b/two.cpp
--- a/two.cpp
+++ b/two.cpp
@@ -0,0 +1,3 @@
+a
+b
+c
'''

SUMMARY = {'line_covered': 750, 'line_total': 1000, 'line_percent': 75.0}


def line(num, count, **extra):
    """Make the record of one line in a coverage report"""
    return {'line_number': num, 'count': count, **extra}


class TestChangedLines(unittest.TestCase):
    """Reading a diff"""

    def test_lines(self):
        """Each file's new lines are found, whatever the hunks contain"""
        self.assertEqual({'one.cpp': {4, 5, 12}, 'two.cpp': {1, 2, 3}},
                         cs.changed_lines(DIFF))

    def test_empty(self):
        """No changes means no lines"""
        self.assertEqual({}, cs.changed_lines(''))


class TestChangedCoverage(unittest.TestCase):
    """Matching changed lines with the report"""

    def test_coverage(self):
        """Only changed code counts"""
        report = {'files': [
            {'file': 'one.cpp', 'lines': [line(4, 2), line(5, 0), line(9, 0)]},
            {'file': 'skip.cpp', 'lines': [line(4, 0)]},
            {'file': 'two.cpp', 'lines': [line(99, 0)]}]}
        self.assertEqual({'one.cpp': (1, [5])},
                         cs.changed_coverage(report, cs.changed_lines(DIFF)))

    def test_repeated_line(self):
        """A line run in any of its instances has been run"""
        report = {'files': [{'file': 'a.cpp',
                             'lines': [line(1, 0), line(1, 3)]}]}
        self.assertEqual({'a.cpp': (1, [])},
                         cs.changed_coverage(report, {'a.cpp': {1}}))

    def test_excluded(self):
        """A line excluded from the report does not count"""
        report = {'files': [{'file': 'a.cpp', 'lines': [
            line(1, 0, **{'gcovr/excluded': True}), line(2, 1)]}]}
        self.assertEqual({'a.cpp': (1, [])},
                         cs.changed_coverage(report, {'a.cpp': {1, 2}}))


class TestFormat(unittest.TestCase):
    """Producing the text"""

    def test_ranges(self):
        """Runs of numbers are shortened"""
        self.assertEqual('1-3, 7, 9-10', cs.ranges([1, 2, 3, 7, 9, 10]))
        self.assertEqual('', cs.ranges([]))

    def test_master_push(self):
        """A run with nothing to compare just gives the total"""
        text = cs.format_summary(SUMMARY)
        self.assertIn('| All code | 75.0% (750 of 1,000) | '
                      'no figures for master yet |', text)
        self.assertNotIn('changed here', text)

    def test_moves(self):
        """The move from master is signed and small moves are no change"""
        self.assertIn('+1.5 points',
                      cs.format_summary(SUMMARY, {'line_percent': 73.5}))
        self.assertIn('-0.5 points',
                      cs.format_summary(SUMMARY, {'line_percent': 75.5}))
        self.assertIn('no change',
                      cs.format_summary(SUMMARY, {'line_percent': 75.04}))

    def test_changed(self):
        """Lines which are not run are listed by file"""
        text = cs.format_summary(SUMMARY, None, {'b.cpp': (1, [5, 6]),
                                                 'a.cpp': (2, [])})
        self.assertIn('| Lines changed here | 60.0% (3 of 5) | |', text)
        self.assertIn('| `b.cpp` | 5-6 |', text)
        self.assertNotIn('`a.cpp`', text)

    def test_no_code_changed(self):
        """A change to something else is not 0% or an error"""
        text = cs.format_summary(SUMMARY, None, {})
        self.assertIn('| Lines changed here | no code changed | |', text)
        self.assertNotIn('do not run', text)


class TestMain(unittest.TestCase):
    """Running the script"""

    def test_missing_base_and_diff(self):
        """Files not available, as on master's own run, are not an error"""
        with tempfile.TemporaryDirectory() as tmp:
            summary = os.path.join(tmp, 'summary.json')
            with open(summary, 'w', encoding='utf-8') as outf:
                json.dump(SUMMARY, outf)
            out = StringIO()
            with redirect_stdout(out):
                result = cs.main([summary,
                                  '--base', os.path.join(tmp, 'none.json'),
                                  '--diff', os.path.join(tmp, 'none.diff'),
                                  '--full', os.path.join(tmp, 'none.json')])
        self.assertEqual(0, result)
        self.assertIn('no figures for master yet', out.getvalue())


if __name__ == '__main__':
    unittest.main()
