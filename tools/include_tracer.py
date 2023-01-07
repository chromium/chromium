#!/usr/bin/env python
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# based on an almost identical script by: jyrki@google.com (Jyrki Alakuijala)

"""Prints out include dependencies in chrome.

Since it ignores defines, it gives just a rough estimation of file size.

Usage:
  tools/include_tracer.py -Iout/Default/gen chrome/browser/ui/browser.h
"""

from __future__ import print_function

import argparse
import os
import re
import sys

# Created by copying the command line for prerender_browsertest.cc, replacing
# spaces with newlines, and dropping everything except -F and -I switches.
# TODO(port): Add windows, linux directories.
INCLUDE_PATHS = [
  '',
  'gpu',
  'skia/config',
  'skia/ext',
  'testing/gmock/include',
  'testing/gtest/include',
  'third_party/google_toolbox_for_mac/src',
  'third_party/icu/public/common',
  'third_party/icu/public/i18n',
  'third_party/protobuf',
  'third_party/protobuf/src',
  'third_party/skia/gpu/include',
  'third_party/skia/include/config',
  'third_party/skia/include/core',
  'third_party/skia/include/effects',
  'third_party/skia/include/gpu',
  'third_party/skia/include/pdf',
  'third_party/skia/include/ports',
  'v8/include',
]


def Walk(include_dirs, seen, filename, parent, indent):
  """Returns the size of |filename| plus the size of all files included by
  |filename| and prints the include tree of |filename| to stdout. Every file
  is visited at most once.
  """
  total_bytes = 0

  # .proto(devel) filename translation
  if filename.endswith('.pb.h'):
    basename = filename[:-5]
    if os.path.exists(basename + '.proto'):
      filename = basename + '.proto'
    else:
      print('could not find ', filename)

  # Show and count files only once.
  if filename in seen:
    return total_bytes
  seen.add(filename)

  # Display the paths.
  print(' ' * indent + filename)

  # Skip system includes.
  if filename[0] == '<':
    return total_bytes

  # Find file in all include paths.
  resolved_filename = filename
  for root in INCLUDE_PATHS + [os.path.dirname(parent)] + include_dirs:
    if os.path.exists(os.path.join(root, filename)):
      resolved_filename = os.path.join(root, filename)
      break

  # Recurse.
  if os.path.exists(resolved_filename):
    lines = open(resolved_filename).readlines()
  else:
    print(' ' * (indent + 2) + "-- not found")
    lines = []
  for line in lines:
    line = line.strip()
    match = re.match(r'#include\s+(\S+).*', line)
    if match:
      include = match.group(1)
      if include.startswith('"'):
        include = include[1:-1]
      total_bytes += Walk(
        include_dirs, seen, include, resolved_filename, indent + 2)
    elif line.startswith('import '):
      total_bytes += Walk(
        include_dirs, seen, line.split('"')[1], resolved_filename, indent + 2)
  return total_bytes + len("".join(lines))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-I', action='append', dest='include_dirs')
  parser.add_argument('source_file')
  options = parser.parse_args(sys.argv[1:])
  if not options.include_dirs:
    options.include_dirs = []

  bytes = Walk(options.include_dirs, set(), options.source_file, '', 0)
  print()
  print(float(bytes) / (1 << 20), "megabytes of chrome source")


if __name__ == '__main__':
  sys.exit(main())
