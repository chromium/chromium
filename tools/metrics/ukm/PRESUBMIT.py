# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for ukm.xml.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into gcl.
"""

UKM_XML = 'ukm.xml'


def CheckChange(input_api, output_api):
  """Checks that ukm.xml is pretty-printed and well-formatted."""
  for f in input_api.AffectedTextFiles():
    p = f.AbsoluteLocalPath()
    if (input_api.basename(p) == UKM_XML
        and input_api.os_path.dirname(p) == input_api.PresubmitLocalPath()):
      cwd = input_api.os_path.dirname(p)

      exit_code = input_api.subprocess.call(
          [input_api.python_executable, 'pretty_print.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                '%s is not prettified; run %s/pretty_print.py to fix.' %
                (UKM_XML, input_api.PresubmitLocalPath())),
        ]

      exit_code = input_api.subprocess.call(
          [input_api.python_executable, 'validate_format.py', '--presubmit'],
          cwd=cwd)
      if exit_code != 0:
        return [
            output_api.PresubmitError(
                '%s does not pass format validation; run %s/validate_format.py '
                'and fix the reported error(s) or warning(s).' %
                (UKM_XML, input_api.PresubmitLocalPath())),
        ]

  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)
