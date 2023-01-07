# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Chromium presubmit script for src/tools/media_engagement_preload."""

# This line is 'magic' in that git-cl looks for it to decide whether to
# use Python3 instead of Python2 when running the code in this file.
USE_PYTHON3 = True


def _RunMakeDafsaTests(input_api, output_api):
  """Runs unittest for make_dafsa if any related file has been modified."""
  files = ('tools/media_engagement_preload/make_dafsa.py',
           'tools/media_engagement_preload/make_dafsa_unittest.py')
  if not any(f in input_api.LocalPaths() for f in files):
    return []

  return input_api.RunTests(
      input_api.canned_checks.RunUnitTestsInDirectory(
          input_api,
          output_api,
          input_api.PresubmitLocalPath(),
          files_to_check=['.*test\.py$'],
          run_on_python2=False,
          skip_shebang_check=True))


def CheckChangeOnUpload(input_api, output_api):
  return _RunMakeDafsaTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _RunMakeDafsaTests(input_api, output_api)
