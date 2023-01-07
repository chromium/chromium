# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper context wrapper for diagnosing crbug.com/1001171.

This module and all uses thereof can and should be removed once
crbug.com/1001171 has been resolved.
"""

from __future__ import print_function

import contextlib
import os
import sys


@contextlib.contextmanager
def DumpStateOnLookupError():
  """Prints potentially useful state info in the event of a LookupError."""
  try:
    yield
  except LookupError:
    print('LookupError diagnosis for crbug.com/1001171:')
    for path_index, path_entry in enumerate(sys.path):
      desc = 'unknown'
      if not os.path.exists(path_entry):
        desc = 'missing'
      elif os.path.islink(path_entry):
        desc = 'link -> %s' % os.path.realpath(path_entry)
      elif os.path.isfile(path_entry):
        desc = 'file'
      elif os.path.isdir(path_entry):
        desc = 'dir'
      print('  sys.path[%d]: %s (%s)' % (path_index, path_entry, desc))

      real_path_entry = os.path.realpath(path_entry)
      if (path_entry.endswith(os.path.join('lib', 'python2.7'))
          and os.path.isdir(real_path_entry)):
        encodings_dir = os.path.realpath(
            os.path.join(real_path_entry, 'encodings'))
        if os.path.exists(encodings_dir):
          if os.path.isdir(encodings_dir):
            print('    %s contents: %s' % (encodings_dir,
                                           str(os.listdir(encodings_dir))))
          else:
            print('    %s exists but is not a directory' % encodings_dir)
        else:
          print('    %s missing' % encodings_dir)

    raise
