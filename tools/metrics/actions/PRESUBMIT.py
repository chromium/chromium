# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for actions.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""


def CheckChange(input_api, output_api):
  """Checks that actions.xml is up to date and pretty-printed."""
  for f in input_api.AffectedTextFiles():
    p = f.AbsoluteLocalPath()
    if (input_api.basename(p) == 'actions.xml'
        and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath()):
      cwd = input_api.os_path.dirname(p)
      exit_code = input_api.subprocess.call(
          [input_api.python3_executable, 'extract_actions.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                'tools/metrics/actions/actions.xml is not up to date or is not '
                'formatted correctly; run tools/metrics/actions/'
                'extract_actions.py to fix')
        ]
  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
