# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Lint as: python3

"""Generate Files app main_modules.html based on the main.html"""

from __future__ import print_function

import fileinput
import optparse
import os
import shutil
import sys

_MAIN = '  <script type="module" src="foreground/js/main.m.js"></script>\n'

def GenerateHtml(source, target):
  """Copy source file to target with edits, then add BUILD time stamp."""

  # Copy source (main.html) file to the target (main.html) file.
  shutil.copyfile(source, target)

  # Edit the target file.
  main_included = False
  for line in fileinput.input(target, inplace=True):
    # Ignore all <script> and <link rel="import">.
    if '<script' in line or 'rel="import"' in line:
      if main_included:
        continue
      else:
        line = ''
        sys.stdout.write(_MAIN)
        main_included = True

    sys.stdout.write(line)

  # Create a BUILD time stamp for the target file.
  open(target + '.stamp', 'a').close()

def main(args):
  parser = optparse.OptionParser()

  parser.add_option('--source', help='Files app main.html source file.')
  parser.add_option('--target', help='Target fianl main.html for output.')

  options, _ = parser.parse_args(args)

  if options.source and options.target:
    GenerateHtml(options.source, options.target)
    return

  raise ValueError('Usage: all arguments are required.')

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))

