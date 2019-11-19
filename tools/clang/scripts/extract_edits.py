#!/usr/bin/env python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to extract edits from clang tool output.

If a clang tool emits edits, then the edits should look like this:
    ...
    ==== BEGIN EDITS ====
    <edit1>
    <edit2>
    ...
    ==== END EDITS ====
    ...

extract_edits.py takes input that is concatenated from multiple tool invocations
and extract just the edits.  In other words, given the following input:
    ...
    ==== BEGIN EDITS ====
    <edit1>
    <edit2>
    ==== END EDITS ====
    ...
    ==== BEGIN EDITS ====
    <yet another edit1>
    <yet another edit2>
    ==== END EDITS ====
    ...
extract_edits.py would emit the following output:
    <edit1>
    <edit2>
    <yet another edit1>
    <yet another edit2>

This python script is mainly needed on Windows.
On unix this script can be replaced with running sed as follows:

    $ cat run_tool.debug.out \
        | sed '/^==== BEGIN EDITS ====$/,/^==== END EDITS ====$/{//!b};d'
        | sort | uniq
"""

from __future__ import print_function

import sys


def main():
  # TODO(dcheng): extract_edits.py should normalize paths. Doing this in
  # apply_edits.py is too late, as a common use case is to apply edits from many
  # different platforms.
  unique_lines = set()
  inside_marker_lines = False
  for line in sys.stdin:
    line = line.rstrip("\n\r")
    if line == '==== BEGIN EDITS ====':
      inside_marker_lines = True
      continue
    if line == '==== END EDITS ====':
      inside_marker_lines = False
      continue
    if inside_marker_lines and line not in unique_lines:
      unique_lines.add(line)
      print(line)
  return 0


if __name__ == '__main__':
  sys.exit(main())
