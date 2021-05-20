# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""Chromium presubmit script for src/tools/media_engagement_preload."""


def _RunMakeDafsaTests(input_api, output_api):
  """Runs unittest for make_dafsa if any related file has been modified."""
  files = ('tools/media_engagement_preload/make_dafsa.py',
           'tools/media_engagement_preload/make_dafsa_unittest.py')
  if not any(f in input_api.LocalPaths() for f in files):
    return []
  test_path = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                     'make_dafsa_unittest.py')
  cmd_name = 'make_dafsa_unittest'
  cmd = [input_api.python_executable, test_path]
  test_cmd = input_api.Command(
    name=cmd_name,
    cmd=cmd,
    kwargs={},
    message=output_api.PresubmitPromptWarning)
  return input_api.RunTests([test_cmd])


def CheckChangeOnUpload(input_api, output_api):
  return _RunMakeDafsaTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _RunMakeDafsaTests(input_api, output_api)
