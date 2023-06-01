# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit script for changes affecting tools/json_data_generator/

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""
import os


TEST_PATTERNS = [r'.+_test.py$']


def _CommonChecks(input_api, output_api):
    env = os.environ
    pythonpath = [os.path.join(os.getcwd(), '..')]
    if 'PYTHONPATH' in env:
        pythonpath.append(env.get('PYTHONPATH'))
    env['PYTHONPATH'] = input_api.os_path.pathsep.join((pythonpath))

    return input_api.canned_checks.RunUnitTestsInDirectory(
        input_api, output_api, '.', files_to_check=TEST_PATTERNS, env=env)


def CheckChangeOnUpload(input_api, output_api):
    return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
    return _CommonChecks(input_api, output_api)
