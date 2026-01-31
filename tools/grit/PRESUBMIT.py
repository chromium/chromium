# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""grit unittests presubmit script.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts for
details on the presubmit API built into gcl.
"""

import os

def RunUnittests(input_api, output_api):
  # The presubmit tests are all run together with the module scheme set to
  # "flat" at the recipe level. But these presubmit tests run through
  # a test runner that parses the test as a "pyunit" test. The environment
  # variable forces the test runner to parse the tests as a flat test,
  # otherwise resultdb will give an error saying a field in the "flat" type
  # is unexpectedly not empty.
  os.environ['RESULTDB_MODULE_SCHEME'] = 'flat'
  presubmit_path = input_api.PresubmitLocalPath()
  return input_api.canned_checks.RunUnitTests(input_api, output_api, [
      input_api.os_path.join('grit', 'test_suite_all.py'),
      input_api.os_path.join(input_api.PresubmitLocalPath(),
                             'preprocess_if_expr_test.py')
  ])


def CheckChangeOnUpload(input_api, output_api):
  return RunUnittests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return RunUnittests(input_api, output_api)
