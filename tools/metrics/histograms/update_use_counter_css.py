#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Scans the Chromium source of UseCounter, formats the Feature enum for
histograms.xml and merges it. This script can also generate a python code
snippet to put in uma.py of Chromium Dashboard. Make sure that you review the
output for correctness.
"""

import optparse
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
import path_util

import update_histogram_enum


USE_COUNTER_MOJOM_PATH = 'third_party/blink/public/mojom/use_counter/'\
                         'metrics/css_property_id.mojom'


def EnumToCssProperty(enum_name):
  """Converts a camel cased enum name to the lower case CSS property."""
  # The first group also searches for uppercase letters to account for single
  # uppercase letters, such as in "ZIndex" that need to convert to "z-index".

  # Special case total page measured for backward compat.
  if enum_name == "TotalPagesMeasured":
    return "Total Pages Measured"

  return re.sub(r'([a-zA-Z])([A-Z])', r'\1-\2', enum_name).lower()


def ReadCssProperties(filename):
  # Read the file as a list of lines
  with open(path_util.GetInputFile(filename)) as f:
    content = f.readlines()

  # Looking for a single line like "kFontWeight = 10,"
  ENUM_REGEX = re.compile(r"""k(\w+)\s*=       # capture the enum name
                              \s*(\d+),?       # capture the id
                              """, re.VERBOSE)

  properties = {}
  for line in content:
    enum_match = ENUM_REGEX.search(line)
    if enum_match:
      enum_name = enum_match.group(1)
      property_id = int(enum_match.group(2))
      # Properties with id = 0 are invalid. Skip them.
      if property_id == 0:
        continue
      label = EnumToCssProperty(enum_name)
      if line.strip().startswith('//'):
        label += ' (obsolete)'
      properties[property_id] = label

  return properties


if __name__ == '__main__':
  update_histogram_enum.UpdateHistogramFromDict(
      'tools/metrics/histograms/enums.xml', 'MappedCSSProperties',
      ReadCssProperties(USE_COUNTER_MOJOM_PATH), USE_COUNTER_MOJOM_PATH,
      os.path.basename(__file__))
