#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Given an expectations file (e.g. web_tests/WebGPUExpectations), extracts only
# the test name from each expectation (e.g. wpt_internal/webgpu/cts.html?...).

import sys
from os import path as os_path

try:
    old_sys_path = sys.path
    third_party_dir = os_path.dirname(
        os_path.dirname(os_path.dirname(os_path.abspath(__file__))))
    sys.path = old_sys_path + [os_path.join(third_party_dir, 'blink', 'tools')]

    from blinkpy.common import path_finder
finally:
    sys.path = old_sys_path

path_finder.add_typ_dir_to_sys_path()

from typ.expectations_parser import TaggedTestListParser

filename = sys.argv[1]
with open(filename) as f:
    parser = TaggedTestListParser(f.read())
    for test_expectation in parser.expectations:
        if test_expectation.test:
            print test_expectation.test
