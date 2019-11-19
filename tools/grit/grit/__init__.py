# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Package 'grit'
'''

from __future__ import print_function

import os
import sys


_CUR_DIR = os.path.abspath(os.path.dirname(__file__))
_GRIT_DIR = os.path.dirname(_CUR_DIR)
_THIRD_PARTY_DIR = os.path.join(_GRIT_DIR, 'third_party')

if _THIRD_PARTY_DIR not in sys.path:
  sys.path.insert(0, _THIRD_PARTY_DIR)
