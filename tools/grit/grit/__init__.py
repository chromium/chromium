# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''Package 'grit'
'''

from __future__ import print_function

import os
import sys


_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.insert(0, os.path.join(_SRC_PATH, 'third_party', 'six', 'src'))
