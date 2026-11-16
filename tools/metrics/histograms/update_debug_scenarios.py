# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Updates the DebugScenario enums in histograms with values read from the
corresponding header file.

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

  update_histogram_enum.UpdateHistogramEnum(
    'tools/metrics/histograms/metadata/stability/enums.xml',
    histogram_enum_name='DebugScenario',
    source_enum_path='content/common/debug_utils.h',
    start_marker='^enum class ?DebugScenario {',
    end_marker='^kMaxValue',
    calling_script=os.path.basename(__file__),
  )
