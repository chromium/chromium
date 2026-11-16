#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the DocumentPolicyFeature enum in enums.xml file with
values read from document_policy_feature.mojom.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import os
import sys

import setup_modules  # pylint: disable=unused-import


import chromium_src.tools.metrics.histograms.update_histogram_enum as update_histogram_enum

if __name__ == '__main__':
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  source_file = (
    'third_party/blink/public/mojom/permissions_policy/'
    + 'document_policy_feature.mojom'
  )
  update_histogram_enum.UpdateHistogramEnum(
    'tools/metrics/histograms/metadata/blink/enums.xml',
    histogram_enum_name='DocumentPolicyFeature',
    source_enum_path=source_file,
    start_marker='^enum DocumentPolicyFeature {',
    end_marker='^};',
    strip_k_prefix=True,
    calling_script=os.path.basename(__file__),
  )
