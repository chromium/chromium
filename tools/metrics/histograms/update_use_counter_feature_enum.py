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

if __name__ == '__main__':
  web_feature_source = \
    'third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom'

  START_MARKER = '^enum WebFeature {'
  END_MARKER = '^kNumberOfFeatures'

  UpdateHistogramEnum('tools/metrics/histograms/enums.xml',
                      histogram_enum_name='FeatureObserver',
                      source_enum_path=web_feature_source,
                      start_marker=START_MARKER,
                      end_marker=END_MARKER,
                      strip_k_prefix=True,
                      calling_script=os.path.basename(__file__))

  webdx_feature_source = \
      'third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom'
  WEBDX_START_MARKER = '^enum WebDXFeature {'

  UpdateHistogramEnum('tools/metrics/histograms/enums.xml',
                      histogram_enum_name='WebDXFeatureObserver',
                      source_enum_path=webdx_feature_source,
                      start_marker=WEBDX_START_MARKER,
                      end_marker=END_MARKER,
                      strip_k_prefix=True,
                      calling_script=os.path.basename(__file__))
