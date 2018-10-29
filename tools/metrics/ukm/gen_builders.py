#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility for generating builder classes for UKM entries.

It takes as input a ukm.xml file describing all of the entries and metrics,
and produces a c++ header and implementation file exposing builders for those
entries and metrics.
"""

import argparse
import sys

import ukm_model
import builders_template
import decode_template

parser = argparse.ArgumentParser(description='Generate UKM entry builders')
parser.add_argument('--input', help='Path to ukm.xml')
parser.add_argument('--output', help='Path to generated files.')


def main(argv):
  args = parser.parse_args()
  data = ukm_model.UKM_XML_TYPE.Parse(open(args.input).read())
  relpath = 'services/metrics/public/cpp/'
  builders_template.WriteFiles(args.output, relpath, data)
  decode_template.WriteFiles(args.output, relpath, data)
  return 0


if '__main__' == __name__:
  sys.exit(main(sys.argv))
