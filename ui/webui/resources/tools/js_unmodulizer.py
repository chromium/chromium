# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates JS files that can be used as standard scripts rather than native
# JS modules, by removing module-only keywords:
# import --> Removes entire line
# export --> Removes "export" keyword only
# Appends a console warning about using pre-JS module files.
#
# This script is intentionally very simple and is intended only to support
# legacy UIs that are not served from a standard chrome:// URL and therefore
# need to inline all scripts into a single HTML file. It does not, for example,
# restore other deprecated patterns like cr.define().

import argparse
import io
import os
import re
import sys

_CWD = os.getcwd()

IMPORT_LINE_REGEX = 'import'
EXPORT_LINE_REGEX = 'export '


def ProcessFile(filename, out_filename):
  with io.open(filename, encoding='utf-8', mode='r') as f:
    lines = f.readlines()

    for i, line in enumerate(lines):
      # Since this tool is used to generate non-module files that are expected
      # to be inlined, any imported dependencies will be missing. Imports should
      # not be used by files passed to this tool.
      assert not re.match(IMPORT_LINE_REGEX, line), 'Unexpected import'
      lines[i] = line.replace(EXPORT_LINE_REGEX, '')

  lines.append(
      '  console.warn(\'crbug/1173575, non-JS module files deprecated.\');');

  # Reconstruct file.
  # Specify the newline character so that the exact same file is generated
  # across platforms.
  with io.open(out_filename, 'wb') as f:
    for l in lines:
      f.write(l.encode('utf-8'))
  return

def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--input_files', nargs='*', required=True)
  parser.add_argument('--output_files', nargs='*', required=True)
  parser.add_argument('--in_folder', required=True)
  parser.add_argument('--out_folder', required=True)
  args = parser.parse_args(argv)

  in_folder = os.path.normpath(os.path.join(_CWD, args.in_folder))
  out_folder = os.path.normpath(os.path.join(_CWD, args.out_folder))

  index = 0
  for f in args.input_files:
    ProcessFile(os.path.join(in_folder, f),
                os.path.join(out_folder, args.output_files[index]))
    index = index + 1

if __name__ == '__main__':
  main(sys.argv[1:])
