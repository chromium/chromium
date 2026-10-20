#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Location of the hermetic JDK. Prints JAVA_HOME when run as a script."""

import os
import sys

JAVA_HOME = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'current')
if sys.platform == 'darwin':
    JAVA_HOME = os.path.join(JAVA_HOME, 'Contents', 'Home')

if __name__ == '__main__':
    print(JAVA_HOME)
