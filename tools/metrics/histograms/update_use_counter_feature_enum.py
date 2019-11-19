#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Scans the Chromium source of UseCounter, formats the Feature enum for
histograms.xml and merges it. This script can also generate a python code
snippet to put in uma.py of Chromium Dashboard. Make sure that you review the
output for correctness.
"""

from __future__ import print_function

import optparse
import os
import sys

from update_histogram_enum import ReadHistogramValues
from update_histogram_enum import UpdateHistogramEnum


def PrintEnumForDashboard(enum_dict):
  """Prints enum_items formatted for use in uma.py of Chromium dashboard."""
  for key in sorted(enum_dict.iterkeys()):
    print('  %d: \'%s\',' % (key, enum_dict[key]))


if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option('--for-dashboard', action='store_true', dest='dashboard',
                    default=False,
                    help='Print enum definition formatted for use in uma.py of '
                    'Chromium dashboard developed at '
                    'https://github.com/GoogleChrome/chromium-dashboard')
  options, args = parser.parse_args()

  source_path = 'third_party/blink/public/mojom/web_feature/web_feature.mojom'

  START_MARKER = '^enum WebFeature {'
  END_MARKER = '^kNumberOfFeatures'

  if options.dashboard:
    enum_dict, ignored = ReadHistogramValues(source_path, START_MARKER,
        END_MARKER, strip_k_prefix=True)
    PrintEnumForDashboard(enum_dict)
  else:
    UpdateHistogramEnum(
        histogram_enum_name='FeatureObserver',
        source_enum_path=source_path,
        start_marker=START_MARKER,
        end_marker=END_MARKER,
        strip_k_prefix=True)
