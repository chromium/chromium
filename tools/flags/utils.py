# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A collection of functions used by other python files
"""

import os
import sys

ROOT_PATH = os.path.join(os.path.dirname(__file__), '..', '..')
PYJSON5_PATH = os.path.join(ROOT_PATH, 'third_party', 'pyjson5', 'src')

sys.path.append(PYJSON5_PATH)

import json5


def load_metadata():
  flags_path = os.path.join(ROOT_PATH, 'chrome', 'browser',
                            'flag-metadata.json')
  return json5.load(open(flags_path))


def keep_expired_by(flags, mstone):
  """Filter flags to contain only flags that expire by mstone.

  Only flags that either never expire or have an expiration milestone <= mstone
  are in the returned list.

  >>> keep_expired_by([{'expiry_milestone': 3}], 2)
  []
  >>> keep_expired_by([{'expiry_milestone': 3}], 3)
  [{'expiry_milestone': 3}]
  >>> keep_expired_by([{'expiry_milestone': -1}], 3)
  []
  """
  return [f for f in flags if -1 != f['expiry_milestone'] <= mstone]
