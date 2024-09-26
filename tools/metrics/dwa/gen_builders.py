#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A utility for generating builder classes for DWA entries.

It takes as input a dwa.xml file describing all of the entries and metrics,
and produces a c++ header and implementation file exposing builders for those
entries and metrics.
"""

import argparse
import sys

import dwa_model
import builders_template
import decode_template

parser = argparse.ArgumentParser(description='Generate DWA entry builders')
parser.add_argument('--input', help='Path to dwa.xml')
parser.add_argument('--output', help='Path to generated files.')


def main(argv):
  args = parser.parse_args()
  data = ReadData(args.input)
  relpath = 'components/metrics/dwa/'
  builders_template.WriteFiles(args.output, relpath, data)
  decode_template.WriteFiles(args.output, relpath, data)
  return 0


def ReadData(path):
  with open(path) as dwa_file:
    return dwa_model.DWA_XML_TYPE.Parse(dwa_file.read())


if '__main__' == __name__:
  sys.exit(main(sys.argv))
