#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pathlib
import unittest

import setup_modules  # pylint: disable=unused-import

import chromium_src.tools.metrics.python_support.quote_checker as quote_checker


class QuoteCheckerTest(unittest.TestCase):
  def _RunQuoteCheck(self, code_text, changed_lines=None):
    filepath = pathlib.Path('test_file.py')
    modified_strings = quote_checker.GetModifiedStrings(filepath, code_text)
    if changed_lines is None:
      changed_lines = set(range(1, len(code_text.splitlines()) + 1))

    violations = []
    for modified_string in modified_strings:
      if not quote_checker.CheckQuoteConsistency(
        modified_string, changed_lines
      ):
        report_line = sorted(
          list(changed_lines.intersection(modified_string.lines))
        )[0]
        violations.append(report_line)
    return sorted(violations)

  def testQuoteConsistency(self):
    code = '''\
a = 'hello'
b = "world"
c = f"hello {name}"
d = "don't"
e = """docstring"""
f = u'unicode'
g = b'bytes'
h = r'raw'
i = rf'raw_format'
j = fr'format_raw'
k = u"unicode_double"
l = b"bytes_double"
m = r"raw_double"
n = rf"raw_format_double"
o = fr"format_raw_double"
p = u"don't"
q = b"don't"
r = r"don't"
s = rf"don't {name}"
t = fr"don't {name}"
'''
    # We expect double-quoted strings without single quotes inside to be
    # flagged:
    # b ("world") -> line 2
    # c (f"hello {name}") -> line 3
    # k (u"unicode_double") -> line 11
    # l (b"bytes_double") -> line 12
    # m (r"raw_double") -> line 13
    # n (rf"raw_format_double") -> line 14
    # o (fr"format_raw_double") -> line 15
    # (p, q, r, s, t are allowed because they contain the single quote).
    self.assertEqual(self._RunQuoteCheck(code), [2, 3, 11, 12, 13, 14, 15])

  def testChangedLinesFiltering(self):
    code = """\
x = "hello"
y = "world"
"""
    # If only line 2 is changed, only line 2 is flagged
    self.assertEqual(self._RunQuoteCheck(code, changed_lines={2}), [2])
    # If no lines are changed, nothing is flagged
    self.assertEqual(self._RunQuoteCheck(code, changed_lines=set()), [])

  def testMultilineStrings(self):
    code = '''\
x = f"""line1
line2 {name}"""
y = (
    f"hello {name} "
    f"world {value}"
)
'''
    # Line 1-2 is allowed (triple double quotes).
    # Line 3-6 is implicitly concatenated double-quoted f-strings without
    # single quotes, flagged.
    self.assertEqual(self._RunQuoteCheck(code), [4])

  def testMultilineConcatenatedWithSingleQuoteAllowed(self):
    code = """\
y = (
    f"hello {name} "
    f"world's {value}"
)
"""
    # Entire concatenated string contains single quote, allowed.
    self.assertEqual(self._RunQuoteCheck(code), [])

  def testNestedFStrings(self):
    code = """\
x = f"outer {f'inner {name}'}"
y = f'outer {f"inner {name}"}'
"""
    # Line 1: outer is double-quoted f-string containing single quotes,
    # inner is single-quoted. Allowed.
    # Line 2: outer is single-quoted f-string containing double quotes. Allowed.
    # Line 2: inner f-string (f"inner {name}") is double-quoted without
    # single quotes. Flagged.
    self.assertEqual(self._RunQuoteCheck(code), [2])


if __name__ == '__main__':
  unittest.main()
