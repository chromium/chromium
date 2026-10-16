#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs glib's gen-visibility-macros.py with the in-tree glib version.

Usage is the same as gen-visibility-macros.py, minus the leading version
argument, e.g.:
  gen_visibility_macros.py visibility-macros GLIB out/glib-visibility.h
  gen_visibility_macros.py versions-macros gversionmacros.h.in out/gversionmacros.h
"""

import os
import runpy
import sys

import glib_version

sys.dont_write_bytecode = True

_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src',
                       'tools', 'gen-visibility-macros.py')

if __name__ == '__main__':
    sys.argv = [_SCRIPT, glib_version.get_version()] + sys.argv[1:]
    runpy.run_path(_SCRIPT, run_name='__main__')
