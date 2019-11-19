# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the various BadMessage enums in histograms.xml file with values read
from the corresponding bad_message.h files.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

from __future__ import print_function

import sys

from update_histogram_enum import UpdateHistogramEnum

if __name__ == '__main__':
  if len(sys.argv) > 1:
    print('No arguments expected!', file=sys.stderr)
    sys.stderr.write(__doc__)
    sys.exit(1)

  histograms = {
    'chrome/browser/bad_message.h': 'BadMessageReasonChrome',
    'content/browser/bad_message.h': 'BadMessageReasonContent',
    'components/guest_view/browser/bad_message.h': 'BadMessageReasonGuestView',
    'components/nacl/browser/bad_message.h': 'BadMessageReasonNaCl',
    'components/password_manager/content/browser/bad_message.h':
      'BadMessageReasonPasswordManager',
    'extensions/browser/bad_message.h': 'BadMessageReasonExtensions',
  }

  for header_file, histogram_name in histograms.items():
    UpdateHistogramEnum(histogram_enum_name=histogram_name,
                        source_enum_path=header_file,
                        start_marker='^enum (class )?BadMessageReason {',
                        end_marker='^BAD_MESSAGE_MAX')
