#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Calculate deltas between results of compiler_inputs_size.py.

As input, the script takes the outputs of compiler_inputs_size.py produced from
two different builds. The output is a list of translation units and deltas in
bytes sorted by deltas in decreasing order. TUs that didn't change in size are
omitted.

Example usage:

$ tools/clang/scripts/compiler_inputs_size_diff.py before.txt after.txt
Before: 343.41 GiB (368729376427)
After: 345.43 GiB (370904487700)
Delta: +2.03 GiB (+2175111273)
Delta %: 0.59%
[...]
third_party/blink/renderer/core/dom/node.cc      +601503
third_party/blink/renderer/core/frame/frame.cc   +560405
third_party/blink/renderer/core/fetch/body.cc    -75
third_party/blink/renderer/core/url/dom_url.cc   -75
third_party/blink/renderer/modules/ml/ml.cc      -75
chrome/browser/ui/browser.cc                     -287
content/browser/browser_interface_binders.cc     -287
[...]

Test this code with:
$ python3 -m doctest -v tools/clang/scripts/compiler_inputs_size_diff.py
"""

import argparse
import re
import sys
from typing import Dict, Iterable

LINE_RE = re.compile(r'(.+) (\d+)\n')


def parse_report_into_tu_size_dict(lines: Iterable[str]) -> Dict[str, int]:
  r"""Parse a report from compiler_inputs_size.py into a dictionary.

  The report is expected to have one line per translation unit followed by
  a blank line and then a line with the total size.

  Args:
    lines: An iterable of lines from the report.

  Returns:
    A dictionary mapping translation unit paths (including a "total" key) to
    their sizes in bytes.

  >>> parse_report_into_tu_size_dict(
  ...   'foo.cc 1234\n'
  ...   'bar.cc 5678\n'
  ...   '\n'
  ...   'Total: 6912\n'.splitlines(keepends=True))
  {'foo.cc': 1234, 'bar.cc': 5678, 'total': 6912}
  """
  sizes = {}
  lines_iter = iter(lines)
  for line in lines_iter:
    m = LINE_RE.match(line)
    if not m:
      assert (line == '\n')
      line = next(lines_iter)
      sizes['total'] = int(line.rstrip().split(' ')[1])
      break
    sizes[m.group(1)] = int(m.group(2))
  return sizes


def diff_tu_sizes(d1: Dict[str, int], d2: Dict[str, int]) -> Dict[str, int]:
  r"""Calculate the size diff for each translation unit between two reports.

  Args:
    d1: dict mapping translation unit paths to their sizes from the
        first report.
    d2: dict mapping translation unit paths to their sizes from the
        second report.

  Returns:
    A dictionary mapping translation unit paths to their size differences.
    Includes entries for all TUs present in either report.

  >>> diff_tu_sizes(
  ...   {'foo.cc': 1234, 'bar.cc': 5678},
  ...   {'foo.cc': 1200, 'baz.cc': 9012})
  {'foo.cc': -34, 'bar.cc': -5678, 'baz.cc': 9012}
  """
  size_diffs = {}
  for path, size in d1.items():
    size_diffs[path] = d2.get(path, 0) - size
  remaining_keys = set(d2) - set(d1)
  for path in remaining_keys:
    size_diffs[path] = d2[path]
  return size_diffs


def bytes_to_human(bytes: int, sign: bool = False) -> str:
  """Converts a number of bytes to a human-readable string.

  Args:
    bytes: The number of bytes to convert.
    sign: whether to prefix with the sign if positive.

  Returns:
    A human-readable string representation of the number of bytes.
  """
  units = ['B', 'KiB', 'MiB', 'GiB', 'TiB']
  i = 0
  while bytes >= 1024 and i < len(units) - 1:
    bytes /= 1024
    i += 1
  if sign:
    return f'{bytes:+.2f} {units[i]}'
  return f'{bytes:.2f} {units[i]}'


def print_diff(before: Dict[str, int], after: Dict[str, int]):
  r"""Print the diff between two compiler_inputs_size.py reports.

  >>> print_diff(
  ...   {'foo.cc': 1234, 'bar.cc': 5678, 'total': 6912},
  ...   {'foo.cc': 1200, 'bar.cc': 5678, 'baz.cc': 1012, 'total': 7912})
  Before: 6.75 KiB (6912)
  After: 7.73 KiB (7912)
  Delta: +1000.00 B (+1000)
  Delta %: 14.47%
  baz.cc +1012
  foo.cc -34
  """
  size_diffs = diff_tu_sizes(before, after)
  max_path_length = max(len(k) for k in size_diffs)
  if max_path_length > 100:
    max_path_length = 100
  size_diffs = sorted([(k, v) for (k, v) in size_diffs.items() if v != 0],
                      key=lambda x: -x[1])

  before_total = before['total']
  after_total = after['total']
  delta = after_total - before_total
  print(f'Before: {bytes_to_human(before_total)} ({before_total})')
  print(f'After: {bytes_to_human(after_total)} ({after_total})')
  print(f'Delta: {bytes_to_human(delta, sign=True)} ({(delta):+d})')
  print(f'Delta %: {(delta) / before_total * 100:.2f}%')
  for name, size in size_diffs:
    if name == 'total':
      continue
    print('{} {:+d}'.format(name.ljust(max_path_length), size))


def main():
  parser = argparse.ArgumentParser(
      description='Calculate deltas between results of compiler_inputs_size.py')
  parser.add_argument('before',
                      type=argparse.FileType('r'),
                      help='First report from compiler_inputs_size.py.')
  parser.add_argument('after',
                      type=argparse.FileType('r'),
                      help='Second report from compiler_inputs_size.py.')
  args = parser.parse_args()

  before = parse_report_into_tu_size_dict(args.before)
  after = parse_report_into_tu_size_dict(args.after)
  print_diff(before, after)

  return 0


if __name__ == '__main__':
  sys.exit(main())
