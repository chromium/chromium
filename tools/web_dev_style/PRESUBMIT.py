# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

USE_PYTHON3 = True


def CheckChangeOnUpload(*args):
  return _CommonChecks(*args)


def CheckChangeOnCommit(*args):
  return _CommonChecks(*args)


def _CommonChecks(input_api, output_api):
  tests = ['test_suite.py']

  return input_api.canned_checks.RunUnitTests(input_api,
                                              output_api,
                                              tests,
                                              run_on_python2=False)
