# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckChangeOnUpload(*args):
  return _CommonChecks(*args)


def CheckChangeOnCommit(*args):
  return _CommonChecks(*args)


def _CommonChecks(input_api, output_api):
  cwd = input_api.PresubmitLocalPath()
  path = input_api.os_path
  files = [path.basename(f.LocalPath()) for f in input_api.AffectedFiles()]

  if any(f for f in files if f.startswith('clean_json_attrs')):
    tests = [path.join(cwd, 'clean_json_attrs_test.py')]
    return input_api.canned_checks.RunUnitTests(input_api, output_api, tests)

  return []
