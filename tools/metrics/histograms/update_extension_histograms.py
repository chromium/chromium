#!/usr/bin/env python3
# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the ExtensionEvents and ExtensionFunctions enums in histograms.xml
with values read from extension_event_histogram_value.h and
extension_function_histogram_value.h.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import os
import sys

from update_histogram_enum import UpdateHistogramEnum

if __name__ == '__main__':
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  histograms = (
    ('ExtensionEvents',
     'extensions/browser/extension_event_histogram_value.h'),
    ('ExtensionFunctions',
     'extensions/browser/extension_function_histogram_value.h'))
  for enum_name, source_header in histograms:
    UpdateHistogramEnum(
        'tools/metrics/histograms/metadata/extensions/enums.xml',
        histogram_enum_name=enum_name,
        source_enum_path=source_header,
        start_marker='^enum HistogramValue {',
        end_marker='^ENUM_BOUNDARY',
        calling_script=os.path.basename(__file__))
