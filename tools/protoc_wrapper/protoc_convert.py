# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script for use by GN to encode/decode proto files. The protoc tool
requires using stdin/stdout for the --encode/--decode options, but that form
of processing is not supported by GN.
"""

import argparse
import subprocess

def Main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--protoc', help='Path to protoc compiler.')
  parser.add_argument('--infile', required=True,
                      help='Path to input file that will be used as stdin.')
  parser.add_argument('--outfile', required=True,
                      help='Path to output file that will be used as stdout.')
  args, passthrough_args = parser.parse_known_args()

  stdin = open(args.infile, 'r')
  stdout = open(args.outfile, 'w')

  subprocess.check_call([args.protoc] + passthrough_args, stdin=stdin,
      stdout=stdout)


if __name__ == '__main__':
  Main()
