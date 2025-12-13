#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A utility for generating builder classes for Private Metrics entries.

It takes as input Private Metrics XML config files describing all of the entries
and metrics, and produces C++ header and implementation files exposing builders
for those entries and metrics.
"""

import argparse
import sys

import dkm_model
import dkm_builders_template
import dwa_model
import dwa_builders_template
import dwa_decode_template

parser = argparse.ArgumentParser(
    description='Generate Private Metrics builders')
parser.add_argument('--input-dkm', help='Path to dkm.xml')
parser.add_argument('--input-dwa', help='Path to dwa.xml')
parser.add_argument('--output', help='Path to generated files.')


def main():
  args = parser.parse_args()

  relpath = 'components/metrics/private_metrics/'
  relpath_dwa = 'components/metrics/dwa/'

  # DKM
  if args.input_dkm:
    data_dkm = read_dkm(args.input_dkm)
    dkm_builders_template.WriteFiles(args.output, relpath, data_dkm)

  # DWA
  if args.input_dwa:
    data_dwa = read_dwa(args.input_dwa)
    dwa_builders_template.WriteFiles(args.output, relpath_dwa, data_dwa)
    dwa_decode_template.WriteFiles(args.output, relpath_dwa, data_dwa)

  return 0


def read_dkm(path):
  return read_xml(path, dkm_model.DKM_XML_TYPE)


def read_dwa(path):
  return read_xml(path, dwa_model.DWA_XML_TYPE)


def read_xml(path, xml_type):
  with open(path) as file:
    return xml_type.Parse(file.read())


if '__main__' == __name__:
  sys.exit(main())
