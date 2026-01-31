# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Tests for demangler.py"""

import os
import pathlib
import sys
import unittest

_SRC_PATH = pathlib.Path(__file__).resolve().parents[4]
sys.path.append(str(_SRC_PATH / 'tools/android'))
from colabutils.memory_usage.demangler import Demangler


class DemanglerTest(unittest.TestCase):

    @unittest.skipIf(sys.platform == 'win32',
                     'llvm-cxxfilt is not fetched on Windows')
    def test_demangle(self):
        with Demangler() as demangler:
            mangled_name = '_Znwm'
            demangled_name = demangler.demangle(mangled_name)
            self.assertEqual(demangled_name, 'operator new(unsigned long)')


if __name__ == '__main__':
    unittest.main()
