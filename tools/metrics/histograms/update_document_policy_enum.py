#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the DocumentPolicyFeature enum in enums.xml file with
values read from document_policy_feature.mojom.

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

  source_file = 'third_party/blink/public/mojom/permissions_policy/' \
                'document_policy_feature.mojom'
  UpdateHistogramEnum('tools/metrics/histograms/enums.xml',
                      histogram_enum_name='DocumentPolicyFeature',
                      source_enum_path=source_file,
                      start_marker='^enum DocumentPolicyFeature {',
                      end_marker='^};',
                      strip_k_prefix=True,
                      calling_script=os.path.basename(__file__))
