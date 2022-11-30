# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check the basic functionalities of WPT tools.

This PRESUBMIT guards against rolling a broken version of WPT tooling. It does
some smoke checks of WPT functionality.
"""

USE_PYTHON3 = True

def _TestWPTLint(input_api, output_api):
  # We test 'wpt lint' by deferring to the web_tests/external presubmit test,
  # which runs 'wpt lint' against web_tests/external/wpt.
  abspath_to_test = input_api.os_path.join(
    input_api.change.RepositoryRoot(),
    'third_party', 'blink', 'web_tests', 'external', 'PRESUBMIT_test.py'
  )
  command = input_api.Command(
    name='web_tests/external/PRESUBMIT_test.py',
    cmd=[abspath_to_test],
    kwargs={},
    message=output_api.PresubmitError,
    python3=True
  )
  if input_api.verbose:
    print('Running ' + abspath_to_test)
  return input_api.RunTests([command])


def _TestWPTManifest(input_api, output_api):
  # We test 'wpt manifest' by making a copy of the base manifest and updating
  # it. A copy is used so that this PRESUBMIT doesn't change files in the tree.
  blink_path = input_api.os_path.join(
      input_api.change.RepositoryRoot(), 'third_party', 'blink')

  base_manifest = input_api.os_path.join(
      blink_path, 'web_tests', 'external', 'WPT_BASE_MANIFEST_8.json')
  with input_api.CreateTemporaryFile(mode = 'wt') as f:
    f.write(input_api.ReadFile(base_manifest))
    f.close()

    wpt_exec_path = input_api.os_path.join(
        input_api.change.RepositoryRoot(), 'third_party', 'wpt_tools', 'wpt', 'wpt')
    external_wpt = input_api.os_path.join(
        blink_path, 'web_tests', 'external', 'wpt')
    try:
      input_api.subprocess.check_output(
          ['python3', wpt_exec_path, 'manifest', '--no-download',
           '--path', f.name, '--tests-root', external_wpt])
    except input_api.subprocess.CalledProcessError as exc:
      return [output_api.PresubmitError('wpt manifest failed:', long_text=exc.output)]

  return []


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results += _TestWPTLint(input_api, output_api)
  results += _TestWPTManifest(input_api, output_api)
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results += _TestWPTLint(input_api, output_api)
  results += _TestWPTManifest(input_api, output_api)
  return results
