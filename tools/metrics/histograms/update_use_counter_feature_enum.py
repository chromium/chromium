#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors
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


def PrintEnumForDashboard(dictionary):
  """Prints dictionary formatted for use in uma.py of Chromium dashboard."""
  for key, value in sorted(dictionary.items()):
    print('  %d: \'%s\',' % (key, value))


if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option('--for-dashboard', action='store_true', dest='dashboard',
                    default=False,
                    help='Print enum definition formatted for use in uma.py of '
                    'Chromium dashboard developed at '
                    'https://github.com/GoogleChrome/chromium-dashboard')
  options, args = parser.parse_args()

  source_path = 'third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom'

  START_MARKER = '^enum WebFeature {'
  END_MARKER = '^kNumberOfFeatures'

  if options.dashboard:
    enum_dict = ReadHistogramValues(source_path,
                                    START_MARKER,
                                    END_MARKER,
                                    strip_k_prefix=True)
    PrintEnumForDashboard(enum_dict)
  else:
    UpdateHistogramEnum(
        histogram_enum_name='FeatureObserver',
        source_enum_path=source_path,
        start_marker=START_MARKER,
        end_marker=END_MARKER,
        strip_k_prefix=True,
        calling_script=os.path.basename(__file__))
