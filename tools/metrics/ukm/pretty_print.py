#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import argparse

import ukm_model

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import presubmit_util



def main():
  """Pretty-prints the Chrome UKM events in ukm.xml file.

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
  # The following optional flags are used by common/presubmit_util.py
  parser.add_argument('--non-interactive', action="store_true")
  parser.add_argument('--presubmit', action="store_true")
  parser.add_argument('--diff', action="store_true")
  parser.add_argument('--cleanup',
                      action="store_true",
                      help="Remove the backup file after a successful run.")

  presubmit_util.DoPresubmitMain(sys.argv, 'ukm.xml', 'ukm.old.xml',
                                 ukm_model.PrettifyXmlAndTrimObsolete)


if __name__ == '__main__':
  sys.exit(main())
