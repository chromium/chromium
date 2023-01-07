#!/usr/bin/env python
# Copyright 2017 The Chromium Authors
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
  data = ReadFilteredData(args.input)
  relpath = 'services/metrics/public/cpp/'
  builders_template.WriteFiles(args.output, relpath, data)
  decode_template.WriteFiles(args.output, relpath, data)
  return 0


def ReadFilteredData(path):
  """Reads data from path and filters out any obsolete metrics.

  Parses data from given path and removes all nodes that contain an
  <obsolete> tag. First iterates through <event> nodes, then <metric>
  nodes within them.

  Args:
    path: The path of the XML data source.

  Returns:
    A dict of the data not including any obsolete events or metrics.
  """
  with open(path) as ukm_file:
    data = ukm_model.UKM_XML_TYPE.Parse(ukm_file.read())
    event_tag = ukm_model._EVENT_TYPE.tag
    metric_tag = ukm_model._METRIC_TYPE.tag
    data[event_tag] = list(filter(ukm_model.IsNotObsolete, data[event_tag]))
    for event in data[event_tag]:
      event[metric_tag] = list(
          filter(ukm_model.IsNotObsolete, event[metric_tag]))
    return data


if '__main__' == __name__:
  sys.exit(main(sys.argv))
