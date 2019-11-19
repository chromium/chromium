#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the FeaturePolicyFeature enum in enums.xml file with
values read from feature_policy.mojom.

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

  source_file = 'third_party/blink/public/mojom/feature_policy/' \
                'feature_policy_feature.mojom'
  UpdateHistogramEnum(histogram_enum_name='FeaturePolicyFeature',
                      source_enum_path=source_file,
                      start_marker='^enum FeaturePolicyFeature {',
                      end_marker='^};',
                      strip_k_prefix=True)
