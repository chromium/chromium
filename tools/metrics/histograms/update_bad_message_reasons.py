# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the various BadMessage enums in histograms.xml file with values read
from the corresponding bad_message.h files.

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

  histograms = {
      'chrome/browser/bad_message.h': {
          'name': 'BadMessageReasonChrome',
      },
      'content/browser/bad_message.h': {
          'name': 'BadMessageReasonContent'
      },
      'components/guest_view/browser/bad_message.h': {
          'name': 'BadMessageReasonGuestView'
      },
      'components/nacl/browser/bad_message.h': {
          'name': 'BadMessageReasonNaCl'
      },
      'components/password_manager/content/browser/bad_message.h': {
          'name': 'BadMessageReasonPasswordManager'
      },
      'extensions/browser/bad_message.h': {
          'name': 'BadMessageReasonExtensions'
      },
  }

  for header_file, details in histograms.items():
    end_marker = details.get('end_marker', '^BAD_MESSAGE_MAX')
    strip_k_prefix = details.get('strip_k_prefix', False)
    UpdateHistogramEnum('tools/metrics/histograms/metadata/stability/enums.xml',
                        histogram_enum_name=details['name'],
                        source_enum_path=header_file,
                        start_marker='^enum (class )?BadMessageReason {',
                        end_marker=end_marker,
                        strip_k_prefix=strip_k_prefix,
                        calling_script=os.path.basename(__file__))
