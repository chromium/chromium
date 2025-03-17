#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import node
import os
import sys

_HERE_PATH = os.path.dirname(__file__)


def main(argv):
  parser = argparse.ArgumentParser()
  parser.add_argument('--expected_version_file', required=True)
  parser.add_argument('--out_file', required=True)
  args = parser.parse_args(argv)

  node.RunNode([
      os.path.join(_HERE_PATH, 'check_version.js'),
      '--expected_version_file', args.expected_version_file,
  ])

  # If the above script succeeded, write a dummy output file, since Ninja
  # requires every target to have an output.
  with open(args.out_file, "w") as file:
    file.write("OK")

if __name__ == '__main__':
  main(sys.argv[1:])
