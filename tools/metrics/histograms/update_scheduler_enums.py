#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the WebSchedulerTrackedFeature enum in enums.xml file with
values read from web_scheduler_tracked_feature.h.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import os
import sys

from update_histogram_enum import UpdateHistogramEnum

XML_FILE = 'tools/metrics/histograms/metadata/navigation/enums.xml'
ENUM_NAME = 'WebSchedulerTrackedFeature'
SOUCRE_FILE = 'third_party/blink/public/mojom/scheduler/' \
  'web_scheduler_tracked_feature.mojom'
START_MARKER = r'^enum WebSchedulerTrackedFeature \{'
END_MARKER = r'^\};'
SCRIPT = os.path.basename(__file__)

if __name__ == '__main__':
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  UpdateHistogramEnum(XML_FILE,
                      histogram_enum_name=ENUM_NAME,
                      source_enum_path=SOUCRE_FILE,
                      start_marker=START_MARKER,
                      end_marker=END_MARKER,
                      strip_k_prefix=True,
                      calling_script=SCRIPT)
