# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the ExtensionPermission3 enum in the histograms.xml file with values
read from api_permission.h.

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

  header_file = 'extensions/common/permissions/api_permission.h'
  UpdateHistogramEnum(histogram_enum_name='ExtensionPermission3',
                      source_enum_path=header_file,
                      start_marker='^enum ID {',
                      end_marker='^kEnumBoundary')
