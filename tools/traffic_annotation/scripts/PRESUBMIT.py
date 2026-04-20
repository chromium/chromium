# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckRunUnitTests(input_api, output_api):
  presubmit_path = input_api.PresubmitLocalPath()

  tests = [
      input_api.os_path.join(presubmit_path, 'annotation_tokenizer_test.py'),
      input_api.os_path.join(presubmit_path, 'extractor_test.py'),
      input_api.os_path.join(presubmit_path, 'auditor', 'auditor_test.py'),
  ]

  return input_api.canned_checks.RunUnitTests(
      input_api,
      output_api,
      tests,
      run_on_python2=False,
      run_on_python3=True,
  )


def CheckChangeOnUpload(input_api, output_api):
  return CheckRunUnitTests(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckRunUnitTests(input_api, output_api)
