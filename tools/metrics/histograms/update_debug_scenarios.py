# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the DebugScenario enums in histograms with values read from the
corresponding header file.

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

  UpdateHistogramEnum('tools/metrics/histograms/metadata/stability/enums.xml',
                      histogram_enum_name='DebugScenario',
                      source_enum_path='content/common/debug_utils.h',
                      start_marker='^enum class ?DebugScenario {',
                      end_marker='^kMaxValue',
                      calling_script=os.path.basename(__file__))
