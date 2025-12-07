#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import argparse

import dkm_model
import dwa_model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util


def main():
  """Pretty-prints Private Metrics' XML configuration files.

  Args:
    --non-interactive: (Optional) Does not print log info messages and does not
        prompt user to accept the diff.
    --presubmit: (Optional) Simply prints a message if the input is not
        formatted correctly instead of modifying the file.
    --diff: (Optional) Prints diff to stdout rather than modifying the file.
    --cleanup: (Optional) Removes any backup file created during the execution.

  Example usage:
    pretty_print.py --diff --cleanup
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('filepath', help="relative path to XML file")
  # The following optional flags are used by common/presubmit_util.py
  parser.add_argument('--non-interactive', action="store_true")
  parser.add_argument('--presubmit', action="store_true")
  parser.add_argument('--diff', action="store_true")
  parser.add_argument('--cleanup',
                      action="store_true",
                      help="Remove the backup file after a successful run.")

  args = parser.parse_args()

  filepath = args.filepath

  status = 0
  if filepath.endswith('dwa.xml'):
    status = presubmit_util.DoPresubmit(sys.argv, filepath, 'dwa.old.xml',
                                        dwa_model.PrettifyXML)
  elif filepath.endswith('dkm.xml'):
    status = presubmit_util.DoPresubmit(sys.argv, filepath, 'dkm.old.xml',
                                        dkm_model.prettify_xml)
  else:
    print(f'Unsupported file: {filepath}', file=sys.stderr)
    return 1

  return status

if __name__ == '__main__':
  sys.exit(main())
