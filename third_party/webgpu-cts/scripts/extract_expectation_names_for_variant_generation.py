#!/usr/bin/env vpython
#
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Given an expectations file (e.g. web_tests/WebGPUExpectations), extracts only
# the test name from each expectation (e.g. wpt_internal/webgpu/cts.html?...).

from os import path as os_path
import sys

try:
    old_sys_path = sys.path
    third_party_dir = os_path.dirname(
        os_path.dirname(os_path.dirname(os_path.abspath(__file__))))
    sys.path = old_sys_path + [os_path.join(third_party_dir, 'blink', 'tools')]

    from run_webgpu_cts import split_cts_expectations_and_web_test_expectations
finally:
    sys.path = old_sys_path

filename = sys.argv[1]
with open(filename) as f:
    expectations = split_cts_expectations_and_web_test_expectations(
        f.read())['web_test_expectations']['expectations']
    for expectation in expectations:
        if expectation.test:
            print(expectation.test)
